// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// Standalone unit test for Pcsx2Libretro::CoreOptions.
// Not built as part of pcsx2_libretro target — manual compile.
//
//   cd pcsx2-libretro/tools
//   clang++ -std=c++20 -I.. test_core_options.cpp ../CoreOptions.cpp \
//       -DSP7B_TEST_CORE_OPTIONS_ONLY -o test_core_options
//   ./test_core_options
//
// SP7B_TEST_CORE_OPTIONS_ONLY gates CoreOptions.cpp's FrontendLog
// dependency so the test links without the rest of pcsx2-libretro.

#include "../CoreOptions.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

using Pcsx2Libretro::CoreOptions::ReadResolved;
using Pcsx2Libretro::CoreOptions::EmitCoreOptionsV2;
using Pcsx2Libretro::CoreOptions::Resolved;

// ---------- Fake env_cb plumbing ----------
//
// libretro's environ_cb is a free C function pointer; we cannot pass
// captures into it. Use file-scope state and a regular function.

namespace fake {
    std::map<std::string, std::string> variables;  // GET_VARIABLE responses
    bool null_for_key = false;                     // if true, return NULL for variables[key]
    std::string null_key;
    bool emit_seen = false;                        // SET_CORE_OPTIONS_V2 was called
    std::vector<std::string> emitted_keys;         // captured for emit-test
    bool emit_returns = true;                      // what the host returns from SET_CORE_OPTIONS_V2

    void reset() {
        variables.clear();
        null_for_key = false;
        null_key.clear();
        emit_seen = false;
        emitted_keys.clear();
        emit_returns = true;
    }
}

static bool fake_env_cb(unsigned cmd, void* data)
{
    if (cmd == RETRO_ENVIRONMENT_GET_VARIABLE) {
        auto* v = static_cast<retro_variable*>(data);
        if (!v || !v->key) return false;
        if (fake::null_for_key && fake::null_key == v->key) {
            v->value = nullptr;
            return true;
        }
        auto it = fake::variables.find(v->key);
        if (it == fake::variables.end()) {
            v->value = nullptr;
            return false;
        }
        v->value = it->second.c_str();
        return true;
    }
    if (cmd == RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2) {
        fake::emit_seen = true;
        auto* opts = static_cast<retro_core_options_v2*>(data);
        if (opts && opts->definitions) {
            for (auto* d = opts->definitions; d->key != nullptr; ++d)
                fake::emitted_keys.emplace_back(d->key);
        }
        return fake::emit_returns;
    }
    return false;
}

static int failures = 0;
static void check_int(const char* label, int got, int want)
{
    const bool ok = (got == want);
    std::printf("[%s] %s: got=%d want=%d\n", ok ? "PASS" : "FAIL", label, got, want);
    if (!ok) ++failures;
}
static void check_bool(const char* label, bool got, bool want)
{
    const bool ok = (got == want);
    std::printf("[%s] %s: got=%s want=%s\n", ok ? "PASS" : "FAIL", label,
                got ? "true" : "false", want ? "true" : "false");
    if (!ok) ++failures;
}

int main()
{
    // -------- Case 1: happy path — all three keys present, non-default --------
    fake::reset();
    fake::variables["pcsx2_renderer"]  = "metal";
    fake::variables["pcsx2_mtvu"]      = "disabled";
    fake::variables["pcsx2_fast_boot"] = "disabled";

    Resolved r = ReadResolved(&fake_env_cb);
    check_int ("Case 1 renderer",  r.renderer,  17);
    check_bool("Case 1 mtvu",      r.mtvu,      false);
    check_bool("Case 1 fast_boot", r.fast_boot, false);

    // -------- Case 2: NULL value for one key — that field stays at default --------
    fake::reset();
    fake::variables["pcsx2_renderer"]  = "software";  // 13
    fake::null_for_key = true;
    fake::null_key = "pcsx2_mtvu";                    // → default true
    fake::variables["pcsx2_fast_boot"] = "disabled";  // → false

    r = ReadResolved(&fake_env_cb);
    check_int ("Case 2 renderer",  r.renderer,  13);
    check_bool("Case 2 mtvu",      r.mtvu,      true);   // default
    check_bool("Case 2 fast_boot", r.fast_boot, false);

    // -------- Case 3: unknown renderer enum → default -1 (Auto) --------
    fake::reset();
    fake::variables["pcsx2_renderer"]  = "vulkan";        // not in our schema
    fake::variables["pcsx2_mtvu"]      = "enabled";
    fake::variables["pcsx2_fast_boot"] = "enabled";

    r = ReadResolved(&fake_env_cb);
    check_int ("Case 3 renderer (unknown)", r.renderer, -1);
    check_bool("Case 3 mtvu",               r.mtvu,      true);
    check_bool("Case 3 fast_boot",          r.fast_boot, true);

    // -------- Case 4: all defaults — every key returns its declared default string --------
    fake::reset();
    fake::variables["pcsx2_renderer"]  = "auto";
    fake::variables["pcsx2_mtvu"]      = "enabled";
    fake::variables["pcsx2_fast_boot"] = "enabled";

    r = ReadResolved(&fake_env_cb);
    check_int ("Case 4 renderer",  r.renderer,  -1);
    check_bool("Case 4 mtvu",      r.mtvu,      true);
    check_bool("Case 4 fast_boot", r.fast_boot, true);

    std::printf("\n%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
