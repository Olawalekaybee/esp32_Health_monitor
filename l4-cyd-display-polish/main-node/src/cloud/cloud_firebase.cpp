#include "cloud/cloud_firebase.h"
#include "cloud/cloud_backend_select.h"

#if CLOUD_BACKEND_FIREBASE

#include <Arduino.h>

bool cloud_sync_init() {
    Serial.println("[cloud_firebase] WARNING: cloud_sync_init() is a stub, not a real implementation.");
    return false;
}

bool cloud_sync_send(const SensorSample &sample, const HealthReport &health) {
    (void)sample; (void)health; // unused — stub only
    return false; // never actually sends, so never actually succeeds
}

#endif // CLOUD_BACKEND_FIREBASE