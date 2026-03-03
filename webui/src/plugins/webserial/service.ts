import { SerialEventMap, SerialState, WebSerialPluginOptions } from "./types";
import { getSerialGlobal } from "./adapter";

export class WebSerialService {
  private serial = getSerialGlobal();
  private port: SerialPort | null = null;
  private reader: ReadableStreamDefaultReader<string | Uint8Array> | null = null;
  private writer: WritableStreamDefaultWriter<string | Uint8Array> | null = null;
  private readAbort = new AbortController();
  private readPipeAbort?: AbortController;
  private writePipeAbort?: AbortController;
  private readPipePromise?: Promise<void>;
  private writePipePromise?: Promise<void>;
  private writeQueue = Promise.resolve(); // semplice lock
  private opts: Required<WebSerialPluginOptions>;
  private textDecoder?: TextDecoderStream;
  private textEncoder?: TextEncoderStream;
  public events = new EventTarget();

  public state: SerialState = {
    isSupported: !!this.serial,
    isOpen: false,
    ports: [],
    selectedPort: null,
    lastError: null,
    rxCounter: 0,
    txCounter: 0,
  };

  constructor(opts: WebSerialPluginOptions) {
    this.opts = {
      filters: opts.filters ?? [],
      baudRate: opts.baudRate ?? 115200,
      lineEnding: opts.lineEnding ?? "\n",
      textMode: opts.textMode ?? true,
      reconnect: opts.reconnect ?? true,
      reconnectMaxDelayMs: opts.reconnectMaxDelayMs ?? 8000,
    };
    if (this.serial) this.refreshAuthorizedPorts();
  }

  async refreshAuthorizedPorts() {
    if (!this.serial) return;
    this.state.ports = await this.serial.getPorts();
  }

  async requestPort() {
    if (!this.serial) throw new Error("Web Serial not available");
    this.port = await this.serial.requestPort({ filters: this.opts.filters });
    this.state.selectedPort = this.port;
    if (this.state.ports.every(p => p !== this.port)) this.state.ports.push(this.port);
    this.port.addEventListener?.('disconnect', () => {
      this.state.isOpen = false;
      this.state.selectedPort = null;
      this.port = null;
      if (this.opts.reconnect) this.tryReconnect();
    });
  }

  /**
   * Seleziona una porta tra quelle già autorizzate (state.ports)
   */
  selectPort(port: SerialPort) {
    this.port = port;
    this.state.selectedPort = port;
  }

  private async tryReconnect() {
    // backoff lineare/semi-esponenziale semplice
    let delay = 500;
    while (!this.state.isOpen && delay <= this.opts.reconnectMaxDelayMs) {
      await new Promise(r => setTimeout(r, delay));
      await this.refreshAuthorizedPorts();
      // l’utente dovrà ri-selezionare/ri-aprire (per policy UX)
      delay = Math.min(delay * 2, this.opts.reconnectMaxDelayMs);
    }
  }

  async open(baud?: number) {
    if (!this.port) throw new Error("No port selected");
    try {
      const info = this.port.getInfo?.();
      console.debug('[WebSerial] open: attempting to open port', { baud: baud ?? this.opts.baudRate, info });
    } catch { }
    await this.port.open({ baudRate: baud ?? this.opts.baudRate });

    // pipeline read
    let readable: ReadableStream<any> | null = this.port.readable;
    if (this.opts.textMode) {
      this.textDecoder = new TextDecoderStream();
      this.readPipeAbort = new AbortController();
      this.readPipePromise = readable?.pipeTo(this.textDecoder.writable, { signal: this.readPipeAbort.signal }).catch(() => { });
      readable = this.textDecoder.readable;
    }
    this.reader = readable!.getReader();

    // pipeline write
    let writable: WritableStream<any> | null = this.port.writable;
    if (this.opts.textMode) {
      this.textEncoder = new TextEncoderStream();
      this.writePipeAbort = new AbortController();
      this.writePipePromise = this.textEncoder.readable
        .pipeTo(this.port.writable!, { signal: this.writePipeAbort.signal })
        .catch(() => { });
      writable = this.textEncoder.writable;
    }
    this.writer = writable!.getWriter();

    this.state.isOpen = true;
    this.state.lastError = null;
    try { console.debug('[WebSerial] open: port opened successfully') } catch { }
    this.readAbort = new AbortController();
    void this.readLoop(this.readAbort.signal);
  }

  private async readLoop(signal: AbortSignal) {
    try {
      let lineBuf = "";
      while (this.reader && !signal.aborted) {
        const { value, done } = await this.reader.read();
        if (done) break;
        if (value != null) {
          // Puoi emettere su un Subject/EventEmitter; qui solo contatore
          this.state.rxCounter += (typeof value === 'string') ? value.length : value.byteLength;
          if (typeof value === 'string') {
            // Accumula e splitta per newline
            lineBuf += value;
            const parts = lineBuf.split("\n");
            // l'ultima è parziale
            lineBuf = parts.pop() ?? "";
            for (const ln of parts) this.emitLine(ln.replace("\r", ""));
          } else {
            // Modalità binaria: mostra come esadecimale
            const hex = Array.from(value as Uint8Array).map(b => b.toString(16).padStart(2, '0')).join(' ');
            this.emitBytes(value as Uint8Array);
          }
        }
      }
    } catch (e: any) {
      this.state.lastError = e?.message ?? String(e);
    }
  }

  private emitLine(line: string) {
    // Do not store frames; only emit event for subscribers
    this.events.dispatchEvent(new CustomEvent('serial-line', { detail: line } as SerialEventMap['serial-line']));
  }

  private emitBytes(bytes: Uint8Array) {
    this.events.dispatchEvent(new CustomEvent('serial-bytes', { detail: bytes } as SerialEventMap['serial-bytes']));
  }

  async write(data: string | Uint8Array) {
    if (!this.writer) throw new Error("Port not open");
    this.writeQueue = this.writeQueue.then(async () => {
      const payload = (typeof data === 'string' && this.opts.textMode && this.opts.lineEnding)
        ? data + this.opts.lineEnding
        : data;
      await this.writer!.ready;
      await this.writer!.write(payload);
      this.state.txCounter += (typeof payload === 'string') ? payload.length : payload.byteLength;
    });
    return this.writeQueue;
  }

  async close() {
    try {
      // Stop the read loop
      this.readAbort.abort();

      // Cancel/cleanup reader chain
      if (this.reader) {
        try { await this.reader.cancel(); } catch { }
        try { this.reader.releaseLock(); } catch { }
      }
      // Abort read pipe (port.readable -> TextDecoderStream.writable)
      try { this.readPipeAbort?.abort(); } catch { }
      try { await this.readPipePromise; } catch { }
      // Tear down TextDecoderStream instance
      this.textDecoder = undefined;

      // Close/cleanup writer chain
      if (this.writer) {
        try { await this.writer.close(); } catch { }
        try { this.writer.releaseLock(); } catch { }
      }
      // Abort write pipe (TextEncoderStream.readable -> port.writable)
      try { this.writePipeAbort?.abort(); } catch { }
      try { await this.writePipePromise; } catch { }
      // Tear down TextEncoderStream instance
      if (this.textEncoder) {
        try { await this.textEncoder.writable.close(); } catch { }
      }
      this.textEncoder = undefined;

      // Finally close the port
      if (this.port) {
        try { await this.port.close(); } catch { }
      }
    } finally {
      this.reader = null; this.writer = null;
      this.state.isOpen = false;
      try { console.debug('[WebSerial] close: port closed') } catch { }
    }
  }
}
