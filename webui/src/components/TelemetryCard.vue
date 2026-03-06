<template>
  <v-card
    variant="elevated"
    rounded="lg"
    prepend-icon="mdi-signal"
    title="Telemetria Real-time"
    class="pa-4"
  >
    <template #append>
      <v-btn
        icon
        size="small"
        variant="flat"
        @click="toggleCollapsed"
        v-if="isMobile"
      >
        <v-icon>{{ collapsed ? 'mdi-chevron-down' : 'mdi-chevron-up' }}</v-icon>
      </v-btn>
    </template>

    <v-card-text>
      <v-row>
        <v-col cols="12">
          <v-progress-linear
            :model-value="powerPerc"
            color="primary"
            height="20"
            rounded
            striped
            class="mb-4"
          >
            <template #default>
              {{ fmt(powerPerc) }} %
            </template>
          </v-progress-linear>
        </v-col>
      </v-row>
      <v-row v-if="!collapsed">
        <v-col cols=12>
          <v-list-item :prepend-icon="isMobile ? '' : 'mdi-thermometer-chevron-up'">
            <v-list-item-title>Temperatura Mandata:</v-list-item-title>
            <template v-slot:append>
              {{ fmt(tempMandataC) }} °C
            </template>
          </v-list-item>

          <v-list-item :prepend-icon="isMobile ? '' : 'mdi-thermometer-chevron-down'">
            <v-list-item-title>Temperatura Ritorno:</v-list-item-title>
            <template v-slot:append>
              {{ fmt(tempRitornoC) }} °C
            </template>
          </v-list-item>

          <v-list-item :prepend-icon="isMobile ? '' : 'mdi-water'">
            <v-list-item-title>Portata :</v-list-item-title>
            <template v-slot:append>
              {{ fmt(waterFlowLh) }} l/h
            </template>
          </v-list-item>

          <v-divider class="my-6" />

          <v-list-item :prepend-icon="isMobile ? '' : 'mdi-transmission-tower'">
            <v-list-item-title>Potenza istantanea:</v-list-item-title>
            <template v-slot:append>
              {{ fmt(powerKw) }} kW
            </template>
          </v-list-item>

          <v-list-item :prepend-icon="isMobile ? '' : 'mdi-clock'">
            <v-list-item-title>Energia 24h</v-list-item-title>
            <template v-slot:append>
              {{ fmt(energy24hKwh) }} kWh
            </template>
          </v-list-item>

          <v-list-item :prepend-icon="isMobile ? '' : 'mdi-lightning-bolt'">
            <v-list-item-title>Energia totale:</v-list-item-title>
            <template v-slot:append>
              {{ fmt(energyTotalKwh) }} kWh
            </template>
          </v-list-item>
        </v-col>
      </v-row>

    </v-card-text>

    <v-card-actions v-if="!collapsed">
      <v-btn
        text="Reset 24h"
        variant="outlined"
        @click="emitReset24h"
        width="48%"
      />
      <v-spacer />
      <v-btn
        text="Reset Totale"
        variant="outlined"
        @click="emitResetTotal"
        width="48%"
      />
    </v-card-actions>
  </v-card>

</template>

<script setup>
import { computed, ref, watch } from 'vue'
import { useDisplay } from 'vuetify'

// Props from parent (all values flow from main)
const props = defineProps({
  tempMandataC: { type: Number, required: true },
  tempRitornoC: { type: Number, required: true },
  waterFlowLh: { type: Number, required: true },
  powerKw: { type: Number, required: true },
  powerPerc: { type: Number, default: 10 },
  energy24hKwh: { type: Number, required: true },
  energyTotalKwh: { type: Number, required: true },
})

// Emits for parent to handle resets
const emit = defineEmits(['reset-energy-24h', 'reset-energy-total'])

// Responsive collapse: on mobile default to collapsed showing only progress bar
const { smAndDown } = useDisplay()
const isMobile = computed(() => !!smAndDown.value)
const collapsed = ref(false)

watch(isMobile, (m) => {
  // default collapsed when entering mobile; expand on desktop
  collapsed.value = !!m
}, { immediate: true })

function toggleCollapsed() {
  collapsed.value = !collapsed.value
}

// Small number formatter
function fmt(v) {
  const n = Number(v)
  return Number.isFinite(n) ? n.toFixed(1) : '-'
}

function emitReset24h() { emit('reset-energy-24h') }
function emitResetTotal() { emit('reset-energy-total') }
</script>
