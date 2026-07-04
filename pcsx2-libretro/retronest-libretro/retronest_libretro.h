/* retronest-libretro — the RetroNest ⇄ libretro-core private contract.
 *
 * CANONICAL COPY: RetroNest-Project/vendor/retronest-libretro/retronest_libretro.h
 * Vendored byte-identical into each core fork by sync.sh. DO NOT EDIT the
 * vendored copies — edit the canonical file and re-run sync.sh. Drift is
 * caught by check-drift.sh (wired as a CMake ALL target in each fork).
 *
 * libretro reserves RETRO_ENVIRONMENT_PRIVATE (0x20000+) for frontend↔core
 * private contracts. This header is the single registry of RetroNest's
 * private environment commands and dlsym'd optional core exports. The
 * numeric values are frozen ABI — pinned by RetroNest's
 * test_retronest_contract and by hardcoded literals in each fork's
 * standalone tests (those literals are deliberate: they are the other
 * side of the contract).
 */
#ifndef RETRONEST_LIBRETRO_H
#define RETRONEST_LIBRETRO_H

#include "libretro.h" /* the pinned copy shipped alongside this header */
#include <stdbool.h>

/* ---- Private environment command registry --------------------------------
 * NEXT FREE SLOT: (6 | RETRO_ENVIRONMENT_PRIVATE) = 0x20006.
 * Register new commands HERE (never locally in a core or the host), with
 * direction, data type, and current users documented.
 */

/* 0x20001 — host→core. data: void** written with the NSView* hosting the
 * core's CAMetalLayer. Returns false when no native view is registered
 * (software-rendered cores). Users: duckstation, pcsx2, dolphin (query);
 * RetroNest environment_callbacks.cpp (serve). */
#define RETRONEST_ENVIRONMENT_GET_MACOS_NSVIEW (1u | RETRO_ENVIRONMENT_PRIVATE)

/* 0x20002 — host→core. data: const char** written with a UTF-8 resume-state
 * path, valid only for the synchronous duration of the env call (the core
 * must copy it). Queried during retro_load_game so PCSX2 can cold-resume via
 * VMBootParameters::save_state. The host marks the path consumed on read and
 * then skips its legacy post-load retro_unserialize fallback. Returns false
 * when no resume path is set. Users: pcsx2. */
#define RETRONEST_ENVIRONMENT_GET_BOOT_STATE_PATH (2u | RETRO_ENVIRONMENT_PRIVATE)

/* 0x20003 / 0x20004 — host→core path overrides (user's Paths settings).
 * data: const char** written with a UTF-8 dir path (same lifetime rule as
 * 0x20002). Returns false when no override exists — the core falls back to
 * its save_dir-derived default, so non-RetroNest hosts work unchanged.
 * Users: pcsx2. */
#define RETRONEST_ENVIRONMENT_GET_MEMCARDS_DIR (3u | RETRO_ENVIRONMENT_PRIVATE)
#define RETRONEST_ENVIRONMENT_GET_TEXTURES_DIR (4u | RETRO_ENVIRONMENT_PRIVATE)

/* 0x20005 — core→host, called during retro_load_game. data: a
 * retronest_game_identity*. The host copies both strings before returning.
 * Lets cores hand over a RetroAchievements hash + serial computed from
 * formats rcheevos can't hash by path (compressed RVZ/CHD via DiscIO).
 * Returns false if data is null. Users: duckstation, dolphin (call);
 * RetroNest (serve). */
#define RETRONEST_ENVIRONMENT_SET_GAME_IDENTITY (5u | RETRO_ENVIRONMENT_PRIVATE)

/* Payload for SET_GAME_IDENTITY. Fields may be "" when unavailable, never
 * null. Layout is frozen ABI (2 pointers). */
struct retronest_game_identity {
    const char* ra_hash; /* rcheevos hash string */
    const char* serial;  /* game id, e.g. "SCUS-94900" / "GZ2P01" */
};

/* ---- Optional core exports ------------------------------------------------
 * The host resolves these via dlsym after dlopen; every one is optional —
 * cores that don't export them get the standard libretro behavior
 * (duckstation exports none of them and is fully conforming).
 *
 *   retronest_set_paused(true/false)      — host-initiated pause/resume for
 *       cores that pace internally (pcsx2, dolphin).
 *   retronest_set_fast_forward(on/off)    — engage the core's internal
 *       turbo/limiter (cores that ignore faster retro_run calls).
 *   retronest_shutdown_wedged() -> true   — the core detached a wedged VM
 *       thread during shutdown; the host MUST skip retro_deinit + dlclose
 *       (keep the dylib mapped) and refuse a new session in-process (pcsx2).
 */
typedef void (*retronest_set_paused_t)(bool paused);
typedef void (*retronest_set_fast_forward_t)(bool fast_forward);
typedef bool (*retronest_shutdown_wedged_t)(void);

#if defined(RETRONEST_LIBRETRO_CORE)
/* A core's defining TU should `#define RETRONEST_LIBRETRO_CORE` before
 * including this header so a signature mismatch is a compile error.
 * Definitions still carry RETRO_API for default visibility. */
#ifdef __cplusplus
extern "C" {
#endif
void retronest_set_paused(bool paused);
void retronest_set_fast_forward(bool fast_forward);
bool retronest_shutdown_wedged(void);
#ifdef __cplusplus
}
#endif
#endif /* RETRONEST_LIBRETRO_CORE */

#endif /* RETRONEST_LIBRETRO_H */
