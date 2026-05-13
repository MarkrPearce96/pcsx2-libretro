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

#include <optional>
#include <string>

// Forward-declared so this header doesn't pull pcsx2/GS.h transitively.
// CoreResources.cpp includes the real header.
enum class GS_VideoMode : int;

namespace Pcsx2Libretro::CoreResources
{
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
