# Layer 3: SD Storage + Wi-Fi + Cloud Sync

**Series:** [ESP32 Health Monitor](../README.md) · Episode 3
**Builds on:** [Layer 2: Sensors + Multi-Transport Comms](../l2-sensors-espnow/README.md)

![CI](https://github.com/Olawalekaybee/esp32_Health_monitor/actions/workflows/l3-sensors-cloud-ci.yml/badge.svg)

This layer folds together what were originally planned as three
separate layers (SD storage, Wi-Fi, Google Sheets sync) : they landed
together in the same build session, so this folder documents that as
one combined milestone rather than pretending three folders exist that
don't.

What it adds on top of Layer 2:
- **SD card logging** via the board's built-in SDMMC slot (1-bit mode
  : only CLK/CMD/D0 are wired on this board), with an automatic
  fallback to Serial-only logging if no card is present or it fails to
  mount. Sensing, health checks, and the display link all keep running
  either way.
- **Real Wi-Fi connect/reconnect state machine** : non-blocking, so a
  dropped or absent network never stalls sensing/storage/comms.
- **Pluggable cloud backend** syncing readings to a Google Sheet via a
  Google Apps Script Web App, with the sheet formatted (header row,
  color-coded status, number formats) and backed by a rolling line
  chart. The backend is deliberately swappable , Firebase and AWS
  stubs exist alongside the real Google Sheets implementation, sharing
  the same two function names (`cloud_sync_init`/`cloud_sync_send`) so
  swapping backends never touches `network_task.cpp`.

## Two firmwares, one folder
```
main-node/           Sensors, health logic, SD storage, Wi-Fi, cloud sync, comms sender
cyd-display-node/    Separate ESP32 board , comms receiver, screen
```

## What CI verifies
Three jobs, path-scoped so they only run when this layer changes:
1. **Unit tests** : `main-node/lib/health_rules` (the actual sensor
   health decision logic) tested natively, no hardware.
2. **Build main-node** : compiles against real
   DHT/OneWire/Dallas/SD_MMC/WiFi/HTTPClient dependencies for
   `esp32-s3`, using a placeholder `wifi_secrets.h` (never real
   credentials) so the build never depends on anything private.
3. **Build cyd-display-node** : compiles against TFT_eSPI/LVGL for the
   CYD.

**Important:** green CI means the logic is correct and both firmwares
compile , it does **not** mean a physical SD card actually mounts, that
Wi-Fi reaches a real access point, or that the Google Apps Script
endpoint actually appends a row to a live Sheet. Those still need a
human with the physical board and a real network. See
[`docs/TESTING.md`](docs/TESTING.md) for the fuller breakdown (written
for Layer 2 , update it if Layer 3's behavior diverges further).

## Setup

1. Copy `main-node/include/wifi_secrets.h.example` to
   `main-node/include/wifi_secrets.h` and fill in your real Wi-Fi
   SSID/password and your Apps Script Web App URL (`google_script_url`,
   ending in `/exec`). This file is gitignored , it should never be
   committed.
2. `main-node/include/config.h` controls whether cloud sync is
   attempted at all (`ENABLE_CLOUD_SYNC`) and which backend is active
   (`main-node/include/cloud/cloud_backend_select.h` : Google Sheets by
   default).
3. Flash `cyd-display-node` first, read its MAC from serial on boot,
   then flash `main-node` (see Layer 2's README for the comms pairing
   details , that part is unchanged here).

```bash
# Unit tests, no hardware needed
cd l3-sensors-cloud/main-node && pio test -e native

# Flash to real hardware, in order
cd l3-sensors-cloud/cyd-display-node && pio run -t upload && pio device monitor
cd ../main-node && pio run -t upload && pio device monitor
```

### Wiring (main-node : GOOUUU ESP32-S3-CAM)
| Component | ESP32-S3 Pin | Notes |
|---|---|---|
| DHT22 data | GPIO 1 | |
| DS18B20 data | GPIO 2 | needs a 4.7kΩ pull-up to 3.3V |
| SD card CLK | GPIO 39 | built-in SDMMC slot, not generic SPI |
| SD card CMD | GPIO 38 | |
| SD card D0 | GPIO 40 | 1-bit mode : D1–D3 aren't wired on this board |
| Status LED | GPIO 21 | |
| I2C SDA (alt. comms transport) | GPIO 14 | |
| I2C SCL (alt. comms transport) | GPIO 47 | |

GPIO 4–18 are physically wired to this board's onboard camera and
cannot be reused; GPIO 19–20 are native USB; GPIO 22–25 don't exist on
this chip. See `main-node/include/pins.h` for the single source of
truth on wiring.

See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the full layer
plan and data-contract details.

## Next episode
Layer 6 : LVGL polish on the CYD display (gauges, status banner) ,
not yet built. (Numbering note, this jumps from 3 to 6 because the
originally-planned Layers 4–5 were absorbed into this one , see the
top-level README for that explanation.)