// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// Reads geometry off the NSView the libretro frontend provides.
//
// PCSX2's GS aspect math (CalculateDrawDstRect in GSRenderer.cpp) reads
// WindowInfo::surface_width / surface_height to compute the displayed
// letterbox. Reporting these as a hardcoded 640x448 makes PCSX2 letterbox
// 16:9 against a 1.43:1 surface (giving ~10% bars) regardless of the
// real screen. We query the actual NSView dimensions instead so the
// aspect math runs against the real surface ratio — matching what
// standalone PCSX2 sees from its own NSWindow.

#pragma once

#include <cstdint>

namespace Pcsx2Libretro::Mac
{
    struct NSViewMetrics
    {
        // surface_width/height in physical pixels (point size × backing scale),
        // matching what GSDeviceMTL stores in CAMetalLayer.drawableSize.
        uint32_t surface_width  = 0;
        uint32_t surface_height = 0;
        // Backing scale factor (1.0 on non-Retina, 2.0 on standard Retina).
        float    surface_scale  = 1.0f;
        // Screen refresh rate in Hz (NSScreen.maximumFramesPerSecond).
        // Falls back to 60.0f when the screen / API is unavailable.
        float    refresh_rate   = 60.0f;
    };

    // Reads NSView.bounds (points) × NSWindow.backingScaleFactor.
    // Returns a zero-width metrics struct if ns_view is null or unbacked.
    NSViewMetrics Query(void* ns_view);
}
