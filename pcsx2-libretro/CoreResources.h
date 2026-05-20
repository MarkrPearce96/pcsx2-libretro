// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// SP7a (Settings push): runtime resource discovery + PS2 region detection.
//
// Helpers, kept together because each is small and they share no state:
//   - ResolveResourcesDir():       dladdr-based path next to the dylib
//   - DetectRegionFromSerial():    GameDB → prefix-heuristic → default
//   - DetectRegionFromSerialPrefix(): exposed for standalone unit testing
//   - RegionFromGsVideoMode():     runtime refinement after SetGsCrt
#pragma once

#include <chrono>
#include <optional>
#include <string>

// Forward-declared so this header doesn't pull pcsx2/GS.h transitively.
// CoreResources.cpp includes the real header.
enum class GS_VideoMode : int;

namespace Pcsx2Libretro::CoreResources
{
    // Region/fps constants shared by every detection path AND by the
    // LibretroFrontend default-state init. Values match RETRO_REGION_NTSC /
    // RETRO_REGION_PAL from libretro.h:450-451 — pinned here so the
    // standalone unit test (DetectRegionFromSerialPrefix) can compile
    // without pulling libretro.h. Previously duplicated across
    // CoreResources.cpp's anonymous namespace + DetectRegionFromSerialPrefix's
    // local constexpr + LibretroFrontend.cpp's `59.94` literals; consolidated
    // here so adding a new region (e.g. NTSC-J 59.82) is a one-line change.
    inline constexpr unsigned kNtsc    = 0;
    inline constexpr unsigned kPal     = 1;
    inline constexpr double   kNtscFps = 59.94;
    inline constexpr double   kPalFps  = 50.0;

    // PS2 native framebuffer dimensions. Width is always 640; height differs
    // by region (NTSC 480-line interlaced renders 448 visible; PAL 576-line
    // interlaced renders 512 visible — PCSX2's GS uses these as the standard
    // output sizes). Used as libretro geometry.base_* in retro_system_av_info.
    inline constexpr unsigned kPs2NativeWidth      = 640;
    inline constexpr unsigned kPs2NativeHeightNtsc = 448;
    inline constexpr unsigned kPs2NativeHeightPal  = 512;

    // PCSX2's standard upscale cap (matches the 8x option in the standalone
    // Graphics dropdown). Per libretro spec, geometry.max_* cannot be raised
    // via SET_SYSTEM_AV_INFO after init, so this value gates the largest
    // frame a user can ever request via pcsx2_upscale_multiplier. 8x is the
    // documented "5K UHD" ceiling; higher values exist (16x/18x) but require
    // GPUs with >16K texture support and aren't exposed by default.
    inline constexpr unsigned kMaxUpscaleMultiplier = 8;

    // VM-lifecycle / save-state handshake thresholds. Consolidated here so a
    // single edit retunes all related deadlines instead of hunting through
    // EmuThread.cpp / LibretroSaveState.cpp / LibretroFrontend.cpp. Picked
    // empirically; rationale lives next to each call site.
    //
    //   kVmInitTimeout       VMManager::Initialize budget (cold start +
    //                        BIOS read + ELF load + plugin warmup). 30 s
    //                        is generous; real init is ~500 ms.
    //   kPostInitSettle      Cross-thread MTGS/MTVU settle sleep applied
    //                        post-init and pre-savestate. Load-bearing —
    //                        see memory/ee_recompiler_freeze_pickup.md.
    //   kFatalPauseWarnAt    Threshold for logging "PCSX2 internally
    //                        paused without our request" on the EmuThread
    //                        watchdog. Longer than the 200 ms save-state
    //                        handshake to avoid false positives.
    //   kFatalPauseShutdownAt
    //                        Threshold for treating an unexpected pause
    //                        as a fatal lockup and requesting graceful
    //                        shutdown — better than freezing forever.
    //   kWaitPausedDeadline  Save-state pause-handshake ceiling. EE thread
    //                        reaches next event-test in ~1 frame (~16 ms);
    //                        200 ms catches a stalled MTGS without locking
    //                        the host indefinitely.
    //   kFrameWaitTimeout    retro_run's wait for MTGS to signal a present.
    //                        Protects against VM hangs / Initialize-failed
    //                        paths where Frame would never be signalled.
    inline constexpr auto kVmInitTimeout        = std::chrono::seconds(30);
    inline constexpr auto kPostInitSettle       = std::chrono::milliseconds(150);
    inline constexpr auto kFatalPauseWarnAt     = std::chrono::milliseconds(500);
    inline constexpr auto kFatalPauseShutdownAt = std::chrono::milliseconds(1000);
    inline constexpr auto kWaitPausedDeadline   = std::chrono::milliseconds(200);
    inline constexpr auto kFrameWaitTimeout     = std::chrono::milliseconds(100);

    struct DetectedRegion
    {
        unsigned libretro_region;  // RETRO_REGION_NTSC | RETRO_REGION_PAL
        double   fps;              // 59.94 | 50.0
    };

    // Returns the absolute path of <dirname(this dylib)>/pcsx2_libretro_resources/.
    // Logs RETRO_LOG_ERROR if dladdr fails or the path doesn't exist on disk;
    // returns the resolved path regardless so downstream code produces clear
    // failure modes rather than silent fallback to a wrong dir.
    std::string ResolveResourcesDir();

    // Three-tier detection: GameDatabase::findGame → prefix heuristic → default NTSC.
    DetectedRegion DetectRegionFromSerial(const std::string& serial);

    // Internal/testable: prefix-only path. Returns default NTSC if the
    // prefix doesn't match a known mapping. Exposed for unit testing
    // since it has no PCSX2-internal dependencies.
    DetectedRegion DetectRegionFromSerialPrefix(const std::string& serial);

    // Maps a runtime gsVideoMode to libretro region/fps. Returns nullopt
    // for Uninitialized (the EE thread hasn't executed SetGsCrt yet).
    std::optional<DetectedRegion> RegionFromGsVideoMode(GS_VideoMode mode);
} // namespace Pcsx2Libretro::CoreResources
