/**
 * @file main.cpp
 * @brief Entry point for the TFM-100 firmware: initialize hardware, schedule tasks and run main loop.
 * @author Salvo Musumeci
 * @date 2025-03-09
 *
 * This module configures peripherals (SPI, CAN, sensors), creates the J1939 manager,
 * registers periodic tasks with the cooperative scheduler, and implements the main
 * non-blocking loop that drives sensor state machines and the transport layer.
 */

// The setup function runs once at reset or power-up

// Framework lib
#include <Arduino.h>
#include <SPI.h>
#include "config.h"
#include <avr/wdt.h>
#include <MAX31865_NonBlocking.h>

// MyLib
#include <scheduler.h>
#include <LEDs.h>
#include <PT100.h>
#include <flow.h>
#include <J1939Manager.h>
#include <diagnostics.h>

// 3rd parties lib
#include <mcp_can.h>

// Include Headers
#include <status.h>
#include <cli.h>
#include <can_messages.h>
#include <McpCanAdapter.h>
#include <dip_switch.h>
#include <dtc.h>

/* CLI */
CLIScreenManager cli = CLIScreenManager();

/* Temperature Sensors Variables */
PT100 supply_sensor = PT100(SUPPLY_CS);
PT100 return_sensor = PT100(RETURN_CS);

/* Flow Sensor Variables */
Flow flowObj = Flow(FLOW_TICKS_PER_LITER);

/* Set-up J1939 Transport Layer */
// Allocation of CAN controller and adapter
MCP_CAN CAN0(MCP_CS);           // Set CS to pin 4
McpCanAdapter canAdapter(CAN0); // Adapter per MCP_CAN

// Pass the callbacks struct as the second argument
J1939Callbacks j1939_cbs;

// J1939 Manager instance (created in setup once CAN is initialized)
J1939Manager *j1939 = nullptr; // allocated in setup

/* Node Status */
NodeStatus_t node_status = SETUP;

/* LED Indicators */
LEDs ledIndicators = LEDs(LED_RED, LED_GREEN);

// Scheduler
Scheduler scheduler = Scheduler();

/* Node ID */
uint8_t node_id;
uint8_t severity = 0;

bool CANbusOff = false;
bool CANbusWarn = false;
bool HwFailure = true; // Assume HW failure until all init done

/* CAN Messages */
HeartbeatMessage HBMessage = HeartbeatMessage(5000); // 5s interval
CAN_Temperature TempMessage = CAN_Temperature();
CAN_FilteredTemperatureAndFlow TempAndFlowMessage = CAN_FilteredTemperatureAndFlow();

/* Diagnostics */
TFM100_DTC_Dict dtc_dict_instance = TFM100_DTC_Dict();
Diagnostics DSM = Diagnostics(&dtc_dict_instance);

// Forward declaration of J1939Descriptor is available via J1939Manager.h
// Provide a simple error callback to forward J1939 manager errors to CLI
static void j1939_on_error(const J1939Descriptor &desc)
{
  char buf[80];
  // Format PGN in hex with leading 0x
  snprintf(buf, sizeof(buf), "J1939 TX ERROR PGN=0x%06lX ", (unsigned long)desc.pgn);
  cli.logError(buf);
}

void update_errors();

void setup()
{
  /**
   * @brief Initialize subsystems and schedule periodic tasks.
   *
   * This routine initializes CLI, serial, CAN controller, sensors and the
   * J1939 transport manager. It registers periodic tasks with the cooperative
   * scheduler and transitions the node into RUN state if initialization
   * succeeds. On failure it displays an error UI for up to 60s then triggers
   * a watchdog reset.
   */

  bool mcp_init = false, mcp_normal = false, supply_init = false, return_init = false;

  /* Configure microcontroller/peripherals. */

  // Serial for CLI/diagnostics output
  Serial.begin(115200);
  cli.begin();

  // Compute node address from DIP switches (base + switch value)
  node_id = dip_switch_read() | NODE_ID_BASE;

  // Initialize MCP2515 CAN controller (MCP_ANY - auto filter, 250kbps, external oscillator 8MHz)
  if ((mcp_init = (CAN0.begin(MCP_ANY, CAN_250KBPS, MCP_8MHZ) == CAN_OK)))
    if ((mcp_normal = (CAN0.setMode(MCP_NORMAL) == MCP2515_OK)))
    {

      // Monitor CAN controller error flags frequently (100ms)
      scheduler.addTask([](uint32_t td)
                        {
                            uint8_t error_flags = CAN0.getError();
                            // Bus-Off when TXBO bit (transmit bus-off) is set
                            CANbusOff = (error_flags & MCP_EFLG_TXBO) != 0;
                            // Warning when any of EWARN, RXWAR, TXWAR bits set (error counters >= 96)
                            CANbusWarn = (error_flags & (MCP_EFLG_EWARN | MCP_EFLG_RXWAR | MCP_EFLG_TXWAR)) != 0; },
                        100);
    }

  // Initialize MAX31865 PT100 sensors (non-blocking drivers)
  supply_init = supply_sensor.begin((float)PT100_ALPHA);
  return_init = return_sensor.begin((float)PT100_ALPHA);

  // If any critical subsystem is not initialized, show hardware failure UI and reboot
  if (!mcp_normal || !supply_init || !return_init)
  {
    DSM.setRaw(DTC_SupplyLineDeviceError, supply_init == false);
    DSM.setRaw(DTC_ReturnLineDeviceError, return_init == false);
    DSM.setRaw(DTC_CANBusDeviceError, mcp_normal == false);

    node_status = STOP;
    bool reboot_now = false;
    while (millis() < 60000 && !reboot_now)
    {
      // drawHardwareFailure returns true when user requests immediate reboot
      reboot_now = cli.drawHardwareFailure(mcp_init, mcp_normal, supply_init, return_init);
      // Keep LED state machine ticking and allow scheduled tasks to run
      severity = DSM.getMaxSeverity();
      ledIndicators.process(true);
      scheduler.run();
    }
    // Force reboot via watchdog if recovery is not requested
    wdt_enable(WDTO_15MS);
    while (1)
      ;
  }

  // Create and initialize J1939 manager with CAN adapter and callbacks
  // Hook error callback to forward J1939 manager errors to CLI
  j1939_cbs.on_error = j1939_on_error;
  j1939 = new J1939Manager(canAdapter, j1939_cbs);
  j1939->begin(node_id);

  // Initialize flow sensor (callback currently a stub)
  flowObj.begin([](float flow) {});

  // Add message objects to J1939 manager
  j1939->registerMessage(&HBMessage);
  j1939->registerMessage(&TempMessage);
  j1939->registerMessage(&TempAndFlowMessage);

  // Schedule periodic PT100 measurements (driver triggers non-blocking measurement)
  scheduler.addTask([](uint32_t td)
                    {
                      if (!supply_sensor.triggerMeasurement())
                        cli.logError("Unable to read SUPPLY");
                      if (!return_sensor.triggerMeasurement())
                        cli.logError("Unable to read RETURN"); },
                    PT100_SAMPLE_RATE);

  // Periodic CLI housekeeping (screens, input processing)
  scheduler.addTask([](uint32_t td)
                    { cli.periodic(); },
                    1000);

  // Periodic diagnostics update (check sensor errors, update DTC memory)
  scheduler.addTask([](uint32_t td)
                    { update_errors(); },
                    1000);

  // All initialization succeeded, enter RUN state
  node_status = RUN;
}

void update_errors()
{
  // Supply Sensor Errors
  DSM.setRaw(DTC_SupplyLineRefInHigh, supply_sensor.last_fault & MAX31865::FAULT_HIGHTHRESH_BIT);
  DSM.setRaw(DTC_SupplyLineRefInLow, supply_sensor.last_fault & MAX31865::FAULT_LOWTHRESH_BIT);
  DSM.setRaw(DTC_SupplyLineRtdInLow, supply_sensor.last_fault & MAX31865::FAULT_RTDINLOW_BIT);
  DSM.setRaw(DTC_SupplyLineUOV, supply_sensor.last_fault & MAX31865::FAULT_OVUV_BIT);

  // Return Sensor Errors
  DSM.setRaw(DTC_ReturnLineRefInHigh, return_sensor.last_fault & MAX31865::FAULT_HIGHTHRESH_BIT);
  DSM.setRaw(DTC_ReturnLineRefInLow, return_sensor.last_fault & MAX31865::FAULT_LOWTHRESH_BIT);
  DSM.setRaw(DTC_ReturnLineRtdInLow, return_sensor.last_fault & MAX31865::FAULT_RTDINLOW_BIT);
  DSM.setRaw(DTC_ReturnLineUOV, return_sensor.last_fault & MAX31865::FAULT_OVUV_BIT);

  // CAN Bus Errors
  DSM.setRaw(DTC_CANBusOff, CANbusOff);
  DSM.setRaw(DTC_CANBusErrorPassive, CANbusWarn);

  DSM.periodic_update();
  severity = DSM.getMaxSeverity();
}

void loop()
{
  /**
   * @brief Main non-blocking loop
   *
   * The scheduler executes registered tasks. The J1939 manager is polled with
   * the current time (ms) so it can manage timed transmissions and timeouts.
   * Sensor state machines and the CLI are progressed here as well. Node power
   * state is updated based on whether flow is present.
   */

  // Run scheduled periodic tasks
  scheduler.run();

  // Let J1939 manager process pending transmissions/timeouts
  if (j1939)
    j1939->process(millis());

  // Advance non-blocking sensor state machines
  supply_sensor.process();
  return_sensor.process();
  flowObj.process();

  // CLI processing (screen refresh, input handling)
  cli.process();

  // Set node mode depending on flow present
  node_status = flowObj.isFlowing() ? RUN : SLEEP;

  // Update LED state machine
  ledIndicators.process();
}
