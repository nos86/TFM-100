<template>
  <v-card
    variant="elevated"
    rounded="lg"
    prepend-icon="mdi-stethoscope"
    title="Diagnostica"
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

    <v-card-text v-if="!collapsed">
      <v-table v-if="props.faults.length > 0">
        <thead>
          <tr>
            <th class="text-left">
              Name
            </th>
            <th class="text-center">
              Code
            </th>
            <th class="text-center">Stato</th>
            <th class="text-center">Occorrenze</th>
          </tr>
        </thead>
        <tbody>
          <tr
            v-for="item in props.faults"
            :key="item.name"
          >
            <td>{{ item.name }}</td>
            <td class="text-center"><v-chip variant="outlined">{{ item.spn }}-{{ item.fmi }}</v-chip></td>
            <td class="text-center"><v-chip :color="getColorForStatus(item.status)">{{ item.status }}</v-chip></td>
            <td class="text-center">{{ item.oc }}</td>
          </tr>
        </tbody>
      </v-table>
      <v-row
        v-else
        justify="center"
      >
        <v-col>
          <div class="text-center text-disabled py-6">
            No faults to display.
          </div>
        </v-col>
      </v-row>

    </v-card-text>

    <v-card-actions v-if="!collapsed && (props.faults.length > 0)">
      <v-btn
        text="Cancella memoria"
        variant="outlined"
        block
        @click="onResetDsm"
      />
    </v-card-actions>
  </v-card>

</template>

<script setup lang="ts">
import { ref, defineEmits, defineProps, withDefaults, watch, computed } from 'vue';
import { useDisplay } from 'vuetify';

const props = withDefaults(defineProps<{
  // Define your props here if needed
  faults: Array<{ name: string; spn: number; fmi: number; oc: number; status: string }>;
}>(), {
  faults: () => [],
});

const emits = defineEmits<{
  (e: 'reset_dsm'): void;
}>();

function toggleCollapsed() {
  collapsed.value = !collapsed.value;
}

function onResetDsm() {
  emits('reset_dsm');
}

function getColorForStatus(status: String) {
  switch (status) {
    case "HISTORY": return "indigo";
    case "PENDING": return "orange";
    case "ACTIVE": return "pink";
    case "HEALING": return "amber";
    default: return "blue";
  }
}

// Responsive collapse: on mobile default to collapsed showing only progress bar
const { smAndDown } = useDisplay()
const isMobile = computed(() => !!smAndDown.value)
const collapsed = ref(false)

watch(isMobile, (m) => {
  // default collapsed when entering mobile; expand on desktop
  collapsed.value = !!m
}, { immediate: true })

</script>
