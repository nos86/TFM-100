#ifndef LEDS_H
#define LEDS_H

#include <Arduino.h>
#include "status.h"

/**
 * @defgroup LED_bitmasks LED bitmasks
 * @{
 * Bitmasks for the LED indicators
 */
#define LED_flicker               0x01U /**< LED flickering 10Hz */
#define LED_blink                 0x02U /**< LED blinking 2,5Hz */
#define LED_flash_1               0x04U /**< LED single flash */
#define LED_flash_2               0x08U /**< LED double flash */
#define LED_flash_3               0x10U /**< LED triple flash */
#define LED_flash_4               0x20U /**< LED quadruple flash */
/** @} */

/**
 * LEDs object, initialized by CO_LEDs_init()
 */
typedef struct {
    uint32_t LEDtmr50ms;        /**< 50ms led timer */
    uint8_t LEDtmr200ms;        /**< 200ms led timer */
    uint8_t LEDtmrflash_1;      /**< single flash led timer */
    uint8_t LEDtmrflash_2;      /**< double flash led timer */
    uint8_t LEDtmrflash_3;      /**< triple flash led timer */
    uint8_t LEDtmrflash_4;      /**< quadruple flash led timer */
    uint8_t GenericLedStatus;   /**< led status bitfield, to be combined with @ref LED_bitmasks */
    uint8_t LedRedPin;          /**< red led pin */
    uint8_t LedGreenPin;        /**< green led pin */
} LEDs_t;

#ifdef __cplusplus
extern "C" {
#endif

void LEDs_init(uint8_t redPin, uint8_t greenPin);
void LEDs_process(NodeStatus_t state, bool ErrCANbusOff, 
    bool ErrCANbusWarn, bool PT100Err, bool HwFailure);

#ifdef __cplusplus
}
#endif

#endif // CO_LEDS_H
