#pragma once
#include "shared_types.h"
// ============================================================
// cloud_google_sheets.h — cloud backend: POSTs to a Google Apps
// Script Web App, which appends each reading as a row in a Sheet.
// Real, working implementation.
//
// Every cloud backend (this one, and any future ones — Firebase,
// AWS, etc.) implements the SAME two function names below. Only one
// backend is ever compiled in at a time (selected in network_task.cpp),
// so network_task.cpp's calls never need to know or care which
// backend is actually active.
// ============================================================

bool cloud_sync_init();
void cloud_sync_send(const SensorSample &sample, const HealthReport &health);