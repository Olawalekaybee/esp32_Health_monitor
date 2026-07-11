# ESP32 Health Monitor

A modular, layer by layer ESP32 project: local sensor health monitoring with offline first storage, a wirelessly linked display board (CYD), optional cloud sync to Google Sheets, OTA updates, and a WebAssembly (WAMR) layer for hot swappable health check logic, all running on FreeRTOS.

This repo doubles as the changelog for a build series. Each numbered folder is one milestone: a working, CI verified state of the project, not a partial commit. If you're following along on [LinkedIn](#) or [YouTube](#), each layer folder is one episode.

**Current status:** Layers 0 through 4 complete. Layers 3 and 6 in the original plan were consolidated; see the note below.

## Why layers, not one big push

Two things this structure is optimized for:

1. **Every layer runs.** You can check out `l1-rtos-core/` on its own and flash it. It's not a broken intermediate state. That's also what makes each one a clean video or post: here's what works now, here's what was added, here's why.
2. **Every layer is CI verified before the next one starts.** Each folder has its own GitHub Actions workflow, scoped to only run when that folder changes (see `.github/workflows/`). A green badge means something specific and checkable, not just an assumption that it works.

## The layers

| # | Layer | What it adds | Status |
|---|---|---|---|
| 0 | [`l0-scaffold`](l0-scaffold/) | PlatformIO project, board target, heartbeat | Complete [![CI](https://github.com/Olawalekaybee/esp32_Health_monitor/actions/workflows/l0-scaffold-ci.yml/badge.svg)](.github/workflows/l0-scaffold-ci.yml) |
| 1 | [`l1-rtos-core`](l1-rtos-core/) | Five task FreeRTOS skeleton over typed queues | Complete [![CI](https://github.com/Olawalekaybee/esp32_Health_monitor/actions/workflows/l1-rtos-core-ci.yml/badge.svg)](.github/workflows/l1-rtos-core-ci.yml) |
| 2 | [`l2-sensors-espnow`](l2-sensors-espnow/) | Real DHT22 and DS18B20 drivers, multi transport link (ESP-NOW, I2C, BLE) to a separate CYD board, unit tested health logic | Complete [![CI](https://github.com/Olawalekaybee/esp32_Health_monitor/actions/workflows/l2-sensors-espnow-ci.yml/badge.svg)](.github/workflows/l2-sensors-espnow-ci.yml) |
| 3 | [`l3-sensors-cloud`](l3-sensors-cloud/) | SD card logging over SDMMC, a real Wi-Fi connect and reconnect state machine, a pluggable cloud backend syncing to Google Sheets with a live chart | Complete [![CI](https://github.com/Olawalekaybee/esp32_Health_monitor/actions/workflows/l3-sensors-cloud-ci.yml/badge.svg)](.github/workflows/l3-sensors-cloud-ci.yml) |
| 4 | [`l4-cyd-display-polish`](l4-cyd-display-polish/) | A system status strip for WiFi, SD, and Cloud on the CYD, an extended wire protocol, real success and failure reporting from the cloud backend, a vertical scrolling status ticker | Complete [![CI](https://github.com/Olawalekaybee/esp32_Health_monitor/actions/workflows/l4-cyd-display-polish-ci.yml/badge.svg)](.github/workflows/l4-cyd-display-polish-ci.yml) |
| 5 | `l5-ota` | Over the air firmware updates | Planned |
| 6 | `l6-wamr` | WAMR, a hot swappable WASM health rule module | Planned |
| 7 | `l7-tinyml-ai` | On device TinyML anomaly detection with optional cloud AI diagnostics | Planned, the full completed project |

Layer 7 is the finished product: everything above it, working together.

**A note on the renumbering.** The plan originally had ten layers, numbered 0 through 9, with SD storage, Wi-Fi, and Google Sheets as three separate layers (3, 4, and 5), and CYD display polish as Layer 6. In practice, layers 3 through 5 landed together in one folder (`l3-sensors-cloud`) across a few days of the same build session, and what was Layer 6 became `l4-cyd-display-polish`, renumbered down rather than left with a gap. The remaining planned layers, OTA, WAMR, and TinyML, shifted down to keep the sequence contiguous: 7 became 5, 8 became 6, and 9 became 7.

## Two physical boards

The CYD (Cheap Yellow Display) is its own ESP32 with its own screen, not a display wired to the sensor board. From Layer 2 onward, this project runs two firmwares that talk wirelessly, using ESP-NOW by default, with I2C and Bluetooth also proven as alternate transports. That's why some layer folders contain two PlatformIO projects, for example `l4-cyd-display-polish/main-node` and `l4-cyd-display-polish/cyd-display-node`.

## What CI verified means here, honestly

Every layer's CI proves the firmware compiles against its real dependencies. From Layer 2 onward, CI also runs unit tests against pure logic, such as the sensor health rules, with no hardware required. Neither of those proves a physical sensor is wired correctly, that a wireless link reaches real hardware, that an SD card actually mounts, that Wi-Fi and cloud sync reach a live network, or that a display renders correctly. Those still need a person with a soldering iron, a real network, and eyes on the screen. Each layer folder's README states exactly what its CI does and does not cover. `l2-sensors-espnow/docs/TESTING.md` has the fullest version of that breakdown.

## Following along

Each layer folder has its own README with build and flash instructions specific to that stage. Start at [`l0-scaffold`](l0-scaffold/) if you want to build this up from nothing alongside the series, or jump to the most recently completed layer, [`l4-cyd-display-polish`](l4-cyd-display-polish/), if you want the most capable working version so far.

## License

MIT. See [LICENSE](LICENSE).