#pragma once

#include "stdint.h"
#include "SerialProtocol.h"

#include "diagnostics.h"
#include "PT100.h"

extern Diagnostics DSM;
extern PT100 supply_sensor;
extern PT100 return_sensor;

class DiagComm
{

public:
    DiagComm() { s_instance = this; };
    ~DiagComm() { s_instance = nullptr; };

    void begin(uint32_t baudrate)
    {
        Serial.begin(baudrate);
        comm.begin(); // Use default Serial writer
        DSM.setOnCountChangedCallback(onDiagnosticsSizeChange);
        DSM.setOnEntryChangedCallback(onDiagnosticsEntryChange);
    }
    static void onDiagnosticsSizeChange(uint8_t newSize);
    static void onDiagnosticsEntryChange(uint8_t index, const dtc_history_t *entry);

    // Instance handlers invoked by the static trampoline callbacks
    void onDiagnosticsSizeChangeInstance(uint8_t newSize);
    void onDiagnosticsEntryChangeInstance(uint8_t index, const dtc_history_t *entry);

    void send_log(SerialProtocol::LogLevel level, const char *message)
    {
        comm.send_log(level, message);
    }
    void send_error(const char *message)
    {
        comm.send_log(SerialProtocol::LOG_ERROR, message);
    }
    void send_warning(const char *message)
    {
        comm.send_log(SerialProtocol::LOG_WARNING, message);
    }
    void send_info(const char *message)
    {
        comm.send_log(SerialProtocol::LOG_INFO, message);
    }

    void process(uint32_t td);

    bool isClientConnected() const { return clientConnected; }

private:
    bool clientConnected = false;
    void send_complete_diagnostics();
    static SerialProtocol comm;  // --- Serial protocol instance ---
    static DiagComm *s_instance; // trampoline instance for static callbacks
};
