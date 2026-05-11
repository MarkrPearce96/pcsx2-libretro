// Standalone libretro core loader for skeleton verification.
// Not built as part of pcsx2_libretro target — manual compile.
//
//   clang test_loader.c -o test_loader
//   ./test_loader path/to/pcsx2_libretro.dylib path/to/some.iso

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>   // getcwd, usleep
#include <stdint.h>   // int16_t

typedef void (*retro_init_fn)(void);
typedef void (*retro_deinit_fn)(void);
typedef unsigned (*retro_api_version_fn)(void);
typedef void (*retro_get_system_info_fn)(void*);
typedef void (*retro_set_environment_fn)(void* cb);
typedef int  (*retro_load_game_fn)(const void*);
typedef void (*retro_unload_game_fn)(void);
typedef void (*retro_run_fn)(void);
typedef void (*retro_set_video_refresh_fn)(void* cb);
typedef void (*retro_set_audio_sample_fn)(void* cb);
typedef void (*retro_set_audio_sample_batch_fn)(void* cb);
typedef void (*retro_set_input_poll_fn)(void* cb);
typedef void (*retro_set_input_state_fn)(void* cb);

struct retro_system_info {
    const char* library_name;
    const char* library_version;
    const char* valid_extensions;
    int need_fullpath;
    int block_extract;
};

struct retro_game_info {
    const char* path;
    const void* data;
    size_t size;
    const char* meta;
};

// Stub libretro callbacks — required before retro_init so the core
// doesn't NULL-dereference. Must match the libretro.h prototypes
// closely enough for ABI compatibility; in C with void* we just
// pass through.
static void vr_cb(const void* data, unsigned w, unsigned h, size_t pitch) {
    (void)data; (void)w; (void)h; (void)pitch;
}
static void as_cb(int16_t left, int16_t right) { (void)left; (void)right; }
static size_t ab_cb(const int16_t* data, size_t frames) { (void)data; return frames; }
static void ip_cb(void) {}
static int16_t is_cb(unsigned port, unsigned device, unsigned index, unsigned id) {
    (void)port; (void)device; (void)index; (void)id; return 0;
}

// Static system directory: test_loader cwd. Allows the core to find a
// BIOS file placed next to where we run from.
static char s_system_dir[4096] = {0};

#define RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY 9
static int env_cb(unsigned cmd, void* data) {
    if (cmd == RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY) {
        if (s_system_dir[0] && data) {
            *(const char**)data = s_system_dir;
            return 1;
        }
        return 0;
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <core.dylib> <game.iso> [<system_dir>]\n"
                        "  <system_dir> defaults to current working directory.\n",
                argv[0]);
        return 1;
    }

    if (argc >= 4) {
        snprintf(s_system_dir, sizeof(s_system_dir), "%s", argv[3]);
    } else {
        if (!getcwd(s_system_dir, sizeof(s_system_dir))) {
            fprintf(stderr, "getcwd failed\n");
            return 1;
        }
    }
    fprintf(stderr, "test_loader: system_dir = %s\n", s_system_dir);

    void* h = dlopen(argv[1], RTLD_NOW);
    if (!h) { fprintf(stderr, "dlopen failed: %s\n", dlerror()); return 1; }

    #define LOAD(sym, type) type sym = (type)dlsym(h, #sym); \
        if (!sym) { fprintf(stderr, "missing symbol: %s\n", #sym); dlclose(h); return 1; }

    LOAD(retro_api_version,             retro_api_version_fn);
    LOAD(retro_set_environment,         retro_set_environment_fn);
    LOAD(retro_set_video_refresh,       retro_set_video_refresh_fn);
    LOAD(retro_set_audio_sample,        retro_set_audio_sample_fn);
    LOAD(retro_set_audio_sample_batch,  retro_set_audio_sample_batch_fn);
    LOAD(retro_set_input_poll,          retro_set_input_poll_fn);
    LOAD(retro_set_input_state,         retro_set_input_state_fn);
    LOAD(retro_init,                    retro_init_fn);
    LOAD(retro_deinit,                  retro_deinit_fn);
    LOAD(retro_get_system_info,         retro_get_system_info_fn);
    LOAD(retro_load_game,               retro_load_game_fn);
    LOAD(retro_unload_game,             retro_unload_game_fn);
    LOAD(retro_run,                     retro_run_fn);
    #undef LOAD

    printf("retro_api_version() = %u\n", retro_api_version());

    retro_set_environment(env_cb);
    retro_set_video_refresh(vr_cb);
    retro_set_audio_sample(as_cb);
    retro_set_audio_sample_batch(ab_cb);
    retro_set_input_poll(ip_cb);
    retro_set_input_state(is_cb);

    retro_init();

    struct retro_system_info info = {0};
    retro_get_system_info(&info);
    printf("library_name     = %s\n", info.library_name);
    printf("library_version  = %s\n", info.library_version);

    struct retro_game_info game = {0};
    game.path = argv[2];

    fprintf(stderr, "test_loader: calling retro_load_game\n");
    int loaded = retro_load_game(&game);
    printf("retro_load_game returned: %s\n", loaded ? "TRUE" : "FALSE");

    if (loaded) {
        fprintf(stderr, "test_loader: VM started — running for 20 seconds\n");
        // ~60 fps × 20 seconds = 1200 retro_run iterations
        for (int i = 0; i < 1200; ++i) {
            retro_run();
            usleep(16667); // ~60 Hz
        }
        fprintf(stderr, "test_loader: unloading game\n");
        retro_unload_game();
        fprintf(stderr, "test_loader: game unloaded\n");
    }

    retro_deinit();
    dlclose(h);
    return loaded ? 0 : 2;
}
