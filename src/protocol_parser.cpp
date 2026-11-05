#include "telemetry/protocol_parser.h"
namespace telemetry{

ParseStatus ProtocolParser::parse_one(std::string &buffer, Message &message)
{
    if(buffer.size() < HEADER_SIZE){
        return ParseStatus::NeedMore;
    }
    uint8_t type = 0;
    uint32_t length = 0;
    
    if(!_protocol.decoder_header(reinterpret_cast<const uint8_t*>(buffer.data()), type, length))
    {
        return ParseStatus::Error;
    }
    std::size_t total = HEADER_SIZE + length;
    if(buffer.size() < total){
        return ParseStatus::NeedMore;
    }
    message.type = static_cast<MessageType>(type);
    message.payload.assign(buffer.data() + HEADER_SIZE, length);
    buffer.erase(0,total);
    return ParseStatus::MessageReady;
}

}