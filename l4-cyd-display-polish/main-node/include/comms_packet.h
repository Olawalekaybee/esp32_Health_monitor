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
};

// CYD -> main-node: lightweight acknowledgement proving the link is
// genuinely two-way, not just main-node broadcasting into the void.
// Extend this later (button presses, threshold changes, etc.) once
// the CYD has real input to report back.
struct __attribute__((packed)) CydAckPacket {
    uint32_t cyd_uptime_ms;
    uint8_t  link_ok;   // 1 = CYD is receiving cleanly
};

// Backward-compat alias — earlier layers called the main-node->CYD
// struct "CommsPacket". Keeping this means old code that hasn't been
// migrated yet still compiles.
using CommsPacket = SensorHealthPacket;
