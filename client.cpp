// g++ -std=c++17 -O2 -Wall client.cpp -o client
#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char** argv) {
    const char* ip   = argc > 1 ? argv[1] : "127.0.0.1";
    int         port = argc > 2 ? std::atoi(argv[2]) : 10000;

    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return 1; }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    if (::inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        std::fprintf(stderr, "bad ip: %s\n", ip);
        return 1;
    }
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        perror("connect");
        return 1;
    }

    std::printf("connected to %s:%d，输入文字回车发送，Ctrl-D 退出\n", ip, port);

    pollfd pfds[2];
    pfds[0].fd = STDIN_FILENO; pfds[0].events = POLLIN;
    pfds[1].fd = fd;           pfds[1].events = POLLIN;

    char buf[4096];
    for (;;) {
        if (::poll(pfds, 2, -1) < 0) {
            if (errno == EINTR) continue;
            perror("poll");
            break;
        }
        if (pfds[0].revents & POLLIN) {
            ssize_t n = ::read(STDIN_FILENO, buf, sizeof(buf));
            if (n <= 0) break;                      // Ctrl-D
            ::send(fd, buf, n, 0);
        }
        if (pfds[1].revents & POLLIN) {
            ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
            if (n <= 0) { std::printf("server closed\n"); break; }
            ::write(STDOUT_FILENO, buf, n);
        }
    }
    ::close(fd);
    return 0;
}
