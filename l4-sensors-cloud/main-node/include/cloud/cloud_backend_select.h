#pragma once
// ============================================================
// cloud_backend_select.h — single source of truth for which cloud
// backend is active. Included by network_task.cpp (to pick which
// header to include) AND by each backend's own .cpp file (to guard
// its implementation).
//
// This second part matters: PlatformIO compiles EVERY .cpp file
// under src/ regardless of which header gets #included elsewhere —
// unlike a simple #if around an #include, that alone doesn't stop
// unselected backends from being compiled too. Since all backends
// deliberately share the same function names (cloud_sync_init,
// cloud_sync_send) for interchangeability, compiling more than one
// unguarded causes "multiple definition" linker errors. Each backend
// .cpp wraps its actual body in #if CLOUD_BACKEND_XXX using the
// macros below, so only the selected one contributes any symbols —
// the other two compile to empty, harmless translation units.
// ============================================================
#define CLOUD_BACKEND_GOOGLE_SHEETS 1
#define CLOUD_BACKEND_FIREBASE      0
#define CLOUD_BACKEND_AWS           0

#if (CLOUD_BACKEND_GOOGLE_SHEETS + CLOUD_BACKEND_FIREBASE + CLOUD_BACKEND_AWS) != 1
    #error "Choose exactly one cloud backend (CLOUD_BACKEND_GOOGLE_SHEETS / _FIREBASE / _AWS)"
#endif