#ifndef  TCPSERVER_H
#define  TCPSERVER_H

#include "telemetry/protocol_parser.h"
#include "telemetry/logger.h"
#include <atomic>
#include <thread>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>

#define PORT 10000
#define BUF_SIZE (16 *1024)

struct ClientState {
    // 发送状态
    std::string send_buf;
    std::size_t send_offset = 0;
    // 接收状态
    std::string recv_buf;
    // 协议解析器
    telemetry::ProtocolParser parser;
    bool client_close = false;
    bool pause = false;
    std::size_t high_watermark_count = 0;
    std::size_t low_watermark_count = 0;
    std::size_t resumed_frame_count = 0;
};

enum class RecvStatus {
    Drained,       // 读到 EAGAIN
    PeerClosed,    // 收到 FIN
    Error,         // 发生错误
    Paused         // 达到高水位，暂停接收
};

enum class ProcessStatus {
    NeedMore,       // 当前没有完整消息
    MessageReady,   // 成功处理一条完整消息
    Error           // 协议错误或发送缓冲区溢出
};

enum class SendStatus {
    Complete,   // 全部发送完成
    WouldBlock, // 缓冲区暂时不可写
    Error       // 发送失败
};



namespace telemetry{
#define TCP_OPEN_MAX 10
constexpr std::size_t MAX_SEND_BUFFER = 64 * 1024;
constexpr std::size_t COMPACT_THRESHOLD = 4096;

constexpr std::size_t HIGH_WATERMARK = 48 * 1024;
constexpr std::size_t LOW_WATERMARK  = 16 * 1024;

class TCPServer{
public:
    TCPServer(Logger &logger);
    ~TCPServer();
    int init(int port);
    int Bind(struct sockaddr_in addr);
    int Listen(unsigned int backlog);
    
private:
    void run();
    void update_read_pause(pollfd& pfd, ClientState& state);
    void compact_send_buffer(ClientState &state);
    int set_nonblocking(int fd);
    bool check_socket_error(int fd);
    void close_client(pollfd* pfds,ClientState *states ,int& nfds, int i);
    RecvStatus recv_client(pollfd & pfds, ClientState& state);
    ParseStatus process_recv_buffer(pollfd& pfd, ClientState& state);
    bool process_buffer_until_blocked( pollfd& pfd, ClientState& state);

    bool send_all(int fd, const char * data, size_t len);

    SendStatus send_nonblocking(int fd, const char* data, size_t len, size_t& offset);

    int m_sockFd{-1};
    std::atomic<bool> _stopping{false};
    std::thread m_thread;
    int inited{-1};
    Logger& _logger;
    telemetry::Protocol _protocol;
};

} //namespace telemetry
#endif 