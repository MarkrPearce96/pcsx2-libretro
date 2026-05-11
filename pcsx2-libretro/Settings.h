// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// pcsx2-libretro Settings layer.
//
// Owns a MemorySettingsInterface populated with the minimum keys
// PCSX2 needs to boot in skeleton/VM-lifecycle mode (sub-project 2):
// BIOS path, software renderer, nullout audio, achievements off,
// fast boot on. Registered as the active base settings layer via
// Host::Internal::SetBaseSettingsLayer before VMManager::Initialize.
//
// Sub-project 7 (Settings push) will replace this hardcoded layer
// with values driven from RetroNest's libretro options system.

#pragma once

#include <string>

class MemorySettingsInterface;

namespace Pcsx2Libretro::Settings
{

// Populate the underlying MemorySettingsInterface with PCSX2 defaults
// (via VMManager::SetDefaultSettings) and then override the SP2-required
// keys. Must be called exactly once before VMManager::Initialize.
//
// system_dir: the libretro system directory (where BIOS lives), obtained
// from RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY.
void InitializeDefaults(const std::string& system_dir);

// Access the populated settings interface. Returns nullptr before
// InitializeDefaults has been called. Pointer is stable for the
// lifetime of the dylib.
MemorySettingsInterface* GetActiveInterface();

} // namespace Pcsx2Libretro::Settings
