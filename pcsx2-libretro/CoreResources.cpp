// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+

#ifndef SP7A_TEST_PREFIX_ONLY
#include "PrecompiledHeader.h"

#include "LibretroFrontend.h"          // FrontendLog
#include "pcsx2/GameDatabase.h"        // GameDatabase::findGame

#include "libretro.h"                  // RETRO_LOG_INFO / RETRO_LOG_WARN

#include "common/FileSystem.h"
#include "common/Path.h"
#include "pcsx2/GS.h"                  // GS_VideoMode

#include <dlfcn.h>                     // dladdr / Dl_info
#endif

#include "CoreResources.h"

#include <array>
#include <string>

#include <string>

namespace Pcsx2Libretro::CoreResources
{

namespace
{
    // Region/fps constants (kNtsc, kPal, kNtscFps, kPalFps) are now public
    // inline-constexpr in CoreResources.h so LibretroFrontend.cpp's default
    // state init can reference them too. Reused via using-aliases below.
    using Pcsx2Libretro::CoreResources::kNtsc;
    using Pcsx2Libretro::CoreResources::kPal;
    using Pcsx2Libretro::CoreResources::kNtscFps;
    using Pcsx2Libretro::CoreResources::kPalFps;

    // Single source of truth for the known PS2 serial prefixes. Both
    // DetectRegionFromSerialPrefix (returns a region) and IsKnownSerialPrefix
    // (used by DetectRegionFromSerial Tier-3 to decide whether the WARN log
    // fires) go through ClassifyPrefix below. Previously these two functions
    // each carried their own copy of the prefix list with a comment warning
    // about keeping them in sync — easy to forget when adding a new region.
    constexpr std::array<const char*, 4> kPalPrefixes = {
        "SLES", "SCES", "SCED", "SLED",
    };
    // Documentation-only — fallthrough below also returns NTSC. Listing the
    // known-NTSC prefixes explicitly makes the supported set obvious at the
    // call site rather than implicit-via-everything-else.
    constexpr std::array<const char*, 9> kNtscPrefixes = {
        "SCUS", "SLUS", "SCAJ", "SLPS", "SLPM",
        "SCKA", "SLKA", "SCKR", "PSXC",
    };

    enum class PrefixClass { Pal, Ntsc, Unknown };

    PrefixClass ClassifyPrefix(const std::string& serial)
    {
        if (serial.size() < 4)
            return PrefixClass::Unknown;
        const std::string p = serial.substr(0, 4);
        for (const char* x : kPalPrefixes)
            if (p == x) return PrefixClass::Pal;
        for (const char* x : kNtscPrefixes)
            if (p == x) return PrefixClass::Ntsc;
        return PrefixClass::Unknown;
    }

    bool IsKnownSerialPrefix(const std::string& serial)
    {
        return ClassifyPrefix(serial) != PrefixClass::Unknown;
    }
}

#ifndef SP7A_TEST_PREFIX_ONLY
std::string ResolveResourcesDir()
{
    Dl_info info{};
    if (dladdr(reinterpret_cast<void*>(&ResolveResourcesDir), &info) == 0
        || info.dli_fname == nullptr)
    {
        FrontendLog(RETRO_LOG_ERROR,
            "[CoreResources] dladdr failed when resolving resources dir; "
            "Metal GS init will fail to find metallibs");
        return {};
    }

    // Path::GetDirectory returns a std::string_view tied to dylib_path; the
    // string ctor on the next line copies it into an owned std::string so the
    // view's lifetime stops mattering. This is the intentional shape — not a
    // missed move.
    const std::string dylib_path(info.dli_fname);
    const std::string dir(Path::GetDirectory(dylib_path));
    const std::string resources = Path::Combine(dir, "pcsx2_libretro_resources");

    if (!FileSystem::DirectoryExists(resources.c_str()))
    {
        FrontendLog(RETRO_LOG_ERROR,
            "[CoreResources] Resources directory not found at '%s' — install layout "
            "missing pcsx2_libretro_resources/ next to the dylib. Metal init will fail.",
            resources.c_str());
    }
    else
    {
        FrontendLog(RETRO_LOG_INFO,
            "[CoreResources] Resources dir = %s", resources.c_str());
    }
    return resources;
}

DetectedRegion DetectRegionFromSerial(const std::string& serial)
{
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
                    "[Region] region=PAL fps=50.00 (GameDB '%s')",
                    region.c_str());
                return {kPal, kPalFps};
            }
            if (!region.empty())
            {
                FrontendLog(RETRO_LOG_INFO,
                    "[Region] region=NTSC fps=59.94 (GameDB '%s')",
                    region.c_str());
                return {kNtsc, kNtscFps};
            }
            // GameDB entry exists but region field is empty — fall through.
        }
    }

    // Tier 2: prefix heuristic. Log only when we actually have a serial
    // to talk about — otherwise the line just emits "... on ''" and pairs
    // awkwardly with the Tier-3 WARN below.
    const DetectedRegion by_prefix = DetectRegionFromSerialPrefix(serial);
    if (!serial.empty())
    {
        FrontendLog(RETRO_LOG_INFO,
            "[Region] region=%s fps=%.2f (prefix heuristic on '%s')",
            by_prefix.libretro_region == kPal ? "PAL" : "NTSC",
            by_prefix.fps, serial.c_str());
    }

    // Tier 3: WARN for anything the prefix heuristic didn't recognise.
    // The heuristic always returns SOMETHING (default NTSC), so this is
    // purely diagnostic. Empty serials and 1-3 char serials are also
    // caught here because IsKnownSerialPrefix returns false for them.
    if (!IsKnownSerialPrefix(serial))
    {
        FrontendLog(RETRO_LOG_WARN,
            "[Region] Unknown disc serial '%s' — defaulting to NTSC", serial.c_str());
    }

    return by_prefix;
}

std::optional<DetectedRegion> RegionFromGsVideoMode(GS_VideoMode mode)
{
    switch (mode)
    {
    case GS_VideoMode::Uninitialized:
    case GS_VideoMode::Unknown:
        // Unknown is the game-programmed-but-PCSX2-doesn't-recognise case.
        // Treat it like Uninitialized: no usable answer; keep the
        // serial-based guess and let retro_run check again next frame.
        return std::nullopt;

    case GS_VideoMode::PAL:
    case GS_VideoMode::DVD_PAL:
    case GS_VideoMode::SDTV_576P:
        return DetectedRegion{kPal, kPalFps};

    case GS_VideoMode::NTSC:
    case GS_VideoMode::DVD_NTSC:
    case GS_VideoMode::SDTV_480P:
    case GS_VideoMode::HDTV_720P:
    case GS_VideoMode::HDTV_1080I:
    case GS_VideoMode::HDTV_1080P:
    case GS_VideoMode::VESA:
        return DetectedRegion{kNtsc, kNtscFps};
    }
    // Unreachable — the enum is exhaustive in pcsx2/GS.h.
    return DetectedRegion{kNtsc, kNtscFps};
}
#endif

DetectedRegion DetectRegionFromSerialPrefix(const std::string& serial)
{
    // Prefix classification: Pal / Ntsc / Unknown via the shared
    // ClassifyPrefix helper in the anonymous namespace above. Unknown
    // falls through to NTSC default; caller (DetectRegionFromSerial) is
    // responsible for logging a WARN. This function stays pure for the
    // standalone unit-test build (-DSP7A_TEST_PREFIX_ONLY).
    switch (ClassifyPrefix(serial))
    {
        case PrefixClass::Pal:     return {kPal,  kPalFps};
        case PrefixClass::Ntsc:    return {kNtsc, kNtscFps};
        case PrefixClass::Unknown: return {kNtsc, kNtscFps};
    }
    // Unreachable — enum is exhaustive.
    return {kNtsc, kNtscFps};
}

} // namespace Pcsx2Libretro::CoreResources
