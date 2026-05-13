// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+

#ifndef SP7A_TEST_PREFIX_ONLY
#include "PrecompiledHeader.h"
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
    // Filled in by SP7a Task 3 (GameDB + prefix chain).
    (void)serial;
    return {0u /* RETRO_REGION_NTSC */, 59.94};
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

    // NTSC territories (US, Japan, Korea, Asia).
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
