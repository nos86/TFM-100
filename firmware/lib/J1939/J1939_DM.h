#pragma once

#include <IJ1939Message.h>

class J1939_DM1Message : public IJ1939Message
{
public:
    J1939_DM1Message(uint16_t interval_ms) : IJ1939Message(), send_interval_ms(interval_ms) {}
    uint32_t getPGN() override { return 0xFECA; } // PGN for DM1

    bool shouldSend(uint16_t now_ms) override;

    uint8_t *buildPayload(uint8_t *len) override;

    bool shallPayloadCopied() override { return true; }

private:
    uint32_t last_send_time = 0;
    uint16_t send_interval_ms;
    bool previously_had_errors = false;
};

namespace J1939_DM
{
    enum LampStatus
    {
        OFF,
        SOLID,
        SLOW_BLINK,
        FAST_BLINK,
    };

    uint8_t getNumberOfActiveErrors();
    void getErrorCode(uint8_t index, uint32_t &spn, uint8_t &fmi, uint8_t &oc);
    LampStatus getMilLampStatus();
    LampStatus getStopLampStatus();
    LampStatus getWarningLampStatus();
    LampStatus getProtectLampStatus();
} // namespace J1939_DM
