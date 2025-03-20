
#include <Arduino.h>
#include "main.h"

uint8_t dip_switch_read(void) {

    uint8_t dip_switch = 0;
    uint8_t dip_switch_pins[4] = {DS_B0, DS_B1, DS_B2, DS_B3};

    for (int i = 0; i < 4; i++) {
        dip_switch |= !digitalRead(dip_switch_pins[i]) << i;
    }

    return dip_switch;
}