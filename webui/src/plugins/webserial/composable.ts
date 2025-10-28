import { computed } from "vue";
import { useSerial } from "./index";

export function useWebSerial() {
  const { service, state } = useSerial();
  return {
    isSupported: computed(() => state.isSupported),
    isOpen: computed(() => state.isOpen),
    ports: computed(() => state.ports),
    selectedPort: computed(() => state.selectedPort),
    lastError: computed(() => state.lastError),
    rxCounter: computed(() => state.rxCounter),
    txCounter: computed(() => state.txCounter),

    refreshAuthorizedPorts: () => service.refreshAuthorizedPorts(),
    requestPort: () => service.requestPort(),
    selectPort: (p: SerialPort) => service.selectPort(p),
    open: (baud?: number) => service.open(baud),
    write: (d: string | Uint8Array) => service.write(d),
    close: () => service.close(),
    // Events API wrappers
    onLine: (cb: (line: string) => void) => {
      const fn = (e: Event) => cb((e as CustomEvent<string>).detail);
      service.events.addEventListener('serial-line', fn);
      return () => service.events.removeEventListener('serial-line', fn);
    },
    onBytes: (cb: (bytes: Uint8Array) => void) => {
      const fn = (e: Event) => cb((e as CustomEvent<Uint8Array>).detail);
      service.events.addEventListener('serial-bytes', fn);
      return () => service.events.removeEventListener('serial-bytes', fn);
    },
  };
}
