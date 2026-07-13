#pragma once
// ============================================================
// cloud_backend_select.h — retired as of Layer 6.
//
// This used to be the single source of truth for which cloud backend
// was compiled in, guarding each backend's .cpp body so only one set
// of cloud_sync_init()/cloud_sync_send() symbols existed at link time.
//
// Layer 6 replaced that with cloud_dispatch.h: each backend now has a
// uniquely-named pair of functions (cloud_sync_init_google_sheets(),
// etc.), so all three compile in simultaneously with no name
// collision, and cloud_dispatch.cpp picks which one actually runs —
// at runtime, from the CYD's settings screen, not just at compile
// time. See cloud_dispatch.h for the CloudBackendId enum and the
// default backend constant.
//
// Nothing includes this file anymore. Kept only so it doesn't vanish
// out from under anyone with it open — safe to delete.
// ============================================================
