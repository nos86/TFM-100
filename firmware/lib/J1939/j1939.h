#ifndef J1939_H
#define J1939_H

#include <stdint.h>
#include <string.h>

#include "status.h"

class J1939
{
public:
    J1939() : messageId(0), length(0), initialized(false)
    {
        memset(data, 0, sizeof(data));
    };
    void begin(uint8_t priority, uint16_t pgn, uint8_t src);
    void setData(uint8_t *data, uint8_t length);
    void getData(uint8_t *data, uint8_t *length);
    bool isInitialized() { return initialized; };

    uint32_t messageId;

protected:
    uint8_t length;
    uint8_t data[8];
    bool initialized;
};

class J1939_HeartBeat : public J1939
{
    /*
     * CAN Matrix for J1939_HeartBeat:
     * +-----------+--------+-------------------+-------------------------+---------------+
     * | Start bit | Length | Name              | Description             |   Byte Order  |
     * +-----------+--------+-------------------+-------------------------+---------------+
     * | 0         | 8 bits | Model             | 0x00 (TFM-100)          | Little Endian |
     * | 8         | 8 bits | Version           | Version of the firmware | Little Endian |
     * | 16        | 4 bits | Variant           | Variant of the firmware | Little Endian |
     * | 20        | 4 bits | State             | State of the node       | Little Endian |
     * | 24        | 8 bits | Reserved          | Unused                  | Little Endian |
     * | 32        | 32 bits| Uptime            | Uptime of the node      | Little Endian |
     * +-----------+--------+-------------------+-------------------------+---------------+
     */
    public:
        void begin(uint8_t src, NodeStatus_t *state, uint32_t (*getUptime)(void));
        void getData(uint8_t *data, uint8_t *length);

    private: 
        NodeStatus_t *state;
        uint32_t (*uptime_fnct)(void);
};

class J1939_Temperature : public J1939 {
    public:
        void begin(uint8_t src, float *supply_temp, float *return_temp);
        void getData(uint8_t *data, uint8_t *length);

    private:
        float *supply_temp;
        float *return_temp;
};

class J1939_FilteredTemperatureAndFlow : public J1939_Temperature {
    public:
        void begin(uint8_t src, float *supply_temp, float *return_temp, uint16_t *flow);
        void getData(uint8_t *data, uint8_t *length);

    private:
        uint16_t *flow_l_h;
};

#endif // J1939_H