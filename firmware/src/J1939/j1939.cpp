#include "j1939.h"

void J1939::begin(uint8_t priority, uint16_t pgn, uint8_t src){
    messageId = (uint32_t)(((uint32_t)(priority & 0x07) << 26) | ((uint32_t)pgn << 8) | src) | 0x80000000;
    length = 0;
}

void J1939::setData(uint8_t *data, uint8_t length){
    this->length = length;
    for (uint8_t i = 0; i < length; i++){
        this->data[i] = data[i];
    }
}

void J1939::getData(uint8_t *data, uint8_t *length){
    *length = this->length;
    for (uint8_t i = 0; i < this->length; i++){
        data[i] = this->data[i];
    }
}
