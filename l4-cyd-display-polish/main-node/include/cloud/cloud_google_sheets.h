#pragma once
#include "shared_types.h"
// ============================================================
// cloud_google_sheets.h — cloud backend: POSTs to a Google Apps
// Script Web App, which appends each reading as a row in a Sheet.
// Real, working implementation.
//
// Every cloud backend (this one, Firebase, AWS) implements the same
// two-function shape, but with a backend-specific suffix on the name
// (_google_sheets / _firebase / _aws) rather than sharing one name —
// this lets all three compile in simultaneously, so cloud_dispatch.cpp
// can switch between them at runtime instead of only at compile time.
// ============================================================

bool cloud_sync_init_google_sheets();
bool cloud_sync_send_google_sheets(const SensorSample &sample, const HealthReport &health);