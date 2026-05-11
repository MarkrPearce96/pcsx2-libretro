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

typedef void (*retro_init_fn)(void);
typedef void (*retro_deinit_fn)(void);
typedef unsigned (*retro_api_version_fn)(void);
typedef void (*retro_get_system_info_fn)(void*);
typedef void (*retro_set_environment_fn)(void* cb);
typedef int  (*retro_load_game_fn)(const void*);
typedef void (*retro_unload_game_fn)(void);

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

static int env_cb(unsigned cmd, void* data) {
    (void)cmd; (void)data;
    return 0; // refuse everything — keeps the core in defaults
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <core.dylib> [<game.iso>]\n", argv[0]);
        return 1;
    }
    void* h = dlopen(argv[1], RTLD_NOW);
    if (!h) { fprintf(stderr, "dlopen failed: %s\n", dlerror()); return 1; }

    #define LOAD(sym, type) type sym = (type)dlsym(h, #sym); \
        if (!sym) { fprintf(stderr, "missing symbol: %s\n", #sym); return 1; }
    LOAD(retro_api_version,       retro_api_version_fn);
    LOAD(retro_set_environment,   retro_set_environment_fn);
    LOAD(retro_init,              retro_init_fn);
    LOAD(retro_deinit,            retro_deinit_fn);
    LOAD(retro_get_system_info,   retro_get_system_info_fn);
    LOAD(retro_load_game,         retro_load_game_fn);
    LOAD(retro_unload_game,       retro_unload_game_fn);
    #undef LOAD

    printf("retro_api_version() = %u\n", retro_api_version());

    retro_set_environment(env_cb);
    retro_init();

    struct retro_system_info info = {0};
    retro_get_system_info(&info);
    printf("library_name     = %s\n", info.library_name);
    printf("library_version  = %s\n", info.library_version);
    printf("valid_extensions = %s\n", info.valid_extensions);

    if (argc >= 3) {
        struct retro_game_info game = {0};
        game.path = argv[2];
        int loaded = retro_load_game(&game);
        printf("retro_load_game returned: %s\n", loaded ? "TRUE" : "FALSE");
        if (loaded) retro_unload_game();
    }

    retro_deinit();
    dlclose(h);
    return 0;
}
