#pragma once
#include "comms_packet.h"
// ============================================================
// ui.h — the dashboard's GUI, isolated from main.cpp's ESP-NOW
// and setup/loop plumbing. main.cpp calls these two functions and
// otherwise doesn't know anything about LVGL widget internals.
// ============================================================

// Builds every widget once. Call after lv_tft_espi_create() and
// lv_display_set_theme(disp, NULL).
void ui_create(void);

// Pushes fresh sensor/health data into the already-built widgets.
// Safe to call repeatedly; does nothing until the first real packet
// arrives (widgets keep showing their "waiting..." placeholders).
// linkStale should be true if no packet has arrived recently, in
// which case the display shows "LINK LOST" regardless of the last
// known health_status.
void ui_update(const CommsPacket &pkt, bool haveEverReceivedPacket, bool linkStale);

// Shows which transport is active and whether data has flowed
// recently, in the title bar. Call this whenever a packet is sent
// or received, and periodically (e.g. once a second) with `active`
// based on "any traffic in the last few seconds" so the indicator
// naturally goes grey if the link drops.
void ui_set_comms_status(const char *transportName, bool active);

// Layer 6 (touch/settings): implemented in main.cpp, called from
// ui.cpp's touch event handlers. ui.cpp owns rendering and touch;
// main.cpp owns building the outgoing ack packet — these two
// functions are the deliberate, narrow crossing point between them.
void app_request_cloud_backend(uint8_t backend_id);
void app_request_force_sync();