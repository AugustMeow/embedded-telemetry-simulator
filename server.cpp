// g++ -std=c++17 -O2 -Wall server.cpp -o server
#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

constexpr int kPort      = 10000;
constexpr int kMaxClients = 1024;   // 最大并发客户端数
constexpr int kBufSize    = 4096;

void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("fcntl");
        std::exit(EXIT_FAILURE);
    }
}

// 删除第 i 个客户端：用最后一个覆盖，O(1) 且保持数组紧凑
void close_client(pollfd* pfds, int& nfds, int i) {
    ::close(pfds[i].fd);
    pfds[i] = pfds[nfds - 1];
    --nfds;
}

}  // namespace

int main() {
    // 客户端提前断开时，send 不要把进程打死
    std::signal(SIGPIPE, SIG_IGN);

    int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) { perror("socket"); return 1; }

    int on = 1;
    ::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(kPort);

    if (::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }
    if (::listen(listen_fd, 128) < 0) {
        perror("listen");
        return 1;
    }
    set_nonblocking(listen_fd);   // listen fd 也设为非阻塞，accept 用循环收干净

    pollfd pfds[kMaxClients + 1]; // [0] 固定是监听 fd
    int nfds = 1;
    pfds[0].fd     = listen_fd;
    pfds[0].events = POLLIN;

    std::printf("server listening on port %d\n", kPort);

    char buf[kBufSize];
    for (;;) {
        int ready = ::poll(pfds, nfds, -1);   // -1 = 无限等待
        if (ready < 0) {
            if (errno == EINTR) continue;     // 被信号打断，继续
            perror("poll");
            return 1;
        }

        // 1) 处理新连接
        if (pfds[0].revents & POLLIN) {
            for (;;) {
                sockaddr_in cli{};
                socklen_t   len = sizeof(cli);
                int fd = ::accept(listen_fd, reinterpret_cast<sockaddr*>(&cli), &len);
                if (fd < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) break; // 已收完
                    perror("accept");
                    break;
                }
                if (nfds >= kMaxClients + 1) {
                    std::printf("too many clients, rejecting %s:%d\n",
                                inet_ntoa(cli.sin_addr), ntohs(cli.sin_port));
                    ::close(fd);
                    continue;
                }
                set_nonblocking(fd);
                pfds[nfds].fd     = fd;
                pfds[nfds].events = POLLIN;
                pfds[nfds].revents = 0;
                ++nfds;
                std::printf("+ %s:%d (fd=%d, clients=%d)\n",
                            inet_ntoa(cli.sin_addr), ntohs(cli.sin_port),
                            fd, nfds - 1);
            }
        }

        // 2) 处理已连接客户端
        for (int i = 1; i < nfds; ) {
            short re = pfds[i].revents;
            if (re == 0) { ++i; continue; }

            if (re & (POLLERR | POLLHUP | POLLNVAL)) {
                std::printf("- fd=%d closed (error)\n", pfds[i].fd);
                close_client(pfds, nfds, i);
                continue;
            }

            if (re & POLLIN) {
                ssize_t n = ::recv(pfds[i].fd, buf, sizeof(buf), 0);
                if (n <= 0) {
                    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                        ++i;
                        continue;
                    }
                    std::printf("- fd=%d disconnected\n", pfds[i].fd);
                    close_client(pfds, nfds, i);
                    continue;
                }

                // 回显：非阻塞 send 要处理部分写
                ssize_t off = 0;
                while (off < n) {
                    ssize_t m = ::send(pfds[i].fd, buf + off, n - off, MSG_NOSIGNAL);
                    if (m < 0) {
                        if (errno == EINTR) continue;
                        break;   // 对端异常，交给下一轮 poll 关闭
                    }
                    off += m;
                }
                ++i;
                continue;
            }
            ++i;
        }
    }
}
