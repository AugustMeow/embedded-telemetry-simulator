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

telemetry::TCPServer::TCPServer()
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

    #if 0
    struct sockaddr_in addr;
    #if 1
    m_sockFd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_sockFd < 0) {
        perror("socket");
        return -1;
    }
    #endif

    /* 2. 绑定地址 */
    int opt = 1;
    setsockopt(m_sockFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); /* 快速重用端口 */
    
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);  /* 监听本机所有网卡 */
    addr.sin_port = htons(port);

    if (bind(m_sockFd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(m_sockFd);
        m_sockFd = -1;
        perror("bind");
        return 1;
    }

    /* 3. 开始监听 */
    if (listen(m_sockFd, 1) < 0) {
        close(m_sockFd);
        m_sockFd = -1;
        perror("listen");
        return 1;
    }
#if 0
    int oflags = 0;
    oflags = fcntl(m_sockFd, F_GETFL);
    oflags |= O_NONBLOCK;
    fcntl(m_sockFd, F_SETFL, oflags);
#endif
    printf("server listening on port %d ...\n", port);
#endif

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

#if 0
// 删除第 i 个客户端：用最后一个覆盖，O(1) 且保持数组紧凑
void telemetry::TCPServer::close_client(pollfd* pfds, int& nfds, int i) {
    ::close(pfds[i].fd);
    pfds[i] = pfds[nfds - 1];
    --nfds;
}
#endif

void telemetry::TCPServer::close_client(pollfd* pfds,ClientState *states ,int& nfds, int i) {
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



void telemetry::TCPServer::run()
{

	struct pollfd client_fd[TCP_OPEN_MAX+1];
	//维护多个client的发送状态
	struct ClientState states[TCP_OPEN_MAX+1];

	client_fd[0].fd = m_sockFd;
	client_fd[0].events = POLLIN;
	client_fd[0].revents = 0;

	telemetry::Protocol protocol;
	int connectFd = -1;
	int nfds = 1;
	char buf[BUF_SIZE];
	constexpr std::size_t MAX_RECV_BUFFER = 65536;
	//socklen_t addr_len = sizeof(struct sockaddr_in);
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
				client_fd[nfds].fd = connectFd;
				client_fd[nfds].events = POLLIN;
				client_fd[nfds].revents = 0;
				nfds++;
				std::printf("+ %s:%d (fd=%d, clients=%d)\n",
						inet_ntoa(cli.sin_addr), ntohs(cli.sin_port),
						connectFd, nfds - 1);

			}
		}



		// 3. 接收客户端数据
		for (int i = 1; i < nfds; ) {
#if 0
			std::cout << "[POLL] fd=" << client_fd[i].fd
				<< " events=" << client_fd[i].events
				<< " revents=" << client_fd[i].revents
				<< std::endl;
#endif
			//每个客户端的状态标识
			bool client_closed = false;
			short re = client_fd[i].revents;
			if (re == 0) { ++i; continue; }

			if(re & POLLNVAL){
				std::printf("- fd=%d closed (error)\n", client_fd[i].fd);
				close_client(client_fd,states, nfds,i);
				continue;
			}
			if(re & POLLOUT){
#if 0
				std::cout << "[POLLOUT] fd=" << client_fd[i].fd
					<< " pending="
					<< states[i].send_buf.size() - states[i].send_offset
					<< std::endl;
#endif
				//std::cout << "i= " << i <<"send_len "<< states[i].send_len << std::endl;
				while (states[i].send_offset < states[i].send_buf.size()){
					ssize_t m = ::send(client_fd[i].fd, states[i].send_buf.c_str() + states[i].send_offset, states[i].send_buf.size() - states[i].send_offset, MSG_NOSIGNAL);
					if (m < 0) {
						std::cerr << "[CLOSE] reason=SEND_ERROR"
							<< " fd=" << client_fd[i].fd
							<< " errno=" << errno
							<< std::endl;
						if (errno == EINTR) 
							continue;
						/*遇到EAGAN直接退出等待下次事件*/
						if (errno == EAGAIN || errno == EWOULDBLOCK) {
							std::cout << "[DEBUG] EAGAIN, pending="
								<< states[i].send_buf.size()
								- states[i].send_offset
								<< std::endl;
							break;
						}

						close_client(client_fd,states, nfds,i);
						client_closed = true;
						break;
					}
					if(m == 0){
						std::cerr << "[CLOSE] reason=SEND_ZERO"
							<< " fd=" << client_fd[i].fd
							<< std::endl;
						close_client(client_fd,states, nfds,i);
						client_closed = true;
						break;
					}
					//std::cout << "[SEND OK] bytes=" << m << std::endl;
					//std::cout << "send to" << i << " " << std::string(states[i].send_buf.c_str(),m) << std::endl;
					states[i].send_offset += m;
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
			}
			if (!client_closed && (re & POLLIN)) {
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
					//std::printf("- fd=%d disconnected\n", client_fd[i].fd);
					close_client(client_fd,states, nfds,i);
					continue;
				}
				if (n == 0) {
					//std::printf("- fd=%d disconnected\n", client_fd[i].fd);
					close_client(client_fd, states, nfds, i);
					continue;
				}
				const std::size_t incoming = static_cast<std::size_t>(n);
#if 0
				std::cout << "[DEBUG] fd=" << client_fd[i].fd
					<< " pending=" << pending
					<< " incoming=" << incoming
					<< " limit=" << MAX_SEND_BUFFER
					<< std::endl;
#endif

				//1.检查接收缓冲区
				if (states[i].recv_buf.size() > MAX_RECV_BUFFER ||
						incoming > MAX_RECV_BUFFER - states[i].recv_buf.size()) {

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
						#if 0
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
					#if 0
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
#if 0
				if(n > 0){
					if (states[i].send_buf.size()
							- states[i].send_offset
							+ static_cast<std::size_t>(n)
							> MAX_SEND_BUFFER) {
						std::cout << "[WARN] Client send buffer overflow, fd="
							<< client_fd[i].fd << std::endl;
						close_client(client_fd, states, nfds, i);
						continue;
					}
					states[i].send_buf.append(buf,n);
					client_fd[i].events |= POLLOUT;
				}
#endif

				//std::cout << "recv from " << i << " " << std::string(buf,n) << std::endl;
				// 回显：非阻塞 send 要处理部分写
#if 0
				ssize_t off = 0;
				while (off < n) {
					ssize_t m = ::send(client_fd[i].fd, buf + off, n - off, MSG_NOSIGNAL);
					if (m < 0) {
						if (errno == EINTR) 
							continue;
						if (errno == EAGAIN || errno == EWOULDBLOCK) {
							// 暂时无法发送，后续需要完善
							break;
						}
						close_client(client_fd, nfds, i);
						break;
					}
					if(m == 0){
						close_client(client_fd, nfds, i);
						break;

					}
					off += m;
				}
				++i;
				continue;
#endif
			}
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

int telemetry::TCPServer::set_nonblocking(int fd)
{
     int flags = fcntl(fd, F_GETFL, 0);
     if(flags < 0){
        return -1;
     }
     if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return -1;
    }
}
