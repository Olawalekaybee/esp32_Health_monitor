#pragma once
#include "shared_types.h"
// SCAFFOLD ONLY, not implemented. Placeholder for when you build this.
// Realtime Database's plain REST interface (PUT to
// https://YOUR-PROJECT.firebaseio.com/path.json) is the simplest
// starting point if you build this later.
bool cloud_sync_init_firebase();
bool cloud_sync_send_firebase(const SensorSample &sample, const HealthReport &health);
