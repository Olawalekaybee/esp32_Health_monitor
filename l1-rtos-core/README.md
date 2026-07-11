---
noteId: "a41568a1756111f1818919cf48c92312"
tags: []

---

# Layer 1 — RTOS Core

**Series:** [ESP32 Health Monitor](../README.md) · Episode 1
**Builds on:** [Layer 0 — Scaffold](../l0-scaffold/README.md)

![CI](https://github.com/Olawalekaybee/esp32_Health_monitor/actions/workflows/l1-rtos-core-ci.yml/badge.svg)

Five FreeRTOS tasks (sensor, storage, network, display, health), each
owning one responsibility, communicating only through typed queues
(`SensorSample`, `HealthReport`). Sensor and health run on core 1;
network and display run on core 0.

At this layer the sensor read is a placeholder (`analogRead`) and
storage/network/display are Serial-print stubs — the goal is proving
the task/queue architecture works before any real hardware gets wired
in during Layer 2.

## What CI verifies
✅ Firmware compiles cleanly for `esp32dev` with all 5 tasks wired up.
No unit tests yet — the logic here (queue plumbing) doesn't have
meaningful behavior to test in isolation until Layer 2 adds real
sensor-failure rules.

## Build & flash
```bash
cd l1-rtos-core
pio run -t upload
pio device monitor
```
Expect all 5 tasks reporting over serial once a second.

## Next episode
[Layer 2 — Sensors + ESP-NOW](../l2-sensors-espnow/README.md) — real
DHT22 + DS18B20 drivers and a wireless link to a separate CYD display
board.
