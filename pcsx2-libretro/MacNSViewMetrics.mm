// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+

#include "MacNSViewMetrics.h"

#import <AppKit/AppKit.h>

namespace Pcsx2Libretro::Mac
{

NSViewMetrics Query(void* ns_view)
{
    NSViewMetrics out{};
    if (!ns_view) return out;

    NSView* view = (__bridge NSView*)ns_view;
    NSRect bounds = [view bounds];
    NSWindow* host_window = [view window];

    // backingScaleFactor is 1.0 on non-Retina; 2.0 on standard Retina.
    // Fall back to the main screen when the view isn't yet hosted in a
    // window (host_window can be nil during early Acquire callbacks).
    CGFloat scale = 1.0;
    if (host_window != nil)
        scale = [host_window backingScaleFactor];
    else if (NSScreen* screen = [NSScreen mainScreen])
        scale = [screen backingScaleFactor];

    out.surface_width  = static_cast<uint32_t>(bounds.size.width  * scale);
    out.surface_height = static_cast<uint32_t>(bounds.size.height * scale);
    out.surface_scale  = static_cast<float>(scale);
    return out;
}

} // namespace Pcsx2Libretro::Mac
