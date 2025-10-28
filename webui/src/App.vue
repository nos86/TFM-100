<template>
  <v-app>
    <v-main>
      <v-row>
        <v-col
          cols="12"
          md=6
          lg=5
        >

          <v-container class="py-6">
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
      </v-row>
    </v-main>
    <LogFooter
      ref="appFooter"
      :logs="log_list"
    />
  </v-app>
</template>

<script setup>
import { ref, onMounted } from 'vue'
import LogFooter from './components/LogFooter.vue';
import TelemetryCard from './components/TelemetryCard.vue';
const logFooter = ref(null);
const log_list = ref([])
// helper - pick random level/message
function randLevel() {
  const list = ['DEBUG', 'INFO', 'WARN', 'ERROR']
  return list[Math.floor(Math.random() * list.length)]
}

function randMessage() {
  const samples = [
    'Connected to device',
    'Temperature reading: 23.5 C',
    'Port opened',
    'CAN bus timeout',
    'Received heartbeat',
    'Warning: sensor calibration needed',
    'Error parsing message',
  ]
  return samples[Math.floor(Math.random() * samples.length)]
}

// try to access AppFooter via template ref on root component
function tryAttachDemo() {
  try {

    // generate logs
    setInterval(() => {
      log_list.value.push({ level: randLevel(), message: randMessage(), dt: new Date() })
    }, 2000)

    return true
  } catch (e) {
    return false
  }
}

// small retry loop to wait until child refs are ready
let tries = 0
const t = setInterval(() => {
  if (tryAttachDemo() || tries++ > 10) clearInterval(t)
}, 300)

// Demo reactive data (in real app, values come from main/serial)
const tempMandataC = ref(42.3)
const tempRitornoC = ref(38.9)
const waterFlowLh = ref(720)
const powerKW = ref(4.6)
const powerPerc = ref(12)
const energy24hKWh = ref(18.2)
const energyTotalKWh = ref(253.7)

// Example small updater to simulate changes
// onMounted(() => {
//   setInterval(() => {
//     // simple random walk simulations
//     tempMandataC.value = Math.max(20, Math.min(80, tempMandataC.value + (Math.random() - 0.5)))
//     tempRitornoC.value = Math.max(15, Math.min(70, tempRitornoC.value + (Math.random() - 0.5)))
//     waterFlowLh.value = Math.max(0, Math.min(2000, waterFlowLh.value + (Math.random() - 0.5) * 10))
//     powerKW.value = Math.max(0, Math.min(maxPowerKW.value, powerKW.value + (Math.random() - 0.5)))
//     // integrate energy roughly
//     energy24hKWh.value = Math.max(0, energy24hKWh.value + powerKW.value / 3600)
//     energyTotalKWh.value = Math.max(0, energyTotalKWh.value + powerKW.value / 3600)
//   }, 1000)
// })

function onReset24h() {
  energy24hKWh.value = 0
}
function onResetTotal() {
  energyTotalKWh.value = 0
}
</script>
