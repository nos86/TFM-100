<template>
  <v-card
    variant="elevated"
    rounded="lg"
    class="pa-4"
  >

    <v-card-title class="d-flex align-center">
      <span class="text-h6">
        <v-icon
          left
          class="mr-2"
        >mdi-stethoscope</v-icon>
        Diagnostica
      </span>
      <v-chip
        v-if="numberOfActiveFaults > 0"
        class="ml-2"
        color="pink"
        density="comfortable"
      >{{ numberOfActiveFaults }}</v-chip>

      <v-spacer></v-spacer>
      <v-btn
        icon
        size="small"
        variant="flat"
        @click="toggleCollapsed"
        v-if="isMobile"
      >
        <v-icon>{{ collapsed ? 'mdi-chevron-down' : 'mdi-chevron-up' }}</v-icon>
      </v-btn>
    </v-card-title>

    <v-card-text v-if="!collapsed">
      <template v-if="props.faults.length > 0">
        <v-table v-if="!isMobile">
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
              v-for="(item, index) in props.faults"
              :key="index"
            >
              <td>{{ item.name }}</td>
              <td class="text-center"><v-chip variant="outlined">{{ item.spn }}-{{ item.fmi }}</v-chip></td>
              <td class="text-center"><v-chip :color="getColorForStatus(item.status)">{{ item.status }}</v-chip></td>
              <td class="text-center">{{ item.oc }}</td>
            </tr>
          </tbody>
        </v-table>
        <v-list
          lines="two"
          v-else
        >
          <v-list-item
            v-for="(item, index) in props.faults"
            :key="index"
            class="px-0"
          >
            <v-list-item-title class="font-weight-bold d-flex text-center">
              {{ item.name }}
              <v-spacer />
              <v-chip
                :color="getColorForStatus(item.status)"
                class="mr-2 text-caption"
              >{{ item.status }}</v-chip>
            </v-list-item-title>
            <v-list-item-subtitle>
              Occorrenze: {{ item.oc }}
            </v-list-item-subtitle>
          </v-list-item>
        </v-list>
      </template>
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

const numberOfActiveFaults = computed(() => {
  return props.faults.filter(fault => fault.status === "ACTIVE").length;
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

function getColorForStatus(status: string) {
  switch (status) {
    case "HISTORY": return "indigo";
    case "PENDING": return "orange";
    case "ACTIVE": return "pink";
    case "HEALING": return "amber";
    default:
      console.warn(`Unexpected fault status '${status}', falling back to 'blue' color`);
      return "blue";
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
