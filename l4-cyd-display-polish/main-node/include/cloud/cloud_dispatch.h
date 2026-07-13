#pragma once
#include "shared_types.h"
// ============================================================
// cloud_dispatch.h — runtime cloud-backend switching.
//
// Previously (Layers 3-5), exactly one backend was selected at
// compile time in cloud_backend_select.h, and all three shared the
// same function names so only the selected one could be compiled in
// (any two sharing a name would cause a "multiple definition" linker
// error). That made switching backends require a rebuild+reflash.
//
// Layer 6 adds a settings screen on the CYD to switch backends live,
// which means main-node now needs all three compiled in at once. The
// fix was renaming each backend's functions with a distinct suffix
// (_google_sheets / _firebase / _aws) — see cloud_google_sheets.h,
// cloud_firebase.h, cloud_aws.h — so nothing collides. This file is
// the single dispatcher network_task.cpp actually calls, picking the
// right underlying function based on whichever backend is currently
// selected.
// ============================================================

enum class CloudBackendId : uint8_t {
    GOOGLE_SHEETS = 0,
    FIREBASE      = 1,
    AWS           = 2,
};

constexpr uint8_t CLOUD_BACKEND_COUNT = 3;

// What runs at boot, before any runtime selection arrives from the
// CYD's settings screen (or if the CYD is never connected at all).
constexpr CloudBackendId DEFAULT_CLOUD_BACKEND = CloudBackendId::GOOGLE_SHEETS;

// True if `id` is one of the known backend values — use this before
// trusting a value that came off the wire (e.g. CydAckPacket's
// requested_cloud_backend), since a corrupted or from-the-future byte
// shouldn't be cast to CloudBackendId and used unchecked.
bool cloud_backend_id_valid(uint8_t id);

bool cloud_dispatch_init(CloudBackendId id);
bool cloud_dispatch_send(CloudBackendId id, const SensorSample &sample, const HealthReport &health);
const char *cloud_backend_name(CloudBackendId id);
