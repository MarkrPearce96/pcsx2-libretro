// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// pcsx2-libretro frontend — shared state and logging helper.
// Declares the singleton FrontendState that holds libretro callbacks
// captured during retro_set_* calls. Both LibretroFrontend.cpp and
// HostStubs.cpp read from this state.

#pragma once

#include "libretro.h"

#include <atomic>
#include <condition_variable>
#include <mutex>

namespace Pcsx2Libretro
{

struct FrontendState
{
    retro_environment_t        environ_cb       = nullptr;
    retro_video_refresh_t      video_cb         = nullptr;
    retro_audio_sample_t       audio_cb         = nullptr;
    retro_audio_sample_batch_t audio_batch_cb   = nullptr;
    retro_input_poll_t         input_poll_cb    = nullptr;
    retro_input_state_t        input_state_cb   = nullptr;
    retro_log_printf_t         log_cb           = nullptr;
    // SP5.5: rumble function pointer fetched from
    // RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE during retro_init. Stays null
    // if the host doesn't advertise rumble support — LibretroInputSource
    // then silently no-ops on motor writes.
    retro_set_rumble_state_t   set_rumble_state = nullptr;
};

extern FrontendState g_frontend;

// Frame-ready synchronization. Defined in HostStubs.cpp.
extern std::mutex g_present_mutex;
extern std::condition_variable g_present_cv;
extern std::atomic<bool> g_present_ready;

// Logging entry point used by HostStubs.cpp and LibretroFrontend.cpp.
// Routes through g_frontend.log_cb if available, else fprintf(stderr).
// Use it instead of printf/fprintf so log handling is consistent and
// libretro hosts can capture our log output.
void FrontendLog(retro_log_level level, const char* fmt, ...);

// Forward declaration of the emu-thread accessor defined in EmuThread.cpp.
class EmuThread;
EmuThread& GetEmuThread();

} // namespace Pcsx2Libretro
