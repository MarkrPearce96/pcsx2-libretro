// See retronest_nsview.h. Compiles under both ARC and MRC (__bridge casts
// are accepted in non-ARC translation units as plain casts).
#include "retronest_nsview.h"

#import <AppKit/AppKit.h>

namespace retronest {

namespace {

NSViewMetrics QueryOnMain(NSView* view)
{
    NSViewMetrics out{};
    if (view == nil)
        return out;

    const NSRect bounds = [view bounds];
    NSWindow* host_window = [view window];

    // Pick the screen the view is displayed on; fall back to the main
    // screen when the view isn't yet hosted in a window (early Acquire).
    NSScreen* screen = (host_window != nil) ? [host_window screen] : nil;
    if (screen == nil)
        screen = [NSScreen mainScreen];

    // Prefer the hosting window's backing scale (matches the layer's real
    // rendering target); only consult the screen if the window is unbacked.
    CGFloat scale = 1.0;
    if (host_window != nil)
        scale = [host_window backingScaleFactor];
    else if (screen != nil)
        scale = [screen backingScaleFactor];

    // NSScreen.maximumFramesPerSecond is macOS 12+; guard for older systems.
    float refresh = 0.0f;
    if (screen != nil && [screen respondsToSelector:@selector(maximumFramesPerSecond)])
    {
        const NSInteger fps = [screen maximumFramesPerSecond];
        if (fps > 0)
            refresh = static_cast<float>(fps);
    }

    out.surface_width  = static_cast<uint32_t>(bounds.size.width * scale);
    out.surface_height = static_cast<uint32_t>(bounds.size.height * scale);
    out.surface_scale  = static_cast<float>(scale);
    out.refresh_rate   = refresh;
    return out;
}

} // namespace

NSViewMetrics QueryNSViewMetrics(void* ns_view)
{
    NSView* view = (__bridge NSView*)ns_view;
    if ([NSThread isMainThread])
        return QueryOnMain(view);

    // Cores call this from video threads; AppKit reads must happen on the
    // main thread. dispatch_sync is safe here because the main run loop is
    // owned by the host app and never blocks on the video thread during
    // AcquireRenderWindow.
    __block NSViewMetrics out{};
    dispatch_sync(dispatch_get_main_queue(), ^{
        out = QueryOnMain(view);
    });
    return out;
}

} // namespace retronest
