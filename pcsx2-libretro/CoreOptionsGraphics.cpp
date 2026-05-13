// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+

#include "CoreOptionsGraphics.h"

#ifdef CORE_OPTIONS_TEST_ONLY
#include <cstdarg>
#include <cstdio>
static void FrontendLog(int /*level*/, const char* fmt, ...)
{
    std::va_list ap;
    va_start(ap, fmt);
    std::vfprintf(stderr, fmt, ap);
    std::fputc('\n', stderr);
    va_end(ap);
}
#else
#include "LibretroFrontend.h"                  // FrontendLog
#include "common/MemorySettingsInterface.h"    // MemorySettingsInterface
#endif

#include <cstring>

namespace Pcsx2Libretro::CoreOptions::Graphics
{

void AppendDefinitions(std::vector<retro_core_option_v2_definition>& /*out*/)
{
    // SP7c Phase 4 — per-sub-tab tasks populate this:
    //   Task 2: Display sub-tab (~17 knobs)
    //   Task 3: Rendering sub-tab (~7 knobs)
    //   Task 4: Texture Replacement sub-tab (~6 knobs)
    //   Task 5: Post-Processing sub-tab (~9 knobs)
    //   Task 6: On-Screen Display sub-tab (~23 knobs)
    //
    // Each entry is a literal out.push_back({...}) block (no lambda
    // helper) so tools/check_schema_fidelity.py's CORE_BLOCK_RE
    // recognizes them.
}

void Parse(retro_environment_t /*cb*/, Values& /*out*/)
{
    // SP7c Phase 4 — per-sub-tab tasks fill this with one branch per knob.
    // Mirror MemoryCards::Parse for bools and Emulation::Parse for combos.
}

#ifndef CORE_OPTIONS_TEST_ONLY
void ApplyDefaults(MemorySettingsInterface& /*si*/, const Values& /*v*/)
{
    // SP7c Phase 4 — per-sub-tab tasks fill this with the matching
    // si.Set{Bool,Int,Float,String}Value calls per knob. Most Graphics
    // keys live under PCSX2's EmuCore/GS INI section; OSD knobs and a
    // few Display-side patches live under EmuCore.
}
#endif

} // namespace Pcsx2Libretro::CoreOptions::Graphics
