// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+

#include "PrecompiledHeader.h"

#include "Settings.h"
#include "CoreResources.h"
#include "CoreOptions.h"
#include "CoreOptionsEmulation.h"
#include "CoreOptionsAudio.h"
#include "CoreOptionsMemoryCards.h"
#include "CoreOptionsGraphics.h"
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

    // RetroNest-private env enums for path overrides. Number must match
    // RetroNest-Project's environment_callbacks.h exactly.
    //   0x20003 = GET_MEMCARDS_DIR
    //   0x20004 = GET_TEXTURES_DIR
    // (0x20002 is GET_BOOT_STATE_PATH; see LibretroFrontend.cpp.)
    // Each returns true with *data set to const char* when an override
    // is configured; returns false otherwise. We fall through to the
    // existing save_dir-based default when false comes back, so any
    // host that doesn't implement these env enums keeps the prior
    // behavior unchanged.
    constexpr unsigned RETRONEST_ENVIRONMENT_GET_MEMCARDS_DIR =
        (3u | RETRO_ENVIRONMENT_PRIVATE);
    constexpr unsigned RETRONEST_ENVIRONMENT_GET_TEXTURES_DIR =
        (4u | RETRO_ENVIRONMENT_PRIVATE);

    // Returns the override for `env_id` from the host, or empty string.
    // Pulled out for standalone unit testing — see tools/test_settings_overrides.cpp.
    std::string QueryPathOverride(retro_environment_t cb, unsigned env_id)
    {
        if (!cb) return {};
        const char* out = nullptr;
        if (cb(env_id, &out) && out && out[0] != '\0')
            return out;
        return {};
    }

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

// SP5: write hardcoded Pad1/Pad2 bindings into the MemorySettingsInterface.
// Each entry maps a PadDualshock2 action name (Cross, LUp, +L2, ...) to a
// LibretroInputSource binding string. The action names exactly match
// PadDualshock2.cpp's s_bindings table (verified during SP5 implementation
// plan step 1). Future SP7 makes these user-overridable.
static void WriteDefaultPadBindings(MemorySettingsInterface& si)
{
    struct Entry
    {
        const char* action;       // PadDualshock2 action name
        const char* libretro;     // LibretroInputSource binding name
    };
    static constexpr Entry kEntries[] = {
        // Digital
        {"Up",       "Up"},
        {"Right",    "Right"},
        {"Down",     "Down"},
        {"Left",     "Left"},
        {"Cross",    "Cross"},
        {"Circle",   "Circle"},
        {"Square",   "Square"},
        {"Triangle", "Triangle"},
        {"L1",       "L1"},
        {"R1",       "R1"},
        {"L2",       "+L2"},
        {"R2",       "+R2"},
        {"L3",       "L3"},
        {"R3",       "R3"},
        {"Start",    "Start"},
        {"Select",   "Select"},
        // Analog stick half-axes
        {"LUp",    "-LeftY"},
        {"LDown",  "+LeftY"},
        {"LLeft",  "-LeftX"},
        {"LRight", "+LeftX"},
        {"RUp",    "-RightY"},
        {"RDown",  "+RightY"},
        {"RLeft",  "-RightX"},
        {"RRight", "+RightX"},
        // SP5.5: rumble motors. PadDualshock2 action names "LargeMotor" and
        // "SmallMotor" — matches the convention XInput / SDL bindings use.
        {"LargeMotor", "LargeMotor"},
        {"SmallMotor", "SmallMotor"},
    };

    for (u32 port = 0; port < 2; ++port)
    {
        const std::string section = "Pad" + std::to_string(port + 1);
        // Ensure the controller type is DualShock 2; without this the
        // bindings above won't match the section's input-binding table.
        si.SetStringValue(section.c_str(), "Type", "DualShock2");
        for (const auto& e : kEntries)
        {
            const std::string value = "Libretro-" + std::to_string(port) + "/" + e.libretro;
            si.SetStringValue(section.c_str(), e.action, value.c_str());
        }
    }
}

void InitializeDefaults(const std::string& system_dir,
                        const std::string& save_dir,
                        const CoreOptions::Resolved* options)
{
    // SP7c Phase 1 followup: the one-shot block below (EmuFolders, fonts,
    // VMManager::SetDefaultSettings, libretro-required INI overrides, pad
    // bindings, memcards) runs exactly once per process — it pokes
    // process-singleton state and re-running would be incorrect (e.g.
    // SetDefaultSettings would clobber the user's accumulated tweaks). But
    // the user-resolvable Emulation::ApplyDefaults call + LoadStartupSettings
    // re-application MUST happen on every retro_load_game so dialog changes
    // take effect on the next launch without restarting RetroNest.
    if (!g_initialized)
    {
    // Step 1: Initialize EmuFolders (AppRoot + DataRoot + Resources).
    // These must be set before SetDefaultSettings or LoadStartupSettings
    // because both call EmuFolders functions that need DataRoot to be valid.
    // Without SetAppRoot(), DataRoot resolves as "/" on macOS → wrong paths.
    EmuFolders::SetAppRoot();
    // SetResourcesDirectory() sets EmuFolders::Resources, which the Metal GS
    // device needs to load Metal23/Metal22/default.metallib.  It returns false
    // if the directory doesn't exist, but we continue regardless.
    EmuFolders::SetResourcesDirectory();

    // SP7a: SetResourcesDirectory() on macOS uses CocoaTools::GetResourcePath()
    // which returns the running app bundle's Resources dir (RetroNest's).
    // RetroNest's bundle doesn't have PCSX2's metallibs / patches.zip /
    // gamedb.yaml. Resolve at runtime via dladdr → <dylib_dir>/pcsx2_libretro_resources/.
    // RetroNest's install layout must place the resources directory next
    // to the dylib (see SP7a plan Task 7 for the rsync step).
    const std::string resources_dir = Pcsx2Libretro::CoreResources::ResolveResourcesDir();
    if (!resources_dir.empty())
    {
        EmuFolders::Resources = resources_dir;
    }
    else
    {
        // dladdr or sibling-dir lookup failed; keep whatever SetResourcesDirectory()
        // set above (the running app bundle's Resources dir — wrong for our needs,
        // but a non-empty path gives Metal/font loads a chance to fail with a
        // specific "file not found" rather than a confusing empty-path error.)
        FrontendLog(RETRO_LOG_WARN,
            "[CoreResources] Resources path resolution failed; falling back to "
            "EmuFolders::SetResourcesDirectory() default '%s' — Metal/font/gamedb "
            "loads will likely fail with file-not-found errors.",
            EmuFolders::Resources.c_str());
    }
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

    // SP7a: point Folders/Resources at the dladdr-derived path resolved
    // above. GSDeviceMTL needs this to load Metal22/23/default.metallib;
    // PCSX2 also reads patches.zip / gamedb.yaml from here.
    g_si.SetStringValue("Folders", "Resources", resources_dir.c_str());

    // SP4: route SPU2 → retro_audio_sample_batch_t via LibretroAudioStream.
    // The Backend value is the AudioBackend name string matching s_backend_names
    // in pcsx2/Host/AudioStream.cpp; "Libretro" is the SP4 enum addition.
    // Section "SPU2/Output" matches Pcsx2Config.cpp:1258's SettingsWrapSection.
    // (The previous "OutputModule"/"nullout" line was a no-op — that key was
    // removed when PCSX2 migrated to the AudioStream/AudioBackend model, so
    // SPU2 was actually defaulting to AudioBackend::Cubeb. SP4 replaces it
    // with the correct, current API.)
    g_si.SetStringValue("SPU2/Output", "Backend", "Libretro");

    // Libretro's batch callback is stereo only. Forcing expansion off
    // avoids LibretroAudioStream's pxAssertRel(expansion_mode == Disabled).
    // Same section because StreamParameters::LoadSave uses CURRENT_SETTINGS_SECTION,
    // which is "SPU2/Output" here.
    g_si.SetStringValue("SPU2/Output", "ExpansionMode", "Disabled");

    // Disable achievements (avoid network init during boot).
    g_si.SetBoolValue("Achievements", "Enabled", false);

    // System console routes Console.WriteLn / Console.Error to stderr.
    // EnableVerbose was previously on to surface diagnostics during SP1-SP4
    // bring-up, but it forces LOGLEVEL_DEV which makes every DevCon.Warning()
    // in PCSX2's DMA/VIF/MTGS hot paths issue a blocking write(fd, ...) syscall
    // on the EE thread — measured ~10-20% emulation-speed regression. Keep
    // system console on for FATAL/ERROR/WARN routing; turn verbose off for perf.
    g_si.SetBoolValue("Logging", "EnableSystemConsole", true);
    g_si.SetBoolValue("Logging", "EnableTimestamps", false);
    g_si.SetBoolValue("Logging", "EnableVerbose", false);

    // SP5: keep upstream sources off (SDL/XInput/DInput init hangs in the
    // libretro process — no controller subsystem); enable our LibretroInputSource
    // instead. PAD bindings written below route Libretro-N/* to Pad{N+1}/*.
    g_si.SetBoolValue("InputSources", "SDL", false);
    g_si.SetBoolValue("InputSources", "XInput", false);
    g_si.SetBoolValue("InputSources", "DInput", false);
    g_si.SetBoolValue("InputSources", "RawInput", false);
    g_si.SetBoolValue("InputSources", "Libretro", true);

    WriteDefaultPadBindings(g_si);

    // SP6 / SP7c-Phase-3: configure memory cards.
    //
    // Filenames: hardcoded "Mcd001.ps2" / "Mcd002.ps2" (PCSX2 standard).
    //   PCSX2 auto-detects MemoryCardType::File from the .ps2 extension
    //   at MemoryCardFile.cpp load time and auto-creates the file on
    //   first write. Filenames are NOT user-tweakable — libretro core
    //   options are Combo-only, not free-form strings.
    //
    // Slot{1,2} enable + Multitap1 slot enables: now user-tweakable via
    //   CoreOptions::MemoryCards (per-call ApplyDefaults below). Defaults
    //   match the standalone PCSX2 dialog (both slots enabled, multitap
    //   slots disabled).
    //
    // Folders/MemoryCards: rooted at libretro save_dir so each
    //   {game_id}/ scope gets its own memcard image (RetroNest's
    //   adapter is responsible for the per-game scoping in save_dir).
    //   Empty save_dir falls back to PCSX2's default ("memcards"
    //   under DataRoot) — usable but not per-game-isolated.
    if (!save_dir.empty())
    {
        const std::string memcards_dir = save_dir + "/memcards";
        g_si.SetStringValue("Folders", "MemoryCards", memcards_dir.c_str());

        // SP7c followup: same per-game rooting for replacement textures.
        // PCSX2's GSTextureReplacements adds /<disc_serial>/ underneath
        // via Path::Combine, so users drop textures into
        //   <save_dir>/textures/<disc_serial>/...
        // PCSX2's default (Folders/Textures = "textures") resolves
        // relative to DataRoot, which on libretro is the dladdr-derived
        // resources dir — not a user-writable location. Rooting under
        // save_dir mirrors the memcards convention and gives users a
        // stable, writable, per-game spot to drop replacement textures.
        const std::string textures_dir = save_dir + "/textures";
        g_si.SetStringValue("Folders", "Textures", textures_dir.c_str());
    }
    else
    {
        FrontendLog(RETRO_LOG_WARN,
            "Host did not provide save_dir — memcards + texture replacements will use PCSX2 default locations");
    }

    // Path-override layer: if the host advertises the new env enums
    // (RetroNest does; RetroArch / other hosts don't), the user's chosen
    // dir replaces the save_dir-derived default above. The env callback
    // lives on the singleton FrontendState set in retro_set_environment;
    // on hosts that don't implement these enums, QueryPathOverride
    // returns empty and we keep the prior default.
    {
        retro_environment_t cb = g_frontend.environ_cb;  // from LibretroFrontend.h
        const std::string mc = QueryPathOverride(cb, RETRONEST_ENVIRONMENT_GET_MEMCARDS_DIR);
        if (!mc.empty())
            g_si.SetStringValue("Folders", "MemoryCards", mc.c_str());

        const std::string tex = QueryPathOverride(cb, RETRONEST_ENVIRONMENT_GET_TEXTURES_DIR);
        if (!tex.empty())
            g_si.SetStringValue("Folders", "Textures", tex.c_str());
    }

    // Slot{1,2}_Enable are now owned by CoreOptions::MemoryCards::ApplyDefaults
    // (called from the per-call user-options block below) so dialog tweaks
    // take effect on the next retro_load_game. Filenames stay hardcoded
    // here — libretro core options are Combo-only.
    g_si.SetStringValue("MemoryCards", "Slot1_Filename", "Mcd001.ps2");
    g_si.SetStringValue("MemoryCards", "Slot2_Filename", "Mcd002.ps2");

        g_initialized = true;
    }  // end of one-shot init block

    // SP7c Phase 1 followup: user-resolvable Emulation knobs re-apply on
    // every retro_load_game so dialog tweaks take effect on the next
    // launch without restarting RetroNest. The pre-SP7c behavior had a
    // hard `if (g_initialized) return;` guard which silently dropped any
    // subsequent retro_load_game's INI overrides — caught during Phase 1
    // smoke when Normal Speed → 50% on launch 1 stuck across reset →
    // launch 2. SetDefaultSettings + WriteDefaultPadBindings + memcards
    // stay one-shot above (re-running them would clobber accumulated
    // user state); only the user-resolvable subset flows through here.
    //
    // SP7b/SP7c: renderer / MTVU / fast_boot live in the Emulation card.
    // When options is null, default Values{} writes the SP7a-era hardcoded
    // defaults — preserves pre-SP7b behavior for any caller that omits options.
    {
        const CoreOptions::Emulation::Values em_defaults{};
        CoreOptions::Emulation::ApplyDefaults(
            g_si, options ? options->emulation : em_defaults);

        const CoreOptions::Audio::Values audio_defaults{};
        CoreOptions::Audio::ApplyDefaults(
            g_si, options ? options->audio : audio_defaults);

        const CoreOptions::MemoryCards::Values mc_defaults{};
        CoreOptions::MemoryCards::ApplyDefaults(
            g_si, options ? options->memory_cards : mc_defaults);

        const CoreOptions::Graphics::Values gfx_defaults{};
        CoreOptions::Graphics::ApplyDefaults(
            g_si, options ? options->graphics : gfx_defaults);
    }

    // Push the layered settings to the live Pcsx2Config so PCSX2's
    // framelimiter / GS / EE / SPU2 pick up the new values on this launch.
    // Runs every call (cheap: just propagates current g_si state).
    VMManager::Internal::LoadStartupSettings();

    FrontendLog(RETRO_LOG_INFO, "Settings::InitializeDefaults complete (BIOS dir = %s)",
                system_dir.c_str());
}

MemorySettingsInterface* GetActiveInterface()
{
    return g_initialized ? &g_si : nullptr;
}

} // namespace Pcsx2Libretro::Settings
