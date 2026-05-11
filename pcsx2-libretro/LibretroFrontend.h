// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// pcsx2-libretro frontend — shared state and logging helper.
// Declares the singleton FrontendState that holds libretro callbacks
// captured during retro_set_* calls. Both LibretroFrontend.cpp and
// HostStubs.cpp read from this state.

#pragma once

#include "libretro.h"

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
};

extern FrontendState g_frontend;

// Logging entry point used by HostStubs.cpp and LibretroFrontend.cpp.
// Routes through g_frontend.log_cb if available, else fprintf(stderr).
// Use it instead of printf/fprintf so log handling is consistent and
// libretro hosts can capture our log output.
void FrontendLog(retro_log_level level, const char* fmt, ...);

} // namespace Pcsx2Libretro
