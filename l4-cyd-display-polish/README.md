# Layer 4: CYD Display Polish

**Series:** [ESP32 Health Monitor](../README.md), Episode 4
**Builds on:** [Layer 3, SD Storage + Wi-Fi + Cloud Sync](../l3-sensors-cloud/README.md)

![CI](https://github.com/Olawalekaybee/esp32_Health_monitor/actions/workflows/l4-cyd-display-polish-ci.yml/badge.svg)

This layer gives the CYD display visibility into things that, until now, only ever showed up in main-node's own Serial log. Layers 3 to 5 in the original plan added SD storage, Wi-Fi, and cloud sync, but the display board sitting right next to the sensors had no way to show any of it. You would only know Wi-Fi had dropped, the SD card had failed to mount, or a Sheets sync had failed by watching a laptop's serial monitor.

(Numbering note: this was originally planned as Layer 6, since Layers 3 through 5 were meant to be separate. They landed together in one folder instead, so this layer was renumbered down to 4 to keep the sequence contiguous. See the top level README for the full explanation.)

What it adds on top of Layer 3:

- **A system status strip on the CYD**, three small badges (WiFi, SD, Cloud) sitting between the title bar and the sensor cards, each with a live status dot, built with the same visual language as the existing comms badge.
- **An extended wire protocol.** The shared packet between main-node and cyd-display-node (`comms_packet.h`, byte for byte identical on both sides) now carries `wifi_connected`, `sd_card_ok`, and `cloud_sync_ok` alongside the sensor and health data it already sent.
- **Real success and failure reporting from the cloud backend.** `cloud_sync_send` used to be a `void` function, so nothing downstream could tell whether a sync actually worked. It now returns a genuine boolean based on the HTTP response, and main-node's `network_task` and `comms_task` carry that result through to the display.
- **A staleness rule for the CYD link itself**, matching the ten second threshold the CYD already used on its own side, so a stale acknowledgement is treated as a dropped link rather than trusted forever.
- **A vertical scrolling status ticker** in place of the old horizontal marquee. The status line ("OK"/"WARNING"/"FAULT") and the health reason now scroll upward together inside a clipped viewport, with the status line in a larger accent font and the reason line in a dimmed cyan tying it into the rest of the screen's accent color, rather than plain grey.

## Two firmwares, one folder

```
main-node/           Sensors, health logic, SD storage, Wi-Fi, cloud sync, system status, comms sender
cyd-display-node/    Separate ESP32 board, comms receiver, screen, system status strip, vertical ticker
```

## What changed under the hood

`shared_types.h` already had a `SystemStatus` struct sitting there unused, seemingly scaffolded in from an earlier layer for exactly this purpose. This layer builds on that rather than introducing a second, competing mechanism:

- A new `statusQueue` (the same length one mailbox pattern as `sensorQueue` and `healthQueue`) is created in `main.cpp` and seeded with a safe default before any task starts.
- `storage_task` publishes `sd_card_ok` once, right after its mount attempt, since that value never changes again after boot.
- `network_task` publishes `wifi_connected` every cycle and `cloud_sync_ok` from the cloud backend's return value.
- `comms_task` applies the staleness check to the last acknowledgement from the CYD, then stamps all three status fields onto every outgoing packet.

Each of these is a read, modify, and write against the same shared queue rather than a single owner writing every field, so it is not a perfectly atomic update. In practice the fields change at most once every few seconds, so the worst case is one stale field for one cycle, not a crash or a permanently wrong value.

## A real bug hit during bring-up, worth knowing about

Extending `comms_packet.h` changes its size in bytes. During initial testing, `cyd-display-node` was reflashed with the new firmware before `main-node` was, and the CYD's I2C receive handler silently dropped every incoming packet because the byte count no longer matched, with no error logged. The display showed "Waiting for main-node..." forever, which looked identical to a hardware fault.

This has two fixes, both in this layer:

1. The immediate fix is procedural: **always rebuild and reflash both boards together** whenever `comms_packet.h` changes.
2. The lasting fix is in `cyd-display-node/src/comms/comms_i2c.cpp`: a size mismatch now logs a specific message identifying the likely cause (`main-node and cyd-display-node likely have different comms_packet.h versions`), instead of failing silently.

## A layout note, checked on real hardware

Adding the status strip meant finding twenty pixels of vertical space on a two hundred forty pixel tall screen. The sensor cards kept their exact Layer 2 dimensions rather than being resized, since that layout was already proven on real hardware and was not worth risking for a cosmetic addition. The status banner absorbed the height difference instead, with its internal padding trimmed to compensate. This was worked out by hand before it was flashed, and confirmed working on the actual board afterward with no clipping or overlap.

### Wiring

Unchanged from Layer 3. See [`l3-sensors-cloud/README.md`](../l3-sensors-cloud/README.md) for the full wiring table and GPIO notes.

## What CI verifies

Same three jobs as Layer 3's workflow, scoped to this folder: native unit tests for `health_rules`, a build of `main-node` for `esp32-s3` (against a placeholder `wifi_secrets.h`, never real credentials), and a build of `cyd-display-node` for `cyd`. As with every layer so far, green CI proves the code compiles and the logic is correct, not that a physical SD card mounts, that Wi-Fi reaches a real access point, that the cloud sync endpoint accepts a request, or that the display actually renders correctly on the physical screen. It also can't catch the specific bug described above, where both boards compile fine individually but are running mismatched versions of the shared wire protocol against each other. That one is still on whoever's flashing hardware.

## Next episode

Layer 5: over the air firmware updates, per the renumbered roadmap in the top level README.