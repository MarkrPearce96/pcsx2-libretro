// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <string>

class MemorySettingsInterface;

namespace Pcsx2Libretro::CoreOptions { struct Resolved; }

namespace Pcsx2Libretro::Settings
{

// Populate the underlying MemorySettingsInterface with PCSX2 defaults
// and the SP2-required overrides. Must be called exactly once before
// VMManager::Initialize.
//
// system_dir: libretro system directory (where BIOS lives).
// save_dir:   libretro save directory (where memcards live).
// options:    optional pointer to user-resolved core options. When
//             non-null, overrides the hardcoded defaults for Renderer,
//             vuThread, and EnableFastBoot. When null, the SP7a-era
//             hardcoded defaults are written (defensive fallback).
void InitializeDefaults(const std::string& system_dir,
                        const std::string& save_dir,
                        const CoreOptions::Resolved* options = nullptr);

MemorySettingsInterface* GetActiveInterface();

} // namespace Pcsx2Libretro::Settings
