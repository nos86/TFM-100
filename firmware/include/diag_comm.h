#pragma once

#include "stdint.h"
#include "SerialProtocol.h"

#include "diagnostics.h"
#include "PT100.h"
#include "flow.h"
#include "status.h"
#include "config.h"
#include "power.h"
#include "energy.h"

extern Diagnostics DSM;
extern PT100 supply_sensor;
extern PT100 return_sensor;
extern Flow flowObj;
extern Energy energyObj;
extern Power powerObj;
extern NodeStatus_t node_status;
extern uint8_t node_id;

class DiagComm
{

public:
    DiagComm() { s_instance = this; };
    ~DiagComm() { s_instance = nullptr; };

    void begin(uint32_t baudrate)
    {
        Serial.begin(baudrate);
        comm.begin(); // Use default Serial writer
        comm.set_erase_diagnostics_callback(onEraseDiagnosticsCallback);
        DSM.setOnCountChangedCallback(onDiagnosticsSizeChange);
        DSM.setOnEntryChangedCallback(onDiagnosticsEntryChange);
    }
    static void onDiagnosticsSizeChange(uint8_t newSize);
    static void onDiagnosticsEntryChange(uint8_t index, const dtc_history_t *entry);
    static void onEraseDiagnosticsCallback(void);

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
    void periodically_update();

    bool isClientConnected() const { return clientConnected; }

private:
    bool clientConnected = false;
    void send_complete_diagnostics();
    static SerialProtocol comm;       // --- Serial protocol instance ---
    static DiagComm *s_instance;      // trampoline instance for static callbacks
    static uint32_t last_diag_update; // timestamp of last periodic update
};
