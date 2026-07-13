#pragma once
#include <stdint.h>

// ============================================================
// comms_packet.h — wire format shared between main-node and
// cyd-display-node, across every transport (ESP-NOW, I2C,
// Bluetooth). MUST be byte-for-byte identical on both sides — if
// you change one copy, change the other and rebuild+reupload both.
// ============================================================

// main-node -> CYD: sensor readings + health verdict.
struct __attribute__((packed)) SensorHealthPacket {
    uint32_t timestamp_ms;

    float    dht_temperature_c;
    float    dht_humidity_pct;
    uint8_t  dht_read_ok;

    float    ds18b20_temperature_c;
    uint8_t  ds18b20_read_ok;

    uint8_t  health_status;   // 0=OK, 1=WARNING, 2=FAULT, 3=UNKNOWN
    char     health_reason[32];

    // Layer 6: system status, previously invisible from the CYD —
    // you'd only see these in main-node's own Serial output before.
    uint8_t  wifi_connected;
    uint8_t  sd_card_ok;
    uint8_t  cloud_sync_ok;

    // Layer 6 (touch/settings): which cloud backend main-node is
    // actually dispatching to right now — 0=Google Sheets, 1=Firebase,
    // 2=AWS (see CloudBackendId in main-node's cloud_dispatch.h). Lets
    // the CYD's settings screen show confirmed-active, not just
    // whatever was last requested.
    uint8_t  active_cloud_backend;
};

// CYD -> main-node: lightweight acknowledgement proving the link is
// genuinely two-way, not just main-node broadcasting into the void.
struct __attribute__((packed)) CydAckPacket {
    uint32_t cyd_uptime_ms;
    uint8_t  link_ok;   // 1 = CYD is receiving cleanly

    // Layer 6 (touch/settings): the CYD's settings-screen selection —
    // matches CloudBackendId (0=Google Sheets, 1=Firebase, 2=AWS).
    uint8_t  requested_cloud_backend;

    // Layer 6 (touch/settings): pulses to 1 for ~2 seconds after a tap
    // on the Cloud badge (see cyd-display-node's app_request_force_sync()),
    // then falls back to 0 on its own. main-node treats this as an
    // edge — see comms_task.cpp — so it triggers one immediate sync
    // attempt, not one per ack that happens to catch it still high.
    uint8_t  force_sync_requested;
};

// Backward-compat alias — earlier layers called the main-node->CYD
// struct "CommsPacket". Keeping this means old code that hasn't been
// migrated yet still compiles.
using CommsPacket = SensorHealthPacket;
