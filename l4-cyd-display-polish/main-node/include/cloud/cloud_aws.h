#pragma once
#include "shared_types.h"
// SCAFFOLD ONLY, not implemented. AWS IoT Core (MQTT+TLS+device
// certs) is a bigger lift than the others; API Gateway + Lambda
// exposing plain HTTPS is a simpler starting point if you build this later.
bool cloud_sync_init_aws();
bool cloud_sync_send_aws(const SensorSample &sample, const HealthReport &health);
