<template>
  <transition name="fade">
    <v-card
      class="log-dock elevation-2"
      :style="cardStyle"
      rounded="0"
    >
      <!-- Resize handle -->
      <div
        class="resize-handle"
        @pointerdown="onPointerDown"
        aria-label="Resize handle"
        v-show="!collapsed"
      ></div>

      <!-- Toolbar -->
      <v-toolbar
        density="compact"
        class="px-2"
      >
        <v-btn
          icon
          :aria-label="collapsed ? 'Expand' : 'Collapse'"
          @click="toggleCollapsed"
        >
          <v-icon>{{ collapsed ? 'mdi-chevron-up' : 'mdi-chevron-down' }}</v-icon>
        </v-btn>
        <div
          class="d-flex align-center flex-grow-1"
          style="min-width:0"
        >
          <span class="font-weight-bold mr-3">Log</span>
          <div
            v-if="collapsed && logs.length"
            class="d-flex align-center flex-grow-1 overflow-hidden"
            style="min-width:0"
          >
            <span
              v-if="!isMobile"
              class="text-caption text-mono text-disabled mr-2 flex-shrink-0"
            >
              {{ formatTs(logs[logs.length - 1]?.dt) }}
            </span>
            <v-chip
              density="compact"
              :color="logs[logs.length - 1]?.backgroundColor || 'primary'"
              class="mr-2 flex-shrink-0"
            >
              {{ isMobile ? logs[logs.length - 1]?.level?.[0] : logs[logs.length - 1]?.level }}
            </v-chip>
            <span
              class="text-caption text-mono text-truncate flex-grow-1"
              style="min-width:0"
            >
              {{ logs[logs.length - 1]?.message ?? logs[logs.length - 1] }}
            </span>
          </div>
        </div>
        <v-spacer v-if="!collapsed"></v-spacer>

        <v-tooltip
          text="Autoscroll to bottom"
          location="top"
        >
          <template #activator="{ props }">
            <v-btn
              v-bind="props"
              icon
              :color="autoScroll ? 'primary' : undefined"
              @click="autoScroll = !autoScroll"
              v-show="!collapsed"
            >
              <v-icon>mdi-arrow-down</v-icon>
            </v-btn>
          </template>
        </v-tooltip>

        <v-tooltip
          text="Pulisci"
          location="top"
        >
          <template #activator="{ props }">
            <v-btn
              v-bind="props"
              icon
              @click="$emit('clear')"
              v-show="!collapsed"
            >
              <v-icon>mdi-delete-outline</v-icon>
            </v-btn>
          </template>
        </v-tooltip>
      </v-toolbar>

      <!-- Log content -->
      <div
        ref="scrollEl"
        class="log-content"
        v-show="!collapsed"
      >
        <div
          v-if="logs?.length"
          class="pa-2"
        >
          <div
            v-for="(row, idx) in logs"
            :key="idx"
            class="log-row text-mono text-caption my-1"
          >
            <span
              class="text-disabled"
              v-show="!isMobile"
            >{{ formatTs(row?.dt) }}</span>
            <v-chip
              density="compact"
              :color="row?.backgroundColor || 'primary'"
              class="mx-2"
            >{{ isMobile ? row?.level[0] :
              row?.level }}</v-chip>
            <span>{{ row?.message }}</span>
          </div>
        </div>
        <div
          v-else
          class="text-center text-disabled py-6"
        >
          No log to display.
        </div>
      </div>
    </v-card>
  </transition>
</template>

<script setup lang="ts">
import { onMounted, onBeforeUnmount, ref, watch, computed, nextTick } from 'vue'

/**
 * Props:
 * - logs: array of log entries { dt?, message, level, backgroundColor? }
 * - defaultHeightVh: initial height (mobile: 35, desktop: 40)
 * - minHeightPx / maxHeightVh: resize limit
 * - persistKey: if set, saves/loads height from localStorage
 */
const props = withDefaults(
  defineProps<{
    logs: Array<{ dt?: Date, message: string, level: string, backgroundColor?: string }>,
    defaultHeightVh?: number,
    minHeightPx?: number,
    maxHeightVh?: number,
    persistKey?: string | null,
    showAtBeginning?: boolean,
  }>(),
  {
    defaultHeightVh: 50,
    minHeightPx: 0, // only toolbar height when collapsed
    maxHeightVh: 120,
    persistKey: 'log-dock-height',
    showAtBeginning: false,
  }
)

const collapsed = ref(!props.showAtBeginning)
const heightPx = ref<number>(0)
const autoScroll = ref(true)
const scrollEl = ref<HTMLElement | null>(null)

const isMobile = computed(() => window.matchMedia('(max-width: 600px)').matches)
const initialVh = computed(() => (isMobile.value ? Math.min(props.defaultHeightVh, props.minHeightPx) : props.defaultHeightVh))

const toolbarPx = isMobile.value ? 70 : 52 // density compact
const collapsedHeightPx = Math.max(props.minHeightPx, toolbarPx) // toolbar + padding

function vhToPx(vh: number) {
  return (vh / 100) * window.innerHeight
}

function clampHeight(px: number) {
  const min = collapsed.value ? collapsedHeightPx : props.minHeightPx
  const max = vhToPx(props.maxHeightVh)
  return Math.min(Math.max(px, min), max)
}

function loadPersistedHeight() {
  if (!props.persistKey) return null
  const saved = Number(localStorage.getItem(props.persistKey))
  return Number.isFinite(saved) && saved > 0 ? saved : null
}

function persistHeight() {
  if (!props.persistKey) return
  localStorage.setItem(props.persistKey, String(heightPx.value))
}

function setHeight(px: number) {
  heightPx.value = clampHeight(px)
  persistHeight()
}

function toggleCollapsed() {
  collapsed.value = !collapsed.value
  if (!collapsed.value && heightPx.value < vhToPx(20)) {
    // se riespando da molto basso, porta a 30vh
    setHeight(vhToPx(30))
  }
}

const cardStyle = computed(() => {
  const h = collapsed.value ? `${collapsedHeightPx}px` : `${heightPx.value}px`
  return {
    height: h,
  }
})

// Resize via pointer
let startY = 0
let startH = 0
function onPointerMove(e: PointerEvent) {
  const dy = e.clientY - startY
  setHeight(startH - dy) // maniglia in alto: trascinare verso l’alto aumenta altezza
}
function onPointerUp(e?: PointerEvent) {
  window.removeEventListener('pointermove', onPointerMove)
  window.removeEventListener('pointerup', onPointerUp)
  document.body.style.userSelect = ''
  document.body.style.cursor = ''
}
function onPointerDown(e: PointerEvent) {
  if (collapsed.value) return
  startY = e.clientY
  startH = heightPx.value
  document.body.style.userSelect = 'none'
  document.body.style.cursor = 'ns-resize'
  window.addEventListener('pointermove', onPointerMove)
  window.addEventListener('pointerup', onPointerUp, { once: true })
}

// Autoscroll quando arrivano nuovi log
watch(
  () => props.logs?.length,
  async () => {
    if (!autoScroll.value || !scrollEl.value) return
    await nextTick()
    scrollEl.value.scrollTop = scrollEl.value.scrollHeight
  }
)

// Aggiorna clamp su resize finestra
function onWindowResize() {
  setHeight(heightPx.value)
}

onMounted(() => {
  const persisted = loadPersistedHeight()
  setHeight(persisted ?? vhToPx(initialVh.value))
  window.addEventListener('resize', onWindowResize)
})
onBeforeUnmount(() => {
  window.removeEventListener('resize', onWindowResize)
})

// Helpers
function formatTs(ts?: number | Date) {
  if (!ts) return ''
  const d = ts instanceof Date ? ts : new Date(ts)
  return d.toLocaleTimeString()
}
</script>

<style scoped>
.log-row {
  gap: 8px;
}

.log-row .msg {
  flex: 1;
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
}

.log-dock {
  position: fixed;
  left: 0;
  right: 0;
  bottom: 0;
  /* Limit mobile space to allow keyboard visibility */
  max-height: 90vh;
  display: flex;
  flex-direction: column;
  z-index: 3000;
  /* over main content */
  background: rgb(var(--v-theme-surface));
  border-top: 1px solid rgba(var(--v-border-color), var(--v-border-opacity));
}

.resize-handle {
  height: 8px;
  cursor: ns-resize;
  touch-action: none;
  display: flex;
  align-items: center;
  justify-content: center;
}

.resize-handle::after {
  content: "";
  width: 64px;
  height: 3px;
  border-radius: 3px;
  background: rgba(var(--v-theme-on-surface), 0.25);
}

.log-content {
  flex: 1;
  overflow: auto;
  background: rgb(var(--v-theme-surface));
}

.log-row {
  white-space: pre-wrap;
  word-break: break-word;
  line-height: 1.25rem;
}

.text-mono {
  font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, "Liberation Mono",
    "Courier New", monospace;
}

.fade-enter-active,
.fade-leave-active {
  transition: opacity 150ms ease;
}

.fade-enter-from,
.fade-leave-to {
  opacity: 0;
}
</style>
