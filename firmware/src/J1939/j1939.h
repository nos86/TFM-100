#ifndef J1939_H
#define J1939_H

#include <Arduino.h>
#include "status.h"

class J1939 {
    public:
        J1939(): messageId(0), length(0) {};
        void begin(uint8_t priority, uint16_t pgn, uint8_t src);
        void setData(uint8_t *data, uint8_t length);
        void getData(uint8_t *data, uint8_t *length);

        uint32_t messageId;

    protected:
        
        uint8_t length;
        uint8_t data[8];

};

class J1939_HeartBeat : public J1939 {
    public:
        void begin(uint8_t src, NodeStatus_t *state);
        void getData(uint8_t *data, uint8_t *length);

    private: 
        NodeStatus_t *state;
};

class J1939_Temperature : public J1939 {
    public:
        void begin(uint8_t src, float *supply_temp, float *return_temp);
        void getData(uint8_t *data, uint8_t *length);

    private:
        float *supply_temp;
        float *return_temp;
};

