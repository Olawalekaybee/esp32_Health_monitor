---
noteId: "a2445220756111f1818919cf48c92312"
tags: []

---

# Layer 0 — Scaffold

**Series:** [ESP32 Health Monitor](../README.md) · Episode 0

![CI](https://github.com/Olawalekaybee/esp32_Health_monitor/actions/workflows/l0-scaffold-ci.yml/badge.svg)

Minimal PlatformIO project: proves the ESP32 toolchain, board target,
and upload path work, with a heartbeat LED and a serial print. No
application logic yet — that starts in Layer 1.

## What CI verifies
✅ Firmware compiles cleanly for `esp32dev`.
That's the entire contract at this layer — there's no logic yet worth
unit-testing.

## Build & flash
```bash
cd l0-scaffold
pio run -t upload
pio device monitor
```
Expect `=== Layer 0 scaffold boot OK ===` followed by a heartbeat print
once a second.

## Next episode
[Layer 1 — RTOS core](../l1-rtos-core/README.md) — the FreeRTOS task
skeleton (sensor, storage, network, display, health tasks talking over
queues).
