#include "stdint.h"
#include <J1939_DM.h>
#include <diagnostics.h>
#include <dtc.h>

extern Diagnostics DSM;                   // Diagnostics manager instance (defined in main.cpp)
extern TFM100_DTC_Dict dtc_dict_instance; // DTC dictionary instance (defined in main.cpp)

// Implement J1939 DM1 harness functions
uint8_t J1939_DM::getNumberOfActiveErrors()
{
    return (uint8_t)DSM.numberOfActiveErrors();
}

void J1939_DM::getErrorCode(uint8_t index, uint32_t &spn, uint8_t &fmi, uint8_t &oc)
{
    const uint8_t idx = DSM.getIndexForActiveErrorAt(index);
    dtc_dict_instance.getSPN(idx, spn);
    dtc_dict_instance.getFMI(idx, fmi);
    oc = DSM.getOccurrenceCount(idx);
}

J1939_DM::LampStatus J1939_DM::getWarningLampStatus()
{
    uint8_t severity = DSM.getMaxSeverity();
    switch (severity)
    {
    case red_lamp_state_t::ONE_BLINK:
        return J1939_DM::LampStatus::SOLID;
    case red_lamp_state_t::TWO_BLINKS:
        return J1939_DM::LampStatus::SLOW_BLINK;
    case red_lamp_state_t::THREE_BLINKS:
        return J1939_DM::LampStatus::FAST_BLINK;
    default:
        return J1939_DM::LampStatus::OFF;
    }
}

J1939_DM::LampStatus J1939_DM::getStopLampStatus()
{
    uint8_t severity = DSM.getMaxSeverity();
    switch (severity)
    {
    case red_lamp_state_t::SOLID:
        return J1939_DM::LampStatus::SOLID;
    case red_lamp_state_t::FLASHING:
        return J1939_DM::LampStatus::SLOW_BLINK;
    case red_lamp_state_t::FLICKERING:
        return J1939_DM::LampStatus::FAST_BLINK;
    default:
        return J1939_DM::LampStatus::OFF;
    }
}