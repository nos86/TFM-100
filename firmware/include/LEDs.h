#pragma once

#include <stdint.h>

class LEDs
{
public:
    LEDs(uint8_t redPin, uint8_t greenPin);
    void process(bool HwFailure = false);

private:
    uint8_t LedRedPin;
    uint8_t LedGreenPin;
    uint32_t last_millis = 0;
    uint32_t LEDtmr50ms;      /**< 50ms led timer */
    uint8_t LEDtmr200ms;      /**< 200ms led timer */
    uint8_t LEDtmrflash_1;    /**< single flash led timer */
    uint8_t LEDtmrflash_2;    /**< double flash led timer */
    uint8_t LEDtmrflash_3;    /**< triple flash led timer */
    uint8_t LEDtmrflash_4;    /**< quadruple flash led timer */
    uint8_t GenericLedStatus; /**< led status bitfield, to be combined with @ref LED_bitmasks */
};
