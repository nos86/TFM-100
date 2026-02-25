<template>
  <v-card
    variant="elevated"
    rounded="lg"
    class="px-4"
  >
    <v-card-title class="px-4">

      <p class="text-h6">
        <v-icon
          left
          class="mr-2"
        >mdi-usb-port</v-icon>
        Connessione Seriale
        <v-chip
          v-if="isOpen"
          class="ml-2"
          :color="props.status === 'RUN' ? 'success' : props.status === 'STOP' ? 'error' : 'warning'"
          density="comfortable"
        >{{ props.status }}</v-chip>
      </p>


    </v-card-title>
    <v-card-subtitle
      v-if="isOpen"
      class="text-center"
    >
      Dispositivo: {{ props.name }}
      {{ Number(props.firmwareVersion) == 255 ? 'PROTO' : 'v' + props.firmwareVersion }}
      (Nodo 0x{{ props.nodeId?.toString(16).padStart(2, '0').toUpperCase() ?? 'N/A' }})
    </v-card-subtitle>

    <v-card-text>
      <v-alert
        v-if="!isSupported"
        type="warning"
        variant="tonal"
        class="mb-4"
        density="comfortable"
      >
        Il browser non supporta Web Serial. Usa Chrome/Edge su desktop.
      </v-alert>

      <v-alert
        v-if="lastError"
        type="error"
        variant="tonal"
        class="mb-4"
        density="comfortable"
      >
        {{ lastError }}
      </v-alert>

      <template v-if="!isOpen">
        <v-row>
          <v-col
            cols="12"
            md="6"
          >
            <v-select
              v-model.number="baud"
              :items="baudItems"
              label="Baud rate"
              density="comfortable"
              variant="outlined"
              hide-details
            />
          </v-col>
          <v-col
            cols="12"
            md="6"
            class="d-flex align-center"
          >
            <v-btn
              color="primary"
              prepend-icon="mdi-link-variant"
              @click="onQuickConnect"
              :disabled="!isSupported"
              block
            >
              Connetti
            </v-btn>
          </v-col>
        </v-row>
      </template>

      <template v-else>

        <v-card-actions>
          <v-spacer />
          <v-btn
            color="error"
            variant="tonal"
            prepend-icon="mdi-close-octagon"
            @click="onDisconnect"
            block
          >
            Disconnetti
          </v-btn>
        </v-card-actions>
      </template>
    </v-card-text>
  </v-card>
</template>

<script setup lang="ts">
import { ref, computed, watch, defineProps } from 'vue'
import { useDisplay } from 'vuetify'
import { useWebSerial } from '@/plugins/webserial/composable'

const props = withDefaults(defineProps<{
  name?: string,
  nodeId?: number,
  status?: string,
  firmwareVersion?: string,
}>(), {
  name: 'unknown',
  nodeId: 0,
  status: 'STOP',
  firmwareVersion: '0'
})

const serial = useWebSerial()
// Unwrap booleans for template-safe usage (avoid unary ! on refs in template)
const isOpen = computed(() => serial.isOpen.value)
const isSupported = computed(() => serial.isSupported.value)
const lastError = computed(() => serial.lastError.value)

const baudItems = [9600, 19200, 38400, 57600, 115200]
const baud = ref<number>(115200)

const selectedTitle = computed(() => {
  const p = serial.selectedPort.value
  if (!p) return ''
  const info = p.getInfo?.()
  const vid = info?.usbVendorId != null ? '0x' + info.usbVendorId.toString(16).padStart(4, '0') : 'N/A'
  const pid = info?.usbProductId != null ? '0x' + info.usbProductId.toString(16).padStart(4, '0') : 'N/A'
  return `VID: ${vid} PID: ${pid}`
})

// Local unwrap helpers
const rxCounter = computed(() => serial.rxCounter.value)
const txCounter = computed(() => serial.txCounter.value)

async function onQuickConnect() {
  try {
    await serial.requestPort()
    await serial.open(baud.value)
  } catch (e) {
    // lastError nel servizio verrà popolato
  }
}

async function onDisconnect() {
  await serial.close()
}

// Responsive collapse like other cards
const { smAndDown } = useDisplay()
const isMobile = computed(() => !!smAndDown.value)
const collapsed = ref(false)
watch(isMobile, (m) => { collapsed.value = !!m }, { immediate: true })
function toggleCollapsed() { collapsed.value = !collapsed.value }

</script>

<style scoped>
.traffic {
  height: 220px;
  overflow: auto;
  background: rgb(var(--v-theme-surface));
  border: 1px solid rgba(var(--v-border-color), var(--v-border-opacity));
  border-radius: 8px;
}

.line {
  white-space: pre-wrap;
  word-break: break-word;
  line-height: 1.25rem;
}

.text-mono {
  font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono", "Courier New", monospace;
}
</style>
