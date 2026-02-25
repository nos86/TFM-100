#include <Arduino.h>
#include <mcp_can.h>
#include <flow.h>
#include <energy.h>
#include <cli.h>
#include <scheduler.h>
#include "can_messages.h"
#include "config.h"
#include "PT100.h"
#include "status.h"

/**
 * @file can_messages.cpp
 * @brief Implementations of J1939 message payload builders.
 *
 * Each `buildPayload()` returns a pointer to a buffer containing the payload
 * and writes its length to `len`. Current implementation returns pointers to
 * dynamically-allocated buffers (via `new[]`). This is NOT SAFE in long-
 * running firmware unless ownership is clearly defined and the caller frees
 * the memory. The `J1939Manager` currently expects `shallPayloadCopied()` to
 * be true for these messages, so a safer implementation is to use a static
 * function-local buffer and return that pointer (the manager will copy it).
 *
 * Payload format notes:
 * - Endianness: multi-byte integers are packed in little-endian order
 *   (low byte first) throughout these payloads.
 * - Temperature conversion: floats are multiplied by 100 and cast to uint16_t.
 *   Recommend rounding and clamping to avoid overflow and preserve accuracy.
 */

#ifndef FIRMWARE_VERSION
#warning "FIRMWARE_VERSION not defined"
#elif FIRMWARE_VERSION == 0xFF
#warning "FIRMWARE_VERSION is 0xFF, please update it"
#endif

#ifndef MODEL
#warning "MODEL not defined"
#endif

#ifndef VARIANT
#warning "VARIANT not defined"
#endif

// Definizioni delle variabili di stato (devono essere fornite dall'applicazione)
extern MCP_CAN CAN0;
extern Flow flowObj;
extern PT100 supply_sensor;
extern PT100 return_sensor;
extern CLIScreenManager cli;
extern Scheduler scheduler;      // Deve essere definito nel main o scheduler
extern NodeStatus_t node_status; // Convertito da NodeStatus_t
extern Energy energyObj;

uint8_t *HeartbeatMessage::buildPayload(uint8_t *len)
{
  if (len == nullptr)
  {
    return nullptr;
  }
  /*
   * Heartbeat payload (8 bytes):
   * - byte 0: model (8-bit)
   * - byte 1: firmware version (8-bit)
   * - byte 2: variant (high nibble) + node state (low nibble)
   * - byte 3: reserved
   * - bytes 4..7: 32-bit uptime (big-endian)
   *
   * Lifetime: currently returns a heap-allocated buffer (new[]). Prefer a
   * static buffer because the manager copies payloads for messages that
   * return true from `shallPayloadCopied()`.
   */

  *len = 8;
  uint8_t *data = new uint8_t[8];
  data[0] = (uint8_t)(MODEL & 0xFF);
  data[1] = (uint8_t)(FIRMWARE_VERSION & 0xFF);
  data[2] = (uint8_t)((VARIANT & 0x0F) << 4);
  switch (node_status)
  {
  case STOP:
    data[2] |= 0x00;
    break;
  case SETUP:
    data[2] |= 0x01;
    break;
  case SLEEP:
    data[2] |= 0x02;
    break;
  case RUN:
    data[2] |= 0x0F;
  }
  // Reserved byte, currently set to 0x00 as per protocol specification or unused.
  data[3] = 0x00;
  uint32_t uptime = scheduler.getUptime();
  data[7] = (uptime >> 24) & 0xFF;
  data[6] = (uptime >> 16) & 0xFF;
  data[5] = (uptime >> 8) & 0xFF;
  data[4] = uptime & 0xFF;
  return data;
};

uint8_t *CAN_FilteredTemperatureAndFlow::buildPayload(uint8_t *len)
{
  if (len == nullptr)
  {
    return nullptr;
  }
  /*
   * Filtered temperature + flow payload (6 bytes):
   * - bytes 0..1: supply temperature in centi-degrees (uint16_t, big-endian)
   * - bytes 2..3: return temperature in centi-degrees (uint16_t, big-endian)
   * - bytes 4..5: flow (uint16_t, big-endian)
   *
   * Conversion: multiply sensor floats by 100, round, then clamp to uint16_t.
   * Lifetime: returns heap-allocated buffer; prefer a static buffer instead.
   */

  *len = 6;
  /* Rounded conversion to centi-degree to improve accuracy */
  uint32_t supply_temp_u = (uint32_t)roundf(supply_sensor.average_temperature * 100.0f);
  uint32_t return_temp_u = (uint32_t)roundf(return_sensor.average_temperature * 100.0f);
  if (supply_temp_u > 0xFFFF)
    supply_temp_u = 0xFFFF;
  if (return_temp_u > 0xFFFF)
    return_temp_u = 0xFFFF;
  uint16_t supply_temp = (uint16_t)supply_temp_u;
  uint16_t return_temp = (uint16_t)return_temp_u;
  uint8_t *data = new uint8_t[6];
  data[0] = supply_temp & 0xFF;
  data[1] = (supply_temp >> 8) & 0xFF;
  data[2] = return_temp & 0xFF;
  data[3] = (return_temp >> 8) & 0xFF;
  uint16_t flow = flowObj.getFlow();
  data[4] = flow & 0xFF;
  data[5] = (flow >> 8) & 0xFF;
  return data;
};

uint8_t *CAN_Temperature::buildPayload(uint8_t *len)
{
  if (len == nullptr)
  {
    return nullptr;
  }
  /*
   * Raw temperature payload (4 bytes):
   * - bytes 0..1: supply temperature in centi-degrees (uint16_t, big-endian)
   * - bytes 2..3: return temperature in centi-degrees (uint16_t, big-endian)
   *
   * Use rounding and clamp to uint16_t to avoid overflow.
   */

  *len = 4;
  uint32_t supply_temp_u = (uint32_t)roundf(supply_sensor.last_temperature * 100.0f);
  uint32_t return_temp_u = (uint32_t)roundf(return_sensor.last_temperature * 100.0f);
  if (supply_temp_u > 0xFFFF)
    supply_temp_u = 0xFFFF;
  if (return_temp_u > 0xFFFF)
    return_temp_u = 0xFFFF;
  uint16_t supply_temp = (uint16_t)supply_temp_u;
  uint16_t return_temp = (uint16_t)return_temp_u;
  uint8_t *data = new uint8_t[4];
  data[0] = supply_temp & 0xFF;
  data[1] = (supply_temp >> 8) & 0xFF;
  data[2] = return_temp & 0xFF;
  data[3] = (return_temp >> 8) & 0xFF;
  return data;
};

uint8_t *CAN_PowerAndEnergy::buildPayload(uint8_t *len)
{
  if (len == nullptr)
  {
    return nullptr;
  }
  /*
   * Power and energy payload (8 bytes, little-endian):
   * - bytes 0..1: Energy in the last 24h (uint16_t, 0.01 kWh per LSB)
   * - bytes 2..4: Energy total in kWh (uint24_t, 1 kWh per LSB)
   * - bytes 5..6: Power in kW (uint16_t, 0.1 kW per LSB)
   * - byte 7: reserved for future use (set to 0)
   */

  *len = 8;
  float actual_power = getThermalPower(supply_sensor.average_temperature, return_sensor.average_temperature, flowObj.getFlow());
  uint32_t energy_total = (uint32_t)max(0.0f, min(energyObj.getEnergyTotal(), 16777215.0f));

  // Encode 24h energy as centi-kWh in uint16_t, with clamping to avoid overflow.
  float energy24h = energyObj.getEnergy24h();
  if (energy24h < 0.0f)
  {
    energy24h = 0.0f;
  }
  // Maximum encodable value with scale factor 100 is 655.35 kWh
  if (energy24h > 655.35f)
  {
    energy24h = 655.35f;
  }
  uint16_t energy_24h = (uint16_t)roundf(energy24h * 100.0f);
  // Scale power to deci-kW, then round and clamp to uint16_t range to avoid overflow.
  float power_scaled = actual_power * 10.0f;
  if (power_scaled < 0.0f)
  {
    power_scaled = 0.0f;
  }
  uint32_t power_u = (uint32_t)roundf(power_scaled);
  if (power_u > 0xFFFFu)
  {
    power_u = 0xFFFFu;
  }
  uint16_t power = (uint16_t)power_u;

  uint8_t *data = new uint8_t[8];
  data[0] = energy_24h & 0xFF;
  data[1] = (energy_24h >> 8) & 0xFF;
  data[2] = (energy_total) & 0xFF;
  data[3] = (energy_total >> 8) & 0xFF;
  data[4] = (energy_total >> 16) & 0xFF;
  data[5] = power & 0xFF;
  data[6] = (power >> 8) & 0xFF;
  data[7] = 0; // reserved for future use
  return data;
};
