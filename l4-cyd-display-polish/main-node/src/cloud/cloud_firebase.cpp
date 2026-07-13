#include "cloud/cloud_firebase.h"
#include <Arduino.h>

bool cloud_sync_init_firebase() {
    Serial.println("[cloud_firebase] WARNING: cloud_sync_init_firebase() is a stub, not a real implementation.");
    return false;
}

bool cloud_sync_send_firebase(const SensorSample &sample, const HealthReport &health) {
    (void)sample; (void)health; // unused — stub only
    return false; // never actually sends, so never actually succeeds
}
