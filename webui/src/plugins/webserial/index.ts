import { App, inject, reactive } from "vue";
import type { UnwrapNestedRefs } from "vue";
import { WebSerialService } from "./service";
import type { WebSerialPluginOptions, SerialState } from "./types";

const key = Symbol("WebSerial");

export function createWebSerialPlugin(options: WebSerialPluginOptions = {}) {
  const service = new WebSerialService(options);
  // Make the service.state itself be the reactive proxy so mutations inside
  // the service trigger UI updates (avoid mutating the raw object).
  const state: UnwrapNestedRefs<SerialState> = reactive(service.state);
  // Re-point the service to the reactive proxy
  // (TS cast because reactive() returns a proxy type).
  (service as any).state = state as unknown as SerialState;

  return {
    install(app: App) {
      app.provide(key, { service, state });
    }
  };
}

export function useSerial(): { service: WebSerialService; state: UnwrapNestedRefs<SerialState> } {
  const ctx = inject<{ service: WebSerialService; state: UnwrapNestedRefs<SerialState> }>(key);
  if (!ctx) throw new Error("WebSerialPlugin non installato");
  return ctx;
}
