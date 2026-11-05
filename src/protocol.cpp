#include "telemetry/protocol.h"
#include <cstring>
#include <iostream>
#include <arpa/inet.h> 
namespace telemetry {
    telemetry::Protocol::Protocol()
    {
    }

    telemetry::Protocol::~Protocol()
    {
    }

bool telemetry::Protocol::encoder_header(uint8_t *buffer, uint8_t type, uint32_t length)
{
    if(buffer == NULL){
        return false;
    }
    if(!valid_type(type)){
        return false;
    }
    if(length > MAX_PAYLOAD_SIZE){
        return false;
    }
    buffer[0] = (MAGIC >> 8) & 0xFF;
    buffer[1] = MAGIC & 0XFF;
    buffer[2] = VERSION;
    buffer[3] = type;
    buffer[4] = (length >> 24) & 0xFF;
    buffer[5] = (length >> 16) & 0xFF;
    buffer[6] = (length >> 8)  & 0xFF;
    buffer[7] = length & 0xFF;
    return true;
}

bool telemetry::Protocol::decoder_header(const uint8_t *buffer, uint8_t &type, uint32_t &length)
{
    if(buffer == NULL){
        return false;
    }
    uint16_t _magic = (static_cast<uint16_t>(buffer[0]) << 8) | (static_cast<uint16_t>(buffer[1]));
    uint8_t _version = buffer[2];
    uint8_t _type = buffer[3];
    uint32_t _length = (static_cast<uint32_t>(buffer[4]) << 24)
       | (static_cast<uint32_t>(buffer[5]) << 16)
       | (static_cast<uint32_t>(buffer[6]) << 8)
       | static_cast<uint32_t>(buffer[7]);
    //printf("%2x %x %x %d\n",_magic,_version,_type,_length);
    if((_magic != MAGIC) || !(valid_type(_type)) || (_length > MAX_PAYLOAD_SIZE) || (_version != VERSION)){
        return false;
    }
    type = _type;
    length = _length;
    return true;
}

bool telemetry::Protocol::valid_type(uint8_t type)
{
    return type == static_cast<uint8_t>(MessageType::Data) ||
           type == static_cast<uint8_t>(MessageType::Ping) ||
           type == static_cast<uint8_t>(MessageType::Pong);
}

}