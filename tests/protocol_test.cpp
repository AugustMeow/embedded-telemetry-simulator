#include <cassert>
#include <cstdio>
#if 0
int main()
{
    telemetry::Protocol protocol;

    uint8_t buffer[telemetry::HEADER_SIZE]{};
    // 编码
    bool ret = protocol.encoder_header(
        buffer,
        static_cast<uint8_t>(telemetry::MessageType::Data),
        4096
    );

    assert(ret);

    // 打印编码结果
    for (auto byte : buffer) {
        printf("%02X ", byte);
    }
    printf("\n");

    // 解码
    uint8_t type = 0;
    uint32_t length = 0;
    buffer[3] = 99;
    ret = protocol.decoder_header(buffer, type, length);

    printf("%d %d\n",type, length);
    assert(ret);
    assert(type == 0);
    assert(length == 0);

    printf("Protocol test PASS\n");

    return 0;
     std::string payload;
    payload.push_back('A');
    payload.push_back('\0');
    payload.push_back('B');
    assert(msg.payload.size() == 3);
    assert(msg.payload[0] == 'A');
    assert(msg.payload[1] == '\0');
    assert(msg.payload[2] == 'B');
}
#else
#include "telemetry/protocol.h"
#include "telemetry/protocol_parser.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>

int main()
{
    telemetry::Protocol protocol;
    telemetry::ProtocolParser parser;

    // 构造三条消息
    const std::string payloads[] = {
        "HELLO",
        "WORLD",
        "TEST"
    };

    std::string buffer;

    // 将三条消息编码后连续拼接
    for (const auto& payload : payloads) {

        uint8_t header[telemetry::HEADER_SIZE]{};

        bool ret = protocol.encoder_header(
            header,
            static_cast<uint8_t>(telemetry::MessageType::Data),
            static_cast<uint32_t>(payload.size())
        );

        assert(ret);

        // Header + Payload
        buffer.append(
            reinterpret_cast<const char*>(header),
            telemetry::HEADER_SIZE
        );

        buffer.append(payload);
    }

    std::cout << "Total buffer size: "
              << buffer.size() << '\n';

    // 此时 buffer 中包含三条完整消息
    telemetry::Message msg;

    // 第一次解析
    auto status = parser.parse_one(buffer, msg);

    assert(status == telemetry::ParseStatus::MessageReady);
    assert(msg.payload == "HELLO");

    std::cout << "Message 1: " << msg.payload << '\n';

    // 第二次解析
    status = parser.parse_one(buffer, msg);

    assert(status == telemetry::ParseStatus::MessageReady);
    assert(msg.payload == "WORLD");

    std::cout << "Message 2: " << msg.payload << '\n';

    // 第三次解析
    status = parser.parse_one(buffer, msg);

    assert(status == telemetry::ParseStatus::MessageReady);
    assert(msg.payload == "TEST");

    std::cout << "Message 3: " << msg.payload << '\n';

    // 三条消息全部消费完毕
    assert(buffer.empty());

    // 再次解析应该返回 NeedMore
    status = parser.parse_one(buffer, msg);

    assert(status == telemetry::ParseStatus::NeedMore);

    std::cout << "TCP sticky packet test PASS\n";

    return 0;
}
#endif
