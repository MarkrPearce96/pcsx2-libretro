// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+

#include "PrecompiledHeader.h"

#include "Settings.h"
#include "LibretroFrontend.h"

#include "common/Error.h"
#include "common/FileSystem.h"
#include "common/MemorySettingsInterface.h"
#include "pcsx2/Host.h"
#include "pcsx2/ImGui/ImGuiManager.h"
#include "pcsx2/VMManager.h"

#include "libretro.h"

#include <vector>

namespace Pcsx2Libretro::Settings
{
namespace
{
    MemorySettingsInterface g_si;
    bool g_initialized = false;

    // Lives for the process lifetime. ImGuiManager::FontInfo holds a
    // std::span<const u8> into this buffer, and ImGui's font atlas is
    // configured with FontDataOwnedByAtlas=false (see ImGuiManager::AddTextFont),
    // so this vector must outlive the libretro core's ImGui usage. We never
    // free it — same approach gsrunner uses (it just leaks until process exit).
    std::vector<u8> g_roboto_font_bytes;
}

// Mirror of pcsx2-gsrunner/Main.cpp's InitializeConfig font-setup block.
// Without this, ImGuiManager::AddTextFont returns nullptr (s_font_info is
// empty) and AddImGuiFonts → "Failed to create ImGui font texture" →
// VMManager::Initialize fails with "Failed to initialize GS." Loads
// Roboto-Regular.ttf from EmuFolders::Resources and registers it as the
// sole standard text font.
static bool InitializeImGuiFonts()
{
    if (!g_roboto_font_bytes.empty())
        return true; // already loaded by a prior call

    const std::string roboto_path =
        EmuFolders::GetOverridableResourcePath(
            "fonts" FS_OSPATH_SEPARATOR_STR "Roboto-Regular.ttf");

    auto data = FileSystem::ReadBinaryFile(roboto_path.c_str());
    if (!data.has_value())
    {
        FrontendLog(RETRO_LOG_ERROR,
            "Failed to load Roboto-Regular.ttf from '%s' — ImGui init will fail.",
            roboto_path.c_str());
        return false;
    }

    g_roboto_font_bytes = std::move(data.value());

    std::vector<ImGuiManager::FontInfo> fonts;
    ImGuiManager::FontInfo fi{};
    fi.data = std::span<const u8>(g_roboto_font_bytes);
    fi.exclude_ranges = {};
    fi.face_name = nullptr;
    fi.is_emoji_font = false;
    fonts.push_back(fi);

    ImGuiManager::SetFonts(std::move(fonts));
    FrontendLog(RETRO_LOG_INFO, "ImGui fonts registered (%zu bytes from %s)",
                g_roboto_font_bytes.size(), roboto_path.c_str());
    return true;
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

    // SP3: SetResourcesDirectory() on macOS uses CocoaTools::GetResourcePath()
    // which returns the running app bundle's Resources dir (RetroNest's).
    // RetroNest's bundle doesn't have PCSX2's metallibs / patches.zip /
    // gamedb.yaml. Override directly to pcsx2-master's bin/resources/.
    //
    // TODO: hardcoded absolute path for SP3 MVP. SP7 (settings) should
    // derive this from dladdr() on our dylib + a known relative offset,
    // or have RetroNest copy these resources into a location adjacent
    // to the dylib at install time.
    EmuFolders::Resources =
        "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master/bin/resources";
    {
        Error err;
        if (!EmuFolders::SetDataDirectory(&err))
        {
            FrontendLog(RETRO_LOG_WARN,
                "EmuFolders::SetDataDirectory failed: %s — continuing anyway",
                err.GetDescription().c_str());
        }
    }

    // Load Roboto-Regular.ttf and hand it to ImGuiManager. Must run after
    // EmuFolders::Resources is finalised (above) and before VMManager init
    // (which downstream triggers ImGuiManager::Initialize → AddImGuiFonts).
    // If this fails, GS init will still try and fail with the documented
    // "Failed to create ImGui font texture" error.
    if (!InitializeImGuiFonts())
    {
        FrontendLog(RETRO_LOG_ERROR,
            "ImGui font loading failed — VM init will fail at GS device creation");
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

    // SP3: point Folders/Resources at pcsx2-master's bin/resources/ so
    // GSDeviceMTL can load Metal22.metallib / Metal23.metallib / default.metallib
    // and PCSX2 can load patches.zip / gamedb.yaml / etc. Without this,
    // GSDeviceMTL::Create silently fails (m_dev.IsOk() returns false
    // when the shader library doesn't load) and AcquireWindow is never
    // called.
    //
    // TODO: this is a hardcoded absolute path for SP3 MVP. SP7 (settings)
    // should derive this from a runtime path (e.g. dladdr() on this dylib
    // to find its on-disk location, then a known relative offset), or
    // have RetroNest copy these resources next to the dylib at install time.
    g_si.SetStringValue("Folders", "Resources",
        "/Users/mark/Documents/Projects/Pcsx2 Experiment /pcsx2-master/bin/resources");

    // SP3: switched from Null (11) to Auto (-1). The Null renderer was
    // appropriate in SP2 when we had no display surface. SP3 provides a
    // real CAMetalLayer via Pattern B, so PCSX2 should actually render.
    // Auto resolves to Metal on macOS via GSUtil::GetPreferredRenderer().
    // GSRendererType::Auto == -1 in pcsx2/Config.h.
    g_si.SetIntValue("EmuCore/GS", "Renderer", static_cast<int>(-1));

    // Disable hardware audio output — SPU2 still initializes but discards.
    g_si.SetStringValue("SPU2/Output", "OutputModule", "nullout");

    // Disable achievements (avoid network init during boot).
    g_si.SetBoolValue("Achievements", "Enabled", false);

    // Fast boot — skip BIOS region check screen.
    g_si.SetBoolValue("EmuCore", "EnableFastBoot", true);

    // Disable HostFS (we don't expose host filesystem to the VM).
    g_si.SetBoolValue("EmuCore", "HostFs", false);

    // Enable system console + verbose logging so PCSX2's Console.WriteLn /
    // Console.Error output reaches our stderr (captured by RetroNest's log
    // file). Without this, internal PCSX2 diagnostics during GS device
    // creation / VM boot are invisible, making it impossible to diagnose
    // failures. Matches gsrunner's pattern (gsrunner/Main.cpp:889-891).
    g_si.SetBoolValue("Logging", "EnableSystemConsole", true);
    g_si.SetBoolValue("Logging", "EnableTimestamps", false);
    g_si.SetBoolValue("Logging", "EnableVerbose", true);

    // Disable input sources — SDL/XInput init during LoadSettings hangs
    // when there's no real controller subsystem to attach to. SP5 (input)
    // re-enables and wires retro_input_state_t to the PAD plugin.
    // This matches gsrunner's pattern (gsrunner/Main.cpp:877-879).
    g_si.SetBoolValue("InputSources", "SDL", false);
    g_si.SetBoolValue("InputSources", "XInput", false);
    g_si.SetBoolValue("InputSources", "DInput", false);
    g_si.SetBoolValue("InputSources", "RawInput", false);

    // Disable memory cards (SP6 will wire these properly). Avoids file
    // sharing violations and unnecessary I/O during VM boot.
    for (int i = 0; i < 2; ++i)
    {
        const std::string enable_key = "Slot" + std::to_string(i + 1) + "_Enable";
        const std::string file_key   = "Slot" + std::to_string(i + 1) + "_Filename";
        g_si.SetBoolValue("MemoryCards", enable_key.c_str(), false);
        g_si.SetStringValue("MemoryCards", file_key.c_str(), "");
    }

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
