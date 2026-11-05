#include "telemetry/application.h"
#include "telemetry/validator.h"
#include "telemetry/tcpserver.h"

#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
namespace telemetry {


TelemetryApplication::TelemetryApplication(Logger &logger):_logger(logger)
{
	
}

TelemetryApplication::~TelemetryApplication()
{
}

int TelemetryApplication::init()
{	
	#if 0
	int listenfd, connfd;
    struct sockaddr_in addr;

    /* 1. 创建 socket */
    _listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (_listenfd < 0) {
        perror("socket");
        return 1;
    }

    /* 2. 绑定地址 */
    int opt = 1;
    setsockopt(_listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); /* 快速重用端口 */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);  /* 监听本机所有网卡 */
    addr.sin_port = htons(PORT);

    if (bind(_listenfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }

    /* 3. 开始监听 */
    if (listen(_listenfd, 1) < 0) {
        perror("listen");
        return 1;
    }
    printf("server listening on port %d ...\n", PORT);

    /* 4. 接受一个连接（单连接） */
    _connfd = accept(_listenfd, NULL, NULL);
    if (_connfd < 0) {
        perror("accept");
        return 1;
    }
    printf("client connected\n");
	#endif

	tcpserver = std::thread(&TelemetryApplication::receive, this);
    return 0;
}

void TelemetryApplication::process_reading(const Sensor &sensor, const SensorReading &reading, double min_value, double max_value, const std::string &unit)
{
	validStatus status = validate(reading, min_value, max_value);
	std::string message;
	char buf[64];
	std::snprintf(
			buf, sizeof(buf), "%.2f", reading.value);
	switch (status) {
		case validStatus::Valid:
			_logger.info(sensor.name() + "=" + buf + " " + unit);
			break;

		case validStatus::OutOfRange:
			_logger.warn(sensor.name() + "=" + buf + " " + unit + " out of range");
			break;
		case validStatus::ReadError:
			_logger.error(sensor.name() + " read failed: " + reading.error);
			break;

		case validStatus::InvalidRange:
			{
				char min[64],max[64];
				std::snprintf(
						buf, sizeof(min), "%.2f", min_value);
				std::snprintf(
						buf, sizeof(max), "%.2f", max_value);
				_logger.warn(sensor.name() + " InvalidRange: "+ " [" + min + "," + max + "]");
			}
			break;
	}
}
void TelemetryApplication::receive()
{
  	char buf[BUF_SIZE];
	/* 5. 循环收发：收到什么就回什么 */
    ssize_t n;
    while ((n = read(_connfd, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        printf("recv: %s", buf);
        write(_connfd, buf, n);          /* 原样回传 */
    }
    /* 6. 连接关闭 */
    printf("client disconnected\n");
    close(_connfd);
    close(_listenfd);
}
} // namespace
