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

    // Pick the screen the view is currently displayed on, falling back to
    // the main screen when the view isn't yet hosted in a window
    // (host_window can be nil during early Acquire callbacks).
    NSScreen* screen = (host_window != nil) ? [host_window screen] : nil;
    if (screen == nil) screen = [NSScreen mainScreen];

    // backingScaleFactor is 1.0 on non-Retina; 2.0 on standard Retina.
    // Prefer the hosting window's value (matches the layer's actual
    // rendering target); only consult the screen if the window is unbacked.
    CGFloat scale = 1.0;
    if (host_window != nil)
        scale = [host_window backingScaleFactor];
    else if (screen != nil)
        scale = [screen backingScaleFactor];

    // NSScreen.maximumFramesPerSecond is macOS 12+. Guard with
    // respondsToSelector to remain safe on older systems (degenerate
    // fallback to the struct's 60.0f default).
    float refresh = 60.0f;
    if (screen != nil && [screen respondsToSelector:@selector(maximumFramesPerSecond)])
    {
        NSInteger fps = [screen maximumFramesPerSecond];
        if (fps > 0) refresh = static_cast<float>(fps);
    }

    out.surface_width  = static_cast<uint32_t>(bounds.size.width  * scale);
    out.surface_height = static_cast<uint32_t>(bounds.size.height * scale);
    out.surface_scale  = static_cast<float>(scale);
    out.refresh_rate   = refresh;
    return out;
}

} // namespace Pcsx2Libretro::Mac
