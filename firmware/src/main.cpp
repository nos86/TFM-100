/*
  TFM-100

  Manage the TFM-100 board reading sensors and sending data via CAN Bus

  created 9 Mar 2025
  by Salvo Musumeci
*/

// the setup function runs once when you press reset or power the board

// Framework lib
#include <Arduino.h>
#include <SPI.h>
#include "config.h"
#include <avr/wdt.h>

// MyLib
#include <scheduler.h>
#include <LEDs.h>
#include <PT100.h>
#include <j1939.h>
#include <flow.h>

// 3rd parties lib
#include <mcp_can.h>

// Include Headers
#include <status.h>
#include <cli.h>

// External functions
uint8_t dip_switch_read(void);               // dip_switch.cpp
void loop_CanMessageEachSecond(uint32_t td); // loop_can_messages.cpp

/****** Shared Objects *******/

// Temperature Sensors Variables
PT100 supply_sensor = PT100(SUPPLY_CS);
PT100 return_sensor = PT100(RETURN_CS);

// Flow Sensor Variables
Flow flowObj = Flow(FLOW_TICKS_PER_LITER);

// CAN-Bus Variables
MCP_CAN CAN0(MCP_CS); // Set CS to pin 4

/* Node Status */
NodeStatus_t node_status = SETUP;

bool CANbusOff = false;
bool CANbusWarn = false;
bool HwFailure = true; // Assume HW failure until all init done

/* Node ID */
uint8_t node_id;

/* LED Indicators */
LEDs ledIndicators = LEDs(LED_RED, LED_GREEN);
/* CLI */
CLIScreenManager cli = CLIScreenManager();
// Scheduler
Scheduler scheduler = Scheduler();

// J1939 Variables
J1939_HeartBeat CAN_heartbeat_msg = J1939_HeartBeat();
J1939_Temperature CAN_Temp_msg = J1939_Temperature();
J1939_FilteredTemperatureAndFlow CAN_TempAndFlow = J1939_FilteredTemperatureAndFlow();

void setup()
{

  cli.begin();

  /* Configure microcontroller. */

  bool mcp_init = false, mcp_normal = false, supply_init = false, return_init = false;

  // Initialize Serial Monitor
  Serial.begin(115200); // This pipes to the serial monitor

  // Read node address for dip-switch
  node_id = dip_switch_read() | NODE_ID_BASE;

  // Initialize CAN-Bus
  if ((mcp_init = (CAN0.begin(MCP_ANY, CAN_250KBPS, MCP_8MHZ) == CAN_OK)))
    if ((mcp_normal = (CAN0.setMode(MCP_NORMAL) == MCP2515_OK)))
    {
      // Set up heartbeat message
      CAN_heartbeat_msg.begin(node_id);
      scheduler.addTask(loop_CanMessageEachSecond, 1000);
      scheduler.addTask([](uint32_t td)
                        {
                            uint8_t error_flags = CAN0.getError();
                            // Bus-Off when TXBO bit (transmit bus-off) is set
                            CANbusOff = (error_flags & MCP_EFLG_TXBO) != 0;
                            // Warning when any of EWARN, RXWAR, TXWAR bits set (error counters >= 96)
                            CANbusWarn = (error_flags & (MCP_EFLG_EWARN | MCP_EFLG_RXWAR | MCP_EFLG_TXWAR)) != 0; },
                        100);
    }

  // Initialize the MAX31865
  supply_init = supply_sensor.begin((float)PT100_ALPHA);
  return_init = return_sensor.begin((float)PT100_ALPHA);

  if (!mcp_normal || !supply_init || !return_init)
  {
    node_status = STOP;
    bool reboot_now = false;
    while (millis() < 60000 && !reboot_now)
    {
      reboot_now = cli.drawHardwareFailure(mcp_init, mcp_normal, supply_init, return_init);
      ledIndicators.process(true);
      scheduler.run();
    }
    wdt_enable(WDTO_15MS);
    while (1)
      ;
  }

  // Initialize Flow Sensor
  flowObj.begin([](float flow) {}); // TODO: Update energy calculation

  // Initialize J1939
  CAN_Temp_msg.begin(node_id);
  CAN_TempAndFlow.begin(node_id);

  // Trigger new reading every 1000ms
  scheduler.addTask([](uint32_t td)
                    {
                      if (!supply_sensor.triggerMeasurement())
                        cli.logError("Unable to read SUPPLY");
                      if (!return_sensor.triggerMeasurement())
                        cli.logError("Unable to read RETURN"); },
                    PT100_SAMPLE_RATE);

  scheduler.addTask([](uint32_t td)
                    { cli.periodic(); },
                    1000);

  // Update node status
  node_status = RUN;
}

void loop()
{
  // Run the scheduler
  scheduler.run();
  // process each sensor
  supply_sensor.process();
  return_sensor.process();
  flowObj.process();
  cli.process();
  // Check if the flow is zero
  if (flowObj.isFlowing())
    node_status = RUN;
  else
    node_status = SLEEP;
  // Update LEDs
  ledIndicators.process();
}
