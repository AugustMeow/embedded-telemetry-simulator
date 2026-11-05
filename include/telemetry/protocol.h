#ifndef  PROTOCOL_H
#define  PROTOCOL_H

#include <cstdint>
#include <cstddef>
#include <string>

namespace telemetry {

constexpr uint16_t MAGIC = 0x544C;
constexpr uint8_t VERSION = 1;

constexpr std::size_t HEADER_SIZE = 8;
constexpr uint32_t MAX_PAYLOAD_SIZE = 4096;

enum class MessageType : uint8_t {
    Data = 1,
    Ping = 2,
    Pong = 3
};

struct Message {
    MessageType type;
    std::string payload;
};

struct MsgHeader{
    uint16_t magic;
    uint8_t version;
    MessageType type;
    uint32_t length;
};

class Protocol{
public:
    Protocol();
    ~Protocol();
    bool encoder_header(uint8_t *buffer, uint8_t type, uint32_t length);
    bool decoder_header(const uint8_t *buffer, uint8_t & type, uint32_t & length);
private:
    bool valid_type(uint8_t type);
};
} // namespace telemetry
#endif   //PROTOCOL_H