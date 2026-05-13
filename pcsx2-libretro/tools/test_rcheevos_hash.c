/* Standalone tool: print rcheevos disc hash for a file path.
 *
 *   clang test_rcheevos_hash.c \
 *     -I/Users/mark/Documents/Projects/RetroNest-Project/cpp/build-arm64/_deps/rcheevos-src/include \
 *     /Users/mark/Documents/Projects/RetroNest-Project/cpp/build-arm64/librcheevos_static.a \
 *     -o test_rcheevos_hash
 *   ./test_rcheevos_hash 21 "/path/to/disc.iso"   # 21 = RC_CONSOLE_PLAYSTATION_2
 *
 * Investigates the rcheevos PAL hash-mismatch bug — see
 * memory/rcheevos_pal_hash_mismatch.md.
 */

#include "rc_hash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: %s <console_id> <disc_path>\n", argv[0]);
        return 2;
    }

    const uint32_t console_id = (uint32_t)strtoul(argv[1], NULL, 10);
    const char* path = argv[2];

    char hash[33] = {0};
    const int ok = rc_hash_generate_from_file(hash, console_id, path);
    if (!ok) {
        fprintf(stderr, "FAIL console_id=%u path=%s\n", console_id, path);
        return 1;
    }

    printf("%s  %s\n", hash, path);
    return 0;
}
