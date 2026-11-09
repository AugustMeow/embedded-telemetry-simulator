#include "telemetry/tcpserverEpoll.h"
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
#include <sys/epoll.h>
#include <unordered_map>

namespace telemetry{
std::atomic<uint64_t> compact_count{0};
std::atomic<uint64_t> compact_bytes{0};
telemetry::TCPServerEpoll::TCPServerEpoll(Logger &logger):_logger(logger)
{

}

telemetry::TCPServerEpoll::~TCPServerEpoll()
{
     _stopping.store(true);
    if (m_thread.joinable()) {
        m_thread.join();
    }
    if(m_epollFd >= 0 ){
        close(m_epollFd);
        m_epollFd = -1;
    }
	if(m_sockFd >= 0 ){
        close(m_sockFd);
        m_sockFd = -1;
    }

    std::cout << "TCPServerEpoll thread exited" << std::endl;
}

int telemetry::TCPServerEpoll::init(int port)
{
     if(inited > 0){
        perror("repeat init");
        return -1;
    }
	if(port < 0 || port >= 65535){
        errno = EINVAL;
        return -1;
    }
    m_sockFd = socket(AF_INET, SOCK_STREAM, 0);
    if(m_sockFd < 0){
		perror("socket");
		return -1;
	}
    struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);  /* 监听本机所有网卡 */
    addr.sin_port = htons(port);

    if (Bind(addr) < 0) {
		perror("bind");
    	close(m_sockFd);
    	m_sockFd = -1;
		return -1;
	}

    m_epollFd = epoll_create1(EPOLL_CLOEXEC);
    if (m_epollFd < 0) {
        perror("epoll_create1");
		close(m_sockFd);
    	m_sockFd = -1;
        return -1;
    }
	//SOMAXCONN 系统建议的最大backlog值
	if (Listen(SOMAXCONN) < 0) {
    	close(m_sockFd);
    	m_sockFd = -1;
		close(m_epollFd);
		m_epollFd = -1;
    	perror("listen");
    	return -1;
	}
	if(set_nonblocking(m_sockFd) < 0){
		perror("set_nonblocking");
		close(m_sockFd);
    	m_sockFd = -1;
		close(m_epollFd);
		m_epollFd = -1;
		return -1;
	}
    printf("epoll server listening on port %d ...\n", port);
    m_thread = std::thread(&TCPServerEpoll::run,this);
    inited = 1;
    return 0;
}

int telemetry::TCPServerEpoll::Bind(sockaddr_in addr)
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

int telemetry::TCPServerEpoll::Listen(unsigned int backlog)
{
    if (m_sockFd < 0 || m_epollFd < 0) {
        errno = EBADF;
        return -1;
    }

    if (listen(m_sockFd, backlog) < 0) {
        perror("listen");
        return -1;
    }

    struct epoll_event ev{};

    ev.events = EPOLLIN;
    ev.data.fd = m_sockFd;

    if (epoll_ctl(
            m_epollFd,
            EPOLL_CTL_ADD,
            m_sockFd,
            &ev) < 0) {
        perror("epoll_ctl");
        return -1;
    }

    return 0;
}

void TCPServerEpoll::run()
{
    constexpr int MAX_EVENTS = 128;

    struct epoll_event events[MAX_EVENTS];

	while(!_stopping.load()){
		int n = epoll_wait(m_epollFd, events, MAX_EVENTS, 500);
		if (_stopping.load()) {
			break;
		}
		if(n < 0){
			if(errno == EINTR){
				continue;
			}
			perror("epoll wait");
			break;
		}
		if(n == 0){
			continue;
		}
		for(int i = 0; i < n; ++i){
			int fd = events[i].data.fd;
			if(fd == m_sockFd){
				//实现accept
				accept_clients();
			}
			else{

				handle_client_event(fd, events[i].events);
			}
		}
	}
}

bool TCPServerEpoll::update_read_pause(int fd, ClientState &state)
{
    assert(state.send_offset <= state.send_buf.size());

	const std::size_t pending =
		state.send_buf.size() - state.send_offset;

	if (!state.pause && pending >= HIGH_WATERMARK) {

		state.pause = true;
		++state.high_watermark_count;
		_logger.log(
				LogLevel::Debug,
				"[HIGH WATERMARK] fd=" +
				std::to_string(fd) +
				" pending=" +
				std::to_string(pending)
				);
	}
	else if (state.pause && pending <= LOW_WATERMARK) {

		state.pause = false;
		++state.low_watermark_count;
		_logger.log(
				LogLevel::Debug,
				"[LOW WATERMARK] fd=" +
				std::to_string(fd) +
				" pending=" +
				std::to_string(pending)
				);
	}

	epoll_event ev{};
	ev.data.fd = fd;
	// 根据当前状态重新构建完整事件集合
	if(!state.pause && !state.client_close){
		ev.events |= EPOLLIN;
	}
	if(pending > 0){
		//ev.events 默认是0 不能用 ev.events &= ~EPOLLIN
		ev.events |= EPOLLOUT;
	}
	if(epoll_ctl(m_epollFd, EPOLL_CTL_MOD, fd, &ev) < 0){
		_logger.log(
            LogLevel::Error,
            "[EPOLL_MOD_ERROR] fd=" +
            std::to_string(fd) +
            " errno=" +
            std::to_string(errno)
        );
		return false;
	};
	return true;
}

void telemetry::TCPServerEpoll::accept_clients()
{
    while (true) {

        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);

        int client_fd = accept(
            m_sockFd,
            reinterpret_cast<sockaddr*>(&client_addr),
            &addr_len
        );

        if (client_fd < 0) {

            if (errno == EINTR) {
                continue;
            }

            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }

            _logger.log(
                LogLevel::Warn,
                "[ACCEPT_ERROR] errno=" + std::to_string(errno)
            );
            break;
        }

        if (set_nonblocking(client_fd) < 0) {
            close(client_fd);
            continue;
        }

        // 创建客户端状态
        auto result = m_clients.emplace(
            client_fd,
            ClientState{}
        );

        if (!result.second) {
            close(client_fd);
            continue;
        }

        // 注册客户端可读事件
        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = client_fd;

        if (epoll_ctl(
                m_epollFd,
                EPOLL_CTL_ADD,
                client_fd,
                &ev
            ) < 0) {

            _logger.log(
                LogLevel::Warn,
                "[EPOLL_ADD_ERROR] fd=" +
                std::to_string(client_fd) +
                " errno=" + std::to_string(errno)
            );

            m_clients.erase(client_fd);
            close(client_fd);
            continue;
        }

        _logger.log(
            LogLevel::Info,
            "[ACCEPT] fd=" + std::to_string(client_fd)
        );
    }
}

void TCPServerEpoll::handle_client_event(
    int fd,
    uint32_t events)
{	
    auto it = m_clients.find(fd);

    if (it == m_clients.end()) {
        return;
    }

    ClientState& state = it->second;

    if (events & EPOLLIN) {

        RecvStatus status = recv_client(fd, state);

        switch (status) {
            case RecvStatus::Drained:
            case RecvStatus::Paused:
                break;
            case RecvStatus::PeerClosed:
                {
					state.client_close = true;
					const std::size_t pending =
							 state.send_buf.size()
							 - state.send_offset;

					if (pending == 0) {
						close_client(fd, state);
						return;
					}
					if(!update_read_pause(fd, state)){
						close_client(fd, state);
						return;
					}
					return;
				}

            case RecvStatus::Error:
                // 关闭客户端
				close_client(fd, state);
                return;
        }

    }

	if (events & EPOLLOUT) {
		//空缓冲区不处理
		if(!state.send_buf.empty()){


			SendStatus status = send_nonblocking(fd, state.send_buf.c_str(), 
					state.send_buf.size(), state.send_offset);
			switch(status){
				case SendStatus::WouldBlock:
					{
						const std::size_t pending =
							state.send_buf.size() - state.send_offset;
						_logger.log(
								LogLevel::Debug,
								"[EAGAIN] fd=" +
								std::to_string(fd) +
								" pending=" +
								std::to_string(pending) +
								" offset=" +
								std::to_string(state.send_offset)
								);
					}
					break;
				case SendStatus::Error:
					{
						_logger.log(
								LogLevel::Warn,
								"[CLOSE] reason=SEND_ERROR " +
								std::to_string(fd) +
								" errno=" + std::to_string(errno)
								);
						close_client(fd,state);
						return;
					}
				case SendStatus::Complete:
					break;
			}

			if(state.send_offset == state.send_buf.size()){
				state.send_buf.clear();
				state.send_offset = 0;
			}
			else{
				compact_send_buffer(state);
			}
		}
		if(!update_read_pause(fd, state)){
			close_client(fd, state);
			return;
		}
		_logger.log(
				LogLevel::Debug,
				"[CHECK_RESUME]"
				" fd=" + std::to_string(fd) +
				" pause=" + std::to_string(state.pause) +
				" recv_size=" + std::to_string(state.recv_buf.size()) +
				" send_pending=" + std::to_string(
					state.send_buf.size() - state.send_offset
					)
				);
		if (!state.pause && !state.recv_buf.empty()) {

			bool ok = process_buffer_until_blocked(fd, state);

			if (!ok) {
				close_client(fd, state);
				return;
			}
			if(!update_read_pause(fd, state)){
				close_client(fd, state);
				return;
			}

		}

		const std::size_t pending = state.send_buf.size() - state.send_offset;
		//FIN 延迟关闭
		if (state.client_close &&
				pending == 0 
				&& state.recv_buf.empty()) {
			close_client(fd, state);
			return;
		}
	}
}

void TCPServerEpoll::compact_send_buffer(ClientState &state)
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

int TCPServerEpoll::set_nonblocking(int fd)
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

void TCPServerEpoll::close_client(int fd, ClientState& state)
{
    auto it = m_clients.find(fd);

    if (it == m_clients.end()) {
        return;
    }

    const std::size_t pending =
        state.send_buf.size() - state.send_offset;

    std::cout
        << " client_close=" << state.client_close
        << " pause=" << state.pause
        << " recv_size=" << state.recv_buf.size()
        << " send_pending=" << pending
        << " HIGH=" << state.high_watermark_count
        << " LOW=" << state.low_watermark_count
        << " RESUME=" << state.resumed_frame_count
        << std::endl;

    if (epoll_ctl(m_epollFd, EPOLL_CTL_DEL, fd, nullptr) < 0) {
        if (errno != ENOENT && errno != EBADF) {
            _logger.log(
                LogLevel::Warn,
                "[EPOLL_DEL_ERROR] fd=" +
                std::to_string(fd) +
                " errno=" + std::to_string(errno)
            );
        }
    }
    ::close(fd);
    m_clients.erase(it);
}

RecvStatus TCPServerEpoll::recv_client(int fd, ClientState &state)
{
    char buffer[BUF_SIZE];
	constexpr std::size_t MAX_RECV_BUFFER = 65536;
	while(true){
		ssize_t n = recv(fd, buffer, sizeof(buffer),0);
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
					std::to_string(fd) +
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
						std::to_string(fd) +
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
					"[DEBUG] fd=" + std::to_string(fd) + " incoming="
					+ std::to_string(incoming) 
					+ " limit=" 
					+ std::to_string(MAX_RECV_BUFFER));
			return RecvStatus::Error;
		}
		state.recv_buf.append(buffer, incoming);
		while(true){
			ParseStatus result = process_recv_buffer(fd, state);
			if(result == ParseStatus::Error){
				return RecvStatus::Error;
			}
			if(result == ParseStatus::NeedMore){
				break;	
			}
			//如果epoll_ctl失败但state.pause 状态已经修改，可能导致错误
			if(!update_read_pause(fd, state))
			{
				return RecvStatus::Error; 
			}
			if(state.pause){
				return RecvStatus::Paused;
			}
		}
	}
}

ParseStatus TCPServerEpoll::process_recv_buffer(int fd, ClientState &state)
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
				std::to_string(fd)
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
				std::to_string(fd)
				);
		return ParseStatus::Error;
	}
	//追加完整协议帧
	state.send_buf.append(reinterpret_cast<const char*>(header),
			telemetry::HEADER_SIZE);
	state.send_buf.append(msg.payload);

	return ParseStatus::MessageReady;
}

bool TCPServerEpoll::process_buffer_until_blocked(int fd, ClientState &state)
{
	_logger.log(
			LogLevel::Debug,
			"[PROCESS_BUFFER_ENTER]"
			" fd=" + std::to_string(fd) +
			" recv_size=" + std::to_string(state.recv_buf.size())
			);
	while (true) {

		ParseStatus status =
			process_recv_buffer(fd, state);

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
					" fd=" + std::to_string(fd) +
					" remaining=" + std::to_string(state.recv_buf.size())
					);
		}

		if (!update_read_pause(fd, state)) {
			return false;
		}

		if (state.pause) {
			return true;
		}
	}
	return false;
}
SendStatus TCPServerEpoll::send_nonblocking(int fd, const char *data, size_t len, size_t &offset)
{
	// offset: 已成功发送的字节数，由调用者维护。
	if (offset > len || (data == nullptr && len > 0)) {
		return SendStatus::Error;
	}
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

}
