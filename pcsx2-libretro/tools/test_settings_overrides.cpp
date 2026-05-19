// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// Standalone unit test for QueryPathOverride and the env-override
// fallback contract in Settings::InitializeDefaults. Doesn't link
// PCSX2 — tests the helper in isolation.
//
// Build:
//   clang++ -std=c++20 test_settings_overrides.cpp -o test_settings_overrides
//   ./test_settings_overrides

#include <cstdio>
#include <cstring>
#include <string>

// Stand-in libretro env signature.
using retro_environment_t = bool (*)(unsigned cmd, void* data);
static constexpr unsigned RETRO_ENVIRONMENT_PRIVATE = 0x20000;
static constexpr unsigned RETRONEST_ENVIRONMENT_GET_MEMCARDS_DIR = (3u | RETRO_ENVIRONMENT_PRIVATE);
static constexpr unsigned RETRONEST_ENVIRONMENT_GET_TEXTURES_DIR = (4u | RETRO_ENVIRONMENT_PRIVATE);

// Helper under test (copy-paste-equivalent to the one in Settings.cpp).
// If the production helper changes, update this in lockstep.
static std::string QueryPathOverride(retro_environment_t cb, unsigned env_id)
{
    if (!cb) return {};
    const char* out = nullptr;
    if (cb(env_id, &out) && out && out[0] != '\0')
        return out;
    return {};
}

// Test scaffolding.
static int failures = 0;
static void expect(const std::string& got, const std::string& want, const char* label) {
    if (got != want) {
        std::fprintf(stderr, "FAIL %s: got=%s want=%s\n", label, got.c_str(), want.c_str());
        ++failures;
    }
}

// Mock env_cb factories.
static const char* g_mock_mc_value = nullptr;
static bool MockEnvWithMc(unsigned cmd, void* data) {
    if (cmd == RETRONEST_ENVIRONMENT_GET_MEMCARDS_DIR && g_mock_mc_value) {
        *static_cast<const char**>(data) = g_mock_mc_value;
        return true;
    }
    return false;
}
static bool MockEnvAlwaysFalse(unsigned, void*) { return false; }

int main() {
    expect(QueryPathOverride(nullptr, RETRONEST_ENVIRONMENT_GET_MEMCARDS_DIR), "",
           "null cb returns empty");
    expect(QueryPathOverride(MockEnvAlwaysFalse, RETRONEST_ENVIRONMENT_GET_MEMCARDS_DIR), "",
           "cb returns false -> empty");

    g_mock_mc_value = "/Volumes/Ext/memcards";
    expect(QueryPathOverride(MockEnvWithMc, RETRONEST_ENVIRONMENT_GET_MEMCARDS_DIR),
           "/Volumes/Ext/memcards",
           "cb returns true + path -> that path");
    expect(QueryPathOverride(MockEnvWithMc, RETRONEST_ENVIRONMENT_GET_TEXTURES_DIR), "",
           "cb returns false for wrong enum -> empty");

    g_mock_mc_value = "";
    expect(QueryPathOverride(MockEnvWithMc, RETRONEST_ENVIRONMENT_GET_MEMCARDS_DIR), "",
           "cb returns empty string -> treated as unset");

    if (failures == 0)
        std::printf("test_settings_overrides: OK (5/5)\n");
    else
        std::printf("test_settings_overrides: %d FAILURES\n", failures);
    return failures;
}
