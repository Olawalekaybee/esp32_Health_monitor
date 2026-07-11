#pragma once
#include <stdint.h>
#include <cstdio>
#include "shared_types.h"

// ============================================================
// health_rules — the actual decision logic for sensor health,
// deliberately kept free of Arduino.h, FreeRTOS, or any hardware
// dependency (shared_types.h itself only needs <stdint.h>, so it's
// safe to include here too). That's what lets this run and be
// unit-tested on a plain host machine in CI, not just on real
// ESP32 hardware.
//
// health_task.cpp (Arduino-side) is a thin wrapper: it reads the
// queue, calls into here, and pushes the result back onto another
// queue. This file is also the seam Layer 8 (WAMR) will eventually
// replace with a swappable .wasm rule module.
// ============================================================

struct HealthDecision {
    HealthStatus status;
    char         reason[64];
};

// Pure function: given current failure streak counts for each sensor
// and a threshold, decide the overall health status and reason.
// No I/O, no globals besides what's passed in — fully deterministic
// and safe to call from a unit test.
inline HealthDecision evaluate_health(uint8_t dht_failures,
                                       uint8_t ds18b20_failures,
                                       uint8_t failure_threshold) {
    HealthDecision d{};

    bool dht_fault = dht_failures >= failure_threshold;
    bool ds_fault  = ds18b20_failures >= failure_threshold;

    if (dht_fault && ds_fault) {
        d.status = HealthStatus::FAULT;
        std::snprintf(d.reason, sizeof(d.reason),
                      "Both sensors down (DHT22 x%d, DS18B20 x%d)",
                      dht_failures, ds18b20_failures);
    } else if (dht_fault) {
        d.status = HealthStatus::FAULT;
        std::snprintf(d.reason, sizeof(d.reason),
                      "DHT22 failed %d reads in a row", dht_failures);
    } else if (ds_fault) {
        d.status = HealthStatus::FAULT;
        std::snprintf(d.reason, sizeof(d.reason),
                      "DS18B20 failed %d reads in a row", ds18b20_failures);
    } else if (dht_failures > 0 || ds18b20_failures > 0) {
        d.status = HealthStatus::WARNING;
        std::snprintf(d.reason, sizeof(d.reason),
                      "Recent failures: DHT22 x%d, DS18B20 x%d",
                      dht_failures, ds18b20_failures);
    } else {
        d.status = HealthStatus::OK;
        std::snprintf(d.reason, sizeof(d.reason), "Both sensors nominal");
    }

    return d;
}
