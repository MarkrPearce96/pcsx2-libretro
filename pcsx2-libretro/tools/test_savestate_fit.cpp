// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// Standalone unit test for Pcsx2Libretro::ClassifySerializeFit — the
// pure fit check behind Serialize()'s overflow guard.
// Header-only seam, no PCSX2 link required.
//
//   clang++ -std=c++20 -I../ test_savestate_fit.cpp -o test_savestate_fit
//   ./test_savestate_fit

#include "../LibretroSaveStateFit.h"

#include <cstdio>
#include <cstring>

using Pcsx2Libretro::ClassifySerializeFit;
using Pcsx2Libretro::SerializeFit;
using Pcsx2Libretro::kSerializeOverflowOsdMessage;

// Mirrors kSentinelSize in LibretroSaveState.cpp: 8 magic bytes
// ("PCSXSIZE") + uint64_t actual zip size. Hard-coded here because the
// real constant lives in an anonymous namespace next to the writer.
constexpr size_t kSentinel = 16;

static int failures = 0;

static const char* Name(SerializeFit f)
{
    switch (f)
    {
    case SerializeFit::ExactFit:  return "ExactFit";
    case SerializeFit::PaddedFit: return "PaddedFit";
    case SerializeFit::Overflow:  return "Overflow";
    }
    return "?";
}

static void check(const char* label, SerializeFit got, SerializeFit want)
{
    const bool ok = (got == want);
    std::printf("[%s] %s: got=%s want=%s\n",
                ok ? "PASS" : "FAIL", label, Name(got), Name(want));
    if (!ok) ++failures;
}

int main()
{
    // The normal path: SerializeSize() reports probe + 8 MB headroom,
    // so produced sits well below the buffer with room for the sentinel.
    check("headroom pad fits",
          ClassifySerializeFit(48'361'703, 48'361'703 + 8 * 1024 * 1024, kSentinel),
          SerializeFit::PaddedFit);

    // The field-observed failure (Looney Tunes Space Race PAL): state
    // grew +178 KB past the probe between SerializeSize and Serialize.
    // With the caller's buffer sized from a stale probe WITHOUT the
    // headroom, that's a classic overflow.
    check("grown state overflows stale buffer",
          ClassifySerializeFit(48'543'463, 48'361'703, kSentinel),
          SerializeFit::Overflow);

    // Exact fit: no pad, no sentinel — readers use the zip EOCD directly.
    check("exact fit",
          ClassifySerializeFit(1000, 1000, kSentinel),
          SerializeFit::ExactFit);

    // One byte over is already an overflow.
    check("one byte over",
          ClassifySerializeFit(1001, 1000, kSentinel),
          SerializeFit::Overflow);

    // Boundary: gap of exactly one sentinel still pads cleanly
    // (zero bytes of zero-fill between zip and sentinel is legal).
    check("gap == sentinel pads",
          ClassifySerializeFit(1000 - kSentinel, 1000, kSentinel),
          SerializeFit::PaddedFit);

    // The corruption window: produced < buffer but the gap can't hold
    // a full sentinel. Writing the sentinel would clobber the zip's
    // tail; skipping it would emit the legacy no-sentinel layout.
    // Must classify as Overflow (fail loudly), not a fit.
    check("gap == sentinel-1 overflows",
          ClassifySerializeFit(1000 - kSentinel + 1, 1000, kSentinel),
          SerializeFit::Overflow);
    check("gap == 1 overflows",
          ClassifySerializeFit(999, 1000, kSentinel),
          SerializeFit::Overflow);

    // Degenerate buffers.
    check("zero-length buffer overflows",
          ClassifySerializeFit(1, 0, kSentinel),
          SerializeFit::Overflow);
    check("zero produced into zero buffer is exact",
          ClassifySerializeFit(0, 0, kSentinel),
          SerializeFit::ExactFit);

    // The OSD message the Overflow branch emits: non-empty and
    // actionable (tells the user to retry).
    {
        const bool ok = kSerializeOverflowOsdMessage != nullptr &&
                        std::strlen(kSerializeOverflowOsdMessage) > 0 &&
                        std::strstr(kSerializeOverflowOsdMessage, "Try again") != nullptr;
        std::printf("[%s] overflow OSD message is actionable: \"%s\"\n",
                    ok ? "PASS" : "FAIL", kSerializeOverflowOsdMessage);
        if (!ok) ++failures;
    }

    if (failures == 0)
    {
        std::printf("All tests passed.\n");
        return 0;
    }
    std::printf("%d test(s) FAILED.\n", failures);
    return 1;
}
