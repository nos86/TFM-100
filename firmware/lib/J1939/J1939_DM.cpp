#include <J1939_DM.h>

bool J1939_DM1Message::shouldSend(uint16_t now_ms)
{
    if (now_ms - last_send_time >= send_interval_ms)
    {
        last_send_time = now_ms;
        if (J1939_DM::getNumberOfActiveErrors() > 0)
        {
            previously_had_errors = true;
            return true;
        }
        else if (previously_had_errors)
        {
            // We had errors previously but now none: arm the final lamp-only DM1
            previously_had_errors = false;
            return true;
        }
    }
    return false;
}

uint8_t *J1939_DM1Message::buildPayload(uint8_t *len)
{
    uint8_t numberOfErrors = J1939_DM::getNumberOfActiveErrors();
    *len = 2 + numberOfErrors * 4;
    uint8_t *payload = new uint8_t[*len];
    if (!payload)
    {
        *len = 0;
        return nullptr;
    }

    // --- Lamp status reporting ---
    // 00 = slow, 01 = fast, 10 = reserved, 11 = no-flash
    constexpr uint8_t FLASH_SLOW = 0u;
    constexpr uint8_t FLASH_FAST = 1u;
    constexpr uint8_t FLASH_NONE = 3u; // “no flash / N.A.”

    uint8_t lampStatus = 0;
    uint8_t lampFlashing = 0;
    J1939_DM::LampStatus lamps[4] = {
        J1939_DM::getProtectLampStatus(),
        J1939_DM::getWarningLampStatus(),
        J1939_DM::getStopLampStatus(),
        J1939_DM::getMilLampStatus(),
    };

    for (uint8_t i = 0; i < 4; i++)
    {
        switch (lamps[i])
        {
        case J1939_DM::LampStatus::OFF:
            // Lamp Status = 00 (off), Flash = 00 (don't care)
            break;
        case J1939_DM::LampStatus::SOLID:
            // Lamp Status = 01 (on), Flash = 11 (no flash)
            lampStatus |= (1 << (2 * i));
            lampFlashing |= (FLASH_NONE << (2 * i));
            break;
        case J1939_DM::LampStatus::FAST_BLINK:
            // Lamp Status = 01 (on), Flash = 01 (fast)
            lampStatus |= (1 << (2 * i));
            lampFlashing |= (FLASH_FAST << (2 * i));
            break;
        case J1939_DM::LampStatus::SLOW_BLINK:
            // Lamp Status = 01 (on), Flash = 00 (slow)
            lampStatus |= (1 << (2 * i));
            lampFlashing |= (FLASH_SLOW << (2 * i));
            break;
        }
    }
    payload[0] = lampStatus;
    payload[1] = lampFlashing;

    // --- DTCs ---
    uint8_t *p = &payload[2];
    for (uint8_t i = 0; i < numberOfErrors; ++i, p += 4)
    {
        uint32_t spn;
        uint8_t fmi, oc;
        J1939_DM::getErrorCode(i, spn, fmi, oc);
        p[0] = (uint8_t)(spn & 0xFF);                // SPN[7:0]
        p[1] = (uint8_t)((spn >> 8) & 0xFF);         // SPN[15:8]
        p[2] = (uint8_t)((((spn >> 16) & 0x07) << 5) // SPN[18:16] nei bit 0..2
                         | (uint8_t)(fmi & 0x1F));   // FMI nei bit 3..7
        p[3] = (uint8_t)(oc & 0x7F);                 // Occurrence Count
    }

    return payload;
}

uint8_t __attribute__((weak)) J1939_DM::getNumberOfActiveErrors()
{
    return 0;
}

void __attribute__((weak)) J1939_DM::getErrorCode(uint8_t index, uint32_t &spn, uint8_t &fmi, uint8_t &oc)
{
    spn = 0;
    fmi = 0;
    oc = 0;
}

J1939_DM::LampStatus __attribute__((weak)) J1939_DM::getMilLampStatus()
{
    return J1939_DM::LampStatus::OFF;
}

J1939_DM::LampStatus __attribute__((weak)) J1939_DM::getStopLampStatus()
{
    return J1939_DM::LampStatus::OFF;
}

J1939_DM::LampStatus __attribute__((weak)) J1939_DM::getWarningLampStatus()
{
    return J1939_DM::LampStatus::OFF;
}

J1939_DM::LampStatus __attribute__((weak)) J1939_DM::getProtectLampStatus()
{
    return J1939_DM::LampStatus::OFF;
}
