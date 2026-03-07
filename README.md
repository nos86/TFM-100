# TFM-100 — Thermal Flow Meter

> **TFM-100** is an open-source embedded system for monitoring thermal energy in hydronic circuits. It measures supply and return water temperatures together with volumetric flow rate, calculates instantaneous power and cumulative energy, and publishes all data in real time over a CAN bus (J1939) and a USB serial interface.

---

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [Hardware](#2-hardware)
3. [Firmware](#3-firmware)
4. [Interface Software — Web UI](#4-interface-software--web-ui)
5. [Communication Protocols](#5-communication-protocols)
6. [Repository Structure](#6-repository-structure)
7. [Getting Started](#7-getting-started)
8. [Hardware Design Files](#8-hardware-design-files)

---

## 1. Project Overview

The TFM-100 is designed for installation in heating/cooling systems. It continuously reads two PT100 RTD temperature probes (supply and return lines) and a pulse-output flow sensor. From these three measurements it computes:

| Measurement | Description |
|---|---|
| Supply temperature | Raw and low-pass-filtered temperature of the supply line |
| Return temperature | Raw and low-pass-filtered temperature of the return line |
| Flow rate | Volumetric flow in litres per hour |
| Thermal power | Instantaneous power in Watts (ΔT × flow × specific heat) |
| Energy total | Cumulative energy in Wh, persisted to EEPROM |
| Energy (24 h) | Rolling 24-hour energy counter, persisted to EEPROM |

All measurements are broadcast on a J1939 CAN bus and are also streamed over USB serial for direct PC access. A browser-based Web UI connects via the **WebSerial API** to display live telemetry, diagnostic trouble codes (DTCs), and a scrolling log—no driver installation required.

---

## 2. Hardware

### 2.1 Microcontroller

| Parameter | Value |
|---|---|
| MCU | **ATmega32U4** |
| Clock | 8 MHz external crystal |
| Supply | 5 V (USB or external) |
| Brown-out detection | 2.6 V (efuse `0xCB`) |
| Bootloader | Caterina (USB-HID, CDC ACM) |
| Flash | 28 672 bytes usable |

The ATmega32U4 provides native USB (CDC-ACM serial) without an external USB-to-UART bridge, which means it enumerates directly as a serial port on any OS.

### 2.2 Temperature Sensing — PT100 RTDs

Two **PT100** resistance temperature detectors are read by two independent **MAX31865** SPI RTD-to-digital converter ICs. Each MAX31865 performs the resistance-to-temperature conversion on-chip and communicates over SPI.

| Signal | Arduino Pin | MCU Pin |
|---|---|---|
| Supply PT100 CS | D2 (SDA) | PD1 |
| Return PT100 CS | D3 (SCL) | PD0 |
| SPI SCK | SCK | PB1 |
| SPI MOSI | MOSI | PB2 |
| SPI MISO | MISO | PB3 |

A software **low-pass filter** (RC ≈ 5 s, sampled at 100 ms) is applied to each reading to suppress noise before the value is used for energy calculations.

### 2.3 Flow Sensing

A pulse-output flow sensor is connected to the hardware interrupt pin. The firmware counts pulses in an ISR with debouncing and converts ticks to litres per hour using a configurable factor (default: **4 ticks/litre**). A timeout detects sensor removal or zero flow.

| Signal | Arduino Pin | MCU Pin |
|---|---|---|
| Flow pulse | D0 (RX) | PD2 |

### 2.4 CAN Bus — MCP2515

A **MCP2515** SPI-to-CAN controller provides a J1939-compatible CAN 2.0B interface (29-bit extended IDs) at 1 Mbit/s.

| Signal | Arduino Pin | MCU Pin |
|---|---|---|
| MCP CS | D4 | PD4 |
| MCP INT | D7 | PE6 |
| SPI (shared with PT100) | SCK / MOSI / MISO | PB1–PB3 |

### 2.5 Node ID Configuration — DIP Switch

Four DIP switch bits select the lower nibble of the J1939 node ID at power-on. The base address is `0x10`, giving a configurable range of `0x10`–`0x1F`.

| Bit | Arduino Pin |
|---|---|
| B0 | D6 |
| B1 | D8 |
| B2 | D9 |
| B3 | D10 |

### 2.6 Status LEDs

| LED | Arduino Pin | Meaning |
|---|---|---|
| Red (RX LED) | `LED_BUILTIN_RX` | Error / fault indicator |
| Green (TX LED) | `LED_BUILTIN_TX` | Heartbeat / normal operation |

---

## 3. Firmware

The firmware is written in C++ for the Arduino / PlatformIO ecosystem and targets the ATmega32U4. It is structured as a set of loosely coupled modules that all run cooperatively under a lightweight task scheduler.

### 3.1 Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                        main loop                            │
│  scheduler::run() dispatches tasks at configured intervals  │
└───────────┬──────────┬──────────┬─────────────┬────────────┘
            │          │          │             │
     ┌──────▼──┐  ┌────▼────┐  ┌──▼───────┐  ┌──▼────────┐
     │  PT100  │  │  Flow   │  │ J1939Mgr │  │  Serial   │
     │ sensor  │  │ sensor  │  │  (CAN)   │  │  CLI/Log  │
     └──────┬──┘  └────┬────┘  └──────────┘  └──────────-┘
            │          │
     ┌──────▼──────────▼──────┐
     │  power.cpp / energy.cpp│  (calculations + EEPROM persistence)
     └────────────────────────┘
```

### 3.2 Key Modules

| Module | Source | Description |
|---|---|---|
| **Scheduler** | `lib/scheduler/` | Cooperative task scheduler; each task specifies its interval in milliseconds |
| **PT100 driver** | `lib/PT100/` | Non-blocking MAX31865 driver with built-in low-pass filter |
| **Flow sensor** | `lib/flow/` | ISR-driven pulse counter with debounce and zero-flow timeout |
| **J1939 Manager** | `lib/J1939/` | Complete J1939 stack: message registry, single-frame TX, BAM Transport Protocol |
| **J1939 DM1** | `lib/J1939/J1939_DM.cpp` | Diagnostics Module 1 — broadcasts active DTCs on the CAN bus |
| **Serial Protocol** | `lib/SerialProtocol/` | ASCII line-oriented CLI (see §5.2) |
| **Diagnostics** | `lib/diagnostics/` | DTC storage, debouncing, SPN/FMI encoding |
| **Power** | `src/power.cpp` | Computes instantaneous thermal power from ΔT and flow |
| **Energy** | `src/energy.cpp` | Integrates power over time; persists totals to EEPROM once per minute |
| **CAN messages** | `src/can_messages.cpp` | Concrete J1939 message objects (Heartbeat, Temperature, Power/Energy) |
| **CLI** | `src/cli.cpp` | Handles host commands (calibration, memory read/write, reboot) |
| **LEDs** | `src/LEDs.cpp` | Maps system state to LED patterns |
| **DIP switch** | `src/dip_switch.cpp` | Reads node-ID DIP switch at startup |

### 3.3 EEPROM Layout

| Region | Offset | Size | Contents |
|---|---|---|---|
| Power | `0x1A–0x1F` | 6 B | Magic word + max observed power |
| Energy | `0x20–0x29` | 10 B | Magic word + cumulative energy + 24 h energy |
| Diagnostics | `0x40+` | variable | DTC ring buffer |

Compile-time `static_assert` guards prevent accidental region overlaps.

### 3.4 CAN Message Summary

All CAN messages use **little-endian (LSB-first)** byte order for multi-byte fields.

| PGN | Period | Payload |
|---|---|---|
| `0xFF10` Heartbeat | 1 s | Uptime counter, firmware version, variant |
| `0xFF11` Raw Temperature | 1 s | Supply & return raw temperatures (×10, °C) |
| `0xFF12` Filtered Temp + Flow | 1 s | Filtered temperatures + flow rate (L/h) |
| `0xFF13` Power & Energy | 1 s | Power (W) + cumulative energy (Wh) + 24 h energy |
| `0xFECA` DM1 | on change | Active diagnostic trouble codes (SPN + FMI) |

### 3.5 Build & Flash

Requirements: [PlatformIO](https://platformio.org/) (VS Code extension or CLI).

```bash
# Build firmware
cd firmware
pio run

# Flash over USB (Caterina bootloader — press reset twice to enter bootloader)
pio run --target upload

# Open serial monitor
pio device monitor --baud 115200
```

The custom board definition (`boards/TFM_100/`) sets `8 MHz`, `5V`, and the correct fuse bytes.

---

## 4. Interface Software — Web UI

The **TFM-100 Web UI** is a single-page application built with **Vue 3** and **Vuetify 3**. It runs entirely in the browser and communicates with the device using the [**WebSerial API**](https://developer.mozilla.org/en-US/docs/Web/API/Web_Serial_API) (no driver, no backend required).

> **Browser support**: Chromium-based browsers (Chrome 89+, Edge 89+). Firefox and Safari do not yet support WebSerial.

### 4.1 Features

| Feature | Description |
|---|---|
| **Live telemetry** | Real-time display of all `P;key;value` parameters (temperatures, flow, power, energy) |
| **Diagnostics panel** | Shows active DTCs with SPN-FMI codes and human-readable descriptions |
| **Log viewer** | Scrolling log with level filter (Error / Warning / Info / Debug) |
| **Serial console** | Manual command entry for `C`, `R`, `W`, `B`, `E` commands |
| **Auto-connect** | Remembers the last serial port and reconnects automatically |

### 4.2 Running the Web UI

Requirements: [Node.js](https://nodejs.org/) 18+.

```bash
cd webui
npm install
npm run dev        # Development server with hot-reload
npm run build      # Production build → dist/
```

### 4.3 UI Architecture

```
webui/src/
├── App.vue                      # Root component, serial lifecycle
├── components/
│   ├── TelemetryCard.vue       # Temperature / flow / power / energy display
│   ├── DiagnosticsCard.vue     # DTC list
│   ├── LogFooter.vue           # Scrolling log
│   └── SerialCard.vue          # Manual command input
└── plugins/webserial/
    ├── service.ts              # WebSerial API wrapper
    ├── composable.ts           # Vue reactive composable
    └── adapter.ts              # Serial protocol framing & parsing
```

---

## 5. Communication Protocols

### 5.1 J1939 CAN Bus

TFM-100 uses the **SAE J1939** protocol over a 1 Mbit/s CAN 2.0B network with 29-bit extended identifiers.

The `J1939Manager` library provides:

- **Message registration** — up to 10 message types with configurable TX periods.
- **Single-frame TX** — payloads ≤ 8 bytes sent directly.
- **BAM Transport Protocol** — larger payloads fragmented and reassembled per SAE J1939-21.
- **DM1 diagnostics** — active DTC broadcast whenever fault state changes.

### 5.2 USB Serial (ASCII CLI)

A line-oriented ASCII protocol is used for configuration, diagnostics, and log streaming over USB CDC-ACM. All frames are semicolon-delimited and terminated with `\r\n`.

#### Host → Device (Commands)

| Command | Format | Description |
|---|---|---|
| Erase diagnostics | `E\r\n` | Clears the DTC ring buffer in EEPROM |
| Set calibration | `C;<id>;<value>\r\n` | Write a calibration coefficient |
| Read memory | `R;<chip>;<addr_hex>;<len_hex>\r\n` | Dump raw memory |
| Write memory | `W;<chip>;<addr_hex>;<data_hex>\r\n` | Write raw bytes |
| Reboot | `B\r\n` | Software reset |

#### Device → Host (Uplink)

| Frame | Format | Description |
|---|---|---|
| Telemetry | `P;<key>;<value>\r\n` | Named parameter (e.g. `P;ST;43.2`) |
| Log | `L;<level>;<message>\r\n` | Level: `E`/`W`/`I`/`D` |
| DTC count | `D;<count>\r\n` | Number of stored DTCs |
| DTC entry | `D;<idx>;<desc>;<SPN>-<FMI>;<status>;<oc>\r\n` | Individual trouble code |
| Memory dump | `V;<chip>;<addr>;<len>;<hex_data>\r\n` | Response to `R` command |
| Calibration | `C;<id>;<type>;<value>\r\n` | Current calibration value |

A full protocol specification is available in [PROTOCOL.md](PROTOCOL.md).

---

## 6. Repository Structure

```
TFM-100/
├── README.md                    # This file
├── PROTOCOL.md                  # Full serial CLI protocol specification
├── firmware/                    # ATmega32U4 firmware (PlatformIO / C++)
│   ├── src/                     # Application source files
│   ├── include/                 # Project-wide headers and config
│   ├── lib/                     # Reusable libraries
│   │   ├── J1939/               # J1939 CAN stack
│   │   ├── SerialProtocol/      # ASCII serial protocol
│   │   ├── PT100/               # MAX31865 RTD driver
│   │   ├── flow/                # Flow sensor driver
│   │   ├── scheduler/           # Cooperative task scheduler
│   │   └── diagnostics/         # DTC management
│   ├── boards/TFM_100/          # Custom PlatformIO board definition
│   └── platformio.ini           # Build configuration
├── webui/                       # Browser-based monitoring UI (Vue 3)
│   ├── src/
│   │   ├── components/          # Vue components
│   │   └── plugins/webserial/   # WebSerial API integration
│   └── package.json
├── hardware/                    # PCB design and manufacturing files
│   ├── schematics.pdf           # Circuit schematic
│   ├── board.png                # PCB layout preview
│   ├── ThermalFlowMeter-BoM.xlsx          # Bill of Materials
│   ├── ThermalFlowMeter_PickAndPlace.xlsx # Pick & place data
│   ├── ThermalFlowMeter-gerber.zip        # Gerber files (ready for fab)
│   └── easyeda_project.epro               # EasyEDA schematic project
└── bootloader/                  # Bootloader flashing guide & hex file
    └── README.md                # avrdude flashing instructions
```

---

## 7. Getting Started

### Prerequisites

| Tool | Purpose |
|---|---|
| [PlatformIO](https://platformio.org/) | Firmware build & upload |
| [Node.js 18+](https://nodejs.org/) | Web UI development |
| USBasp programmer | Initial bootloader programming (if blank chip) |
| Chromium-based browser | Web UI runtime |

### Step 1 — Flash the Bootloader (first time only)

If your ATmega32U4 is blank, follow the instructions in [`bootloader/README.md`](bootloader/README.md) to flash the Caterina USB bootloader using a USBasp programmer.

### Step 2 — Build and Upload Firmware

```bash
cd firmware
pio run --target upload   # Connect USB; double-tap reset to enter bootloader
```

### Step 3 — Run the Web UI

```bash
cd webui
npm install
npm run dev
```

Open `http://localhost:3000` in Chrome or Edge, click **Connect**, and select the TFM-100 serial port.

---

## 8. Hardware Design Files

All hardware design files are located in the [`hardware/`](hardware/) directory.

| File | Description |
|---|---|
| `schematics.pdf` | Full circuit schematic |
| `board.png` | PCB layout preview image |
| `ThermalFlowMeter-BoM.xlsx` | Bill of Materials |
| `ThermalFlowMeter_PickAndPlace.xlsx` | SMT pick & place data |
| `ThermalFlowMeter-gerber.zip` | Gerber + drill files for PCB fabrication |
| `easyeda_project.epro` | Editable EasyEDA schematic project |

---

*For questions, bug reports, or contributions, please open an issue or pull request on this repository.*
