#pragma once
#include <stdint.h>

struct SensorSample {
    uint32_t timestamp_ms;
    float    value;
    bool     read_ok;
};

enum class HealthStatus : uint8_t {
    OK = 0,
    WARNING = 1,
    FAULT = 2,
    UNKNOWN = 3
};

struct HealthReport {
    uint32_t     timestamp_ms;
    HealthStatus status;
    char         reason[64];
};

struct SystemStatus {
    bool wifi_connected;
    bool sd_card_ok;
    bool cloud_sync_ok;
    HealthStatus overall_health;
};
