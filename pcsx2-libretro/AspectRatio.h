// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// pcsx2-libretro — translates PCSX2's display aspect to a libretro float.
//
// Mirrors GSRenderer's GetCurrentAspectRatioFloat (which is file-static
// upstream). Reads EmuConfig.GS.AspectRatio, EmuConfig.CurrentCustomAspectRatio,
// and gsVideoMode (for the Auto branch's progressive detection).
//
// Stretch returns 4.0f/3.0f in v1 — RetroNest's display item treats
// aspect_ratio <= 0 as a fallback to 4:3, not as "fill". Stretch-as-fill
// is delivered via RetroNest's per-emulator aspect mode, not this option.
// See spec 2026-05-18-pcsx2-libretro-aspect-ratio-design.md for rationale.

#pragma once

namespace Pcsx2Libretro::AspectRatio
{
    // Pure function — testable without PCSX2 globals.
    // aspect_ratio_enum: cast of AspectRatioType (Config.h:224).
    // custom_override:   value of EmuConfig.CurrentCustomAspectRatio; > 0.f
    //                    only when widescreen patches active under Auto.
    // video_mode_enum:   cast of GS_VideoMode (GS.h:215); used for
    //                    progressive detection in the Auto branch.
    float ComputeFromInputs(int aspect_ratio_enum, float custom_override, int video_mode_enum);

    // Reads the three PCSX2 globals and calls ComputeFromInputs.
    // Call from retro_get_system_av_info / retro_run only.
    // Gated out of standalone-test builds (SP_ASPECT_TEST_ONLY); the test
    // exercises ComputeFromInputs directly and never links the production
    // .cpp's globals-reading wrapper.
#ifndef SP_ASPECT_TEST_ONLY
    float Compute();
#endif
}
