// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+

#include "AspectRatio.h"

#ifndef SP_ASPECT_TEST_ONLY
#include "pcsx2/Config.h"   // EmuConfig, AspectRatioType
#include "pcsx2/GS.h"       // gsVideoMode, GS_VideoMode
#endif

namespace Pcsx2Libretro::AspectRatio
{

namespace {
    // Mirrors AspectRatioType (Config.h:224). Duplicated as plain int values
    // so ComputeFromInputs has no dependency on the PCSX2 enum at compile time.
    constexpr int kStretch       = 0;
    constexpr int kAuto4_3_3_2   = 1;
    constexpr int kR4_3          = 2;
    constexpr int kR16_9         = 3;
    constexpr int kR10_7         = 4;

    // GS_VideoMode (GS.h:215). Only SDTV_480P is treated as progressive
    // by GSRenderer's GetCurrentAspectRatioFloat — match that behavior.
    constexpr int kVideoModeSDTV_480P = 5;
}

float ComputeFromInputs(int aspect_ratio_enum, float custom_override, int video_mode_enum)
{
    switch (aspect_ratio_enum)
    {
        case kR4_3:    return 4.0f / 3.0f;
        case kR16_9:   return 16.0f / 9.0f;
        case kR10_7:   return 10.0f / 7.0f;

        case kStretch:
            // See spec: RetroNest's aspect_ratio <= 0 fallback is 4:3, not
            // fill. Stretch-as-fill is delivered via RetroNest's per-emulator
            // aspect mode in v1; the libretro Stretch option is a no-op.
            return 4.0f / 3.0f;

        case kAuto4_3_3_2:
        default:
            // Widescreen patches override (Patch.cpp:825 sets this when AR=Auto).
            if (custom_override > 0.0f)
                return custom_override;
            // Only SDTV_480P counts as progressive in PCSX2's mapping.
            if (video_mode_enum == kVideoModeSDTV_480P)
                return 3.0f / 2.0f;
            return 4.0f / 3.0f;
    }
}

#ifndef SP_ASPECT_TEST_ONLY
float Compute()
{
    return ComputeFromInputs(
        static_cast<int>(EmuConfig.GS.AspectRatio),
        EmuConfig.CurrentCustomAspectRatio,
        static_cast<int>(gsVideoMode));
}
#endif

} // namespace Pcsx2Libretro::AspectRatio
