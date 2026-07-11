# Layer 2 — Sensors + ESP-NOW

**Series:** [ESP32 Health Monitor](../README.md) · Episode 2
**Builds on:** [Layer 1 — RTOS Core](../l1-rtos-core/README.md)

![CI](https://github.com/Olawalekaybee/esp32_Health_monitor/actions/workflows/l2-sensors-espnow-ci.yml/badge.svg)

Real DHT22 + DS18B20 sensor drivers, tracked independently, feeding a
FreeRTOS task pipeline that now also pushes live status to a
physically separate CYD display board over ESP-NOW (peer-to-peer, no
router or internet needed).

## Two firmwares, one folder
```
main-node/           Sensors, health logic, storage, cloud sync, ESP-NOW sender
cyd-display-node/     Separate ESP32 board — ESP-NOW receiver, screen
```

## What CI verifies
Three jobs, path-scoped so they only run when this layer changes:
1. **Unit tests** — `main-node/lib/health_rules` (the actual sensor
   health decision logic) tested natively, no hardware, 8 test cases.
2. **Build main-node** — compiles against real DHT/OneWire/Dallas
   libraries for `esp32dev`.
3. **Build cyd-display-node** — compiles against TFT_eSPI for the CYD.

**Important:** green CI means the logic is correct and both firmwares
compile — it does **not** mean a physical sensor is wired right or that
ESP-NOW reaches a real board. See [`docs/TESTING.md`](docs/TESTING.md)
for the honest breakdown.

## Pairing the two boards
1. Flash `cyd-display-node` first, read its MAC from serial on boot.
2. `main-node/include/config.h` defaults to ESP-NOW broadcast (works
   immediately, no config needed). For a dedicated link, set
   `ESPNOW_USE_BROADCAST` to `false` and paste the CYD's MAC into
   `ESPNOW_PEER_MAC`.
3. Flash `main-node`. The CYD's screen should show live data within a
   couple seconds.

```bash
# Unit tests, no hardware needed
cd l2-sensors-espnow/main-node && pio test -e native

# Flash to real hardware, in order
cd l2-sensors-espnow/cyd-display-node && pio run -t upload && pio device monitor
cd ../main-node && pio run -t upload && pio device monitor
```

### Wiring (main-node)
| Component | ESP32 Pin |
|---|---|
| DHT22 data | GPIO 4 |
| DS18B20 data | GPIO 15 (needs a 4.7kΩ pull-up to 3.3V) |
| Status LED | GPIO 2 |

See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the full layer
plan and data-contract details.

## Next episode
Layer 3 — SD card storage + ring buffer (not yet built).
