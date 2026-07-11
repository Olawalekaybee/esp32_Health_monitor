---
noteId: "a42fa761756111f1818919cf48c92312"
tags: []

---

# Architecture

This project is built in strict layers. Each layer is a working, bootable
state of the device — never a half-finished mess. That's what makes it
safe to break into separate modules/repos later.

## Layer 0 — Scaffold
PlatformIO project, `platformio.ini`, folder layout. No logic.

## Layer 1 — RTOS core (this commit)
Five FreeRTOS tasks, each owning exactly one domain:

| Task | Core | Owns |
|---|---|---|
| `sensorTask` | 1 | The sensor pin(s) |
| `healthTask` | 1 | Health verdicts |
| `storageTask` | 1 | The SD card |
| `networkTask` | 0 | Wi-Fi + cloud sync |
| `displayTask` | 0 | The screen |

Rule: **no task touches another task's hardware.** All communication goes
through `shared_types.h` structs over FreeRTOS queues. This is what lets
Layer 2–9 replace a task's internals without touching the others.

Sensing/health run on core 1 (timing-sensitive). Network/display run on
core 0 (can block on Wi-Fi/HTTP without stalling sensing).

## Layer 2 — Sensor/HAL + comms to CYD (this commit)
Two real sensors, read independently in `sensor_task.cpp`:
- **DHT22** — temperature + humidity, single digital pin
- **DS18B20** — temperature, OneWire bus (supports multiple sensors later)

Each has its own `read_ok` flag through the whole pipeline — a dead DHT22
never hides a working DS18B20, or vice versa. `health_task.cpp` reasons
about both independently for the same reason — though as of this repo,
that reasoning itself has moved into `main-node/lib/health_rules/`, a
pure C++ header with no Arduino dependency, specifically so it can be
unit-tested on CI without hardware. See [`docs/TESTING.md`](TESTING.md)
for what's actually verified automatically vs. what still needs a human
with real hardware.

**Important structural note:** the CYD is a *separate physical ESP32
board* with its own screen — not a display wired to main-node. So this
layer also introduces `comms_task.cpp`, which owns an **ESP-NOW** link
to the CYD. ESP-NOW is peer-to-peer, works with no router or internet,
and fits the offline-first goal exactly. The repo now has two firmware
targets:

```
main-node/          ← sensors, health logic, storage, cloud sync, ESP-NOW sender
cyd-display-node/    ← separate ESP32 board, ESP-NOW receiver, screen
```

Both sides share the exact same `CommsPacket` struct layout (duplicated
by necessity — ESP-NOW has no schema negotiation). If you change one,
change the other and rebuild+reupload both.

**Pairing the two boards:**
1. Flash `cyd-display-node` first, open its serial monitor — it prints
   its own MAC address on boot.
2. Either leave `main-node`'s `ESPNOW_PEER_MAC` as broadcast (works
   immediately, simplest for bring-up), or set `ESPNOW_USE_BROADCAST`
   to `false` and paste the CYD's MAC into `config.h`.
3. Flash `main-node`. Within ~1s the CYD's screen should start showing
   live sensor + health data.

## Layer 3 — Storage
Mount SD (SPI), write CSV rows, implement ring-buffer file rotation so
offline operation never runs out of space or loses the newest data.

## Layer 4 — Connectivity
Wi-Fi connect/reconnect state machine in `network_task.cpp` (main-node).
Runs independently of the ESP-NOW link — the CYD keeps showing live data
even if internet Wi-Fi is down, since ESP-NOW doesn't need it.

## Layer 5 — Cloud sync
POST batched samples to a Google Apps Script Web App endpoint, which
appends rows to a Google Sheet. Sheet's native charts handle graphing.

## Layer 6 — Display polish (CYD)
This commit's `cyd-display-node/src/main.cpp` is intentionally plain —
raw TFT_eSPI text, no layout system. Layer 6 replaces it with LVGL
widgets: gauges, a status banner, maybe a scrolling mini-graph.

## Layer 7 — OTA
ArduinoOTA (or ESP-IDF OTA API) added to `network_task.cpp`, gated
behind Wi-Fi connectivity from Layer 4.

## Layer 8 — WAMR (WebAssembly Micro Runtime)
Health rules currently hardcoded in `health_task.cpp` move into a
`.wasm` module that WAMR executes. This lets you push a *new health
policy* — new thresholds, new rules — as a small WASM binary over OTA,
without recompiling or reflashing the whole firmware.

## Layer 9 — Health brain ("eps-claw")
- TinyML model (Edge Impulse / TFLite Micro) for on-device drift/anomaly
  scoring — genuinely local, no network needed.
- Optional: when online, batch-send recent anomalies to a cloud AI
  endpoint for deeper diagnosis, verdict written back to the Sheet.

---

## Data contracts (never break these across layers)

- `SensorSample` — sensor_task → storage/network/display/health/comms (main-node only)
- `HealthReport` — health_task → display/network/comms (main-node only)
- `SystemStatus` — aggregated system state → display (main-node only)
- `CommsPacket` — comms_task (main-node) → ESP-NOW → cyd-display-node
  (this one crosses the wire, so it's duplicated in both firmwares —
  see Layer 2 note above)
