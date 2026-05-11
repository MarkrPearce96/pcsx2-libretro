// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// pcsx2-libretro — exported retro_* C functions.
//
// Skeleton phase: enough to load, identify as PCSX2, and gracefully
// refuse retro_load_game. No VM is initialized; no PCSX2 code runs.

#include "LibretroFrontend.h"
#include "libretro.h"

#include <cstdio>
#include <cstring>
#include <string>

using namespace Pcsx2Libretro;

extern "C" {

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
    info->library_version  = "skeleton-0.1";  // bumped manually until phase 2 hooks up BuildVersion.cpp
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
    // No-op. Skeleton produces no frames and no audio.
}

RETRO_API size_t retro_serialize_size(void) { return 0; }
RETRO_API bool   retro_serialize(void*, size_t)         { return false; }
RETRO_API bool   retro_unserialize(const void*, size_t) { return false; }

RETRO_API void   retro_cheat_reset(void) {}
RETRO_API void   retro_cheat_set(unsigned, bool, const char*) {}

RETRO_API bool retro_load_game(const struct retro_game_info* game)
{
    if (game && game->path)
        FrontendLog(RETRO_LOG_INFO, "retro_load_game called with path: %s", game->path);
    else
        FrontendLog(RETRO_LOG_INFO, "retro_load_game called with null game info");

    static const char* const refusal =
        "PCSX2 libretro core skeleton \xE2\x80\x94 game loading not implemented yet (phase 1)";

    // Surface the message in libretro hosts via SET_MESSAGE so it reaches
    // any frontend, not just RetroNest.
    if (g_frontend.environ_cb)
    {
        struct retro_message msg{};
        msg.msg    = refusal;
        msg.frames = 180; // ~3 seconds at 60fps
        g_frontend.environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &msg);
    }

    FrontendLog(RETRO_LOG_WARN, "%s", refusal);
    return false;
}

RETRO_API bool retro_load_game_special(unsigned, const struct retro_game_info*, size_t)
{
    return false;
}

RETRO_API void retro_unload_game(void)
{
    FrontendLog(RETRO_LOG_INFO, "retro_unload_game");
}

RETRO_API unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }

RETRO_API void* retro_get_memory_data(unsigned) { return nullptr; }
RETRO_API size_t retro_get_memory_size(unsigned) { return 0; }

} // extern "C"
