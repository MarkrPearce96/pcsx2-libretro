// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// pcsx2-libretro — exported retro_* C functions.
//
// Skeleton phase: enough to load, identify as PCSX2, and gracefully
// refuse retro_load_game. No VM is initialized; no PCSX2 code runs.

#include "LibretroFrontend.h"
#include "libretro.h"

#include "EmuThread.h"
#include "Settings.h"

#include "pcsx2/VMManager.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

using namespace Pcsx2Libretro;

extern "C" {

namespace {

// Returns a likely PS2 BIOS file in `dir`, or empty string if none.
// Filenames PCSX2 historically recognises: SCPH*.BIN, ps2-*.bin,
// and the simple "bios.bin" / "BIOS.bin".
//
// IMPORTANT: PS2 BIOSes are typically 4 MB. PSX BIOSes (which may
// coexist in the same directory if the user runs both emulators)
// are 512 KB. We require size > 1 MB to filter out PSX BIOSes that
// happen to also start with "scph".
std::string FindPS2BiosFile(const std::string& dir)
{
    namespace fs = std::filesystem;
    if (dir.empty() || !fs::exists(dir) || !fs::is_directory(dir))
        return {};

    for (const auto& entry : fs::directory_iterator(dir))
    {
        if (!entry.is_regular_file()) continue;

        // PS2 BIOSes are ~4 MB. Reject anything under 1 MB to skip
        // PSX BIOSes (~512 KB) that share the "scph" prefix.
        std::error_code ec;
        const auto sz = entry.file_size(ec);
        if (ec || sz < (1u << 20)) continue;

        const std::string name = entry.path().filename().string();
        std::string lower = name;
        for (auto& c : lower) c = static_cast<char>(::tolower(c));

        // Common PS2 BIOS naming patterns.
        if (lower.rfind("scph", 0) == 0 && (lower.ends_with(".bin") || lower.ends_with(".bin.ecm")))
            return entry.path().string();
        if (lower.rfind("ps2-", 0) == 0 && lower.ends_with(".bin"))
            return entry.path().string();
        if (lower == "bios.bin")
            return entry.path().string();
    }
    return {};
}

// Returns the libretro system directory, or empty string if the host
// does not provide one. Cached after first call.
std::string GetSystemDirectory()
{
    static std::string s_cached;
    static bool s_resolved = false;
    if (s_resolved) return s_cached;

    const char* dir = nullptr;
    if (g_frontend.environ_cb &&
        g_frontend.environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir) &&
        dir != nullptr)
    {
        s_cached = dir;
    }
    s_resolved = true;
    return s_cached;
}

// Atomic, used by retro_run to log VM state transitions only once.
std::atomic<bool> g_logged_running{false};

} // namespace

RETRO_API void retro_set_environment(retro_environment_t cb)        { g_frontend.environ_cb     = cb; }
RETRO_API void retro_set_video_refresh(retro_video_refresh_t cb)    { g_frontend.video_cb       = cb; }
RETRO_API void retro_set_audio_sample(retro_audio_sample_t cb)      { g_frontend.audio_cb       = cb; }
RETRO_API void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { g_frontend.audio_batch_cb = cb; }
RETRO_API void retro_set_input_poll(retro_input_poll_t cb)          { g_frontend.input_poll_cb  = cb; }
RETRO_API void retro_set_input_state(retro_input_state_t cb)        { g_frontend.input_state_cb = cb; }

RETRO_API void retro_init(void)
{
    // Try to obtain the libretro log interface for better log routing.
    retro_log_callback log_iface{};
    if (g_frontend.environ_cb &&
        g_frontend.environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log_iface))
    {
        g_frontend.log_cb = log_iface.log;
    }
    FrontendLog(RETRO_LOG_INFO, "retro_init — PCSX2 libretro skeleton initialised");
}

RETRO_API void retro_deinit(void)
{
    // Defensive: ensure emu thread is stopped before tearing down g_frontend.
    Pcsx2Libretro::EmuThread& emu = Pcsx2Libretro::GetEmuThread();
    emu.RequestShutdown();
    emu.Join();
    FrontendLog(RETRO_LOG_INFO, "retro_deinit");
    g_frontend = FrontendState{};
}

RETRO_API unsigned retro_api_version(void)
{
    return RETRO_API_VERSION;
}

RETRO_API void retro_get_system_info(struct retro_system_info* info)
{
    if (!info) return;
    std::memset(info, 0, sizeof(*info));
    info->library_name     = "PCSX2";
    info->library_version  = "vm-0.1";  // bumped manually until phase 2 hooks up BuildVersion.cpp
    info->valid_extensions = "iso|chd|cso|bin|cue|m3u|gz";
    info->need_fullpath    = true;
    info->block_extract    = true;
}

RETRO_API void retro_get_system_av_info(struct retro_system_av_info* info)
{
    if (!info) return;
    std::memset(info, 0, sizeof(*info));
    info->geometry.base_width   = 640;
    info->geometry.base_height  = 448;
    info->geometry.max_width    = 1280;
    info->geometry.max_height   = 1024;
    info->geometry.aspect_ratio = 4.0f / 3.0f;
    info->timing.fps            = 60.0;       // placeholder — phase 3 will derive from GS region
    info->timing.sample_rate    = 48000.0;
}

RETRO_API void retro_set_controller_port_device(unsigned port, unsigned device)
{
    FrontendLog(RETRO_LOG_INFO, "retro_set_controller_port_device(port=%u, device=%u) — ignored in skeleton",
                port, device);
}

RETRO_API void retro_reset(void)
{
    FrontendLog(RETRO_LOG_INFO, "retro_reset — no-op in skeleton");
}

RETRO_API void retro_run(void)
{
    // SP2: no frame output. Observe state transitions and log once when
    // the VM reaches Running with a non-zero CRC (proves the game booted).
    Pcsx2Libretro::EmuThread& emu = Pcsx2Libretro::GetEmuThread();
    if (!emu.IsRunning()) return;

    if (!g_logged_running.load() && VMManager::GetState() == VMState::Running)
    {
        const u32 crc = VMManager::GetCurrentCRC();
        if (crc != 0)
        {
            FrontendLog(RETRO_LOG_INFO, "VM RUNNING — title=%s serial=%s crc=0x%08X",
                        VMManager::GetTitle(true).c_str(),
                        VMManager::GetDiscSerial().c_str(),
                        crc);
            g_logged_running.store(true);
        }
    }
}

RETRO_API size_t retro_serialize_size(void) { return 0; }
RETRO_API bool   retro_serialize(void*, size_t)         { return false; }
RETRO_API bool   retro_unserialize(const void*, size_t) { return false; }

RETRO_API void   retro_cheat_reset(void) {}
RETRO_API void   retro_cheat_set(unsigned, bool, const char*) {}

RETRO_API bool retro_load_game(const struct retro_game_info* game)
{
    if (!game || !game->path)
    {
        FrontendLog(RETRO_LOG_ERROR, "retro_load_game called with null game info");
        return false;
    }

    FrontendLog(RETRO_LOG_INFO, "retro_load_game: %s", game->path);

    // 1. Resolve BIOS path from libretro system directory.
    const std::string system_dir = GetSystemDirectory();
    if (system_dir.empty())
    {
        FrontendLog(RETRO_LOG_ERROR, "Host did not provide a system directory");
        struct retro_message msg{};
        msg.msg = "PCSX2 libretro: host provides no system directory — cannot locate BIOS";
        msg.frames = 240;
        if (g_frontend.environ_cb)
            g_frontend.environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &msg);
        return false;
    }

    const std::string bios_path = FindPS2BiosFile(system_dir);
    if (bios_path.empty())
    {
        FrontendLog(RETRO_LOG_ERROR, "No PS2 BIOS file found in %s", system_dir.c_str());
        const std::string msg_text = "PS2 BIOS not found in " + system_dir;
        struct retro_message msg{};
        msg.msg = msg_text.c_str();
        msg.frames = 240;
        if (g_frontend.environ_cb)
            g_frontend.environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &msg);
        return false;
    }
    FrontendLog(RETRO_LOG_INFO, "Found PS2 BIOS: %s", bios_path.c_str());

    // 2. Populate the in-memory settings layer.
    Pcsx2Libretro::Settings::InitializeDefaults(system_dir);

    // 3. Build VMBootParameters and start the emu thread.
    VMBootParameters params{};
    params.filename = game->path;
    params.fast_boot = true;

    Pcsx2Libretro::EmuThread& emu = Pcsx2Libretro::GetEmuThread();
    const bool ok = emu.Start(params);
    if (!ok)
    {
        FrontendLog(RETRO_LOG_ERROR, "retro_load_game: emu thread Start returned false");
        struct retro_message msg{};
        msg.msg = "PCSX2 libretro: VM init failed (see log)";
        msg.frames = 240;
        if (g_frontend.environ_cb)
            g_frontend.environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &msg);
        return false;
    }

    FrontendLog(RETRO_LOG_INFO, "retro_load_game: VM started successfully");
    g_logged_running.store(false);
    return true;
}

RETRO_API bool retro_load_game_special(unsigned, const struct retro_game_info*, size_t)
{
    return false;
}

RETRO_API void retro_unload_game(void)
{
    FrontendLog(RETRO_LOG_INFO, "retro_unload_game: requesting VM shutdown");
    Pcsx2Libretro::EmuThread& emu = Pcsx2Libretro::GetEmuThread();
    emu.RequestShutdown();
    emu.Join();
    FrontendLog(RETRO_LOG_INFO, "retro_unload_game: emu thread joined cleanly");
}

RETRO_API unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }

RETRO_API void* retro_get_memory_data(unsigned) { return nullptr; }
RETRO_API size_t retro_get_memory_size(unsigned) { return 0; }

} // extern "C"
