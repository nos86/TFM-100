#pragma once

#define LED_RED LED_BUILTIN_RX
#define LED_GREEN LED_BUILTIN_TX

// SPI Slave Select Pins
#define SUPPLY_CS 2 // PIN19 - D2/SDA
#define RETURN_CS 3 // PIN18 - D3/SCL
#define MCP_CS 4    // PIN25 - D4/A6
#define MCP_INT 7   // PIN1  - D7

// Flow Sensor Pins
#define FLOW_SENSOR_PIN 0 // RX - D0
#define FLOW_TICKS_PER_LITER 4

// Dip switch pin definitions
#define DS_B3 10
#define DS_B2 9
#define DS_B1 8
#define DS_B0 6

#define NODE_ID_BASE 0x10 // Base node ID

#define VARIANT_FULL 0x01           // Default variant
#define VARIANT_SIMULATED_FLOW 0x02 // Simulated flow variant

#define MODEL 0x01            // TFM-100
#define VARIANT VARIANT_FULL  // Default variant
#define FIRMWARE_VERSION 0x01 // Production version 1

#define PT100_FILTER_RC 5000  // 5s
#define PT100_SAMPLE_RATE 100 // 100ms
#define PT100_ALPHA ((float)PT100_SAMPLE_RATE / ((float)PT100_FILTER_RC + PT100_SAMPLE_RATE))

// EEPROM layout
// First byte reserved for diagnostics storage start (logical offset)
//
// Layout (byte offsets, inclusive ranges):
//   0x00           : reserved (logical diagnostics start pointer)
//   0x1A - 0x1F    : POWER region (6 bytes: 2 bytes magic + 4 bytes max_power)
//   0x20 - 0x29    : ENERGY region (10 bytes: 2 bytes magic + 4 bytes energy_total + 4 bytes energy_24h)
//   0x40 - ...     : DIAGNOSTICS region (starts at 0x40; size defined elsewhere)
//
// Offsets:
#define POWER_EEPROM_OFFSET 0x1A
#define ENERGY_EEPROM_OFFSET 0x20
#define DIAGNOSTICS_EEPROM_OFFSET 0x40

// Region sizes (in bytes) — keep in sync with EEPROM data structures:
#define POWER_EEPROM_SIZE   6   // 2 bytes magic + 4 bytes max_power
#define ENERGY_EEPROM_SIZE 10   // 2 bytes magic + 4 bytes energy_total + 4 bytes energy_24h

#define ENERGY_EEPROM_SAVE_INTERVAL_MS 60000UL // Persist energy to EEPROM at most once per minute

// Compile-time checks to ensure EEPROM regions do not overlap.
static_assert(POWER_EEPROM_OFFSET + POWER_EEPROM_SIZE <= ENERGY_EEPROM_OFFSET,
              "EEPROM layout error: POWER region overlaps ENERGY region");
static_assert(ENERGY_EEPROM_OFFSET + ENERGY_EEPROM_SIZE <= DIAGNOSTICS_EEPROM_OFFSET,
              "EEPROM layout error: ENERGY region overlaps DIAGNOSTICS region");