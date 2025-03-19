#ifndef MAIN_H
#define MAIN_H

#include <Arduino.h>

// Dip switch pin definitions
#define DS_B3 10
#define DS_B2 9
#define DS_B1 8
#define DS_B0 6

#define NODE_ID_BASE 0x10

#define FIRMWARE_VERSION 0x01


uint8_t dip_switch_read(void);

#endif // MAIN_H