#include "diag_comm.h"
#include "diagnostics.h"
#include "dtc.h"
#include <vscp.h>

extern TFM100_DTC_Dict dtc_dict_instance; // DTC dictionary instance (defined in main.cpp)

// Static member definitions
SerialProtocol DiagComm::comm;
uint32_t DiagComm::last_diag_update = 0;
DiagComm *DiagComm::s_instance = nullptr;

void DiagComm::process(uint32_t td)
{
    // Check if client is connected
    if ((Serial.dtr() == HIGH) and !clientConnected)
    {
        clientConnected = true;
        // Client just connected
        comm.send_log(SerialProtocol::LOG_INFO, "Sending complete diagnostics memory");
        comm.send_param("FW", "TFM-100");
        comm.send_param("FV", (uint32_t)FIRMWARE_VERSION);
        comm.send_param("ID", (uint32_t)node_id);
        periodically_update();
        send_complete_diagnostics();
    }
    else if ((Serial.dtr() == LOW) and clientConnected)
    {
        clientConnected = false;
        // Client just disconnected
    }
    else if (clientConnected)
    {
        if (td - last_diag_update >= 1000) // 1s interval for periodic updates
        {
            last_diag_update = td;
            periodically_update();
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
}

void DiagComm::periodically_update()
{
    uint32_t ts = (uint32_t)millis();
    uint8_t  buf[6];

    // ---- Supply temperature (CLASS1.MEASUREMENT, Temperature, sensor 0) ----
    // Unit: Celsius (opt unit 1), exponent -2 (centi-degrees as int16, big-endian)
    {
        int32_t raw = (int32_t)roundf(supply_sensor.average_temperature * 100.0f);
        if (raw > 32767)  raw = 32767;
        if (raw < -32768) raw = -32768;
        vscp_encode_int16(buf, VSCP_UNIT_TEMP_CELSIUS, 0, -2, (int16_t)raw);
        comm.send_vscp_event(VSCP_CLASS1_MEASUREMENT,
                             VSCP_TYPE_MEASUREMENT_TEMPERATURE, node_id, ts, buf, 4);
    }

    // ---- Return temperature (CLASS1.MEASUREMENT, Temperature, sensor 1) ----
    {
        int32_t raw = (int32_t)roundf(return_sensor.average_temperature * 100.0f);
        if (raw > 32767)  raw = 32767;
        if (raw < -32768) raw = -32768;
        vscp_encode_int16(buf, VSCP_UNIT_TEMP_CELSIUS, 1, -2, (int16_t)raw);
        comm.send_vscp_event(VSCP_CLASS1_MEASUREMENT,
                             VSCP_TYPE_MEASUREMENT_TEMPERATURE, node_id, ts, buf, 4);
    }

    // ---- Volume flow rate (CLASS1.MEASUREMENT, Flow type=36, sensor 0) ----
    // Unit: Litres/second (VSCP_UNIT_FLOW_LS=1), exponent -3
    // Internal l/h converted: value = round(l/h × 10 / 36)
    {
        uint16_t flow_lh = flowObj.getFlow();
        uint16_t mls = (uint16_t)(((uint32_t)flow_lh * 10u + 18u) / 36u);
        vscp_encode_uint16(buf, VSCP_UNIT_FLOW_LS, 0, -3, mls);
        comm.send_vscp_event(VSCP_CLASS1_MEASUREMENT,
                             VSCP_TYPE_MEASUREMENT_FLOW, node_id, ts, buf, 4);
    }

    // ---- Thermal power (CLASS1.MEASUREMENT, Power type=14, sensor 0) ----
    // Unit: Watts (default), exponent 0
    {
        float kw     = getThermalPower(supply_sensor.average_temperature,
                                       return_sensor.average_temperature,
                                       flowObj.getFlow());
        float watts  = kw * 1000.0f;
        if (watts < 0.0f)    watts = 0.0f;
        if (watts > 65535.0f) watts = 65535.0f;
        vscp_encode_uint16(buf, VSCP_UNIT_POWER_WATT, 0, 0,
                           (uint16_t)roundf(watts));
        comm.send_vscp_event(VSCP_CLASS1_MEASUREMENT,
                             VSCP_TYPE_MEASUREMENT_POWER, node_id, ts, buf, 4);
    }

    // ---- Energy last 24 h (CLASS1.MEASUREMENT, Energy type=13, sensor 0) ----
    // Unit: kWh (opt unit 1), exponent -2 (centi-kWh as uint16)
    {
        float e24 = energyObj.getEnergy24h();
        if (e24 < 0.0f)    e24 = 0.0f;
        if (e24 > 655.35f) e24 = 655.35f;
        vscp_encode_uint16(buf, VSCP_UNIT_ENERGY_KWH, 0, -2,
                           (uint16_t)roundf(e24 * 100.0f));
        comm.send_vscp_event(VSCP_CLASS1_MEASUREMENT,
                             VSCP_TYPE_MEASUREMENT_ENERGY, node_id, ts, buf, 4);
    }

    // ---- Total energy (CLASS1.MEASUREMENT, Energy type=13, sensor 1) ----
    // Unit: kWh (opt unit 1), exponent 0 (integer kWh as uint32)
    {
        float et = energyObj.getEnergyTotal();
        if (et < 0.0f) et = 0.0f;
        if (et > 4294967295.0f) et = 4294967295.0f;
        vscp_encode_uint32(buf, VSCP_UNIT_ENERGY_KWH, 1, 0, (uint32_t)et);
        comm.send_vscp_event(VSCP_CLASS1_MEASUREMENT,
                             VSCP_TYPE_MEASUREMENT_ENERGY, node_id, ts, buf, 6);
    }

    // ---- Heartbeat (CLASS1.INFORMATION, Node Heartbeat type=9) ----
    // Per VSCP spec: byte0=user-specified, byte1=Zone(0xFF), byte2=SubZone(0xFF)
    // bytes 3+: optional device-specific data
    {
        buf[0] = (uint8_t)node_status;               // user-specified: node status
        buf[1] = 0xFFu;                               // zone: 0xFF = all zones
        buf[2] = 0xFFu;                               // sub-zone: 0xFF = all sub-zones
        buf[3] = (uint8_t)(FIRMWARE_VERSION & 0xFFu); // optional: firmware version
        comm.send_vscp_event(VSCP_CLASS1_INFORMATION,
                             VSCP_TYPE_INFORMATION_HEARTBEAT, node_id, ts, buf, 4);
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

void DiagComm::onEraseDiagnosticsCallback(void)
{
    // Static trampoline: forward to instance if available
    if (s_instance)
    {
        // Erase the diagnostics memory
        DSM.clear();
    }
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
    dtc_dict_instance.getSPN(entry->dtc_idx, spn);
    dtc_dict_instance.getFMI(entry->dtc_idx, fmi);
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
    comm.send_diag_info(index, dtc_dict_instance.getName(entry->dtc_idx), spn, fmi, status, oc);
}