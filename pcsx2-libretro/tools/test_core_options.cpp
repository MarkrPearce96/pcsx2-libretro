// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// Standalone unit test for Pcsx2Libretro::CoreOptions.
// Not built as part of pcsx2_libretro target — manual compile.
//
//   cd pcsx2-libretro/tools
//   clang++ -std=c++20 -I.. test_core_options.cpp \
//       ../CoreOptions.cpp ../CoreOptionsEmulation.cpp \
//       -DCORE_OPTIONS_TEST_ONLY -o test_core_options
//   ./test_core_options
//
// CORE_OPTIONS_TEST_ONLY gates each CoreOptions*.cpp's FrontendLog and
// MemorySettingsInterface dependencies so the test links without the
// rest of pcsx2-libretro.

#include "../CoreOptions.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

using Pcsx2Libretro::CoreOptions::ReadResolved;
using Pcsx2Libretro::CoreOptions::EmitCoreOptionsV2;
using Pcsx2Libretro::CoreOptions::Resolved;
using Pcsx2Libretro::CoreOptions::BuildDefinitions;

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
    check_int ("Case 1 renderer",  r.emulation.renderer,  17);
    check_bool("Case 1 mtvu",      r.emulation.mtvu,      false);
    check_bool("Case 1 fast_boot", r.emulation.fast_boot, false);

    // -------- Case 2: NULL value for one key — that field stays at default --------
    fake::reset();
    fake::variables["pcsx2_renderer"]  = "software";  // 13
    fake::null_for_key = true;
    fake::null_key = "pcsx2_mtvu";                    // → default true
    fake::variables["pcsx2_fast_boot"] = "disabled";  // → false

    r = ReadResolved(&fake_env_cb);
    check_int ("Case 2 renderer",  r.emulation.renderer,  13);
    check_bool("Case 2 mtvu",      r.emulation.mtvu,      true);   // default
    check_bool("Case 2 fast_boot", r.emulation.fast_boot, false);

    // -------- Case 3: unknown renderer enum → default -1 (Auto) --------
    fake::reset();
    fake::variables["pcsx2_renderer"]  = "vulkan";        // not in our schema
    fake::variables["pcsx2_mtvu"]      = "enabled";
    fake::variables["pcsx2_fast_boot"] = "enabled";

    r = ReadResolved(&fake_env_cb);
    check_int ("Case 3 renderer (unknown)", r.emulation.renderer, -1);
    check_bool("Case 3 mtvu",               r.emulation.mtvu,      true);
    check_bool("Case 3 fast_boot",          r.emulation.fast_boot, true);

    // -------- Case 4: all defaults — every key returns its declared default string --------
    fake::reset();
    fake::variables["pcsx2_renderer"]  = "auto";
    fake::variables["pcsx2_mtvu"]      = "enabled";
    fake::variables["pcsx2_fast_boot"] = "enabled";

    r = ReadResolved(&fake_env_cb);
    check_int ("Case 4 renderer",  r.emulation.renderer,  -1);
    check_bool("Case 4 mtvu",      r.emulation.mtvu,      true);
    check_bool("Case 4 fast_boot", r.emulation.fast_boot, true);

    // -------- Case 5: EmitCoreOptionsV2 dispatches all three keys --------
    fake::reset();
    fake::emit_returns = true;
    const bool emit_ok = EmitCoreOptionsV2(&fake_env_cb);
    check_bool("Case 5 emit returned true", emit_ok, true);
    check_int ("Case 5 emit was seen",
               static_cast<int>(fake::emit_seen ? 1 : 0), 1);
    check_int ("Case 5 emitted 3 keys",
               static_cast<int>(fake::emitted_keys.size()), 3);
    // Order matches the kDefinitions[] table order in CoreOptions.cpp.
    auto str_eq = [](const std::string& a, const char* b) { return a == b; };
    check_bool("Case 5 key 0 = pcsx2_renderer",
               !fake::emitted_keys.empty()
               && str_eq(fake::emitted_keys[0], "pcsx2_renderer"), true);
    check_bool("Case 5 key 1 = pcsx2_mtvu",
               fake::emitted_keys.size() > 1
               && str_eq(fake::emitted_keys[1], "pcsx2_mtvu"), true);
    check_bool("Case 5 key 2 = pcsx2_fast_boot",
               fake::emitted_keys.size() > 2
               && str_eq(fake::emitted_keys[2], "pcsx2_fast_boot"), true);

    // -------- Case 6: BuildDefinitions returns the expected category structure --------
    //
    // SP7c Phase 0: assert that the master definitions table built by
    // BuildDefinitions() contains exactly the 3 SP7b knobs in the documented
    // order, plus the libretro terminator. Future SP7c phases extend this.
    {
        const auto& defs = BuildDefinitions();
        check_int("Case 6 count = 4 (3 knobs + terminator)",
                  static_cast<int>(defs.size()), 4);

        if (defs.size() >= 4) {
            check_bool("Case 6 [0].key = pcsx2_renderer",
                       defs[0].key && std::strcmp(defs[0].key, "pcsx2_renderer") == 0, true);
            check_bool("Case 6 [1].key = pcsx2_mtvu",
                       defs[1].key && std::strcmp(defs[1].key, "pcsx2_mtvu") == 0, true);
            check_bool("Case 6 [2].key = pcsx2_fast_boot",
                       defs[2].key && std::strcmp(defs[2].key, "pcsx2_fast_boot") == 0, true);
            check_bool("Case 6 [3] terminator (key == nullptr)",
                       defs[3].key == nullptr, true);
        }
    }

    std::printf("\n%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
