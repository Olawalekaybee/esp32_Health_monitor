#include "cloud/cloud_firebase.h"
#include "cloud/cloud_backend_select.h"

#if CLOUD_BACKEND_FIREBASE

#include <Arduino.h>

bool cloud_sync_init() {
    Serial.println("[cloud_firebase] WARNING: cloud_sync_init() is a stub, not a real implementation.");
    return false;
}

void cloud_sync_send(const SensorSample &sample, const HealthReport &health) {
    (void)sample; (void)health; // unused — stub only
}

#endif // CLOUD_BACKEND_FIREBASE