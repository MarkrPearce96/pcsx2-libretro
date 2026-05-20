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
#include "LibretroSaveState.h"

#include "pcsx2/GS.h"
#include "pcsx2/VMManager.h"
#include "pcsx2/Config.h"           // LimiterModeType
#include "MemoryTypes.h"   // eeMem, Ps2MemSize::MainRam
#include "CoreResources.h"
#include "CoreOptions.h"
#include "AspectRatio.h"

#include <atomic>
#include <chrono>
#include <cmath>
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

// SP7a: cached region/fps reported to libretro. Defaults to NTSC/kNtscFps
// until DetectRegionFromSerial runs in retro_load_game. The refined
// flag gates the gsVideoMode-driven SET_SYSTEM_AV_INFO re-emit so we
// never emit twice. NTSC/PAL constants live in CoreResources.h as the
// single source of truth.
unsigned g_detected_region = RETRO_REGION_NTSC;
double   g_detected_fps    = Pcsx2Libretro::CoreResources::kNtscFps;
bool     g_region_refined  = false;

// Tracks the last aspect_ratio float we emitted via SET_SYSTEM_AV_INFO.
// retro_run compares each frame; re-emits only when the value changes by
// more than 0.001 (covers core-option toggles, widescreen-patch activation,
// and progressive↔interlaced video-mode transitions in Auto mode). -1.0f
// at startup forces first-frame emission to land in the cache.
float    g_last_emitted_aspect = -1.0f;

// Atomic, used by retro_run to log VM state transitions only once.
std::atomic<bool> g_logged_running{false};

// Atomic, used by retro_run to issue SET_MEMORY_MAPS exactly once per
// loaded game. Reset to false in retro_unload_game so the next loaded
// game re-issues with fresh pointers (eeMem may be reallocated across
// VM init/shutdown cycles).
std::atomic<bool> g_memory_map_issued{false};

// Pause-on-menu prev-state across paired retronest_set_paused calls.
// Shutdown = "not paused by us" sentinel (also what WaitForVmPaused
// returns on the 200ms-deadline timeout — ResumeVm no-ops in both
// cases, so the dual meaning is safe). Reset in retro_unload_game so
// a session that ends while paused doesn't leak state into the next
// game load.
static VMState s_pause_prev_state = VMState::Shutdown;

// SP6.5 Task 4.5: private env call agreed with RetroNest. RetroNest
// stores the resume-state path in EnvironmentContext::bootStatePath
// before retro_load_game; we query during retro_load_game and, if a
// path comes back, set params.save_state so VMManager::Initialize
// loads the state via DoLoadState after full BIOS init / ELF discovery
// (pcsx2/VMManager.cpp:1636-1643) — the only ordering that produces a
// runnable VM for cold-resume on launch.
//
// Number must match RetroNest's environment_callbacks.h define exactly.
// RETRO_ENVIRONMENT_PRIVATE = 0x20000 per libretro.h; 0x20001 is
// already used by RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW; 0x20002 is
// the next free RetroNest-private slot.
constexpr unsigned RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH =
    (2u | RETRO_ENVIRONMENT_PRIVATE);

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
// Two descriptors are issued:
//   1. EE main RAM (32 MB at PS2-real 0x00000000) — covers rcheevos's
//      Kernel RAM (0x00000000-0x000FFFFF) and System RAM
//      (0x00100000-0x01FFFFFF) regions (per
//      cpp/build-arm64/_deps/rcheevos-src/src/rcheevos/consoleinfo.c:813).
//   2. EE Scratchpad RAM (16 KB at PS2-real 0x70000000) — rcheevos
//      maps the cheevo address range 0x02000000-0x02003FFF onto this
//      hardware address. Originally deferred in SP6 ("not needed for
//      current RA cheevos") but R&C 2's set DOES read scratchpad —
//      rc_client_validate_addresses computes a host pointer that
//      lands outside our single 32 MB EE descriptor and crashes
//      ~20 s after game launch when the achievement-list HTTP comes
//      back. Issuing the second descriptor lets rc_libretro_memory_init
//      match all three rcheevos PS2 regions instead of guessing.
//
// RetroAchievements needs these descriptors to read PS2 cheevo memory
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

    retro_memory_descriptor descs[2]{};

    // EE Main RAM — 32 MB at PS2-real 0x00000000.
    descs[0].ptr       = eeMem->Main;
    descs[0].start     = 0x00000000;
    descs[0].len       = Ps2MemSize::MainRam;     // 32 MB
    descs[0].select    = 0;                       // RA infers from start+len
    descs[0].flags     = RETRO_MEMDESC_SYSTEM_RAM;
    descs[0].addrspace = "";

    // EE Scratchpad — 16 KB at PS2-real 0x70000000. rcheevos PS2 region
    // table marks this as RC_MEMORY_TYPE_SYSTEM_RAM so we use the same
    // flag as the main RAM descriptor.
    descs[1].ptr       = eeMem->Scratch;
    descs[1].start     = 0x70000000;
    descs[1].len       = Ps2MemSize::Scratch;     // 16 KB
    descs[1].select    = 0;
    descs[1].flags     = RETRO_MEMDESC_SYSTEM_RAM;
    descs[1].addrspace = "";

    retro_memory_map mm{};
    mm.descriptors     = descs;
    mm.num_descriptors = 2;

    const bool ok = g_frontend.environ_cb(
        RETRO_ENVIRONMENT_SET_MEMORY_MAPS, &mm);
    FrontendLog(ok ? RETRO_LOG_INFO : RETRO_LOG_WARN,
        "SET_MEMORY_MAPS issued: ee_ram=%p (%u bytes), scratchpad=%p (%u bytes) %s",
        descs[0].ptr, static_cast<unsigned>(descs[0].len),
        descs[1].ptr, static_cast<unsigned>(descs[1].len),
        ok ? "(accepted)" : "(frontend returned false)");
    g_memory_map_issued.store(true);
}

} // namespace

RETRO_API void retro_set_environment(retro_environment_t cb)
{
    g_frontend.environ_cb = cb;
    // SP7b: declare core options as soon as the env_cb is available.
    // This is the only legal time per the libretro spec (before retro_init).
    CoreOptions::EmitCoreOptionsV2(cb);
}
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

    // SP5.5: query rumble interface. Hosts that don't support rumble simply
    // return false, leaving set_rumble_state null — LibretroInputSource's
    // UpdateMotorState then silently no-ops. RetroNest exposes this via the
    // weak/strong-bridge to SdlInputManager::setRumbleMotor (RetroNest
    // commit e647577).
    retro_rumble_interface rumble_iface{};
    if (g_frontend.environ_cb &&
        g_frontend.environ_cb(RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE, &rumble_iface))
    {
        g_frontend.set_rumble_state = rumble_iface.set_rumble_state;
        FrontendLog(RETRO_LOG_INFO, "retro_init — rumble interface acquired");
    }
    else
    {
        FrontendLog(RETRO_LOG_INFO, "retro_init — rumble interface not available (host opt-out)");
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
    info->geometry.aspect_ratio = AspectRatio::Compute();
    info->timing.fps            = g_detected_fps;
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

    // SP7a: gsVideoMode-driven refinement of region/fps. The serial-based
    // guess in retro_load_game is accurate for retail discs but homebrew
    // and region-modded discs can run in a different mode than their
    // serial implies. We re-check once per call until gsVideoMode returns
    // a usable answer, then either confirm or re-emit SET_SYSTEM_AV_INFO
    // and stop checking.
    //
    // Intentional data race: gsVideoMode is written by the EE thread
    // (in SetGsCrt) and read here from the host thread without a lock.
    // On x86_64 and arm64, an aligned int-sized load is atomic at the
    // instruction level; a torn value would simply not match any enum
    // case and RegionFromGsVideoMode returns the safe NTSC default. The
    // worst case is one extra refinement cycle, not a corrupt result.
    if (!g_region_refined)
    {
        if (auto refined = Pcsx2Libretro::CoreResources::RegionFromGsVideoMode(gsVideoMode))
        {
            const bool disagrees =
                refined->libretro_region != g_detected_region
                || std::abs(refined->fps - g_detected_fps) > 0.05;
            if (disagrees)
            {
                g_detected_region = refined->libretro_region;
                g_detected_fps    = refined->fps;
                retro_system_av_info av{};
                retro_get_system_av_info(&av);
                if (g_frontend.environ_cb)
                    g_frontend.environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &av);
                FrontendLog(RETRO_LOG_INFO,
                    "[Region] refined to %s fps=%.2f from gsVideoMode",
                    g_detected_region == RETRO_REGION_PAL ? "PAL" : "NTSC",
                    g_detected_fps);
            }
            g_region_refined = true;
        }
    }

    // Aspect ratio change detection. The current effective aspect can shift
    // mid-session via three independent inputs:
    //   1. user toggling pcsx2_aspect_ratio (libretro options layer)
    //   2. widescreen patches activating (Patch.cpp:825 writes
    //      EmuConfig.CurrentCustomAspectRatio in the Auto branch)
    //   3. gsVideoMode progressive↔interlaced transitions in the Auto branch
    // Compute() is ~5 enum compares per frame; the actual SET_SYSTEM_AV_INFO
    // call is rare (gated on >0.001 float change).
    {
        const float current_aspect = AspectRatio::Compute();
        if (std::fabs(current_aspect - g_last_emitted_aspect) > 0.001f)
        {
            retro_system_av_info av{};
            retro_get_system_av_info(&av);
            if (g_frontend.environ_cb)
                g_frontend.environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &av);
            // av.geometry.aspect_ratio (not current_aspect) is what the
            // frontend actually received — retro_get_system_av_info called
            // Compute() again, and on a cross-thread write window the two
            // values can disagree. Cache what was emitted, not what we
            // saw on the comparison read.
            FrontendLog(RETRO_LOG_INFO,
                "[AspectRatio] re-emitted aspect=%.4f (was %.4f)",
                av.geometry.aspect_ratio, g_last_emitted_aspect);
            g_last_emitted_aspect = av.geometry.aspect_ratio;
        }
    }

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

RETRO_API size_t retro_serialize_size(void)                  { return Pcsx2Libretro::SerializeSize(); }
RETRO_API bool   retro_serialize(void* dst, size_t len)      { return Pcsx2Libretro::Serialize(dst, len); }
RETRO_API bool   retro_unserialize(const void* src, size_t len) { return Pcsx2Libretro::Unserialize(src, len); }

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

    // SP7b: read user-tweaked core options once at load time. Renderer,
    // MTVU, and FastBoot all take effect at VM init / boot — none are
    // safe to swap mid-run, so "restart to apply" UX is intentional.
    const auto resolved = CoreOptions::ReadResolved(g_frontend.environ_cb);

    // 2. Populate the in-memory settings layer.
    const std::string save_dir = GetSaveDirectory();
    Pcsx2Libretro::Settings::InitializeDefaults(system_dir, save_dir, &resolved);

    // 3. Build VMBootParameters and start the emu thread.
    VMBootParameters params{};
    params.filename = game->path;
    // SP7b: VMBootParameters.fast_boot overrides the INI EnableFastBoot
    // value (PCSX2's VMManager::BootSystem reads this field directly).
    // Wire it to the same resolved.fast_boot the INI write uses, so the
    // user's choice applies at both layers.
    params.fast_boot = resolved.emulation.fast_boot;

    // SP6.5 Task 4.5: cold-resume on launch.
    //
    // RetroNest stashes a resume-state path in its EnvironmentContext
    // BEFORE calling retro_load_game (CoreRuntime::runLoop). We query
    // it here; if a non-empty path comes back, we set params.save_state
    // so VMManager::Initialize runs DoLoadState at the end of full VM
    // init (after BIOS init / ELF discovery / s_fast_boot_requested) —
    // exactly the ordering PCSX2-Qt uses when launching with a state.
    //
    // The env handler marks the path consumed so RetroNest's post-
    // retro_load_game legacy retro_unserialize block skips itself; the
    // load happened inside VMManager::Initialize and re-loading would
    // race the freshly-loaded VM state.
    //
    // mGBA and other cores that don't define this env call get
    // false from the env_cb, take the empty-path fast path here, and
    // RetroNest's legacy retro_unserialize block handles their cold-
    // resume as before. Backward-compatible by construction.
    if (g_frontend.environ_cb)
    {
        const char* boot_state = nullptr;
        if (g_frontend.environ_cb(RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH,
                                  &boot_state) &&
            boot_state && boot_state[0] != '\0')
        {
            // Copy into params.save_state immediately — the env handler's
            // returned pointer is only stable for the duration of this
            // env_cb call (RetroNest sets bootStatePathConsumed=true on
            // read and may later destroy the backing QByteArray).
            params.save_state = boot_state;
            FrontendLog(RETRO_LOG_INFO,
                "retro_load_game: cold-boot via save state %s",
                params.save_state.c_str());

            // The .resume file on disk is the padded buffer that
            // retro_serialize handed the frontend — real_zip ||
            // zeros || sentinel. Trim it in place so PCSX2's
            // upstream file-based loader (which uses libzip's
            // EOCD scan, capped at ~64 KB back from EOF) can
            // actually open it. See LibretroSaveState.cpp.
            Pcsx2Libretro::TrimPaddedSaveStateFile(
                params.save_state.c_str());
        }
    }

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

    // SP7a: reset region cache and detect from disc serial.
    // VMManager::GetDiscSerial() is valid as soon as Initialize completed
    // (EmuThread::Start waits on m_init_done), so it's safe to read here.
    // gsVideoMode is still Uninitialized at this point; the retro_run
    // refinement pass picks it up once the EE has executed SetGsCrt.
    g_detected_region = RETRO_REGION_NTSC;
    g_detected_fps    = Pcsx2Libretro::CoreResources::kNtscFps;
    g_region_refined  = false;
    g_last_emitted_aspect = -1.0f;
    const std::string disc_serial = VMManager::GetDiscSerial();
    const auto detected = Pcsx2Libretro::CoreResources::DetectRegionFromSerial(disc_serial);
    g_detected_region = detected.libretro_region;
    g_detected_fps    = detected.fps;

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
    g_region_refined  = false;            // re-run gsVideoMode refinement on next game load
    g_last_emitted_aspect = -1.0f;        // force re-emit on next game's first frame
    s_pause_prev_state = VMState::Shutdown;   // clear pause state — don't leak into next game
    Pcsx2Libretro::GetEmuThread().SetHostInitiatedPause(false);  // clear suppression — don't leak into next game
    Pcsx2Libretro::ResetSerializeSizeCache();  // re-probe on next game load (SP6.5)
    FrontendLog(RETRO_LOG_INFO, "retro_unload_game: requesting VM shutdown");
    Pcsx2Libretro::EmuThread& emu = Pcsx2Libretro::GetEmuThread();
    emu.RequestShutdown();
    emu.Join();
    FrontendLog(RETRO_LOG_INFO, "retro_unload_game: emu thread joined cleanly");
}

RETRO_API unsigned retro_get_region(void) { return g_detected_region; }

RETRO_API void* retro_get_memory_data(unsigned) { return nullptr; }
RETRO_API size_t retro_get_memory_size(unsigned) { return 0; }

// Host→core pause signal. Not part of libretro spec; RetroNest probes
// for this symbol via dlsym and calls it when the in-game menu opens
// (PCSX2's EE/MTGS/MTVU threads are decoupled from retro_run, so
// pausing the frame pump alone isn't enough — the host needs an
// explicit way to halt internal emulation threads).
//
// Implementation delegates to the SP6.5 save-state pause-handshake
// helpers in LibretroSaveState.h, which already handle idempotency
// and the 200 ms VMState polling deadline correctly.
//
// Thread expectations: called from the RetroNest GUI thread (Qt slot
// fired by AppController::openLibretroOverlayMenu →
// GameSession::pauseEmulation). s_pause_prev_state is a file-scope
// static touched only from that thread; reset in retro_unload_game.
RETRO_API void retronest_set_paused(bool paused)
{
    if (paused)
    {
        // Mark the pause as host-initiated BEFORE WaitForVmPaused so
        // the watchdog already sees the flag set when it observes the
        // Paused state transition.
        Pcsx2Libretro::GetEmuThread().SetHostInitiatedPause(true);
        s_pause_prev_state = Pcsx2Libretro::WaitForVmPaused();
    }
    else
    {
        Pcsx2Libretro::ResumeVm(s_pause_prev_state);
        s_pause_prev_state = VMState::Shutdown;
        // Clear AFTER ResumeVm so the watchdog stays suppressed
        // during the un-pause handshake (state may briefly be
        // Paused during the SetPaused(false) → EmuThread observes
        // Running transition).
        Pcsx2Libretro::GetEmuThread().SetHostInitiatedPause(false);
    }
}

// Host-side fast-forward toggle, mirroring retronest_set_paused.
//
// PCSX2 doesn't speed up just because retro_run is called more often:
// the EmuThread paces itself internally to the target VSync rate via
// g_present_cv, so the libretro-standard "frontend calls retro_run
// faster" approach (which mGBA and other synchronous cores honor) is
// a no-op for us. To actually fast-forward we have to disengage the
// in-VM framelimiter — which is what LimiterModeType::Turbo does (it
// switches the target speed to TurboScalar; configurable via the
// pcsx2_fast_forward_speed core option, default 2.0×).
//
// RetroNest probes this symbol via dlsym; cores that don't export it
// (mGBA, etc.) get their existing libretro-standard FF path unchanged.
RETRO_API void retronest_set_fast_forward(bool fast_forward)
{
    if (!VMManager::HasValidVM()) return;
    VMManager::SetLimiterMode(fast_forward ? LimiterModeType::Turbo
                                           : LimiterModeType::Nominal);
}

} // extern "C"
