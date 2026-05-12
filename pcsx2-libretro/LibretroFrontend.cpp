// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// pcsx2-libretro — exported retro_* C functions.
//
// Skeleton phase: enough to load, identify as PCSX2, and gracefully
// refuse retro_load_game. No VM is initialized; no PCSX2 code runs.

#include "LibretroFrontend.h"
#include "LibretroAudioStream.h"
#include "libretro.h"

#include "EmuThread.h"
#include "Settings.h"

#include "pcsx2/VMManager.h"
#include "MemoryTypes.h"   // eeMem, Ps2MemSize::MainRam

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>
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

// Returns the libretro save directory, or empty string if the host
// does not provide one. Cached after first call.
std::string GetSaveDirectory()
{
    static std::string s_cached;
    static bool s_resolved = false;
    if (s_resolved) return s_cached;

    const char* dir = nullptr;
    if (g_frontend.environ_cb &&
        g_frontend.environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &dir) &&
        dir != nullptr)
    {
        s_cached = dir;
    }
    s_resolved = true;
    return s_cached;
}

// Atomic, used by retro_run to log VM state transitions only once.
std::atomic<bool> g_logged_running{false};

// Atomic, used by retro_run to issue SET_MEMORY_MAPS exactly once per
// loaded game. Reset to false in retro_unload_game so the next loaded
// game re-issues with fresh pointers (eeMem may be reallocated across
// VM init/shutdown cycles).
std::atomic<bool> g_memory_map_issued{false};

// RETRONEST_STATE_TRACE: env-gated trace at retro_reset boundary.
// Zero overhead when unset (single getenv at first call, cached bool
// thereafter). Mirrors RETRONEST_AUDIO_TRACE (SP4) and
// RETRONEST_INPUT_TRACE (SP5).
//
// SP6 only adds the retro_reset boundary; save-state / pause-handshake
// trace boundaries are deferred to SP6.5 along with save-state.
bool IsStateTraceEnabled()
{
    static const bool s_enabled = (std::getenv("RETRONEST_STATE_TRACE") != nullptr);
    return s_enabled;
}

// Issue SET_MEMORY_MAPS once per loaded game. Idempotent — guarded by
// g_memory_map_issued atomic so concurrent calls from retro_load_game
// (primary path, lands before RetroNest's RcheevosRuntime memory_init)
// and retro_run first-Running frame (safety belt) cooperate cleanly.
//
// EE main RAM is the 32 MB region at PS2-physical 0x00000000.
// RetroAchievements needs this descriptor to read PS2 cheevo memory
// addresses; if rcheevos initializes BEFORE this fires, the cheevo
// session loads with regions=0 and achievements never trigger even
// though the set is logged-in.
//
// RetroNest's env handler at environment_callbacks.cpp:133-163 copies
// the descriptor array + addrspace strings before this call returns,
// so stack-allocated structs are safe.
void TryIssueMemoryMaps()
{
    if (g_memory_map_issued.load()) return;
    if (eeMem == nullptr) return;             // VM init may not yet have allocated EE RAM
    if (!g_frontend.environ_cb) return;       // frontend not yet wired

    retro_memory_descriptor desc{};
    desc.ptr       = eeMem->Main;
    desc.start     = 0x00000000;          // PS2-physical
    desc.len       = Ps2MemSize::MainRam; // 32 MB
    desc.select    = 0;                   // RA infers from start+len
    desc.flags     = RETRO_MEMDESC_SYSTEM_RAM;
    desc.addrspace = "";                  // unnamed default

    retro_memory_map mm{};
    mm.descriptors     = &desc;
    mm.num_descriptors = 1;

    const bool ok = g_frontend.environ_cb(
        RETRO_ENVIRONMENT_SET_MEMORY_MAPS, &mm);
    FrontendLog(ok ? RETRO_LOG_INFO : RETRO_LOG_WARN,
        "SET_MEMORY_MAPS issued: ee_ram_ptr=%p len=%u %s",
        desc.ptr, static_cast<unsigned>(desc.len),
        ok ? "(accepted)" : "(frontend returned false)");
    g_memory_map_issued.store(true);
}

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
    info->library_version  = "video-0.1";  // bumped manually until phase 2 hooks up BuildVersion.cpp
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

    if (std::getenv("RETRONEST_AUDIO_TRACE"))
    {
        FrontendLog(RETRO_LOG_INFO,
            "[AUDIO_TRACE] av_info fps=%.4f sample_rate=%.0f",
            info->timing.fps, info->timing.sample_rate);
    }
}

RETRO_API void retro_set_controller_port_device(unsigned port, unsigned device)
{
    // SP5: we want both physical PS2 ports treated as analog DualShock 2.
    // Accept JOYPAD and ANALOG (both query through the same trampoline);
    // log + ignore other types (mouse, lightgun, keyboard) for now.
    if (port >= 2)
    {
        FrontendLog(RETRO_LOG_WARN,
            "retro_set_controller_port_device: port %u out of range (max 2)", port);
        return;
    }

    if (device != RETRO_DEVICE_NONE &&
        device != RETRO_DEVICE_JOYPAD &&
        device != RETRO_DEVICE_ANALOG)
    {
        FrontendLog(RETRO_LOG_INFO,
            "retro_set_controller_port_device(port=%u, device=%u) — unsupported, ignoring",
            port, device);
        return;
    }

    FrontendLog(RETRO_LOG_INFO,
        "retro_set_controller_port_device(port=%u, device=%u) acknowledged", port, device);
}

RETRO_API void retro_reset(void)
{
    if (!VMManager::HasValidVM())
    {
        FrontendLog(RETRO_LOG_INFO, "retro_reset called with no valid VM — ignoring");
        return;
    }
    if (IsStateTraceEnabled())
        FrontendLog(RETRO_LOG_INFO,
            "[STATE_TRACE] retro_reset state=%d",
            static_cast<int>(VMManager::GetState()));
    FrontendLog(RETRO_LOG_INFO, "retro_reset → VMManager::Reset()");
    VMManager::Reset();
}

RETRO_API void retro_run(void)
{
    Pcsx2Libretro::EmuThread& emu = Pcsx2Libretro::GetEmuThread();
    if (!emu.IsRunning()) return;

    // One-shot log when VM first reports Running with a non-zero CRC.
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

    // Safety-belt SET_MEMORY_MAPS issue, in case retro_load_game's
    // primary call fired before eeMem was allocated. Idempotent.
    TryIssueMemoryMaps();

    // Frame-paced wait. PCSX2's MTGS thread signals g_present_cv from
    // Host::BeginPresentFrame after each rendered frame. retro_run returns
    // as soon as a frame is ready, so the host (RetroArch / RetroNest)
    // drives ~60Hz cadence by calling us once per host frame.
    //
    // 100 ms timeout protects against VM hangs / Initialize-failed paths
    // where Frame would never be signalled.
    using namespace std::chrono_literals;
    std::unique_lock<std::mutex> lock(g_present_mutex);
    g_present_cv.wait_for(lock, 100ms, [] { return g_present_ready.load(); });
    g_present_ready.store(false, std::memory_order_release);
    lock.unlock(); // release before draining; drain doesn't touch g_present_*

    // SP4: drain SPU2 output. ActiveStream() is null pre-VM-init or post-
    // shutdown; audio_batch_cb is null until the frontend has called
    // retro_set_audio_sample_batch. Both are common at startup, so silently
    // skip if either is missing.
    if (auto* stream = Pcsx2Libretro::LibretroAudioStream::ActiveStream())
    {
        if (g_frontend.audio_batch_cb)
        {
            // SP4.x fix: drain ONE host-frame's worth of audio per retro_run.
            //
            // Originally we passed MAX_FRAMES_PER_DRAIN (2048). At a 60Hz host
            // cadence that's 122 880 samples/sec pushed into the frontend, but
            // the device only plays back at 48 000/sec — the queue grows at
            // 1.5 s of audio per real second, so audio drifts minutes behind
            // picture within a few minutes of play.
            //
            // PCSX2's audio-sync framelimiter (via SoundTouch) tracks the
            // consumer rate of this callback. Feeding it ~800 frames per call
            // (= sample_rate / host_fps) tells the framelimiter "consumer
            // wants 48 kHz", and emulation self-throttles to produce that
            // exact rate. Steady state: ring buffer stays small, SDL queue
            // stays bounded near zero, audio stays in sync with video.
            //
            // 800 = av.timing.sample_rate / av.timing.fps. Both are currently
            // hardcoded in retro_get_system_av_info; once that derives fps
            // from the GS region (SP4.x M4), this should derive too.
            // MAX_FRAMES_PER_DRAIN still bounds the pending-buffer staging
            // size — only the per-call drain target changes.
            constexpr u32 frames_per_host_frame = 48000 / 60;
            stream->DrainToLibretroCallback(g_frontend.audio_batch_cb,
                frames_per_host_frame);
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
    const std::string save_dir = GetSaveDirectory();
    Pcsx2Libretro::Settings::InitializeDefaults(system_dir, save_dir);

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

    // Primary SET_MEMORY_MAPS issue site. VMManager::Initialize completed
    // successfully → eeMem is allocated → safe to descriptor-up the EE
    // main RAM region. Issuing here (rather than from retro_run's first
    // Running frame) lands BEFORE RetroNest's RcheevosRuntime calls
    // rc_libretro_memory_init — without this ordering, the cheevo set
    // loads with regions=0 and achievements never trigger even when
    // logged-in. The retro_run call site below is now a safety-belt no-op
    // in the normal case (eeMem was already valid at this point).
    TryIssueMemoryMaps();

    return true;
}

RETRO_API bool retro_load_game_special(unsigned, const struct retro_game_info*, size_t)
{
    return false;
}

RETRO_API void retro_unload_game(void)
{
    g_memory_map_issued.store(false);     // re-issue on next game load
    g_logged_running.store(false);        // re-log on next Running
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
