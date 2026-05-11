// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+

#include "PrecompiledHeader.h"

#include "Settings.h"
#include "LibretroFrontend.h"

#include "common/MemorySettingsInterface.h"
#include "pcsx2/Host.h"
#include "pcsx2/VMManager.h"

#include "libretro.h"

namespace Pcsx2Libretro::Settings
{
namespace
{
    MemorySettingsInterface g_si;
    bool g_initialized = false;
}

void InitializeDefaults(const std::string& system_dir)
{
    if (g_initialized)
    {
        FrontendLog(RETRO_LOG_WARN, "Settings::InitializeDefaults called twice — ignoring");
        return;
    }

    // Register our MemorySettingsInterface as PCSX2's base settings layer
    // BEFORE asking VMManager to fill it with defaults — SetDefaultSettings
    // dispatches through the active layer.
    Host::Internal::SetBaseSettingsLayer(&g_si);

    // Populate with PCSX2's full defaults (folders, GS, SPU2, achievements,
    // EmuCore — every section). Same call gsrunner uses (gsrunner/Main.cpp:152).
    VMManager::SetDefaultSettings(g_si, true, true, true, true, true);

    // Now override the SP2-required minimums.
    g_si.SetStringValue("Folders", "Bios", system_dir.c_str());

    // Force software GS renderer (no native window required).
    // GSRendererType::SW == 13 in pcsx2/Config.h; we use the string the
    // SettingsWrapper will parse back to that enum value.
    g_si.SetIntValue("EmuCore/GS", "Renderer", static_cast<int>(13));

    // Disable hardware audio output — SPU2 still initializes but discards.
    g_si.SetStringValue("SPU2/Output", "OutputModule", "nullout");

    // Disable achievements (avoid network init during boot).
    g_si.SetBoolValue("Achievements", "Enabled", false);

    // Fast boot — skip BIOS region check screen.
    g_si.SetBoolValue("EmuCore", "EnableFastBoot", true);

    // Disable HostFS (we don't expose host filesystem to the VM).
    g_si.SetBoolValue("EmuCore", "HostFs", false);

    // Apply the layered settings to the live Pcsx2Config.
    VMManager::Internal::LoadStartupSettings();

    g_initialized = true;

    FrontendLog(RETRO_LOG_INFO, "Settings::InitializeDefaults complete (BIOS dir = %s)",
                system_dir.c_str());
}

MemorySettingsInterface* GetActiveInterface()
{
    return g_initialized ? &g_si : nullptr;
}

} // namespace Pcsx2Libretro::Settings
