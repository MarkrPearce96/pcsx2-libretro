// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+

#include "PrecompiledHeader.h"

#include "CoreResources.h"

namespace Pcsx2Libretro::CoreResources
{

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

DetectedRegion DetectRegionFromSerialPrefix(const std::string& serial)
{
    // Filled in by SP7a Task 2 (prefix heuristic).
    (void)serial;
    return {0u /* RETRO_REGION_NTSC */, 59.94};
}

std::optional<DetectedRegion> RegionFromGsVideoMode(GS_VideoMode mode)
{
    // Filled in by SP7a Task 4.
    (void)mode;
    return std::nullopt;
}

} // namespace Pcsx2Libretro::CoreResources
