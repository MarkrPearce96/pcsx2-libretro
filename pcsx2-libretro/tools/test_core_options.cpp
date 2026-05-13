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

    // -------- Case 5: EmitCoreOptionsV2 dispatches the SP7b head triplet --------
    //
    // SP7c Phase 1+ appends more knobs, so the total count grows over time.
    // We assert >= 3 (sentinel) and pin the first three positions to the
    // SP7b order — same regression-sentinel pattern Case 6 adopted in
    // Task 1. Per-entry shape is covered by Case 7.
    fake::reset();
    fake::emit_returns = true;
    const bool emit_ok = EmitCoreOptionsV2(&fake_env_cb);
    check_bool("Case 5 emit returned true", emit_ok, true);
    check_int ("Case 5 emit was seen",
               static_cast<int>(fake::emit_seen ? 1 : 0), 1);
    check_bool("Case 5 emitted >= 3 keys",
               fake::emitted_keys.size() >= 3, true);
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

    // -------- Case 6: BuildDefinitions retains SP7b's 3 keys at the head, terminator at the tail --------
    //
    // SP7c Phase 1+ adds more knobs; this case is a regression sentinel,
    // not a count fixture. Per-knob sanity is covered by Case 7's data-driven
    // sweep. Three invariants:
    //   1. The first 3 keys are still pcsx2_renderer / pcsx2_mtvu / pcsx2_fast_boot
    //      in that order (SP7b call-site ordering must not regress).
    //   2. Total entries strictly greater than 4 once Phase 1 lands (>= 19 after
    //      all 15 Phase-1 knobs); Phase 0 leaves it at 4. We assert >= 4 here
    //      and let Case 7 catch sub-entry shape.
    //   3. The final entry is the libretro terminator (key == nullptr).
    {
        const auto& defs = BuildDefinitions();
        check_bool("Case 6 size >= 4 (SP7b minimum)",
                   defs.size() >= 4, true);

        if (defs.size() >= 4) {
            check_bool("Case 6 [0].key = pcsx2_renderer",
                       defs[0].key && std::strcmp(defs[0].key, "pcsx2_renderer") == 0, true);
            check_bool("Case 6 [1].key = pcsx2_mtvu",
                       defs[1].key && std::strcmp(defs[1].key, "pcsx2_mtvu") == 0, true);
            check_bool("Case 6 [2].key = pcsx2_fast_boot",
                       defs[2].key && std::strcmp(defs[2].key, "pcsx2_fast_boot") == 0, true);
            const auto& last = defs.back();
            check_bool("Case 6 last entry is terminator",
                       last.key == nullptr, true);
        }
    }

    // -------- Case 7: structural sanity for every definition --------
    //
    // For every non-terminator definition:
    //   - key is non-NULL
    //   - desc is non-NULL
    //   - default_value is non-NULL and appears in values[]
    //   - values[] has at least one non-terminator entry
    //   - values[] is itself terminated by {nullptr, nullptr}
    //
    // This catches accidental missing fields when adding new options.
    {
        const auto& defs = BuildDefinitions();
        std::map<std::string, int> key_counts;

        for (size_t i = 0; i + 1 < defs.size(); ++i) {
            const auto& d = defs[i];
            check_bool("Case 7 key non-null",   d.key != nullptr, true);
            check_bool("Case 7 desc non-null",  d.desc != nullptr, true);
            check_bool("Case 7 default non-null", d.default_value != nullptr, true);

            // Look up default in values[].
            bool default_in_values = false;
            int values_count = 0;
            for (const auto& vp : d.values) {
                if (vp.value == nullptr) break;
                ++values_count;
                if (d.default_value && std::strcmp(vp.value, d.default_value) == 0)
                    default_in_values = true;
            }
            check_bool("Case 7 values has at least 1 entry", values_count >= 1, true);
            check_bool("Case 7 default appears in values", default_in_values, true);

            if (d.key)
                ++key_counts[d.key];
        }

        // No duplicate keys.
        for (const auto& [k, c] : key_counts) {
            check_int(("Case 7 unique key " + k).c_str(), c, 1);
        }

        // The last entry must be the terminator.
        if (!defs.empty()) {
            const auto& last = defs.back();
            check_bool("Case 7 last entry is terminator", last.key == nullptr, true);
        }
    }

    // -------- Case 8: Speed Control round-trip --------
    //
    // SP7c Phase 1 representative test for the Speed Control sub-group.
    // We pick one knob per sub-group (Case 8/9/10) rather than testing all
    // 15 individually — Case 7's structural sweep already proves every
    // entry has well-formed values/default; this case proves the parse
    // path's float conversion works end-to-end.
    fake::reset();
    fake::variables["pcsx2_renderer"]            = "auto";
    fake::variables["pcsx2_mtvu"]                = "enabled";
    fake::variables["pcsx2_fast_boot"]           = "enabled";
    fake::variables["pcsx2_normal_speed"]        = "1.5";
    fake::variables["pcsx2_fast_forward_speed"]  = "4";
    fake::variables["pcsx2_slow_motion_speed"]   = "0.25";

    r = ReadResolved(&fake_env_cb);
    {
        // Float comparisons: literal "1.5" → 1.5f exactly under IEEE 754.
        const bool ns_ok  = r.emulation.normal_speed       == 1.5f;
        const bool ffs_ok = r.emulation.fast_forward_speed == 4.0f;
        const bool sms_ok = r.emulation.slow_motion_speed  == 0.25f;
        check_bool("Case 8 normal_speed=1.5",        ns_ok,  true);
        check_bool("Case 8 fast_forward_speed=4",    ffs_ok, true);
        check_bool("Case 8 slow_motion_speed=0.25",  sms_ok, true);
    }

    // Unparseable string falls back to default 1.0
    fake::reset();
    fake::variables["pcsx2_normal_speed"] = "not-a-number";
    r = ReadResolved(&fake_env_cb);
    check_bool("Case 8 garbled normal_speed → default 1.0",
               r.emulation.normal_speed == 1.0f, true);

    // -------- Case 9: System Settings round-trip --------
    fake::reset();
    fake::variables["pcsx2_renderer"]      = "auto";
    fake::variables["pcsx2_mtvu"]          = "enabled";
    fake::variables["pcsx2_fast_boot"]     = "enabled";
    fake::variables["pcsx2_ee_cycle_rate"] = "-1";
    fake::variables["pcsx2_ee_cycle_skip"] = "2";
    fake::variables["pcsx2_thread_pinning"]= "enabled";
    fake::variables["pcsx2_cheats"]        = "enabled";
    fake::variables["pcsx2_host_fs"]       = "disabled";
    fake::variables["pcsx2_cdvd_precache"] = "enabled";
    fake::variables["pcsx2_fast_boot_ff"]  = "enabled";

    r = ReadResolved(&fake_env_cb);
    check_int ("Case 9 ee_cycle_rate=-1",  r.emulation.ee_cycle_rate,  -1);
    check_int ("Case 9 ee_cycle_skip=2",   r.emulation.ee_cycle_skip,   2);
    check_bool("Case 9 thread_pinning",    r.emulation.thread_pinning,  true);
    check_bool("Case 9 cheats",            r.emulation.cheats,          true);
    check_bool("Case 9 host_fs=off",       r.emulation.host_fs,         false);
    check_bool("Case 9 cdvd_precache",     r.emulation.cdvd_precache,   true);
    check_bool("Case 9 fast_boot_ff",      r.emulation.fast_boot_ff,    true);

    std::printf("\n%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
