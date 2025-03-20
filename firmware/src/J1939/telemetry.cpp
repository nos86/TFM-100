#include "j1939.h"
#include "main.h"
#include "config.h"

#ifndef FIRMWARE_VERSION
#warning "FIRMWARE_VERSION not defined"
#endif

#ifndef MODEL
#warning "MODEL not defined"
#endif

#ifndef VARIANT
#warning "VARIANT not defined"
#endif

void J1939_HeartBeat::begin(uint8_t src, NodeStatus_t *state){
    this->state = state;
    J1939::begin(6, 0xFF10, src);
}

void J1939_HeartBeat::getData(uint8_t *data, uint8_t *length){
    if (state == nullptr || (data == nullptr) || (length == nullptr)){
        return; // Exit the function if state is null
    }
    *length = 8;
    data[0] = (uint8_t)(MODEL & 0xFF);
    data[1] = (uint8_t)(FIRMWARE_VERSION & 0xFF);
    data[2] = (uint8_t)((VARIANT & 0x0F) << 4);
    switch (*state){
        case STOP:
        data[2] |= 0x00;
            break;
        case SETUP:
        data[2] |= 0x01;
            break;
        case SLEEP:
        data[2] |= 0x02;
            break;
        case RUN:
        data[2] |= 0x0F;
    }
    // Reserved byte, currently set to 0x00 as per protocol specification or unused.
    data[3] = 0x00;
    // Calculate uptime using millis(). The millis() function overflows every ~49.7 days (2^32 ms).
    // Modular arithmetic ensures that the calculation remains correct even after overflow.
    uint32_t uptime = millis(); 
    data[7] = (uptime >> 24) & 0xFF;
    data[6] = (uptime >> 16) & 0xFF;
    data[5] = (uptime >> 8) & 0xFF;
    data[4] = uptime & 0xFF;
};

void J1939_Temperature::begin(uint8_t src, float *supply_temp, float *return_temp){
    this->supply_temp = supply_temp;
    this->return_temp = return_temp;
    J1939::begin(6, 0xFF11, src);
}

void J1939_Temperature::getData(uint8_t *data, uint8_t *length){
    *length = 4;
    uint16_t supply_temp = (uint16_t)((*this->supply_temp) * 100);
    uint16_t return_temp = (uint16_t)((*this->return_temp) * 100);
    data[0] = (supply_temp >> 8) & 0xFF;
    data[1] = supply_temp & 0xFF;
    data[2] = (return_temp >> 8) & 0xFF;
    data[3] = return_temp & 0xFF;
};

