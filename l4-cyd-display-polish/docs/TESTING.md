---
noteId: "a42fa762756111f1818919cf48c92312"
tags: []

---

# Testing strategy

This project draws a hard line between two kinds of "working":

## 1. Logic correctness — verified by CI, every push
Anything that's pure decision-making (no hardware, no I/O) lives in
`main-node/lib/*` and is unit-tested with PlatformIO's `native`
platform, which compiles and runs on the CI runner directly — no ESP32
required. Right now that's `health_rules.h`: given failure counts, does
it return the right status and a non-empty reason string? 8 test cases,
run automatically.

As later layers add more pure logic (e.g. a ring-buffer rotation
policy in Layer 3, an ESP-NOW packet framing check in this layer),
that logic should also move into `lib/` and get the same treatment.

## 2. Firmware correctness — verified by CI as "compiles", not "works"
Both `main-node` and `cyd-display-node` build against their real
dependencies (DHT sensor library, OneWire, DallasTemperature, TFT_eSPI)
on every push. A broken include, a typo'd API call, a missing library
version — CI catches these before they reach hardware.

**What a green build does NOT mean:** it does not mean a DHT22 is
correctly wired to GPIO 4, that a DS18B20's pull-up resistor is in
place, that ESP-NOW packets actually reach a physical CYD board, or
that the screen renders legibly. None of that is observable from a
GitHub Actions runner with no radios or sensors attached.

## Closing the gap: hardware-in-the-loop (future option)
If/when this matters enough to automate, the standard approach is a
self-hosted GitHub Actions runner physically wired to real main-node
and cyd-display-node boards, which flashes both, waits a few seconds,
and asserts on serial output (e.g. "CYD received packet" appears
within N seconds of main-node booting). That's a meaningfully bigger
lift than the native unit tests above and isn't set up yet — flagging
it here so "CI is green" is never mistaken for "hardware is verified."

## Running tests locally
```bash
cd main-node
pio test -e native          # unit tests, no hardware
pio run -e esp32dev          # compile check, no hardware
pio run -e esp32dev -t upload && pio device monitor   # actual hardware
```
