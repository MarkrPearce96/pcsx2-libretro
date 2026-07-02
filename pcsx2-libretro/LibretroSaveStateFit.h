// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// pcsx2-libretro — pure fit check for Serialize()'s overflow guard.
//
// Split out of LibretroSaveState.cpp so the decision "does this
// measured zip fit the caller's buffer, and how must it be laid out"
// is unit-testable without linking PCSX2 (see
// tools/test_savestate_fit.cpp). Header-only, no dependencies beyond
// <cstddef> — LibretroSaveState.h pulls in pcsx2/VMManager.h, which
// standalone tests can't compile.

#pragma once

#include <cstddef>

namespace Pcsx2Libretro
{

// User-facing OSD text for the overflow failure. Kept next to the fit
// classifier (the only producer of SerializeFit::Overflow) so the
// message and the condition it describes stay in one place.
inline constexpr const char* kSerializeOverflowOsdMessage =
    "Save state failed: state grew beyond expected size. Try again.";

// How a measured zip relates to the caller-provided buffer. The buffer
// format is real_zip || zeros || sentinel(magic+size) — see
// LibretroSaveState.h.
enum class SerializeFit
{
    // produced == buffer: copy verbatim. No pad, therefore no sentinel
    // (readers locate the zip end via the EOCD record directly).
    ExactFit,

    // produced < buffer AND the tail gap holds at least a full
    // sentinel: copy, zero-fill, write sentinel. The normal path —
    // SerializeSize() reports probe + 8 MB headroom.
    PaddedFit,

    // Buffer can't represent the state. Two sub-cases collapse here:
    //   a) produced > buffer — the classic overflow (state grew past
    //      the probe-once size + headroom between calls).
    //   b) produced < buffer but the gap is smaller than the sentinel
    //      (16 bytes). Writing the sentinel at buffer-end would then
    //      overwrite the zip's own tail bytes, corrupting the state —
    //      and skipping the sentinel would silently produce the legacy
    //      pad-without-sentinel layout. With 8 MB of headroom this
    //      window is vanishingly small; failing loudly is safer than
    //      either corruption mode.
    Overflow,
};

constexpr SerializeFit ClassifySerializeFit(size_t produced_bytes,
                                            size_t buffer_len,
                                            size_t sentinel_size)
{
    if (produced_bytes == buffer_len)
        return SerializeFit::ExactFit;
    if (produced_bytes < buffer_len &&
        buffer_len - produced_bytes >= sentinel_size)
        return SerializeFit::PaddedFit;
    return SerializeFit::Overflow;
}

} // namespace Pcsx2Libretro
