// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+

#include "CoreOptionsMemoryCards.h"

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

namespace Pcsx2Libretro::CoreOptions::MemoryCards
{

void AppendDefinitions(std::vector<retro_core_option_v2_definition>& out)
{
    // SP7c Phase 3 — Memory Cards card (5 knobs under MemoryCards).
    //
    // All 5 are independent bool toggles. Slot{1,2}_Enable govern whether
    // PCSX2 mounts a virtual memory card in slots 1/2. Multitap1_Slot{2,3,4}
    // expose the additional memcard slots that become accessible when
    // Multitap 1 is connected — they are independent of the Multitap1
    // master enable (which lives in the unrelated Pcsx2 section and is
    // managed by the controllers settings, not the Memory Cards card).
    //
    // The 2 filename rows from the standalone dialog (Slot1_Filename,
    // Slot2_Filename) are dropped — libretro core options are Combo-only,
    // not free-form strings, so the filenames stay hardcoded in
    // Settings.cpp's one-shot init block.
    //
    // Each entry is a literal out.push_back({...}) block (no lambda
    // helper) so tools/check_schema_fidelity.py's CORE_BLOCK_RE
    // recognizes them.
    out.push_back({
        "pcsx2_mc_slot1_enable",
        "Memory Card Slot 1",
        nullptr,
        "Inserts a virtual memory card into Slot 1. The card image is "
        "stored as Mcd001.ps2 under the per-game memcards folder. "
        "Disabling this prevents games from saving/loading via Slot 1.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled" },
            { nullptr,    nullptr },
        },
        "enabled",
    });

    out.push_back({
        "pcsx2_mc_slot2_enable",
        "Memory Card Slot 2",
        nullptr,
        "Inserts a virtual memory card into Slot 2. The card image is "
        "stored as Mcd002.ps2 under the per-game memcards folder. "
        "PCSX2 only auto-creates the file the first time a game writes "
        "to Slot 2.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled" },
            { nullptr,    nullptr },
        },
        "enabled",
    });

    out.push_back({
        "pcsx2_mc_multitap1_slot2",
        "Multitap 1 - Slot 2",
        nullptr,
        "Enables the second memory-card slot of Multitap 1. Only useful "
        "when a game supports Multitap 1 and you need additional save "
        "slots for extra players.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    out.push_back({
        "pcsx2_mc_multitap1_slot3",
        "Multitap 1 - Slot 3",
        nullptr,
        "Enables the third memory-card slot of Multitap 1.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled" },
            { nullptr,    nullptr },
        },
        "disabled",
    });

    out.push_back({
        "pcsx2_mc_multitap1_slot4",
        "Multitap 1 - Slot 4",
        nullptr,
        "Enables the fourth memory-card slot of Multitap 1.",
        nullptr,
        nullptr,
        {
            { "enabled",  "Enabled" },
            { "disabled", "Disabled" },
            { nullptr,    nullptr },
        },
        "disabled",
    });
}

void Parse(retro_environment_t cb, Values& out)
{
    if (!cb) return;

    auto query = [&cb](const char* key) -> const char* {
        retro_variable var{};
        var.key = key;
        if (cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
            return var.value;
        return nullptr;
    };

    // 5 bool knobs. "enabled" → true, anything else → false. Mirrors the
    // single-line bool branches used in CoreOptionsAudio.cpp::Parse for
    // pcsx2_audio_muted (no helper lambda — at 5 callsites, inline is
    // shorter and matches Phase 2's precedent).
    if (const char* v = query("pcsx2_mc_slot1_enable"))
        out.slot1_enable = (std::strcmp(v, "enabled") == 0);
    if (const char* v = query("pcsx2_mc_slot2_enable"))
        out.slot2_enable = (std::strcmp(v, "enabled") == 0);
    if (const char* v = query("pcsx2_mc_multitap1_slot2"))
        out.multitap1_slot2 = (std::strcmp(v, "enabled") == 0);
    if (const char* v = query("pcsx2_mc_multitap1_slot3"))
        out.multitap1_slot3 = (std::strcmp(v, "enabled") == 0);
    if (const char* v = query("pcsx2_mc_multitap1_slot4"))
        out.multitap1_slot4 = (std::strcmp(v, "enabled") == 0);
}

#ifndef CORE_OPTIONS_TEST_ONLY
void ApplyDefaults(MemorySettingsInterface& si, const Values& v)
{
    // All 5 keys live under PCSX2's MemoryCards INI section.
    // Pcsx2Config::McdOptions::LoadSave (Pcsx2Config.cpp:2051-2065) reads
    // these via SettingsWrapEntry at VMManager refresh time —
    // LoadStartupSettings() runs after this in InitializeDefaults so the
    // new values take effect on this launch.
    //
    // The Slot{1,2}_Filename writes stay in Settings.cpp's one-shot block
    // (libretro Combo-only constraint — filenames are not user-tweakable).
    si.SetBoolValue("MemoryCards", "Slot1_Enable",           v.slot1_enable);
    si.SetBoolValue("MemoryCards", "Slot2_Enable",           v.slot2_enable);
    si.SetBoolValue("MemoryCards", "Multitap1_Slot2_Enable", v.multitap1_slot2);
    si.SetBoolValue("MemoryCards", "Multitap1_Slot3_Enable", v.multitap1_slot3);
    si.SetBoolValue("MemoryCards", "Multitap1_Slot4_Enable", v.multitap1_slot4);
}
#endif

} // namespace Pcsx2Libretro::CoreOptions::MemoryCards
