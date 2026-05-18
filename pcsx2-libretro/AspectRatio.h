// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// pcsx2-libretro — translates PCSX2's display aspect to a libretro float.
//
// Mirrors GSRenderer's GetCurrentAspectRatioFloat (which is file-static
// upstream). Reads GSConfig.AspectRatio, EmuConfig.CurrentCustomAspectRatio,
// and gsVideoMode (for the Auto branch's progressive detection).
//
// Stretch returns 0.0f — libretro's "no aspect specified" sentinel.
// Frontends that honor it (RetroNest as of 2026-05-19) fill the display
// item edge-to-edge, matching standalone PCSX2's Stretch semantics.
// Frontends that don't honor it fall back to their own default (usually
// 4:3). See spec 2026-05-19-retronest-libretro-stretch-flow-through-design.md.

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
