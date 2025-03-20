#ifndef CONFIG_H
#define CONFIG_H

#define LED_RED LED_BUILTIN_RX
#define LED_GREEN LED_BUILTIN_TX

// SPI Slave Select Pins
#define SUPPLY_CS 2 // PIN19 - D2/SDA
#define RETURN_CS 3 // PIN18 - D3/SCL
#define MCP_CS 4    // PIN25 - D4/A6
#define MCP_INT 7   // PIN1  - D7


#define VARIANT_FULL 0x00 // Default variant
#define VARIANT_SIMULATED_FLOW 0x01 // Simulated flow variant


#define MODEL 0x01 // TFM-100
#define VARIANT VARIANT_FULL // Default variant
#define FIRMWARE_VERSION 0xFF //Test

#endif // CONFIG_H