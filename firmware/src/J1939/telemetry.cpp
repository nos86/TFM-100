#include "j1939.h"
#include "main.h"

void J1939_HeartBeat::begin(uint8_t src, NodeStatus_t *state){
    this->state = state;
    J1939::begin(6, 0xFF10, src);
}

void J1939_HeartBeat::getData(uint8_t *data, uint8_t *length){
    *length = 6;
    uint16_t version = (FIRMWARE_VERSION & 0x7FF) << 4;
    switch (*state){
        case STOP:
            version |= 0x00;
            break;
        case SETUP:
            version |= 0x01;
            break;
        case SLEEP:
            version |= 0x02;
            break;
        case RUN:
            version |= 0x0F;
            break;        
    }
    data[0] = (version >> 8) & 0xFF;
    data[1] = version & 0xFF;
    uint32_t uptime = millis(); 
    data[2] = (uptime >> 24) & 0xFF;
    data[3] = (uptime >> 16) & 0xFF;
    data[4] = (uptime >> 8) & 0xFF;
    data[5] = uptime & 0xFF;
};
