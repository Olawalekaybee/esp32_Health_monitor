#include "wifi_secrets.h"
#pragma once
// ============================================================
// config.h — system-wide tunables. Kept separate from pins.h
// so hardware changes and behavior changes never mix in one diff.
// ============================================================

// --- Mode ---
// Device always logs locally. This only controls whether it ALSO
// tries to sync to the cloud when Wi-Fi is available.

#define ENABLE_CLOUD_SYNC     true
// --- Cloud sync (Layer 5) ---
// Paste your Google Apps Script Web App URL here (ends in /exec).
// See network_task.cpp's header comment for the paired Apps Script code.
#define CLOUD_ENDPOINT_URL google_script_url
#define WIFI_CONNECT_TIMEOUT_MS  15000   // give up on one attempt after this long
#define WIFI_RECONNECT_INTERVAL_MS 30000 // how often to retry if disconnected

// --- Timing (milliseconds) ---
#define SENSOR_READ_INTERVAL_MS      2000
#define STORAGE_WRITE_INTERVAL_MS    5000
#define CLOUD_SYNC_INTERVAL_MS       60000
#define HEALTH_CHECK_INTERVAL_MS     2000
#define DISPLAY_REFRESH_INTERVAL_MS  500
#define COMMS_SEND_INTERVAL_MS       1000   // how often we push updates to the CYD

// --- FreeRTOS task priorities (0 = lowest) ---
#define PRIORITY_SENSOR_TASK    3
#define PRIORITY_HEALTH_TASK    3
#define PRIORITY_STORAGE_TASK   2
#define PRIORITY_NETWORK_TASK   1
#define PRIORITY_DISPLAY_TASK   1
#define PRIORITY_COMMS_TASK     1

// --- FreeRTOS task stack sizes (bytes) ---
#define STACK_SENSOR_TASK    4096
#define STACK_STORAGE_TASK   4096
#define STACK_NETWORK_TASK   8192
#define STACK_DISPLAY_TASK   4096
#define STACK_HEALTH_TASK    4096
#define STACK_COMMS_TASK     4096

// --- Core pinning (ESP32 has core 0 and core 1) ---
// Networking/display on core 0, timing-sensitive sensing/health on core 1
#define CORE_NETWORK_DISPLAY  0
#define CORE_SENSOR_HEALTH    1

// --- ESP-NOW: main-node -> cyd-display-node ---
#define ESPNOW_USE_BROADCAST   false
// CYD's actual WiFi/ESP-NOW MAC (confirmed from its own serial boot log):
static const uint8_t ESPNOW_PEER_MAC[6] = {0xF4, 0x2D, 0xC9, 0x79, 0x9E, 0xC8};
// CYD's Bluetooth MAC (for whenever the Bluetooth transport is
// actually implemented — currently a stub, see comms_bluetooth.h):
// F4:2D:C9:79:9E:CA
// To fall back to broadcast (e.g. testing with a different CYD unit):
// #define ESPNOW_USE_BROADCAST   true
// static const uint8_t ESPNOW_PEER_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};