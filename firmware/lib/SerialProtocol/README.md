# SerialProtocol (minimal)

Lightweight line-based text protocol for MCU ↔️ Web (Web Serial) communications.

Features

- Line-based, tokens separated with `;`, terminated by `\n` (accepts `\r\n`).
- No dynamic allocations, no printf/snprintf, no std::string.
- Transport-agnostic: write output via weak `proto_write_bytes` hook.
- Minimal parser for downlink commands: C,R,W,B and send helpers P,L,D,V for uplink.

Building

- Compatible with PlatformIO and Arduino/ATmega32U4 (C++11).
- Place the library under `lib/SerialProtocol` (already done in this repo).

Usage

- Instantiate `SerialProtocol` in your main code.
- Override `proto_write_bytes` to route outgoing bytes to Serial or other transport.
- Call `feed()` with incoming bytes as they arrive (e.g., from `Serial.read()` loop).
- Register callbacks using `set_on_C`, `set_on_R`, `set_on_W`, `set_on_B`.

Notes

- This implementation is intentionally minimal. It is safe for use on small MCUs.
- Future extension: msgid and CRC can be appended without changing public API.
