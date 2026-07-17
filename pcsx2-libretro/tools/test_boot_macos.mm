// Standalone macOS boot harness: proves the core boots a game outside
// RetroNest. Creates a real NSWindow/NSView, serves the private
// RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW command, and drives the full
// retro_load_game / retro_run lifecycle from a worker thread while the
// main thread pumps the AppKit runloop (the Metal device attaches its
// CAMetalLayer via dispatch to the main queue).
//
// Not built as part of the pcsx2_libretro target — manual compile:
//
//   clang++ -std=c++17 -fobjc-arc -I.. test_boot_macos.mm \
//       -framework Cocoa -framework QuartzCore -o test_boot_macos
//   codesign -s - -f --entitlements test_boot_macos.entitlements test_boot_macos
//
// The codesign step is REQUIRED on Apple Silicon: the arm64 recompilers
// allocate MAP_JIT pages, which need com.apple.security.cs.allow-jit.
//
//   ./test_boot_macos <core.dylib> <game.iso> <system_dir> [save_dir] [seconds]
//
// system_dir must contain a PS2 BIOS (>1 MB, scph-style name). Exit codes:
// 0 = ran for the requested duration, 2 = retro_load_game failed,
// 1 = setup failure. Writes boot_NN.png window captures next to save_dir.

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#import <ImageIO/ImageIO.h>

#include <dlfcn.h>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

#include "libretro.h"
#include "retronest-libretro/retronest_libretro.h"

namespace {

NSWindow* g_window = nil;
NSView* g_view = nil;
std::string g_system_dir;
std::string g_save_dir;

// Keyboard -> libretro joypad bits (RETRO_DEVICE_ID_JOYPAD_* bit index).
// Set from AppKit key monitors on the main thread, read from the core's
// input poll on the EE thread.
std::atomic<uint32_t> g_buttons{0};

// kVK_* keycode -> joypad id. Layout mirrors RetroArch's default binds:
// arrows=dpad, Z=Cross X=Circle A=Square S=Triangle, Enter=Start,
// Tab=Select, Q/W=L1/R1, 1/2=L2/R2.
int JoypadIdForKeyCode(unsigned short keycode)
{
    switch (keycode)
    {
        case 126: return 4;  // up
        case 125: return 5;  // down
        case 123: return 6;  // left
        case 124: return 7;  // right
        case 6:   return 0;  // Z -> Cross (B)
        case 7:   return 8;  // X -> Circle (A)
        case 0:   return 1;  // A -> Square (Y)
        case 1:   return 9;  // S -> Triangle (X)
        case 36:  return 3;  // Return -> Start
        case 48:  return 2;  // Tab -> Select
        case 12:  return 10; // Q -> L1
        case 13:  return 11; // W -> R1
        case 18:  return 12; // 1 -> L2
        case 19:  return 13; // 2 -> R2
        default:  return -1;
    }
}

void LogCb(enum retro_log_level level, const char* fmt, ...)
{
    static const char* names[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    fprintf(stderr, "[core %s] %s", names[level <= RETRO_LOG_ERROR ? level : 3], buf);
    fflush(stderr);
}

bool EnvCb(unsigned cmd, void* data)
{
    switch (cmd)
    {
        case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
            static_cast<retro_log_callback*>(data)->log = LogCb;
            return true;
        case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
            *static_cast<const char**>(data) = g_system_dir.c_str();
            return true;
        case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
            *static_cast<const char**>(data) = g_save_dir.c_str();
            return true;
        case RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW:
            *static_cast<void**>(data) = (__bridge void*)g_view;
            fprintf(stderr, "[harness] served NSView %p to core\n", (__bridge void*)g_view);
            return true;
        case RETRO_ENVIRONMENT_SET_MESSAGE:
            fprintf(stderr, "[harness] core message: %s\n",
                static_cast<const retro_message*>(data)->msg);
            return true;
        case RETRO_ENVIRONMENT_SET_MESSAGE_EXT:
            fprintf(stderr, "[harness] core message: %s\n",
                static_cast<const retro_message_ext*>(data)->msg);
            return true;
        case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
        {
            const auto* av = static_cast<const retro_system_av_info*>(data);
            fprintf(stderr, "[harness] av_info: %ux%u @ %.2f fps\n",
                av->geometry.base_width, av->geometry.base_height, av->timing.fps);
            return true;
        }
        case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
            return true;
        default:
            return false; // GET_VARIABLE lands here -> core option defaults
    }
}

void VideoCb(const void*, unsigned, unsigned, size_t) {}
void AudioSampleCb(int16_t, int16_t) {}
size_t AudioBatchCb(const int16_t*, size_t frames) { return frames; }
void InputPollCb() {}
int16_t InputStateCb(unsigned port, unsigned device, unsigned, unsigned id)
{
    if (port == 0 && device == RETRO_DEVICE_JOYPAD && id < 16)
        return (g_buttons.load(std::memory_order_relaxed) >> id) & 1;
    return 0; // analog axes stay centered
}

void CaptureWindow(const std::string& path)
{
    // Own-window capture via the screencapture CLI; needs the
    // screen-recording TCC grant and silently produces nothing without
    // it — that's fine, it's a bonus on top of the log-based proof.
    const long wid = (long)[g_window windowNumber];
    char cmd[1200];
    snprintf(cmd, sizeof(cmd), "screencapture -x -o -l%ld '%s'", wid, path.c_str());
    const int rc = system(cmd);
    fprintf(stderr, "[harness] capture '%s' -> rc=%d\n", path.c_str(), rc);
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 4)
    {
        fprintf(stderr,
            "usage: %s <core.dylib> <game.iso> <system_dir> [save_dir] [seconds]\n",
            argv[0]);
        return 1;
    }
    const char* core_path = argv[1];
    const char* game_path = argv[2];
    g_system_dir = argv[3];
    g_save_dir = (argc > 4) ? argv[4] : g_system_dir;
    const int run_seconds = (argc > 5) ? atoi(argv[5]) : 30;

    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

    const NSRect rect = NSMakeRect(100, 100, 640, 448);
    g_window = [[NSWindow alloc]
        initWithContentRect:rect
                  styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable)
                    backing:NSBackingStoreBuffered
                      defer:NO];
    [g_window setTitle:@"pcsx2_libretro arm64 boot test"];
    g_view = [[NSView alloc] initWithFrame:rect];
    [g_window setContentView:g_view];
    [g_window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];

    // Keyboard -> joypad. Local monitors only see events while our window
    // is key; returning nil swallows handled keys (no system beep).
    id (^keyHandler)(NSEvent*) = ^NSEvent*(NSEvent* ev) {
        const int id_ = JoypadIdForKeyCode([ev keyCode]);
        if (id_ < 0)
            return ev;
        if ([ev type] == NSEventTypeKeyDown)
            g_buttons.fetch_or(1u << id_, std::memory_order_relaxed);
        else
            g_buttons.fetch_and(~(1u << id_), std::memory_order_relaxed);
        return nil;
    };
    [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyDown handler:keyHandler];
    [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyUp handler:keyHandler];
    fprintf(stderr,
        "[harness] keys: arrows=dpad Z=Cross X=Circle A=Square S=Triangle "
        "Enter=Start Tab=Select Q/W=L1/R1 1/2=L2/R2\n");

    std::thread worker([=] {
        void* h = dlopen(core_path, RTLD_NOW | RTLD_LOCAL);
        if (!h)
        {
            fprintf(stderr, "[harness] dlopen FAILED: %s\n", dlerror());
            exit(1);
        }
        #define SYM(name) auto* name = reinterpret_cast<decltype(&::name)>(dlsym(h, #name))
        SYM(retro_set_environment);
        SYM(retro_set_video_refresh);
        SYM(retro_set_audio_sample);
        SYM(retro_set_audio_sample_batch);
        SYM(retro_set_input_poll);
        SYM(retro_set_input_state);
        SYM(retro_init);
        SYM(retro_load_game);
        SYM(retro_run);
        SYM(retro_unload_game);
        SYM(retro_deinit);
        SYM(retro_get_system_av_info);
        #undef SYM
        if (!retro_set_environment || !retro_init || !retro_load_game || !retro_run)
        {
            fprintf(stderr, "[harness] dlsym FAILED\n");
            exit(1);
        }

        retro_set_environment(EnvCb);
        retro_set_video_refresh(VideoCb);
        retro_set_audio_sample(AudioSampleCb);
        retro_set_audio_sample_batch(AudioBatchCb);
        retro_set_input_poll(InputPollCb);
        retro_set_input_state(InputStateCb);
        retro_init();
        fprintf(stderr, "[harness] retro_init done\n");

        retro_game_info gi{};
        gi.path = game_path;
        if (!retro_load_game(&gi))
        {
            fprintf(stderr, "[harness] retro_load_game FAILED\n");
            exit(2);
        }
        retro_system_av_info av{};
        retro_get_system_av_info(&av);
        fprintf(stderr, "[harness] LOADED: %ux%u @ %.2f fps — running %d s\n",
            av.geometry.base_width, av.geometry.base_height, av.timing.fps,
            run_seconds);

        const double fps = (av.timing.fps > 1.0) ? av.timing.fps : 60.0;
        const long total = static_cast<long>(fps * run_seconds);
        int shot = 0;
        for (long i = 0; i < total; i++)
        {
            retro_run();
            if (i > 0 && i % 300 == 0)
                fprintf(stderr, "[harness] frame %ld/%ld\n", i, total);
            // Capture at ~5 s intervals for visual verification.
            if (i == static_cast<long>(fps) * (5 + 10 * shot))
            {
                const std::string path =
                    g_save_dir + "/boot_" + std::to_string(shot) + ".png";
                shot++;
                dispatch_sync(dispatch_get_main_queue(), ^{ CaptureWindow(path); });
            }
        }

        fprintf(stderr, "[harness] run complete (%ld frames) — unloading\n", total);
        retro_unload_game();
        retro_deinit();
        fprintf(stderr, "[harness] PASS\n");
        exit(0);
    });
    worker.detach();

    [NSApp run];
    return 0;
}
