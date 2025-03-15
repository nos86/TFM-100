/*
 * CANopen LED Behavior
 * ----------------------------------------------------------------------
 * | Behavior          | Green LED           | Red LED                   |
 * ----------------------------------------------------------------------
 * | Off               | Init                | No Error                  |
 * | Flickering (10Hz) | Setup               | Hardware Failure          |
 * | Blinking (2.5Hz)  | -                   | PT100 Reading Error       |
 * | Single Flash      | Sleep               | CAN warning               |
 * | On                | RUN                 | CAN Bus Off               |
 * ----------------------------------------------------------------------
 * 
 * Flashing is implemented as following:
 * - 200ms on
 * - 200ms off (except for the last flash)
 * - 200ms on (except for the last flash)
 * - 1000ms off
 */

#include "LEDs.h"
 
LEDs_t *LEDs;

void LEDs_init(uint8_t redPin, uint8_t greenPin){
    if (LEDs) free(LEDs);
    LEDs = (LEDs_t *)malloc(sizeof(LEDs_t));
    (void)memset(LEDs, 0, sizeof(LEDs_t));
    LEDs->LedRedPin = redPin;
    LEDs->LedGreenPin = greenPin;
    pinMode(redPin, OUTPUT);
    pinMode(greenPin, OUTPUT);
    digitalWrite(redPin, HIGH);
    digitalWrite(greenPin, HIGH);
}
 
void LEDs_process(uint32_t timeDifference_ms, NodeStatus_t state, bool ErrCANbusOff, 
bool ErrCANbusWarn, bool PT100Err, bool HwFailure){
uint8_t rd = 0;
uint8_t gr = 0;
bool tick = false;

    LEDs->LEDtmr50ms += timeDifference_ms;
    while (LEDs->LEDtmr50ms >= 50U) {
        bool rdFlickerNext = (LEDs->LEDred & (uint8_t)LED_flicker) == 0U;

        tick = true;
        LEDs->LEDtmr50ms -= 50U;

        if (++LEDs->LEDtmr200ms > 3U) {
            /* calculate 2,5Hz blinking and flashing */
            LEDs->LEDtmr200ms = 0;
            rd = 0;
            gr = 0;

            if ((LEDs->LEDred & LED_blink) == 0U) {
                rd |= LED_blink;
            } else {
                gr |= LED_blink;
            }

            switch (++LEDs->LEDtmrflash_1) {
                case 1: rd |= LED_flash_1; break;
                case 2: gr |= LED_flash_1; break;
                case 6: LEDs->LEDtmrflash_1 = 0; break;
                default: /* none */ break;
            }
            switch (++LEDs->LEDtmrflash_2) {
                case 1:
                case 3: rd |= LED_flash_2; break;
                case 2:
                case 4: gr |= LED_flash_2; break;
                case 8: LEDs->LEDtmrflash_2 = 0; break;
                default: /* none */ break;
            }
            switch (++LEDs->LEDtmrflash_3) {
                case 1:
                case 3:
                case 5: rd |= LED_flash_3; break;
                case 2:
                case 4:
                case 6: gr |= LED_flash_3; break;
                case 10: LEDs->LEDtmrflash_3 = 0; break;
                default: /* none */ break;
            }
            switch (++LEDs->LEDtmrflash_4) {
                case 1:
                case 3:
                case 5:
                case 7: rd |= LED_flash_4; break;
                case 2:
                case 4:
                case 6:
                case 8: gr |= LED_flash_4; break;
                case 12: LEDs->LEDtmrflash_4 = 0; break;
                default: /* none */ break;
            }
        } else {
            /* clear flicker and CANopen bits, keep others */
            rd = LEDs->LEDred & (0xFFU ^ (LED_flicker));
            gr = LEDs->LEDgreen & (0xFFU ^ (LED_flicker));
        }

        /* calculate 10Hz flickering */
        if (rdFlickerNext) {
            rd |= LED_flicker;
        } else {
            gr |= LED_flicker;
        }

    } /* while (LEDs->LEDtmr50ms >= 50) */

    if (tick) {
        uint8_t rd_co, gr_co;
        
        LEDs->LEDred = rd;
        LEDs->LEDgreen = gr;
        
        /* red ERROR LED */
        if (ErrCANbusOff) {
            rd_co = 1;
        } else if (HwFailure) {
            rd_co = rd & LED_flicker;
        } else if (ErrCANbusWarn) {
            rd_co = rd & LED_flash_1;
        } else if (PT100Err) {
            rd_co = rd & LED_blink;
        } else {
            rd_co = 0;
        }
        
        /* green RUN LED */
        if (state == SLEEP) {
            gr_co = gr & LED_flash_1;
        } else if (state == SETUP) {
            gr_co = gr & LED_flicker;
        } else if (state == RUN) {
            gr_co = 1;
        } else {
            gr_co = 0;
        }
        
        // Set the LEDs
        if (rd_co) {
            digitalWrite(LEDs->LedRedPin, LOW);
        } else {
            digitalWrite(LEDs->LedRedPin, HIGH);
        }
        if (gr_co) {
            digitalWrite(LEDs->LedGreenPin, LOW);
        } else {
            digitalWrite(LEDs->LedGreenPin, HIGH);
        }
        
    } /* if (tick) */
}
