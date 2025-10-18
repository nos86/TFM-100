#include "diag_comm.h"
#include "diagnostics.h"
#include "dtc.h"

extern TFM100_DTC_Dict dtc_dict_instance; // DTC dictionary instance (defined in main.cpp)

// Static member definitions
SerialProtocol DiagComm::comm;
DiagComm *DiagComm::s_instance = nullptr;

void DiagComm::process(uint32_t td)
{
    // Check if client is connected
    if ((Serial.dtr() == HIGH) and !clientConnected)
    {
        clientConnected = true;
        // Client just connected
        comm.send_log(SerialProtocol::LOG_INFO, "Sending complete diagnostics memory");
        send_complete_diagnostics();
    }
    else if ((Serial.dtr() == LOW) and clientConnected)
    {
        clientConnected = false;
        // Client just disconnected
    }
    // Read from Serial and feed protocol (non-blocking)
    while (Serial.available() > 0)
    {
        int c = Serial.read();
        if (c >= 0)
        {
            uint8_t ch = (uint8_t)c;
            comm.feed(&ch, 1);
        }
    }
}

void DiagComm::send_complete_diagnostics()
{
    // Send complete diagnostics memory over serial

    comm.send_diag_count(DSM.numberOfErrors());
    for (uint8_t i = 0; i < DSM.numberOfErrors(); i++)
        onDiagnosticsEntryChange(i, &DSM.getMemory()[i]);
}

void DiagComm::onDiagnosticsSizeChange(uint8_t newSize)
{
    // Static trampoline: forward to instance if available
    if (s_instance)
        s_instance->onDiagnosticsSizeChangeInstance(newSize);
}

void DiagComm::onDiagnosticsEntryChange(uint8_t index, const dtc_history_t *entry)
{
    // Static trampoline: forward to instance if available
    if (s_instance)
        s_instance->onDiagnosticsEntryChangeInstance(index, entry);
}

void DiagComm::onDiagnosticsSizeChangeInstance(uint8_t newSize)
{
    // Notify host of updated diagnostics size
    comm.send_diag_count(newSize);
}

void DiagComm::onDiagnosticsEntryChangeInstance(uint8_t index, const dtc_history_t *entry)
{
    // Notify host of updated diagnostics entry
    uint32_t spn = 0;
    uint8_t fmi = 0;
    const char *status = "UNKNOWN";
    dtc_dict_instance.getSPN(index, spn);
    dtc_dict_instance.getFMI(index, fmi);
    uint8_t oc = entry->occurrence;
    switch (dtc_get_state(entry))
    {
    case DTC_EMPTY:
        status = "EMPTY";
        break;
    case DTC_PENDING:
        status = "PENDING";
        break;
    case DTC_ACTIVE:
        status = "ACTIVE";
        break;
    case DTC_HEALING:
        status = "HEALING";
        break;
    case DTC_HISTORY:
        status = "HISTORY";
        break;
    default:
        status = "UNKNOWN";
        break;
    }
    comm.send_diag_info(index, dtc_dict_instance.getName(index), spn, fmi, status, oc);
}