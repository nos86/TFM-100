// Note: SerialPort and SerialPortFilter are provided as global ambient types
// via @types/w3c-web-serial (referenced in tsconfig and env.d.ts). No import needed.

export type WebSerialPluginOptions = {
  filters?: SerialPortFilter[];     // [{ usbVendorId: 0x2341 }]
  baudRate?: number;                // 115200 default
  lineEnding?: "\n" | "\r\n" | ""; // per Text mode
  textMode?: boolean;               // true = TextDecoder/Encoder
  reconnect?: boolean;              // auto-reconnect
  reconnectMaxDelayMs?: number;     // backoff max
};

export type SerialState = {
  isSupported: boolean;
  isOpen: boolean;
  ports: SerialPort[];
  selectedPort: SerialPort | null;
  lastError: string | null;
  rxCounter: number;
  txCounter: number;
};

// Event types emitted by the WebSerialService via EventTarget
export type SerialEventMap = {
  'serial-line': CustomEvent<string>;        // decoded text line (without CR/LF)
  'serial-bytes': CustomEvent<Uint8Array>;   // raw bytes chunk
};
