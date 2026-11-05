#ifndef  TCPSERVER_H
#define  TCPSERVER_H

#include "telemetry/protocol_parser.h"
#include <atomic>
#include <thread>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>
#define PORT 10000
#define BUF_SIZE 1024

struct ClientState {
    // 发送状态
    std::string send_buf;
    std::size_t send_offset = 0;

    // 接收状态
    std::string recv_buf;

    // 协议解析器
    telemetry::ProtocolParser parser;
};

namespace telemetry{
#define TCP_OPEN_MAX 10
constexpr std::size_t MAX_SEND_BUFFER = 64 * 1024;

class TCPServer{
public:
    TCPServer();
    ~TCPServer();
    int init(int port);
    int Bind(struct sockaddr_in addr);
    int Listen(unsigned int backlog);
private:
    void run();
    int set_nonblocking(int fd);
   // void close_client(pollfd* pfds, int& nfds, int i);
    void close_client(pollfd* pfds,ClientState *states ,int& nfds, int i);
    int m_sockFd{-1};
    std::atomic<bool> _stopping{false};
    std::thread m_thread;
    int inited{-1};
};

} //namespace telemetry
#endif 