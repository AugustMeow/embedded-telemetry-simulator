#ifndef  PROTOCOL_PARSER_H
#define  PROTOCOL_PARSER_H
#include "telemetry/protocol.h"
namespace telemetry{

enum class ParseStatus {
    NeedMore,       // 数据不完整，等待下一次 recv
    MessageReady,   // 成功解析一条消息
    Error           // 协议错误
};

class ProtocolParser {
public:
    ParseStatus parse_one(
        std::string& buffer,
        Message& message
    );

private:
    Protocol _protocol;
};

}
#endif