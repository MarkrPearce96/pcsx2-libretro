// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// Standalone unit test for AspectRatio::ComputeFromInputs.
// Pure function — no PCSX2 link required.
//
//   clang++ -std=c++20 -I../ test_aspect_ratio.cpp \
//       ../AspectRatio.cpp -o test_aspect_ratio \
//       -DSP_ASPECT_TEST_ONLY
//   ./test_aspect_ratio

#include "../AspectRatio.h"

#include <cmath>
#include <cstdio>

using Pcsx2Libretro::AspectRatio::ComputeFromInputs;

// Enum values mirror AspectRatioType (Config.h:224) and
// GS_VideoMode (GS.h:215). Hard-coded here to avoid pulling PCSX2 headers.
constexpr int AR_STRETCH    = 0;
constexpr int AR_AUTO       = 1;
constexpr int AR_4_3        = 2;
constexpr int AR_16_9       = 3;
constexpr int AR_10_7       = 4;

constexpr int VM_UNINITIALIZED = 0;
constexpr int VM_NTSC          = 2;
constexpr int VM_SDTV_480P     = 5;
constexpr int VM_HDTV_1080I    = 8;

static int failures = 0;

static void check(const char* label, float got, float want)
{
    const bool ok = std::fabs(got - want) < 0.001f;
    std::printf("[%s] %s: got=%.4f want=%.4f\n",
                ok ? "PASS" : "FAIL", label, got, want);
    if (!ok) ++failures;
}

int main()
{
    // Fixed enum values — video mode and custom override irrelevant.
    check("4:3",            ComputeFromInputs(AR_4_3,     0.0f, VM_NTSC),       4.0f / 3.0f);
    check("16:9",           ComputeFromInputs(AR_16_9,    0.0f, VM_NTSC),       16.0f / 9.0f);
    check("10:7",           ComputeFromInputs(AR_10_7,    0.0f, VM_NTSC),       10.0f / 7.0f);

    // Stretch: 0.0 sentinel = "no aspect specified" (RetroNest fills the
    // display item edge-to-edge — see Stretch flow-through spec).
    check("Stretch → 0.0",  ComputeFromInputs(AR_STRETCH, 0.0f, VM_NTSC),       0.0f);
    check("Stretch ignores custom", ComputeFromInputs(AR_STRETCH, 1.777f, VM_NTSC), 0.0f);
    // Stretch deliberately diverges from upstream here — GSRenderer's
    // GetCurrentAspectRatioFloat would fall through to the Auto branch
    // and return 3:2 for progressive. We emit the 0.0 sentinel regardless
    // of video mode so the frontend's fill semantics kick in.
    check("Stretch + SDTV_480P stays 0.0",
                                     ComputeFromInputs(AR_STRETCH, 0.0f, VM_SDTV_480P),    0.0f);

    // Auto branch — no patch override, interlaced → 4:3.
    check("Auto NTSC interlaced",   ComputeFromInputs(AR_AUTO, 0.0f, VM_NTSC),       4.0f / 3.0f);
    check("Auto HDTV 1080I",        ComputeFromInputs(AR_AUTO, 0.0f, VM_HDTV_1080I), 4.0f / 3.0f);
    check("Auto Uninitialized",     ComputeFromInputs(AR_AUTO, 0.0f, VM_UNINITIALIZED), 4.0f / 3.0f);

    // Auto branch — progressive (SDTV_480P) → 3:2.
    check("Auto SDTV 480P → 3:2",   ComputeFromInputs(AR_AUTO, 0.0f, VM_SDTV_480P),  3.0f / 2.0f);

    // Auto branch — widescreen-patch override wins regardless of video mode.
    check("Auto + patch 16:9",      ComputeFromInputs(AR_AUTO, 16.0f / 9.0f, VM_NTSC),       16.0f / 9.0f);
    check("Auto + patch 21:9",      ComputeFromInputs(AR_AUTO, 21.0f / 9.0f, VM_SDTV_480P),  21.0f / 9.0f);
    check("Auto + patch overrides interlaced default",
                                     ComputeFromInputs(AR_AUTO, 16.0f / 9.0f, VM_HDTV_1080I), 16.0f / 9.0f);

    // Unknown enum value (out-of-range) → falls into default branch (treated as Auto).
    check("Unknown enum → Auto default", ComputeFromInputs(99, 0.0f, VM_NTSC),   4.0f / 3.0f);

    std::printf("\n%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
