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
#define FIRMWARE_VERSION 0xFF // Test

#define PT100_FILTER_RC 5000  // 5s
#define PT100_SAMPLE_RATE 100 // 100ms
#define PT100_ALPHA ((float)PT100_SAMPLE_RATE / ((float)PT100_FILTER_RC + PT100_SAMPLE_RATE))
