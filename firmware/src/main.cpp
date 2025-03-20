/*
  TFM-100

  Manage the TFM-100 board reading sensors and sending data via CAN Bus

  created 9 Mar 2025
  by Salvo Musumeci
*/

// the setup function runs once when you press reset or power the board



//Framework lib
#include <Arduino.h>
#include <SPI.h>
#include "config.h"

//MyLib
#include <scheduler.h>
#include <logger.h>

//3rd parties lib
#include <mcp_can.h>

//Project Headers
#include "LEDs/LEDs.h"
#include "PT100/PT100.h"
#include "J1939/j1939.h"
#include "main.h"
#include "status.h"

/* Node Status */
NodeStatus_t node_status = SETUP;
uint8_t node_id;
bool CANbusOff = false;
bool CANbusWarn = false;
bool PT100Err = false;
bool HwFailure = false;

// Scheduler
Scheduler scheduler = Scheduler();

// CAN-Bus Variables
MCP_CAN CAN0(MCP_CS);     // Set CS to pin 4
uint32_t rxId;
uint8_t len;
uint8_t rxBuf[8];

// J1939 Variables
J1939_HeartBeat CAN_heartbeat_msg = J1939_HeartBeat();
J1939_Temperature CAN_Temp_msg = J1939_Temperature();
J1939_FilteredTemperatureAndFlow CAN_TempAndFlow = J1939_FilteredTemperatureAndFlow();

// Temperature Sensors Variables
PT100 supply_sensor = PT100(SUPPLY_CS);
PT100 return_sensor = PT100(RETURN_CS);

// Flow Sensor Variables
float flow = 0.0;

/* Send messages on CAN each second */
void loop_CanMessageEachSecond(uint32_t td){
  uint8_t data[8];
  uint8_t length;
  // Send HeartBeat
  CAN_heartbeat_msg.getData(data, &length);
  if (CAN0.sendMsgBuf(CAN_heartbeat_msg.messageId, length, data) != CAN_OK)
    AddMessageToLog("Unable to send heartbeat", false);
  
  // Send Temperature
  CAN_Temp_msg.getData(data, &length);
  if (CAN0.sendMsgBuf(CAN_Temp_msg.messageId, length, data) != CAN_OK)
    AddMessageToLog("Unable to send temperature", false);
  
  // Send Temperature and Flow
  CAN_TempAndFlow.getData(data, &length);
  if (CAN0.sendMsgBuf(CAN_TempAndFlow.messageId, length, data) != CAN_OK)
    AddMessageToLog("Unable to send temperature and flow", false);  

}


void setup()
{
  LEDs_init(LED_RED, LED_GREEN);

  // Initialize Serial Monitor
  Serial.begin(115200); // This pipes to the serial monitor
  delay(5000);

  // if (USBDevice.configured()) {
  //   // Wait for the USB to be configured
  //   delay(10000);
  // }
  

  /* Configure microcontroller. */
  AddInfoToLog("Configuring microcontroller...");
  pinMode(DS_B0, INPUT_PULLUP);
  pinMode(DS_B1, INPUT_PULLUP);
  pinMode(DS_B2, INPUT_PULLUP);
  pinMode(DS_B3, INPUT_PULLUP);

  // Read node address for dip-switch
  node_id = dip_switch_read() | NODE_ID_BASE;
  AddInfoToLog("Node ID: " + String(node_id));

  // Initialize CAN-Bus
  bool mcp_init = (CAN0.begin(MCP_ANY, CAN_250KBPS, MCP_8MHZ) == CAN_OK);
  if (AddMessageToLog("MCP25625 Initialization", mcp_init)){
    mcp_init = (CAN0.setMode(MCP_NORMAL) == MCP2515_OK);
    AddMessageToLog("Set to Normal Mode", mcp_init, true);
  }
  // Configuring pin for /INT input
  pinMode(MCP_INT, INPUT_PULLUP); 

  // Initialize the MAX31865
  bool supply_init = supply_sensor.begin((float)PT100_ALPHA);
  // Add some reading to understand if the micro is working
  if (!AddMessageToLog("MAX31865 - Supply Initialization", supply_init)){
    AddInfoToLog("Fault register: 0x" + String(supply_sensor.last_fault, HEX), true);
  }
  bool return_init = return_sensor.begin((float)PT100_ALPHA);
  // Add some reading to understand if the micro is working
  if (!AddMessageToLog("MAX31865 - Return Initialization", return_init)){
    AddInfoToLog("Fault register: 0x" + String(return_sensor.last_fault, HEX), true);
  }
  
  //Calculate value of HW failure
  HwFailure = !(mcp_init && supply_init && return_init);

  // Initialize J1939
  CAN_heartbeat_msg.begin(node_id, &node_status, []() { return scheduler.getUptime(); });
  CAN_Temp_msg.begin(node_id, &(supply_sensor.last_temperature), &(return_sensor.last_temperature));
  CAN_TempAndFlow.begin(node_id, &(supply_sensor.average_temperature), &(return_sensor.average_temperature), &flow);

  // Initialize Scheduler
  scheduler.addTask([](uint32_t timeDifference_ms) {
    LEDs_process(timeDifference_ms, node_status, CANbusOff, CANbusWarn, PT100Err, HwFailure);
  }, 50);

  if (HwFailure) // Do not continue in case of HW failure
  {
    node_status = STOP;
    return;
  }

  // Trigger new reading every 1000ms
  scheduler.addTask([](uint32_t td){ 
      if(!supply_sensor.triggerMeasurement()) AddMessageToLog("Unable to read SUPPLY", false);
      if(!return_sensor.triggerMeasurement()) AddMessageToLog("Unable to read RETURN", false);
  }, PT100_SAMPLE_RATE);

  // Send data every 1000ms
  scheduler.addTask(loop_CanMessageEachSecond, 1000);
  node_status = RUN;
}

void loop()
{
  scheduler.run();
  supply_sensor.process();
  return_sensor.process();
  PT100Err = supply_sensor.errorDetected || return_sensor.errorDetected;
  
  
}
