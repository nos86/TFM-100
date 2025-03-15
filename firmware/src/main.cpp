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
#include <Adafruit_MAX31865.h>
#include <mcp_can.h>

//Project Headers
#include "LEDs/LEDs.h"


/* Node Status */
NodeStatus_t node_status = SETUP;
bool CANbusOff = false;
bool CANbusWarn = false;
bool InvalidConfig = false;
bool HwFailure = false;

// Scheduler
Scheduler scheduler = Scheduler();

// CAN-Bus Variables
MCP_CAN CAN0(MCP_CS);     // Set CS to pin 4
uint32_t rxId;
uint8_t len;
uint8_t rxBuf[8];

// Temperature Sensors Variables
Adafruit_MAX31865 supply_temperature = Adafruit_MAX31865(SUPPLY_CS);
Adafruit_MAX31865 return_temperature = Adafruit_MAX31865(RETURN_CS);

// CAN TX Variables
unsigned long prevTX = 0;                                       // Variable to store last execution time
const unsigned int invlTX = 1000;                               // One second interval constant
byte data[] = {0xAA, 0x55, 0x01, 0x10, 0xFF, 0x12, 0x34, 0x56}; // Generic CAN data to send
//

// Serial Output String Buffer
char msgString[128];




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
  bool mcp_init = (CAN0.begin(MCP_ANY, CAN_250KBPS, MCP_8MHZ) == CAN_OK);
  if (AddMessageToLog("MCP25625 Initialization", mcp_init)){
    mcp_init = (CAN0.setMode(MCP_NORMAL) == MCP2515_OK);
    AddMessageToLog("Set to Normal Mode", mcp_init, true);
  }
  pinMode(MCP_INT, INPUT); // Configuring pin for /INT input

  // Initialize the MAX31865
  bool supply_init = supply_temperature.begin(MAX31865_2WIRE);
  bool return_init = return_temperature.begin(MAX31865_2WIRE);
  AddMessageToLog("MAX31865 - Supply Initialization", supply_init);
  AddMessageToLog("MAX31865 - Return Initialization", return_init);
  
  //Calculate value of HW failure
  HwFailure = !(mcp_init && supply_init && return_init);

  // Initialize Scheduler
  scheduler.addTask([](uint32_t timeDifference_ms) {
    LEDs_process(timeDifference_ms, node_status, CANbusOff, CANbusWarn, InvalidConfig, HwFailure);
  }, 50);

  if (HwFailure) return; // Do not continue in case of HW failure

  // scheduler.addTask(loop_read_temperature, 1000);

  node_status = RUN;
}




void loop_read_temperature()
{
  unsigned long start_time = millis();
  float supply_temperature_value = supply_temperature.temperature(100, 400);
  float return_temperature_value = return_temperature.temperature(100, 400);
  unsigned long end_time = millis();
  printf("Supply Temperature: %.2f - Return Temperature: %.2f in %lu ms\n", (double)supply_temperature_value, (double)return_temperature_value, (end_time - start_time));
}

void loop_handle_canbus()
{
    // if(!digitalRead(MCP_INT))                          // If CAN0_INT pin is low, read receive buffer
  // {
  //   CAN0.readMsgBuf(&rxId, &len, rxBuf);              // Read data: len = data length, buf = data byte(s)

  //   if((rxId & 0x80000000) == 0x80000000)             // Determine if ID is standard (11 bits) or extended (29 bits)
  //     sprintf(msgString, "Extended ID: 0x%.8lX  DLC: %1d  Data:", (rxId & 0x1FFFFFFF), len);
  //   else
  //     sprintf(msgString, "Standard ID: 0x%.3lX       DLC: %1d  Data:", rxId, len);

  //   Serial.print(msgString);

  //   if((rxId & 0x40000000) == 0x40000000){            // Determine if message is a remote request frame.
  //     sprintf(msgString, " REMOTE REQUEST FRAME");
  //     Serial.print(msgString);
  //   } else {
  //     for(byte i = 0; i<len; i++){
  //       sprintf(msgString, " 0x%.2X", rxBuf[i]);
  //       Serial.print(msgString);
  //     }
  //   }

  //   Serial.println();
  // }

  // if(millis() - prevTX >= invlTX){                    // Send this at a one second interval.
  //   prevTX = millis();
  //   byte sndStat = CAN0.sendMsgBuf(0x100, 8, data);

  //   if(sndStat == CAN_OK)
  //     Serial.println("Message Sent Successfully!");
  //   else
  //     Serial.println("Error Sending Message...");

  // }

  // byte sndStat = CAN0.sendMsgBuf(0x100, 0, 8, data);
  // if(sndStat == CAN_OK){
  //   Serial.println("Message Sent Successfully!");
  //   TXLED1; //TX LED is not tied to a normally controlled pin so a macro is needed, turn LED ON
  // } else {
  //   Serial.println("Error Sending Message...");
  // }
  //   digitalWrite(LED_RED, LOW);   // set the RX LED ON
  // digitalWrite(4, LOW);

  // digitalWrite(LED_RED, HIGH);    // set the RX LED OFF
  // // digitalWrite(4, HIGH);
  // // TXLED0; //TX LED macro to turn LED OFF
  // delay(200);              // wait for a second
}

void loop()
{
  scheduler.run();
}
