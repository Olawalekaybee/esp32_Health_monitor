#pragma once
#include <stdint.h>

// ============================================================
// shared_types.h — the "contract" between tasks.
// Every task communicates ONLY through these structs + FreeRTOS
// queues, never through shared globals. This is what makes each
// task swappable later without touching the others.
// ============================================================

// One sensor sample, produced by sensor_task, consumed by
// storage_task, network_task, display_task, health_task, and comms_task.
// Two independent sensors are tracked separately with their own
// read_ok flags — a failure in one must never mask the other.
struct SensorSample {
    uint32_t timestamp_ms;

    float    dht_temperature_c;
    float    dht_humidity_pct;
    bool     dht_read_ok;

    float    ds18b20_temperature_c;
    bool     ds18b20_read_ok;
};

// Health verdict, produced by health_task, consumed by
// display_task and network_task.
enum class HealthStatus : uint8_t {
    OK = 0,
    WARNING = 1,
    FAULT = 2,
    UNKNOWN = 3
};

struct HealthReport {
    uint32_t     timestamp_ms;
    HealthStatus status;
    char         reason[64];   // short human-readable explanation
};

// System-wide status, mostly for display_task.
struct SystemStatus {
    bool wifi_connected;
    bool sd_card_ok;
    bool cloud_sync_ok;
    bool cyd_link_ok;      // last ESP-NOW send to CYD succeeded
    HealthStatus overall_health;

    // Layer 6 (touch/settings): which cloud backend the CYD has asked
    // for (set by comms_task from the CYD's ack, read by network_task),
    // and which one network_task is actually dispatching to right now
    // (the authoritative value, read by comms_task to report back to
    // the CYD so its settings screen can show confirmed-active, not
    // just requested). Values match CloudBackendId in cloud_dispatch.h;
    // stored as uint8_t here since shared_types.h doesn't depend on
    // that header. force-sync is a semaphore (see forceSyncSemaphore),
    // not a status field — it needs to wake network_task immediately,
    // which a polled mailbox field can't do without a busy-wait.
    uint8_t requested_cloud_backend;
    uint8_t active_cloud_backend;
};

// CommsPacket (the main-node -> CYD wire format) now lives in its
// own file, comms_packet.h, since it's shared verbatim with the
// cyd-display-node project — see that file for details.
