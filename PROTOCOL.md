# TFM-100 Serial / CLI Protocol

## Scope

This document describes the simple, line-oriented serial protocol implemented by the firmware's `SerialProtocol` (see `firmware/lib/SerialProtocol/SerialProtocol.*`). It's used for diagnostics, configuration (calibration), and binary memory read/write operations. The protocol is intentionally minimal and human-readable for easy debugging over serial terminals and bridges.

## Framing

- Messages are ASCII, terminated by LF ("\n") and CR ("\r"), even if, it is optional. Each message is a semicolon-separated list of tokens.
- No binary framing, no escaping. Maximum line size is limited by the firmware buffer (~128 bytes).

## Command directions

- Uplink (host -> device): single-letter commands the firmware parses when a line arrives.
- Downlink (device -> host): responses and notifications emitted by the firmware using the same semicolon-separated format.

## Uplink (host -> device) commands

- `E`
  - Erase diagnostics memory. No parameters.
  - Handler: triggers the erase diagnostics callback (set via `set_erase_diagnostics_callback`).
  - Answer: device will respond with:
    - `E;OK\r\n` on success
    - `E;ERR;error_code\r\n` on failure

- `C;id;value`
  - Calibration command: it is used to change calibration in the firmware.
    - `id` is the identifier of the calibration. Only calibration with identifier can be modified.
    - `value` is an arbitrary string payload interpreted by the firmware.
  - Example: `C;1;5.2\r\n`
  - Handler: triggers the calibration callback (set via `set_calibration_callback`).
  - Answer: device will response as:
    - `C;OK\r\n` to indicate success
    - `C;ERR;error_code\r\n` to indicate failure
- `R;chip_id;addrhex;lenhex`
  - Read address on companion chips:
    - `chip_id` is decimal chip id
    - `addrhex` is an address in hex (big-endian, no 0x)
    - `lenhex` is a decimal length.
  - Example: `R;0;1F2A;16\r\n` (read 16 bytes from address 0x1F2A of chip 0)
  - Handler: triggers the read callback (set via `set_read_callback`).
  - Answer: device will respond with:
    - `V;chip;addrhex;lenhex;datahex\r\n` on success
    - `V;ERR;error_code\r\n` on failure

- `W;chip_id;addrhex;datahex`
  - Write address on companion chips:
    - `chip_id` is decimal chip id
    - `addrhex` is an address in hex (big-endian, no 0x)
    - `datahex` is even-length hex-encoded bytes. Max data length limited by firmware (48 bytes).
  - Example: `W;0;1F2A;0A1B2C3D\r\n` (write bytes 0x0A,0x1B,0x2C,0x3D to address 0x1F2A)
  - Handler: triggers the write callback (set via `set_write_callback`).
  - Answer: device will respond with:
    - `W;OK\r\n` on success
    - `W;ERR;error_code\r\n` on failure

- `B`
  - Reboot. No parameters.
  - Handler: triggers the reboot callback (set via `set_reboot_callback`).
  - Answer: device will respond with `B;OK\r\n` and then reboot.

## Downlink (device -> host) messages

- `P;key;value`
  - Parameter report:
    - `key` is a short ASCII key
    - `value` is a decimal or string. Used for telemetry/parameters.
  - Example: `P;ST;43.2\r\n` (parameter "ST" with value 43.2 - Supply Temperature)

- `L;level;message`
  - Log line:
    - `level` is one of `E` (error), `W` (warning), `I` (info), `D` (debug).
  - Example: `L;I;Initialization complete\r\n`

- `D;count`
  - Diagnostics count: it indicates how many diagnostic trouble codes (DTCs) are currently stored in the device.

- `D;index;Description;SPN-FMI;status;OC`
  - Diagnostics entry. Each line provides details about a specific DTC:
    - `index`: zero-based index of the DTC.
    - `Description`: human-readable description of the DTC.
    - `SPN-FMI`: SPN and FMI code in hex format `SPN-FMI` (e.g., `12F4-A`).
    - `status`: indicates the status of DTC (`PENDING`, `ACTIVE`, `HEALING`, `HISTORY`)
    - `OC`: occurrence count (decimal).
  - Example: `D;3\r\n` then several `D;...` lines.

- `V;chip;addrhex;lenhex;datahex`
  - Hex dump of `lenhex` bytes at `addrhex` for `chip`. `lenhex` is exactly two hex digits; `datahex` is twice that many hex chars.
  - It is the response to a read command.
  - Example: `V;0;1F2A;04;0A1B2C3D\r\n`

- `C;id;type;value`
  - Calibration status from device:
    - `id` is the calibration identifier
    - `type` is one of `I`,`F`,`S` (int, float, string)
    - `value` is the current calibration value
  - Example: `C;1;I;0\r\n`

## Error handling and limits

- The parser silently ignores malformed or unknown commands.
- Maximum inbound line length: ~128 bytes. Overflow resets the inbound buffer.
- Max binary write length: 48 bytes (enforced by `DATA_MAX_BYTES`).

## Implementation notes for integrators

- The firmware `DiagComm` wrapper calls `comm.begin()` to install a Serial writer and exposes higher-level helpers (send_param, send_log, send_diag_info, send_v_dump).
- When adding new downlink message types or changing existing formats, update both the firmware serializer and any web UI parsers that consume these lines.
- The wire format is intentionally human-readable. For automation, prefer using the explicit hex fields (for binary blobs) and PGN/field formats for CAN messages.

Examples

- Host requests 16 bytes starting at 0x100 on chip 0:

```text
R;0;0100;16
```

- Device responds with a dump (example):

```text
V;0;0100;10;11223344556677889900AABBCCDDEEFF
```

- Host sets calibration id 2 to value 1.5:

```text
C;2;F;1.5
```

- Device logs an info:

```text
L;I;Flow started
```

Where to find code

- Parser and emitter: `firmware/lib/SerialProtocol/SerialProtocol.h` and `.cpp`.
- Integration wrapper and usage: `firmware/include/diag_comm.h` and `firmware/src/main.cpp`.

License and authorship

- See repository root `README.md` for project authorship. This protocol document is a derivative description of the in-repo implementation.
