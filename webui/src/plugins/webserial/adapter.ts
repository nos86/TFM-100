export interface SerialGlobal {
  getPorts(): Promise<SerialPort[]>;
  requestPort(options?: { filters?: SerialPortFilter[] }): Promise<SerialPort>;
}

export function getSerialGlobal(): SerialGlobal | null {
  if (typeof window === 'undefined') return null;
  const nav = (window.navigator as Navigator & { serial?: any });
  return nav.serial ? nav.serial as SerialGlobal : null;
}
