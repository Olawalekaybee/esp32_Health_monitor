# ESP32 Health Monitor

A modular, layer-by-layer ESP32 project, built and documented in
public: local sensor health monitoring with offline-first storage, a
wirelessly-linked display board (CYD), optional cloud sync to Google
Sheets, OTA updates, and a WebAssembly (WAMR) layer for hot-swappable
health-check logic: all on FreeRTOS.

This repo doubles as the changelog for a build series. Each numbered
folder is one milestone: a real, working, CI-verified state of the
project, not a partial commit. If you're following along on
[LinkedIn](#) or [YouTube](#), each layer folder is one episode.

**Current status:** 🚧 Layers 0–3 complete (Layer 3 folded the originally-planned Layers 3–5 into one milestone — see note below).

## Why layers, not one big push

Two things this structure is optimized for:

1. **Every layer runs.** You can check out `l1-rtos-core/` on its own
   and flash it — it's not a broken intermediate state. That's also
   what makes each one a clean video/post: "here's what works now,
   here's what I added, here's why."
2. **Every layer is CI-verified before the next one starts.** Each
   folder has its own GitHub Actions workflow, scoped to only run when
   that folder changes (see `.github/workflows/`). A green badge means
   something specific and checkable, not just "I think it works."

## The layers

| # | Layer | What it adds | Status |
|---|---|---|---|
| 0 | [`l0-scaffold`](l0-scaffold/) | PlatformIO project, board target, heartbeat | ✅ [![CI](https://github.com/Olawalekaybee/esp32_Health_monitor/actions/workflows/l0-scaffold-ci.yml/badge.svg)](.github/workflows/l0-scaffold-ci.yml) |
| 1 | [`l1-rtos-core`](l1-rtos-core/) | 5-task FreeRTOS skeleton over typed queues | ✅ [![CI](https://github.com/Olawalekaybee/esp32_Health_monitor/actions/workflows/l1-rtos-core-ci.yml/badge.svg)](.github/workflows/l1-rtos-core-ci.yml) |
| 2 | [`l2-sensors-espnow`](l2-sensors-espnow/) | Real DHT22 + DS18B20 drivers, multi-transport link (ESP-NOW/I2C/BLE) to a separate CYD board, unit-tested health logic | ✅ [![CI](https://github.com/Olawalekaybee/esp32_Health_monitor/actions/workflows/l2-sensors-espnow-ci.yml/badge.svg)](.github/workflows/l2-sensors-espnow-ci.yml) |
| 3 | [`l3-sensors-cloud`](l3-sensors-cloud/) | SD card logging (SDMMC), real Wi-Fi connect/reconnect, pluggable cloud backend syncing to Google Sheets with a live chart | ✅ [![CI](https://github.com/Olawalekaybee/esp32_Health_monitor/actions/workflows/l3-sensors-cloud-ci.yml/badge.svg)](.github/workflows/l3-sensors-cloud-ci.yml) |
| 6 | `l6-cyd-display-polish` | LVGL UI on the CYD (gauges, status banner) | ⏳ planned |
| 7 | `l7-ota` | Over-the-air firmware updates | ⏳ planned |
| 8 | `l8-wamr` | WAMR — hot-swappable WASM health-rule modules | ⏳ planned |
| 9 | `l9-tinyml-ai` | On-device TinyML anomaly detection + optional cloud AI diagnostics | ⏳ planned — **full completed project** |

Layer 9 is the finished product: everything above it, working together.

**Note on the gap between 3 and 6:** the plan originally split SD
storage, Wi-Fi, and Google Sheets sync into three separate layers
(3/4/5). In practice they landed together in one folder,
`l3-sensors-cloud`, across a few days of the same build session — so
that's documented here as one combined Layer 3 rather than pretending
three folders exist that don't. The remaining layer numbers (6–9) are
left as originally planned; whether they get renumbered down to 4–7
once they're actually built is an open call, not decided yet.

## Two physical boards

The CYD (Cheap Yellow Display) is its own ESP32 with its own screen —
not a display wired to the sensor board. From Layer 2 onward this
project runs **two firmwares** that talk wirelessly (ESP-NOW by
default, with I2C and Bluetooth also proven as alternate transports),
which is why some layer folders contain two PlatformIO projects (e.g.
`l3-sensors-cloud/main-node` and `l3-sensors-cloud/cyd-display-node`).

## What "CI-verified" means here, honestly

Every layer's CI proves the firmware **compiles** against its real
dependencies. From Layer 2 onward, CI also runs real **unit tests**
against pure logic (e.g. sensor health rules) with no hardware
required. Neither of those proves a physical sensor is wired correctly,
that a wireless link reaches real hardware, that an SD card actually
mounts, or that Wi-Fi/cloud sync reaches a live network — that still
needs a human with a soldering iron and a real network. Each layer
folder's README says exactly what its CI does and doesn't cover;
`l2-sensors-espnow/docs/TESTING.md` has the fullest version of that
breakdown.

## Following along

Each layer folder has its own README with build/flash instructions
specific to that stage. Start at [`l0-scaffold`](l0-scaffold/) if you
want to build this up from nothing alongside the series, or jump to
the highest-numbered complete layer ([`l3-sensors-cloud`](l3-sensors-cloud/))
if you just want the most capable working version so far.

## License

MIT — see [LICENSE](LICENSE).