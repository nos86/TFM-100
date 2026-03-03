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
              :power-kW="powerKW"
              :power-perc="powerPerc"
              :energy24h-kwh="energy24hKWh"
              :energy-total-kwh="energyTotalKWh"
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
// helper - pick random level/message
function randLevel() {
  const list = ['DEBUG', 'INFO', 'WARN', 'ERROR']
  return list[Math.floor(Math.random() * list.length)]
}

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
  serial.write('E')
  // Clear faults from UI
  faults.value = []
}


// Subscribe to serial events and push to log footer
const serial = useWebSerial()
const off = []
onMounted(() => {
  off.push(serial.onLine((line) => {
    const data = line.split(";")
    switch (data[0]) {
      case "L":
        const logLevelMap = {
          D: 'DEBUG',
          E: 'ERROR',
          W: 'WARNING',
          I: 'INFO'
        };
        const level = logLevelMap[data[1]] || 'INFO';
        log_list.value.push({ level, message: data[2], dt: new Date() })
        break;
      case "P":
        switch (data[1]) {
          case "ST": tempMandataC.value = parseFloat(data[2]); break;
          case "RT": tempRitornoC.value = parseFloat(data[2]); break;
          case "WF": waterFlowLh.value = parseFloat(data[2]); break;
          case "ET": energyTotalKWh.value = parseFloat(data[2]); break;
          case "E24": energy24hKWh.value = parseFloat(data[2]); break;
          case "P%": powerPerc.value = parseFloat(data[2]); break;
          case "PWR": powerKW.value = parseFloat(data[2]); break;
          case "NS": nodeStatus.value = parseInt(data[2]); break;
          case "FW": deviceName.value = data[2]; break;
          case "FV": firmwareVersion.value = data[2]; break;
          case "ID": nodeId.value = parseInt(data[2]); break;
          default: console.warn("Unknown parameter: " + data[1] + "=" + data[2]); break;
        }
        break;
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
