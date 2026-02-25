<template>
  <v-app>
    <v-main>
      <v-row>
        <v-col
          cols="12"
          md=6
          lg=5
        >

          <v-container class="pt-6">
            <SerialCard
              :name="deviceName"
              :node-id="nodeId"
              :status="nodeStatusDescription"
              :firmware-version="firmwareVersion"
            />
          </v-container>
          <v-container class="pt-6">
            <TelemetryCard
              :temp-mandata-c="tempMandataC"
              :temp-ritorno-c="tempRitornoC"
              :water-flow-lh="waterFlowLh"
              :powerKW="powerKW"
              :powerPerc="powerPerc"
              :energy24hKWh="energy24hKWh"
              :energyTotalKWh="energyTotalKWh"
              @reset-energy-24h="onReset24h"
              @reset-energy-total="onResetTotal"
            />
          </v-container>
        </v-col>
        <v-col
          cols="12"
          md="6"
          lg="7"
        >
          <v-container class="pt-6">
            <DiagnosticsCard
              :faults="faults"
              @reset_dsm="onResetDsm"
            />
          </v-container>
        </v-col>
      </v-row>
    </v-main>
    <LogFooter
      ref="appFooter"
      :logs="log_list"
      :show-at-beginning="false"
    />
  </v-app>
</template>

<script setup>
import { ref, onMounted, onBeforeUnmount, computed } from 'vue'
import LogFooter from './components/LogFooter.vue';
import TelemetryCard from './components/TelemetryCard.vue';
import DiagnosticsCard from './components/DiagnosticsCard.vue';
import SerialCard from './components/SerialCard.vue';
import { useWebSerial } from './plugins/webserial/composable';
const log_list = ref([])
const faults = ref([])

// Demo reactive data (in real app, values come from main/serial)
const deviceName = ref(null)
const firmwareVersion = ref(null)
const nodeId = ref(null)

const tempMandataC = ref(0)
const tempRitornoC = ref(0)
const waterFlowLh = ref(0)
const powerKW = ref(0)
const powerPerc = ref(0)
const energy24hKWh = ref(0)
const energyTotalKWh = ref(0)
const nodeStatus = ref(null)

const nodeStatusDescription = computed(() => {
  switch (nodeStatus.value) {
    case 0: return "SETUP"
    case 1: return "RUN"
    case 127: return "SLEEP"
    case 255: return "STOP"
    default: return String(nodeStatus.value ?? "undefined")
  }
})


function onReset24h() {
  energy24hKWh.value = 0
}
function onResetTotal() {
  energyTotalKWh.value = 0
}

function onResetDsm() {
  // Send erase command to device: E\r\n
  serial.write('E\r\n')
  // Clear faults from UI
  faults.value = []
}

/* --------------------------------------------------------------------------
 * VSCP event constants (mirrors firmware/lib/VSCP/vscp.h)
 * -------------------------------------------------------------------------- */
const VSCP_CLASS1_INFORMATION   = 20
const VSCP_CLASS1_MEASUREMENT   = 10

const VSCP_TYPE_INFORMATION_HEARTBEAT      = 9
const VSCP_TYPE_MEASUREMENT_TEMPERATURE    = 6
const VSCP_TYPE_MEASUREMENT_ENERGY         = 10
const VSCP_TYPE_MEASUREMENT_POWER          = 11
const VSCP_TYPE_MEASUREMENT_VOLUME_FLOW    = 40

// Data coding byte constants
const VSCP_DCTYPE_NORMALIZED_INT = 0
const VSCP_UNIT_TEMP_CELSIUS     = 1
const VSCP_UNIT_ENERGY_KWH       = 1
const VSCP_UNIT_POWER_WATT       = 0
const VSCP_UNIT_POWER_KW         = 1
const VSCP_UNIT_FLOW_LH          = 3

/**
 * Decode a VSCP CLASS1.MEASUREMENT normalised-integer payload.
 * @param {number[]} bytes  Array of payload bytes
 * @returns {{ unit: number, sensorIdx: number, value: number } | null}
 */
function decodeVscpMeasurement(bytes) {
  if (!bytes || bytes.length < 4) return null
  const coding   = bytes[0]
  const dcType   = (coding >> 5) & 0x07
  const unit     = (coding >> 3) & 0x03
  const sensorIdx = coding & 0x07

  if (dcType !== VSCP_DCTYPE_NORMALIZED_INT) return null

  const exponent = bytes[1] > 127 ? bytes[1] - 256 : bytes[1]  // signed int8

  let intVal
  if (bytes.length === 4) {
    // 2-byte signed integer (big-endian)
    intVal = (bytes[2] << 8) | bytes[3]
    if (intVal & 0x8000) intVal -= 65536
  } else if (bytes.length >= 6) {
    // 4-byte unsigned integer (big-endian)
    intVal = ((bytes[2] << 24) | (bytes[3] << 16) | (bytes[4] << 8) | bytes[5]) >>> 0
  } else {
    return null
  }

  const value = intVal * Math.pow(10, exponent)
  return { unit, sensorIdx, value }
}

/**
 * Handle a decoded VSCP event and update reactive state.
 * @param {number} vscp_class
 * @param {number} vscp_type
 * @param {number} ts         Timestamp (ms)
 * @param {number[]} bytes    Payload bytes
 */
function handleVscpEvent(vscp_class, vscp_type, ts, bytes) {
  if (vscp_class === VSCP_CLASS1_MEASUREMENT) {
    const m = decodeVscpMeasurement(bytes)
    if (!m) return

    if (vscp_type === VSCP_TYPE_MEASUREMENT_TEMPERATURE) {
      // Celsius only (unit 1); sensor 0 = supply, sensor 1 = return
      if (m.unit === VSCP_UNIT_TEMP_CELSIUS) {
        if (m.sensorIdx === 0) tempMandataC.value = parseFloat(m.value.toFixed(2))
        else if (m.sensorIdx === 1) tempRitornoC.value = parseFloat(m.value.toFixed(2))
      }
    } else if (vscp_type === VSCP_TYPE_MEASUREMENT_VOLUME_FLOW) {
      // Litres/hour (unit 3); sensor 0 = main flow
      if (m.unit === VSCP_UNIT_FLOW_LH && m.sensorIdx === 0)
        waterFlowLh.value = parseFloat(m.value.toFixed(1))
    } else if (vscp_type === VSCP_TYPE_MEASUREMENT_POWER) {
      // Watts (unit 0) → convert to kW; Kilowatts (unit 1) → direct
      if (m.unit === VSCP_UNIT_POWER_WATT)
        powerKW.value = parseFloat((m.value / 1000).toFixed(3))
      else if (m.unit === VSCP_UNIT_POWER_KW)
        powerKW.value = parseFloat(m.value.toFixed(3))
    } else if (vscp_type === VSCP_TYPE_MEASUREMENT_ENERGY) {
      // kWh (unit 1); sensor 0 = 24 h energy, sensor 1 = total energy
      if (m.unit === VSCP_UNIT_ENERGY_KWH) {
        if (m.sensorIdx === 0) energy24hKWh.value = parseFloat(m.value.toFixed(2))
        else if (m.sensorIdx === 1) energyTotalKWh.value = parseFloat(m.value.toFixed(2))
      }
    }
  } else if (vscp_class === VSCP_CLASS1_INFORMATION) {
    if (vscp_type === VSCP_TYPE_INFORMATION_HEARTBEAT && bytes.length >= 1) {
      // Byte 0: node status (NodeStatus_t: 0=SETUP, 1=RUN, 127=SLEEP, 255=STOP)
      nodeStatus.value = bytes[0]
      if (bytes.length >= 2)
        firmwareVersion.value = '0x' + bytes[1].toString(16).toUpperCase().padStart(2, '0')
    }
  }
}

// Subscribe to serial events and push to log footer
const serial = useWebSerial()
const off = []
onMounted(() => {
  off.push(serial.onLine((line) => {
    const data = line.split(";")
    switch (data[0]) {
      case "L": {
        const logLevelMap = {
          D: 'DEBUG',
          E: 'ERROR',
          W: 'WARNING',
          I: 'INFO'
        };
        const level = logLevelMap[data[1]] || 'INFO';
        log_list.value.push({ level, message: data[2], dt: new Date() })
        break;
      }
      case "P":
        // Legacy P;key;value – still handled for FW, FV, ID sent at connect time
        switch (data[1]) {
          case "FW": deviceName.value = data[2]; break;
          case "FV": firmwareVersion.value = data[2]; break;
          case "ID": nodeId.value = parseInt(data[2]); break;
          default: break;
        }
        break;
      case "VSCP": {
        // VSCP;class;type;timestamp;hh,hh,...
        const vscp_class = parseInt(data[1])
        const vscp_type  = parseInt(data[2])
        const ts         = parseInt(data[3])
        const bytes = data[4]
          ? data[4].trim().split(',').map(h => parseInt(h, 16))
          : []
        handleVscpEvent(vscp_class, vscp_type, ts, bytes)
        break;
      }
      case "D":
        if (data.length == 2) {
          // D;count - Update array size: keep existing entries, add/remove placeholders as needed
          const newCount = parseInt(data[1])
          const oldCount = faults.value.length
          
          if (newCount > oldCount) {
            // Add placeholders for new entries
            for (let i = oldCount; i < newCount; i++) {
              faults.value[i] = {
                name: "UNKNOWN",
                spn: "0",
                fmi: "0",
                oc: 0,
                status: "ACTIVE"
              }
            }
          } else if (newCount < oldCount) {
            // Remove entries beyond newCount
            faults.value.splice(newCount)
          }
          // If newCount == oldCount, do nothing (no change in size)
        } else {
          // D;index;description;SPN-FMI;status;OC - Replace entry with actual DTC
          const index = parseInt(data[1])
          faults.value[index] = {
            name: data[2],
            spn: data[3].split("-")[0],
            fmi: data[3].split("-")[1],
            oc: parseInt(data[5]),
            status: data[4]
          }
        }
        break;
      default:
        log_list.value.push({ level: 'DEBUG', message: line, dt: new Date() })
        break;
    }

  }))
  off.push(serial.onBytes((bytes) => {
    const hex = Array.from(bytes).map(b => b.toString(16).padStart(2, '0')).join(' ')
    log_list.value.push({ level: 'DEBUG', message: hex, dt: new Date() })
  }))
})
onBeforeUnmount(() => {
  off.forEach(fn => fn())
  off.length = 0
})
</script>
