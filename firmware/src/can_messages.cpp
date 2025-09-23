#include <Arduino.h>
#include <j1939.h>
#include <mcp_can.h>
#include <flow.h>
#include <cli.h>

extern J1939_HeartBeat CAN_heartbeat_msg;
extern J1939_Temperature CAN_Temp_msg;
extern J1939_FilteredTemperatureAndFlow CAN_TempAndFlow;

extern MCP_CAN CAN0;
extern Flow flowObj;

extern CLIScreenManager cli;

/* Send messages on CAN each second */
void loop_CanMessageEachSecond(uint32_t td)
{
  uint8_t data[8];
  uint8_t length;
  // Send HeartBeat
  if (CAN_heartbeat_msg.isInitialized())
  {
    CAN_heartbeat_msg.getData(data, &length);
    if (CAN0.sendMsgBuf(CAN_heartbeat_msg.messageId, length, data) != CAN_OK)
      cli.logError("Unable to send heartbeat");
  }

  // Send Temperature
  if (CAN_Temp_msg.isInitialized())
  {
    CAN_Temp_msg.getData(data, &length);
    if (CAN0.sendMsgBuf(CAN_Temp_msg.messageId, length, data) != CAN_OK)
      cli.logError("Unable to send temperature");
  }

  // Send Temperature and Flow
  if (CAN_TempAndFlow.isInitialized())
  {
    CAN_TempAndFlow.getData(data, &length);
    if (CAN0.sendMsgBuf(CAN_TempAndFlow.messageId, length, data) != CAN_OK)
      cli.logError("Unable to send temperature and flow");
  }
}
