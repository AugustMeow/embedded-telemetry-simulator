#ifndef   TCPSERVER_EPOLL_H
#define   TCPSERVER_EPOLL_H
#include "telemetry/protocol_parser.h"
#include "telemetry/logger.h"
#include "telemetry/clientstate.h"

#include <atomic>
#include <thread>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>
#include <unordered_map>

namespace telemetry{

class TCPServerEpoll{
public:
    TCPServerEpoll(Logger &logger);
    ~TCPServerEpoll();
    int init(int port);
    int Bind(struct sockaddr_in addr);
    int Listen(unsigned int backlog);
    
private:
    void run();
    //事件管理
    bool add_event(int fd, uint32_t events);

    bool modify_event(int fd, uint32_t events);

    void remove_event(int fd);

    bool update_read_pause(int fd, ClientState& state);

    void accept_clients();

    void handle_client_event(int fd, uint32_t events);

    void compact_send_buffer(ClientState &state);

    int set_nonblocking(int fd);

    void close_client(int fd,ClientState &states);

    RecvStatus recv_client(int fd, ClientState& state);

    ParseStatus process_recv_buffer(int fd, ClientState& state);

    bool process_buffer_until_blocked(int fd, ClientState& state);

    bool send_all(int fd, const char * data, size_t len);

    SendStatus send_nonblocking(int fd, const char* data, size_t len, size_t& offset);

    int m_sockFd{-1};
    int m_epollFd{-1};
    std::atomic<bool> _stopping{false};
    std::thread m_thread;
    int inited{-1};
    Logger& _logger;
    telemetry::Protocol _protocol;
    std::unordered_map<int, ClientState> m_clients;
};

} //namespace telemetry
#endif 