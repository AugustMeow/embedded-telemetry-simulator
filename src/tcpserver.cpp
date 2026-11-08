#include "telemetry/tcpserver.h"
#include "telemetry/protocol.h"
#include "telemetry/protocol_parser.h"
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <string>
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdexcept>
#include <cassert>
std::atomic<uint64_t> compact_count{0};
std::atomic<uint64_t> compact_bytes{0};

telemetry::TCPServer::TCPServer(Logger &logger):_logger(logger)
{

}

telemetry::TCPServer::~TCPServer()
{
     _stopping.store(true);
    if (m_thread.joinable()) {
        m_thread.join();
    }
    if(m_sockFd >= 0 ){
        close(m_sockFd);
        m_sockFd = -1;
    }

    std::cout << "TCPServer thread exited" << std::endl;
}

int telemetry::TCPServer::init(int port)
{
	if(inited > 0){
		perror("repeat init");
		return -1;
	}
	m_sockFd = socket(AF_INET, SOCK_STREAM, 0);
	if(m_sockFd < 0)
		throw std::runtime_error("create socket failed");

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);  /* 监听本机所有网卡 */
	if(port < 0 || port >= 65535){
		close(m_sockFd);
		m_sockFd = -1;
		return -1;
	}
	addr.sin_port = htons(port);

	if (Bind(addr) < 0) {
		close(m_sockFd);
		m_sockFd = -1;
		perror("bind");
		return -1;
	}

	//SOMAXCONN 系统建议的最大backlog值
	if (Listen(SOMAXCONN) < 0) {
		close(m_sockFd);
		m_sockFd = -1;
		perror("listen");
		return -1;
	}
	set_nonblocking(m_sockFd);
	printf("server listening on port %d ...\n", port);
	m_thread = std::thread(&TCPServer::run,this);
	inited = 1;
	return 0;
}

int telemetry::TCPServer::Bind(sockaddr_in addr)
{
	int opt = 1;
	/* 快速重用端口 */
	if(setsockopt(m_sockFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0){
		perror("setsockopt");
		return -1;
	}
	if (bind(m_sockFd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		return -1;
	}
	return 0;
}

int telemetry::TCPServer::Listen(unsigned int backlog)
{
	if(listen(m_sockFd,backlog)){
		return -1;
	}
	return 0;
}

// 删除第 i 个客户端：用最后一个覆盖，O(1) 且保持数组紧凑
void telemetry::TCPServer::close_client(pollfd* pfds,ClientState *states ,int& nfds, int i) {
	if (i <= 0 || i >= nfds) {
		return;
	}
	std::cout << "[CLOSE_CLIENT]"
		<< " fd=" << pfds[i].fd
		<< " index=" << i
		<< " send_size=" << states[i].send_buf.size()
		<< " send_offset=" << states[i].send_offset
		<< " recv_size=" << states[i].recv_buf.size()
		<<  "compact_count: " << compact_count
		<< "compact_bytes: "  << compact_bytes
		<< std::endl;

	std::cout
		<< " client_close=" << states[i].client_close
		<< " pause=" << states[i].pause
		<< " recv_size=" << states[i].recv_buf.size()
		<< " send_pending="
		<< states[i].send_buf.size() - states[i].send_offset
		<< " HIGH=" << states[i].high_watermark_count
		<< " LOW=" << states[i].low_watermark_count
		<< " RESUME=" << states[i].resumed_frame_count
		<< std::endl;
	::close(pfds[i].fd);

	int last = nfds - 1;
	/*如果删除的不是最后一个，就将最后一个客户端移动过来 */
	if(i != last){
		pfds[i] = pfds[last];
		states[i] = std::move(states[last]);
	}
	states[last] = ClientState{};
	pfds[last] = pollfd{};
	--nfds;
}

RecvStatus telemetry::TCPServer::recv_client(pollfd & pfds, ClientState &state)
{
	//if(fd < 0) return RecvStatus::Error;
	char buffer[BUF_SIZE];
	constexpr std::size_t MAX_RECV_BUFFER = 65536;
	while(true){
		ssize_t n = recv(pfds.fd, buffer, sizeof(buffer),0);
		if(n < 0){
			if(errno == EINTR){
				continue;
			}
			if(errno == EAGAIN || errno == EWOULDBLOCK){
				return RecvStatus::Drained;
			}

			_logger.log(
					LogLevel::Warn,
					"[RECV_ERROR] fd=" +
					std::to_string(pfds.fd) +
					" errno=" +
					std::to_string(errno)
					);

			return RecvStatus::Error;
		}
		if(n == 0){
			if(!state.recv_buf.empty()){
				_logger.log(
						LogLevel::Warn,
						"[INCOMPLETE_FRAME] fd=" +
						std::to_string(pfds.fd) +
						" recv_size=" +
						std::to_string(state.recv_buf.size())
						);

				return RecvStatus::Error;
			}
			return RecvStatus::PeerClosed;
		}
		const std::size_t incoming = static_cast<std::size_t>(n);

		if(state.recv_buf.size() > MAX_RECV_BUFFER ||
				incoming > MAX_RECV_BUFFER - state.recv_buf.size()){
			_logger.log(LogLevel::Debug,
					"[DEBUG] fd=" + std::to_string(pfds.fd) + " incoming="
					+ std::to_string(incoming) 
					+ " limit=" 
					+ std::to_string(MAX_RECV_BUFFER));
			return RecvStatus::Error;
		}
		state.recv_buf.append(buffer, incoming);
		while(true){
			ParseStatus result = process_recv_buffer(pfds, state);
			if(result == ParseStatus::Error){
				return RecvStatus::Error;
			}
			if(result == ParseStatus::NeedMore){
				break;	
			}
			//成功处理一条消息
			pfds.events |= POLLOUT;
			update_read_pause(pfds, state);
			if(state.pause){
				return RecvStatus::Paused;
			}
		}
	}
}

telemetry::ParseStatus telemetry::TCPServer::process_recv_buffer(pollfd &pfds, ClientState &state)
{
	telemetry::Message msg;
	auto status = state.parser.parse_one(state.recv_buf, msg);
	if(status == telemetry::ParseStatus::NeedMore){
		return ParseStatus::NeedMore;
	}
	if(status == telemetry::ParseStatus::Error){
		_logger.log(
				LogLevel::Warn,
				"[PROTOCOL_ERROR] fd=" +
				std::to_string(pfds.fd)
				);
		return ParseStatus::Error;
	}
	uint8_t header[telemetry::HEADER_SIZE]{};
	bool ret = _protocol.encoder_header(header,
			static_cast<uint8_t>(msg.type),
			static_cast<uint32_t>(msg.payload.size())
			);

	if (!ret) {
		return ParseStatus::Error;
	}
	//检查发送缓冲区
	const std::size_t pending =
		state.send_buf.size() - state.send_offset;

	const std::size_t frame_size =
		telemetry::HEADER_SIZE + msg.payload.size();

	if (pending > MAX_SEND_BUFFER ||
			frame_size > MAX_SEND_BUFFER - pending) {
		_logger.log(
				LogLevel::Warn,
				"[SEND_OVERFLOW] fd=" +
				std::to_string(pfds.fd)
				);
		return ParseStatus::Error;
	}
	//追加完整协议帧
	state.send_buf.append(reinterpret_cast<const char*>(header),
			telemetry::HEADER_SIZE);
	state.send_buf.append(msg.payload);

	return ParseStatus::MessageReady;
}

bool telemetry::TCPServer::process_buffer_until_blocked(
		pollfd& pfd,
		ClientState& state)
{
	_logger.log(
			LogLevel::Debug,
			"[PROCESS_BUFFER_ENTER]"
			" fd=" + std::to_string(pfd.fd) +
			" recv_size=" + std::to_string(state.recv_buf.size())
			);
	while (true) {

		ParseStatus status =
			process_recv_buffer(pfd, state);

		if (status == ParseStatus::Error) {
			return false;
		}

		if (status == ParseStatus::NeedMore) {
			return true;
		}
		if (status == ParseStatus::MessageReady) {

			++state.resumed_frame_count;

			_logger.log(
					LogLevel::Debug,
					"[BUFFER_MESSAGE_READY]"
					" fd=" + std::to_string(pfd.fd) +
					" remaining=" + std::to_string(state.recv_buf.size())
					);
		}
		// 成功处理了一条消息
		pfd.events |= POLLOUT;

		update_read_pause(pfd, state);

		if (state.pause) {
			return true;
		}
	}
}

//阻塞式一次发送所有数据
bool telemetry::TCPServer::send_all(int fd, const char *data, size_t len)
{
	size_t n = 0;
	ssize_t offset = 0;
	while(n < len){
		offset = send(fd, data + offset, len - offset, 0);
		if(offset < 0){
			if(errno == EINTR){
				continue;
			}
			if(errno == EAGAIN){
				return false;
			}
		}
		if(offset == 0){
			return false;
		}
		n += offset;
	}
	return true;
}

SendStatus telemetry::TCPServer::send_nonblocking(int fd, const char *data, size_t len, size_t &offset)
{
	while(offset < len){
		ssize_t n = send(fd, data + offset, len - offset, MSG_NOSIGNAL);
		if(n < 0){
			if(errno == EINTR){
				continue;
			}
			if(errno == EAGAIN || errno == EWOULDBLOCK){
				return SendStatus::WouldBlock;
			}
			return SendStatus::Error;
		}
		if( n == 0){
			return SendStatus::Error;
		}
		offset += static_cast<size_t>(n);
	}
    return SendStatus::Complete;
}

void telemetry::TCPServer::run()
{

	//维护多个client的发送状态
	struct pollfd client_fd[TCP_OPEN_MAX+1];
	struct ClientState states[TCP_OPEN_MAX+1];

	client_fd[0].fd = m_sockFd;
	client_fd[0].events = POLLIN;
	client_fd[0].revents = 0;

	telemetry::Protocol protocol;
	int connectFd = -1;
	int nfds = 1;
	while(!_stopping.load()){
		int ret = poll(client_fd, nfds, 500);

		if (_stopping.load()) {
			break;
		}

		if (ret < 0) {
			if (errno == EINTR) {
				continue;
			}

			perror("poll");
			break;
		}

		if (ret == 0) {
			continue;
		}

		if(client_fd[0].revents & POLLIN){
			for(;;){

				sockaddr_in cli{};
				socklen_t   len = sizeof(cli);
				connectFd = accept(m_sockFd, reinterpret_cast<sockaddr*>(&cli), &len);
				if (connectFd < 0)
				{
					if (errno == EINTR) {
						continue;
					}

					if (errno == EAGAIN ||
							errno == EWOULDBLOCK) {
						break;
					}

					perror("accept");
					break;
				}
				if(nfds >= TCP_OPEN_MAX + 1){
					std::printf("too many clients, rejecting %s:%d\n",
							inet_ntoa(cli.sin_addr), ntohs(cli.sin_port));
					close(connectFd);
					continue;
				};
				if(set_nonblocking(connectFd) < 0){
					perror("set client nonblocking");
					close(connectFd);
					continue;
				};

				int sndbuf = 8192;

				if (setsockopt(
							connectFd,
							SOL_SOCKET,
							SO_SNDBUF,
							&sndbuf,
							sizeof(sndbuf)) < 0) {

					perror("setsockopt SO_SNDBUF");
					close(connectFd);
					continue;
				}


				client_fd[nfds].fd = connectFd;
				client_fd[nfds].events = POLLIN;
				client_fd[nfds].revents = 0;
				nfds++;
				std::string ip(inet_ntoa(cli.sin_addr));
				_logger.log(LogLevel::Info,"+ " + ip + ":"
						+ std::to_string(ntohs(cli.sin_port)) + " fd= "
						+ std::to_string(connectFd) + ",clients="
						+ std::to_string(nfds-1));

			}
		}



		// 3. 接收客户端数据
		for (int i = 1; i < nfds; ) {
			//每个客户端的状态标识
			bool client_closed = false;
			short re = client_fd[i].revents;
			if (re == 0) { ++i; continue; }

			if(re & POLLNVAL){
				std::printf("- fd=%d closed (error)\n", client_fd[i].fd);
				close_client(client_fd,states, nfds,i);
				continue;
			}
			if (re & POLLERR) {
				check_socket_error(client_fd[i].fd);
				close_client(client_fd, states, nfds, i);
				continue;
			}
			if(re & POLLOUT){
#if 0
				std::cout << "[POLLOUT] fd=" << client_fd[i].fd
					<< " pending="
					<< states[i].send_buf.size() - states[i].send_offset
					<< std::endl;
#endif
				SendStatus status = send_nonblocking(client_fd[i].fd, states[i].send_buf.c_str(), 
						states[i].send_buf.size(), states[i].send_offset);
				switch(status){
					case SendStatus::WouldBlock:
						{
							const std::size_t pending =
								states[i].send_buf.size() - states[i].send_offset;
							_logger.log(
									LogLevel::Debug,
									"[EAGAIN] fd=" +
									std::to_string(client_fd[i].fd) +
									" pending=" +
									std::to_string(pending) +
									" offset=" +
									std::to_string(states[i].send_offset)
									);
						}
						break;
					case SendStatus::Error:
						{
							_logger.log(
									LogLevel::Warn,
									"[CLOSE] reason=SEND_ERROR " +
									std::to_string(client_fd[i].fd) +
									" errno=" + std::to_string(errno)
									);
							close_client(client_fd,states, nfds,i);
							client_closed = true;
						}
						break;
					case SendStatus::Complete:
						break;
				}
				if(client_closed){
					continue;
				}
				//发送完毕后再清理缓冲区
				if (states[i].send_offset == states[i].send_buf.size()) {
					states[i].send_buf.clear();
					states[i].send_offset = 0;
					/*位运算, 不影响其他事件插入*/
					client_fd[i].events &= ~POLLOUT;
				}
				else{
					compact_send_buffer(states[i]);
				}
				update_read_pause(client_fd[i], states[i]);
				_logger.log(
						LogLevel::Debug,
						"[CHECK_RESUME]"
						" fd=" + std::to_string(client_fd[i].fd) +
						" pause=" + std::to_string(states[i].pause) +
						" recv_size=" + std::to_string(states[i].recv_buf.size()) +
						" send_pending=" + std::to_string(
							states[i].send_buf.size() - states[i].send_offset
							)
						);
				if (!states[i].pause && !states[i].recv_buf.empty()) {

					bool ok = process_buffer_until_blocked(client_fd[i], states[i]);

					if (!ok) {
						close_client(client_fd, states, nfds, i);
						continue;
					}
				}
				// 确保新产生的响应能够发送
				if (!states[i].send_buf.empty()) {
					client_fd[i].events |= POLLOUT;
				}

				const std::size_t pending = states[i].send_buf.size() - states[i].send_offset;

				if (states[i].client_close &&
						pending == 0 
						&& states[i].recv_buf.empty()) {
					close_client(client_fd, states, nfds, i);
					continue;
				}
			}
			if (!client_closed && (re & POLLIN) && !states[i].pause && !states[i].client_close) {

				RecvStatus status = recv_client(
						client_fd[i],
						states[i]
						);
				switch (status) {

					case RecvStatus::Drained:
					case RecvStatus::Paused:
						++i;
						continue;

					case RecvStatus::PeerClosed: 
						{

							states[i].client_close = true;

							// 不再等待客户端发送数据
							client_fd[i].events &= ~POLLIN;

							const std::size_t pending =
								states[i].send_buf.size()
								- states[i].send_offset;

							if (pending == 0) {
								close_client(client_fd, states, nfds, i);
								continue;
							}

							// 还有数据需要发送
							client_fd[i].events |= POLLOUT;

							++i;
							continue;
						}

					case RecvStatus::Error:

						close_client(client_fd, states, nfds, i);
						continue;
				}
			}
#if 0
			if (!client_closed && (re & (POLLIN | POLLHUP))) {
				ssize_t n;
				do {
					n = ::recv(client_fd[i].fd, buf, sizeof(buf), 0);
				} while (n < 0 && errno == EINTR);
				if(n < 0){
					if (errno == EAGAIN || errno == EWOULDBLOCK) {
						std::cout << "[DEBUG] EAGAIN, pending="
							<< states[i].send_buf.size()
							- states[i].send_offset
							<< std::endl;
						++i;
						continue;
					}
					std::printf("- fd=%d disconnected\n", client_fd[i].fd);
					close_client(client_fd,states, nfds,i);
					continue;
				}
				if (n == 0) {
					if(re & POLLHUP){
						_logger.log(LogLevel::Warn, "[POLLUP] peer fully disconnected fd=" + std::to_string(client_fd[i].fd));
						close_client(client_fd, states, nfds, i);
						continue;
					}
					//std::printf("recv client FIN PACKAGE\n");
					if(!states[i].client_close){
						states[i].client_close = true;
						// 不再等待客户端发送数据
						client_fd[i].events &= ~POLLIN;
						std::printf("[FIN] client write side closed fd=%d\n",client_fd[i].fd);
						_logger.log(LogLevel::Warn,"[FIN] client write side closed fd= " + std::to_string(client_fd[i].fd));
					}
					const std::size_t pending = states[i].send_buf.size() - states[i].send_offset;
					// 如果还有未完成的协议数据，需要单独处理
					if (!states[i].recv_buf.empty()) {
						std::cerr << "[CLOSE] reason=INCOMPLETE_FRAME"
							<< " fd=" << client_fd[i].fd
							<< std::endl;

						close_client(client_fd, states, nfds, i);
						continue;
					}

					// 没有待发送数据，可以关闭连接
					if (pending == 0) {
						_logger.log(LogLevel::Info,"finish send buffer data");
						close_client(client_fd, states, nfds, i);
						continue;
					}

					++i;
					continue;
				}
				const std::size_t incoming = static_cast<std::size_t>(n);
				//1.检查接收缓冲区
				if (states[i].recv_buf.size() > MAX_RECV_BUFFER ||
						incoming > MAX_RECV_BUFFER - states[i].recv_buf.size()) {
					std::cout << "[DEBUG] fd=" << client_fd[i].fd
						<< " incoming=" << incoming
						<< " limit=" << MAX_SEND_BUFFER
						<< std::endl;
					close_client(client_fd, states, nfds, i);
					continue;
				}
				//2. 保存接收数据
				states[i].recv_buf.append(buf,incoming);
				//3. 尝试解析完整消息
				while(true){
					telemetry::Message msg;
					auto status = states[i].parser.parse_one(states[i].recv_buf, msg);
					if (status == telemetry::ParseStatus::NeedMore) {
#if 1
						std::cout << "[NEED MORE] recv_buffer="
							<< " fd=" << client_fd[i].fd
							<< " recv_size=" << states[i].recv_buf.size()
							<< std::endl;
#endif
						break;
					}

					if (status == telemetry::ParseStatus::Error) {

						std::cerr << "[CLOSE] reason=PROTOCOL_ERROR"
							<< " fd=" << client_fd[i].fd
							<< std::endl;

						close_client(client_fd, states, nfds, i);
						client_closed = true;
						break;
					}
					//4. 编码相应header
					uint8_t header[telemetry::HEADER_SIZE]{};
					bool ret = protocol.encoder_header(header,
							static_cast<uint8_t>(msg.type),
							static_cast<uint32_t>(msg.payload.size())
							);

					if (!ret) {
						close_client(client_fd, states, nfds, i);
						client_closed = true;
						break;
					}
					//5. 检查发送缓冲区
					const std::size_t pending =
						states[i].send_buf.size() - states[i].send_offset;

					const std::size_t frame_size =
						telemetry::HEADER_SIZE + msg.payload.size();

					if (pending > MAX_SEND_BUFFER ||
							frame_size > MAX_SEND_BUFFER - pending) {

						close_client(client_fd, states, nfds, i);
						client_closed = true;
						break;
					}
					//6. 追加完整协议帧
					states[i].send_buf.append(reinterpret_cast<const char*>(header), telemetry::HEADER_SIZE);
					states[i].send_buf.append(msg.payload);
#if 1
					std::cout << "[MESSAGE READY]" << " fd=" << client_fd[i].fd
						<< " payload=" << msg.payload
						<< std::endl;
#endif
					//7. 有数据需要发送才注册POLLOUT
					client_fd[i].events |= POLLOUT;

				}
				if(client_closed){
					continue;
				}
				update_read_pause(client_fd[i], states[i]);
			}
			#endif
#if 0
			//因为POLLIN和POLLHUP 可能同时出现，客户端发送完就关闭的情况
			if (re & (POLLERR | POLLHUP)) {
				std::cerr << "[CLOSE] reason=POLL_ERROR"
					<< " fd=" << client_fd[i].fd
					<< " revents=" << re
					<< std::endl;
				std::printf("- fd=%d closed (error)\n", client_fd[i].fd);
				close_client(client_fd,states, nfds,i);
				continue;
			}
#endif
			++i;
			continue;
		}
	}
	//遍历关闭所有客户端
	for (int i = 1; i < nfds; ++i) {
		if (client_fd[i].fd >= 0) {
			::close(client_fd[i].fd);
			client_fd[i].fd = -1;
		}
	}

	std::cout << "TCPServer run exit" << std::endl;
}

void telemetry::TCPServer::update_read_pause(
		pollfd& pfd,
		ClientState& state)
{
	assert(state.send_offset <= state.send_buf.size());

	const std::size_t pending =
		state.send_buf.size() - state.send_offset;

#if 0
	if (state.client_close) {
		return;
	}
#endif

	if (!state.pause && pending >= HIGH_WATERMARK) {

		state.pause = true;
		pfd.events &= ~POLLIN;
		++state.high_watermark_count;
		_logger.log(
				LogLevel::Debug,
				"[HIGH WATERMARK] fd=" +
				std::to_string(pfd.fd) +
				" pending=" +
				std::to_string(pending)
				);
	}
	else if (state.pause && pending <= LOW_WATERMARK) {

		state.pause = false;
		++state.low_watermark_count;
		//FIN 后不再恢复POLLIN
		if(!state.client_close){
			pfd.events |= POLLIN;
		}
		_logger.log(
				LogLevel::Debug,
				"[LOW WATERMARK] fd=" +
				std::to_string(pfd.fd) +
				" pending=" +
				std::to_string(pending)
				);
	}
}

void telemetry::TCPServer::compact_send_buffer(ClientState &state)
{
	if (state.send_offset == 0) {
		return;
	}

	if (state.send_offset == state.send_buf.size()) {
		state.send_buf.clear();
		state.send_offset = 0;
		return;
	}
	// 已发送部分达到压缩条件COMPACT_THRESHOLD
	if (state.send_offset >= HIGH_WATERMARK &&
			state.send_offset >= state.send_buf.size() / 2) {
		const std::size_t old_size = state.send_buf.size();
		const std::size_t sent = state.send_offset;

		state.send_buf.erase(0, state.send_offset);
		state.send_offset = 0;
		compact_count++;
		compact_bytes += sent;
		_logger.log(
				LogLevel::Info,
				"[COMPACT] old_size=" +
				std::to_string(old_size) +
				" sent=" +
				std::to_string(sent) +
				" new_size=" +
				std::to_string(state.send_buf.size())
				);
	}
}


int telemetry::TCPServer::set_nonblocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if(flags < 0){
		return -1;
	}
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
		return -1;
	}
	return 0;
}

bool telemetry::TCPServer::check_socket_error(int fd)
{
	int error = 0;
	socklen_t len = sizeof(error);

	if (getsockopt(fd, SOL_SOCKET, SO_ERROR,
				&error, &len) < 0) {
		return true;
	}

	if (error != 0) {
		_logger.log(
				LogLevel::Warn,
				"[POLLERR] fd=" + std::to_string(fd) +
				" error=" + std::to_string(error)
				);

		return true;
	}

	return false;
}
