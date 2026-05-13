// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+

#ifndef SP7A_TEST_PREFIX_ONLY
#include "PrecompiledHeader.h"

#include "LibretroFrontend.h"          // FrontendLog
#include "pcsx2/GameDatabase.h"        // GameDatabase::findGame

#include "libretro.h"                  // RETRO_LOG_INFO / RETRO_LOG_WARN
#endif

#include "CoreResources.h"

#include <string>

namespace Pcsx2Libretro::CoreResources
{

#ifndef SP7A_TEST_PREFIX_ONLY
std::string ResolveResourcesDir()
{
    // Filled in by SP7a Task 4.
    return {};
}

DetectedRegion DetectRegionFromSerial(const std::string& serial)
{
    constexpr unsigned NTSC = RETRO_REGION_NTSC; // == 0
    constexpr unsigned PAL  = RETRO_REGION_PAL;  // == 1
    constexpr double NTSC_FPS = 59.94;
    constexpr double PAL_FPS  = 50.0;

    // Tier 1: GameDatabase lookup. Authoritative for thousands of retail
    // games; entry->region is a free-form string ("NTSC-U", "NTSC-J",
    // "PAL", etc.). Case-insensitive "starts with PAL" → PAL.
    if (!serial.empty())
    {
        if (const auto* entry = GameDatabase::findGame(serial))
        {
            const std::string& region = entry->region;
            if (region.size() >= 3
                && (region[0] == 'P' || region[0] == 'p')
                && (region[1] == 'A' || region[1] == 'a')
                && (region[2] == 'L' || region[2] == 'l'))
            {
                FrontendLog(RETRO_LOG_INFO,
                    "[SP7a] region=PAL fps=50.00 (GameDB '%s')",
                    region.c_str());
                return {PAL, PAL_FPS};
            }
            if (!region.empty())
            {
                FrontendLog(RETRO_LOG_INFO,
                    "[SP7a] region=NTSC fps=59.94 (GameDB '%s')",
                    region.c_str());
                return {NTSC, NTSC_FPS};
            }
            // GameDB entry exists but region field is empty — fall through.
        }
    }

    // Tier 2: prefix heuristic.
    const DetectedRegion by_prefix = DetectRegionFromSerialPrefix(serial);
    FrontendLog(RETRO_LOG_INFO,
        "[SP7a] region=%s fps=%.2f (prefix heuristic on '%s')",
        by_prefix.libretro_region == PAL ? "PAL" : "NTSC",
        by_prefix.fps, serial.c_str());

    // Tier 3 (warn for empty / clearly unknown serials). The prefix
    // heuristic always returns SOMETHING, so this is purely diagnostic.
    if (serial.empty()
        || (serial.size() >= 4
            && serial.substr(0, 4) != "SLES" && serial.substr(0, 4) != "SCES"
            && serial.substr(0, 4) != "SCED" && serial.substr(0, 4) != "SLED"
            && serial.substr(0, 4) != "SCUS" && serial.substr(0, 4) != "SLUS"
            && serial.substr(0, 4) != "SCAJ" && serial.substr(0, 4) != "SLPS"
            && serial.substr(0, 4) != "SLPM" && serial.substr(0, 4) != "SCKA"
            && serial.substr(0, 4) != "SLKA" && serial.substr(0, 4) != "SCKR"
            && serial.substr(0, 4) != "PSXC"))
    {
        FrontendLog(RETRO_LOG_WARN,
            "[SP7a] Unknown disc serial '%s' — defaulting to NTSC", serial.c_str());
    }

    return by_prefix;
}

std::optional<DetectedRegion> RegionFromGsVideoMode(GS_VideoMode mode)
{
    // Filled in by SP7a Task 4.
    (void)mode;
    return std::nullopt;
}
#endif

DetectedRegion DetectRegionFromSerialPrefix(const std::string& serial)
{
    constexpr unsigned NTSC = 0; // RETRO_REGION_NTSC
    constexpr unsigned PAL  = 1; // RETRO_REGION_PAL
    constexpr double NTSC_FPS = 59.94;
    constexpr double PAL_FPS  = 50.0;

    // Canonical form is PREFIX-NNNNN (per ExecutablePathToSerial in
    // pcsx2/CDVD/CDVD.cpp:525). Prefix is always the first 4 chars,
    // uppercase. Anything shorter than 4 chars can't be a valid serial.
    if (serial.size() < 4)
        return {NTSC, NTSC_FPS};

    const std::string prefix = serial.substr(0, 4);

    // PAL territories (Europe/Australia).
    if (prefix == "SLES" || prefix == "SCES"
        || prefix == "SCED" || prefix == "SLED")
        return {PAL, PAL_FPS};

    // NTSC territories (US, Japan, Korea, Asia). This branch is
    // documentation-only — the fallthrough below also returns NTSC.
    // It's retained so the known-NTSC prefix set is explicit at the
    // call site rather than implicit via "everything not PAL".
    if (prefix == "SCUS" || prefix == "SLUS"
        || prefix == "SCAJ" || prefix == "SLPS" || prefix == "SLPM"
        || prefix == "SCKA" || prefix == "SLKA" || prefix == "SCKR"
        || prefix == "PSXC")
        return {NTSC, NTSC_FPS};

    // Unknown prefix — default NTSC. Caller (DetectRegionFromSerial) is
    // responsible for logging a WARN; this function is pure for unit
    // testability.
    return {NTSC, NTSC_FPS};
}

} // namespace Pcsx2Libretro::CoreResources
