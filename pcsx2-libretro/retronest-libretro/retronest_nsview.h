// retronest-libretro — shared NSView-metrics query for Metal-backed cores.
// CANONICAL COPY: RetroNest-Project/vendor/retronest-libretro/. Do not edit
// vendored copies; see retronest_libretro.h for the sync/drift rules.
//
// Merged from duckstation's libretro_window.mm (which carries the
// main-thread dispatch fix) and pcsx2's MacNSViewMetrics.mm. AppKit requires
// UI-object access on the main thread; cores call this from their video
// threads, so QueryNSViewMetrics bounces the reads to the main queue via
// dispatch_sync when needed.
#pragma once

#include <cstdint>

namespace retronest {

struct NSViewMetrics {
    // Physical pixels (view bounds × backing scale). 0 when the view is
    // null or not yet realized — callers pick their own fallback.
    uint32_t surface_width = 0;
    uint32_t surface_height = 0;
    // Backing scale factor (1.0 non-Retina, 2.0 standard Retina).
    float surface_scale = 1.0f;
    // NSScreen.maximumFramesPerSecond in Hz; 0.0f when unknown — callers
    // pick their own fallback (pcsx2 uses 60.0f, duckstation lets the
    // Metal device decide).
    float refresh_rate = 0.0f;
};

// Main-thread-safe. Returns a zeroed struct (scale 1.0) if ns_view is null.
NSViewMetrics QueryNSViewMetrics(void* ns_view);

} // namespace retronest
