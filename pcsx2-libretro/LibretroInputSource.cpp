// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// LibretroInputSource — implementation. Skeleton; ParseKeyString /
// ConvertKeyToString / GetGenericBindingMapping land in Task 4;
// PollEvents lands in Task 5.

#include "PrecompiledHeader.h"

#include "LibretroInputSource.h"
#include "LibretroFrontend.h"

#include "common/SmallString.h"
#include "common/SettingsInterface.h"

namespace Pcsx2Libretro
{

LibretroInputSource::LibretroInputSource() = default;
LibretroInputSource::~LibretroInputSource() = default;

bool LibretroInputSource::Initialize(SettingsInterface& /*si*/, std::unique_lock<std::mutex>& /*lock*/)
{
    m_initialized = true;
    FrontendLog(RETRO_LOG_INFO, "LibretroInputSource initialized");
    return true;
}

void LibretroInputSource::UpdateSettings(SettingsInterface& /*si*/, std::unique_lock<std::mutex>& /*lock*/)
{
    // Nothing per-source to update; Settings.cpp's bindings are re-parsed by
    // InputManager::ReloadBindings independently of this call.
}

bool LibretroInputSource::ReloadDevices()
{
    // Libretro doesn't surface hotplug events. No devices to reload.
    return false;
}

void LibretroInputSource::Shutdown()
{
    m_initialized = false;
    m_ports = {};
    m_first_event_logged = false;
    FrontendLog(RETRO_LOG_INFO, "LibretroInputSource shutdown");
}

bool LibretroInputSource::IsInitialized()
{
    return m_initialized;
}

void LibretroInputSource::PollEvents()
{
    // Real implementation in Task 5. Stub leaves controller dead but
    // keeps the build valid.
}

std::optional<InputBindingKey> LibretroInputSource::ParseKeyString(
    const std::string_view /*device*/, const std::string_view /*binding*/)
{
    // Real implementation in Task 4. Returning nullopt means no bindings
    // resolve through us; PAD config is silently dropped. Test before
    // shipping by checking the diagnostic log fires (Task 5).
    return std::nullopt;
}

TinyString LibretroInputSource::ConvertKeyToString(InputBindingKey /*key*/, bool /*display*/, bool /*migration*/)
{
    return TinyString();
}

TinyString LibretroInputSource::ConvertKeyToIcon(InputBindingKey /*key*/)
{
    return TinyString();
}

std::vector<std::pair<std::string, std::string>> LibretroInputSource::EnumerateDevices()
{
    return {
        {"Libretro-0", "Libretro Pad 0"},
        {"Libretro-1", "Libretro Pad 1"},
    };
}

std::vector<InputBindingKey> LibretroInputSource::EnumerateMotors()
{
    return {}; // SP5: rumble deferred to SP5.5.
}

bool LibretroInputSource::GetGenericBindingMapping(
    const std::string_view /*device*/, InputManager::GenericInputBindingMapping* /*mapping*/)
{
    // Real implementation in Task 4.
    return false;
}

InputLayout LibretroInputSource::GetControllerLayout(u32 /*index*/)
{
    return InputLayout::Playstation;
}

void LibretroInputSource::UpdateMotorState(InputBindingKey /*key*/, float /*intensity*/)
{
    // SP5: rumble deferred to SP5.5. Drop motor writes silently.
}

void LibretroInputSource::PollPort(u32 /*port*/) {}
void LibretroInputSource::EmitDigitalEdges(u32 /*port*/, uint16_t /*new_digital*/) {}
void LibretroInputSource::EmitAnalogEdges(u32 /*port*/, const std::array<int16_t, NUM_ANALOG>& /*new_analog*/) {}

} // namespace Pcsx2Libretro
