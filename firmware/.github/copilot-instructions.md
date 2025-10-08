# TFM-100 Firmware Development Guide

## Project Overview

This is an embedded C++ firmware for the TFM-100 board, an ATmega32U4-based industrial sensor node that monitors temperature (PT100 sensors) and flow sensors, communicating via J1939 CAN bus protocol. The system implements industrial-grade diagnostics and real-time scheduling for automotive/industrial applications.

## Architecture & Core Components

### Hardware Architecture

- **MCU**: ATmega32U4 @ 8MHz (custom TFM_100 board variant)
- **Communication**: MCP2515 CAN controller for J1939 protocol
- **Sensors**: MAX31865 (PT100 temperature sensors), flow sensor with interrupt-based counting
- **Pin configuration**: Defined in `include/config.h` - supply/return CS pins, MCP CS/INT pins, DIP switch pins

### Software Architecture

The system follows a **task-based scheduler pattern** with **state machines** for sensor management:

1. **Scheduler** (`lib/scheduler/`): Custom cooperative scheduler managing periodic tasks
2. **Sensor Libraries** (`lib/PT100/`, `lib/flow/`): Non-blocking sensor interfaces with state machines
3. **J1939 Communication** (`lib/J1939/`): CAN message handling with adapter pattern for MCP_CAN
4. **Diagnostics System** (`lib/diagnostics/`): Industrial DTC (Diagnostic Trouble Code) management
5. **CLI Interface** (`include/cli.h`): Serial-based command line interface for debugging

## Critical Development Patterns

### J1939 CAN Communication Pattern

All CAN messages inherit from `J1939` base class and use specific PGN (Parameter Group Number) assignments:

```cpp
// Always use the adapter pattern for CAN drivers
McpCanAdapter canAdapter(CAN0);
J1939Manager j1939(canAdapter, circularBuffer, sa);

// Message classes follow this pattern:
class J1939_HeartBeat : public J1939 {
    void begin(uint8_t src);  // Initialize with source address
    void getData(uint8_t *data, uint8_t *length);  // Pack data for transmission
};
```

### Scheduler Task Registration

Use lambda functions for inline tasks or function pointers for complex operations:

```cpp
// Periodic sensor reading (common pattern)
scheduler.addTask([](uint32_t td) {
    if (!supply_sensor.triggerMeasurement())
        cli.logError("Unable to read SUPPLY");
}, PT100_SAMPLE_RATE);
```

### Diagnostics & DTC Management

DTCs are defined in `include/dtc.h` with PROGMEM strings and structured as SPN (Suspect Parameter Number) + FMI (Failure Mode Identifier):

```cpp
// Always use setRaw() to update fault status, then call periodic_update()
DSM.setRaw(DTC_SupplyLineRefInHigh, supply_sensor.last_fault & MAX31865::FAULT_HIGHTHRESH_BIT);
DSM.periodic_update(); // Handles debouncing and state transitions
```

### Memory Management

- Use `PROGMEM` for constant strings (flash storage): `const char string_name[] PROGMEM = "text";`
- Prefer fixed-size circular buffers over dynamic allocation
- Template-based containers with compile-time size: `Queue<128> buffer;`

## Build & Development Workflow

### PlatformIO Commands

```bash
# Build for TFM_100 target
pio run

# Upload to device (COM5 configured in platformio.ini)
pio run --target upload

# Run tests
pio test

# Monitor serial output
# DO NOT USE serial since CLI is based on VT100 terminal which is not supported by standard serial monitor
```

### Custom Board Configuration

The project uses a custom board definition (`boards/TFM_100.json`) based on ATmega32U4 with:

- 8MHz clock frequency
- Custom pin variant in `boards/variants/TFM_100/pins_arduino.h`
- USB VID/PID for "TFM-100" product identification

### Testing Strategy

Unit tests are in `test/` directory following PlatformIO Unity framework:

- `test_circular_queue.cpp`: Template container testing
- `test_diagnostics.cpp`: DTC state machine validation

## Key Integration Points

### Sensor State Machine Pattern

All sensors implement non-blocking state machines with `process()` methods called in main loop:

```cpp
void loop() {
    scheduler.run();        // Execute scheduled tasks
    supply_sensor.process(); // State machine progression
    return_sensor.process();
    flowObj.process();
    cli.process();          // Handle CLI commands
}
```

### Error Handling & Watchdog

- Hardware initialization failures trigger immediate reboot via watchdog timer
- Use `cli.logError()` for runtime diagnostics rather than Serial.print()
- Node status transitions: `SETUP` → `RUN` → `SLEEP` (when no flow detected)

### Configuration Management

Critical constants in `include/config.h`:

- `PT100_FILTER_RC`: Low-pass filter time constant (5000ms)
- `PT100_SAMPLE_RATE`: Sensor sampling interval (100ms)
- `NODE_ID_BASE`: J1939 source address base (0x10 + DIP switch value)

## Common Gotchas

1. **SPI Pin Conflicts**: PT100 sensors and MCP2515 share SPI bus - ensure proper CS pin management
2. **J1939 Extended Frames**: Always use 29-bit CAN IDs for J1939 compliance
3. **Interrupt Safety**: Flow sensor uses interrupts - ensure atomic operations for shared variables
4. **Memory Constraints**: ATmega32U4 has only 2.5KB RAM - monitor stack usage carefully
5. **PROGMEM Access**: Use `pgm_read_*()` functions to access PROGMEM data correctly

## Dependencies

Key external libraries (managed in `platformio.ini`):

- `coryjfowler/mcp_can@^1.5.1`: MCP2515 CAN controller driver
- `robtillaart/ANSI`: Terminal color support for CLI
- `MAX31865_NonBlocking`: Custom fork for non-blocking PT100 operations

When adding new features, maintain the non-blocking, scheduler-driven architecture and follow the established sensor state machine patterns.

## Doxygen commenting rules (English)

When adding or updating code, document files, classes, functions and public APIs using Doxygen-style comments in English. Follow these concise project rules so generated documentation and AI agents remain consistent:

- File header: Every new source (`.cpp`) or header (`.h`) file must start with a brief file-level comment block containing:

  - @file filename (use the actual file name)
  - Short one-line summary (what the file implements)
  - Author tag `@author` and year
  - Optional `@note` for important platform-specific notes (e.g. PROGMEM usage, memory constraints)

  Example:
  /\*\*

  - @file J1939Manager.cpp
  - @brief J1939 transport layer manager: queueing and TP/BAM handling
  - @author Salvo Musumeci
  - @note Uses fixed-size pool and must respect memory constraints of ATmega32U4
    \*/

- Function and method docs: For every non-trivial function or public method include:

  - Short one-line description
  - `@param` tags for each parameter (name and concise meaning)
  - `@return` with returned value meaning (if any)
  - `@remarks` for important behavior such as reentrancy, blocking, or locking requirements

  Example:
  /\*\*

  - @brief Enqueue a J1939 descriptor for later transmission
  - @param desc Descriptor describing PGN, length and source buffer
  - @return true if descriptor successfully enqueued, false on queue full
  - @remarks This is non-blocking and will fail if the message queue or pool is full
    \*/

- Inline and tricky behavior: Use `@note` or `@warning` to highlight:

  - Interrupt-safety issues (e.g., flow sensor counters)
  - Shared resource locking (e.g., `ILockableBuffer` usage in transport layer)
  - Memory and PROGMEM access patterns

- Examples and code snippets: Provide short usage snippets when helpful, marked as `@code` / `@endcode`.

- Style rules:

  - Use English for all Doxygen comments.
  - Keep short descriptions to one sentence; move longer explanations to `@remarks`.
  - Use present tense and active voice ("Enqueues a descriptor" not "Will enqueue").
  - Keep comment width to ~100 columns when possible.

- Automation hints for AI agents:
  - When generating code changes, also generate matching Doxygen comments for any new public APIs.
  - Prefer referencing concrete files from the repo in `@see` tags (e.g. `@see lib/J1939/J1939Manager.cpp`).

Add this section to the end of the existing instructions to ensure all contributors (including AI agents) document code consistently.
