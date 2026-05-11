// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+

#include "PrecompiledHeader.h"

#include "Settings.h"
#include "LibretroFrontend.h"

#include "common/Error.h"
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

    // Step 1: Initialize EmuFolders (AppRoot + DataRoot + Resources).
    // These must be set before SetDefaultSettings or LoadStartupSettings
    // because both call EmuFolders functions that need DataRoot to be valid.
    // Without SetAppRoot(), DataRoot resolves as "/" on macOS → wrong paths.
    EmuFolders::SetAppRoot();
    // SetResourcesDirectory() sets EmuFolders::Resources, which the Metal GS
    // device needs to load Metal23/Metal22/default.metallib.  It returns false
    // if the directory doesn't exist, but we continue regardless.
    EmuFolders::SetResourcesDirectory();
    {
        Error err;
        if (!EmuFolders::SetDataDirectory(&err))
        {
            FrontendLog(RETRO_LOG_WARN,
                "EmuFolders::SetDataDirectory failed: %s — continuing anyway",
                err.GetDescription().c_str());
        }
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

    // Force null GS renderer — does not require a display window or GPU.
    // GSRendererType::Null == 11 in pcsx2/Config.h. SW (13) requires a
    // real render device and crashes without a window; Null is the correct
    // headless choice for test_loader and libretro bootstrap.
    g_si.SetIntValue("EmuCore/GS", "Renderer", static_cast<int>(11));

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
