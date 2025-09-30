/**
 * @file McpCanAdapter.h
 * @brief Adapter that implements the project's ICanDriver using the MCP_CAN library.
 *
 * Provides a minimal wrapper around the `MCP_CAN` class so the J1939 manager
 * can use a common `ICanDriver` interface. The adapter always sends extended
 * (29-bit) frames as required by J1939.
 */
#pragma once

#include "J1939Manager.h" // for ICanDriver
#include <mcp_can.h>

class McpCanAdapter : public ICanDriver
{
public:
    explicit McpCanAdapter(MCP_CAN &mcp_can) : mcp_can_(mcp_can) {}

    // Send an extended (29-bit) CAN frame. Returns true on success.
    bool sendExtendedFrame(uint32_t can_id, const uint8_t *data, uint8_t len) override
    {
        // MCP_CAN::sendMsgBuf(uint32_t id, bool is_extended, uint8_t len, const uint8_t *buf)
        // J1939 uses 29-bit extended frames
        return (mcp_can_.sendMsgBuf(can_id, 1, len, const_cast<uint8_t *>(data)) == CAN_OK);
    }

private:
    MCP_CAN &mcp_can_;
};
