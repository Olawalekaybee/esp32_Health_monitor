#include "cloud/cloud_dispatch.h"
#include "cloud/cloud_google_sheets.h"
#include "cloud/cloud_firebase.h"
#include "cloud/cloud_aws.h"

bool cloud_backend_id_valid(uint8_t id) {
    return id < CLOUD_BACKEND_COUNT;
}

bool cloud_dispatch_init(CloudBackendId id) {
    switch (id) {
        case CloudBackendId::GOOGLE_SHEETS: return cloud_sync_init_google_sheets();
        case CloudBackendId::FIREBASE:      return cloud_sync_init_firebase();
        case CloudBackendId::AWS:           return cloud_sync_init_aws();
    }
    return false; // unreachable if cloud_backend_id_valid() was checked first
}

bool cloud_dispatch_send(CloudBackendId id, const SensorSample &sample, const HealthReport &health) {
    switch (id) {
        case CloudBackendId::GOOGLE_SHEETS: return cloud_sync_send_google_sheets(sample, health);
        case CloudBackendId::FIREBASE:      return cloud_sync_send_firebase(sample, health);
        case CloudBackendId::AWS:           return cloud_sync_send_aws(sample, health);
    }
    return false;
}

const char *cloud_backend_name(CloudBackendId id) {
    switch (id) {
        case CloudBackendId::GOOGLE_SHEETS: return "Google Sheets";
        case CloudBackendId::FIREBASE:      return "Firebase";
        case CloudBackendId::AWS:           return "AWS";
    }
    return "Unknown";
}
