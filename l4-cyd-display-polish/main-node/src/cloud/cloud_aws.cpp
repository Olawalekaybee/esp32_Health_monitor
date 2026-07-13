#include "cloud/cloud_aws.h"
#include <Arduino.h>

bool cloud_sync_init_aws() {
    Serial.println("[cloud_aws] WARNING: cloud_sync_init_aws() is a stub, not a real implementation.");
    return false;
}

bool cloud_sync_send_aws(const SensorSample &sample, const HealthReport &health) {
    (void)sample; (void)health; // unused — stub only
    return false; // never actually sends, so never actually succeeds
}
