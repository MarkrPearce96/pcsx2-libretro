// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// Standalone unit test for DetectRegionFromSerialPrefix.
// Not built as part of pcsx2_libretro target — manual compile, no PCSX2
// link required (mirrors the test_loader.c pattern).
//
//   clang++ -std=c++20 -I../ test_region_prefix.cpp \
//       ../CoreResources.cpp -o test_region_prefix \
//       -DSP7A_TEST_PREFIX_ONLY
//   ./test_region_prefix
//
// SP7A_TEST_PREFIX_ONLY gates CoreResources.cpp's PCSX2-dependent
// includes (GameDatabase, GS) so the test compiles without a PCSX2
// build tree.

#include "../CoreResources.h"

#include <cstdio>
#include <string>

using Pcsx2Libretro::CoreResources::DetectRegionFromSerialPrefix;
using Pcsx2Libretro::CoreResources::DetectedRegion;

// RETRO_REGION_NTSC = 0, RETRO_REGION_PAL = 1 (per libretro.h:450-451).
// Hard-coded here to avoid pulling libretro.h into the standalone test.
constexpr unsigned NTSC = 0;
constexpr unsigned PAL  = 1;

static int failures = 0;
static void check(const char* label, const DetectedRegion& got,
                  unsigned want_region, double want_fps)
{
    const bool ok = (got.libretro_region == want_region)
                 && (got.fps == want_fps);
    std::printf("[%s] %s: got region=%u fps=%.2f, want region=%u fps=%.2f\n",
                ok ? "PASS" : "FAIL", label,
                got.libretro_region, got.fps, want_region, want_fps);
    if (!ok) ++failures;
}

int main()
{
    // US NTSC (R&C 2's actual serial)
    check("SCUS US NTSC", DetectRegionFromSerialPrefix("SCUS-97268"), NTSC, 59.94);
    check("SLUS US NTSC", DetectRegionFromSerialPrefix("SLUS-21134"), NTSC, 59.94);

    // PAL territories
    check("SLES PAL", DetectRegionFromSerialPrefix("SLES-50001"), PAL, 50.0);
    check("SCES PAL", DetectRegionFromSerialPrefix("SCES-50001"), PAL, 50.0);
    check("SCED PAL", DetectRegionFromSerialPrefix("SCED-50001"), PAL, 50.0);
    check("SLED PAL", DetectRegionFromSerialPrefix("SLED-50001"), PAL, 50.0);

    // Japan / Asia NTSC
    check("SLPS JP NTSC", DetectRegionFromSerialPrefix("SLPS-25001"), NTSC, 59.94);
    check("SCAJ JP NTSC", DetectRegionFromSerialPrefix("SCAJ-20001"), NTSC, 59.94);
    check("SLPM JP NTSC", DetectRegionFromSerialPrefix("SLPM-65001"), NTSC, 59.94);

    // Unknown prefix → default NTSC
    check("Unknown prefix", DetectRegionFromSerialPrefix("ZZZZ-12345"), NTSC, 59.94);

    // Empty string → default NTSC
    check("Empty string",  DetectRegionFromSerialPrefix(""),            NTSC, 59.94);

    // Too-short string → default NTSC
    check("Short string",  DetectRegionFromSerialPrefix("SL"),          NTSC, 59.94);

    std::printf("\n%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
