#ifndef   CLIENTSTATE_H
#define   CLIENTSTATE_H
#include "telemetry/protocol_parser.h"
#include <cstring>

#define PORT 10000
#define BUF_SIZE (16 *1024)

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

namespace telemetry{
#define TCP_OPEN_MAX 10
constexpr std::size_t MAX_SEND_BUFFER = 64 * 1024;
constexpr std::size_t COMPACT_THRESHOLD = 4096;

constexpr std::size_t HIGH_WATERMARK = 48 * 1024;
constexpr std::size_t LOW_WATERMARK  = 16 * 1024;
}

#endif