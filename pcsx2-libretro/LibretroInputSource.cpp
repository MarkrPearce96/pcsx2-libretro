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

#include "libretro.h" // RETRO_DEVICE_ID_JOYPAD_*

#include <array>
#include <charconv>
#include <cstring>

namespace
{

// Maps a libretro RETRO_DEVICE_ID_JOYPAD_* id (0..15) to the binding
// string we accept. Indexed directly by the libretro id. The same
// strings are used in Settings.cpp's PAD-binding output and in
// ParseKeyString below.
constexpr const char* kDigitalNames[16] = {
    "Cross",    // RETRO_DEVICE_ID_JOYPAD_B  = 0
    "Square",   // RETRO_DEVICE_ID_JOYPAD_Y  = 1
    "Select",   // RETRO_DEVICE_ID_JOYPAD_SELECT = 2
    "Start",    // RETRO_DEVICE_ID_JOYPAD_START  = 3
    "Up",       // RETRO_DEVICE_ID_JOYPAD_UP     = 4
    "Down",     // RETRO_DEVICE_ID_JOYPAD_DOWN   = 5
    "Left",     // RETRO_DEVICE_ID_JOYPAD_LEFT   = 6
    "Right",    // RETRO_DEVICE_ID_JOYPAD_RIGHT  = 7
    "Circle",   // RETRO_DEVICE_ID_JOYPAD_A  = 8
    "Triangle", // RETRO_DEVICE_ID_JOYPAD_X  = 9
    "L1",       // RETRO_DEVICE_ID_JOYPAD_L  = 10
    "R1",       // RETRO_DEVICE_ID_JOYPAD_R  = 11
    "L2",       // RETRO_DEVICE_ID_JOYPAD_L2 = 12  (digital fallback; analog path queries ANALOG_BUTTON)
    "R2",       // RETRO_DEVICE_ID_JOYPAD_R2 = 13
    "L3",       // RETRO_DEVICE_ID_JOYPAD_L3 = 14
    "R3",       // RETRO_DEVICE_ID_JOYPAD_R3 = 15
};

// Encoding of the analog half-axis in InputBindingKey.data:
//   Bit 0  : 1 = positive half (+), 0 = negative half (-)
//   Bits 1-3: 0=LeftX 1=LeftY 2=RightX 3=RightY 4=L2 5=R2
// Bits 1-3 align with the AnalogIndex enum in LibretroInputSource.h.
constexpr u32 AnalogKeyData(u32 analog_idx, bool positive)
{
    return (analog_idx << 1) | (positive ? 1u : 0u);
}

constexpr u32 AnalogKeyIndex(u32 data) { return data >> 1; }
constexpr bool AnalogKeyPositive(u32 data) { return (data & 1u) != 0; }

// Lookup tables for ParseKeyString / ConvertKeyToString.
struct AnalogNameEntry
{
    const char* name;
    u32 data;
};
constexpr AnalogNameEntry kAnalogNames[] = {
    {"+LeftX",  AnalogKeyData(0, true)},
    {"-LeftX",  AnalogKeyData(0, false)},
    {"+LeftY",  AnalogKeyData(1, true)},
    {"-LeftY",  AnalogKeyData(1, false)},
    {"+RightX", AnalogKeyData(2, true)},
    {"-RightX", AnalogKeyData(2, false)},
    {"+RightY", AnalogKeyData(3, true)},
    {"-RightY", AnalogKeyData(3, false)},
    {"+L2",     AnalogKeyData(4, true)},
    {"+R2",     AnalogKeyData(5, true)},
};

// Parses "Libretro-N" into N. Returns std::nullopt on mismatch.
std::optional<u32> ParsePortFromDevice(const std::string_view device)
{
    constexpr std::string_view prefix = "Libretro-";
    if (device.size() <= prefix.size() || device.substr(0, prefix.size()) != prefix)
        return std::nullopt;

    u32 port = 0;
    const auto first = device.data() + prefix.size();
    const auto last = device.data() + device.size();
    auto result = std::from_chars(first, last, port);
    if (result.ec != std::errc{} || result.ptr != last)
        return std::nullopt;
    if (port >= Pcsx2Libretro::LibretroInputSource::NUM_PORTS)
        return std::nullopt;
    return port;
}

} // namespace

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
    if (!m_initialized)
        return;

    // Libretro spec: input_poll_cb must be called once per frame before any
    // input_state_cb queries. RetroNest's trampoline is currently a no-op
    // (state is updated independently), but other frontends rely on this.
    if (g_frontend.input_poll_cb)
        g_frontend.input_poll_cb();

    if (!g_frontend.input_state_cb)
        return;

    for (u32 port = 0; port < NUM_PORTS; ++port)
        PollPort(port);
}

std::optional<InputBindingKey> LibretroInputSource::ParseKeyString(
    const std::string_view device, const std::string_view binding)
{
    const auto port = ParsePortFromDevice(device);
    if (!port.has_value())
        return std::nullopt;

    // Try digital names first.
    for (u32 i = 0; i < 16; ++i)
    {
        if (binding == kDigitalNames[i])
        {
            InputBindingKey key = {};
            key.source_type = InputSourceType::Libretro;
            key.source_index = *port;
            key.source_subtype = InputSubclass::ControllerButton;
            key.data = i;
            return key;
        }
    }

    // Then analog half-axes. data = analog index (0..5); modifier encodes
    // direction (Negate for "-..." names) so InputBindingKey::MaskDirection
    // strips direction for lookup — polled keys (from MakeGenericControllerAxisKey,
    // which produces modifier=None) and bound keys then hash to the same bucket.
    // The sign of the polled value (passed to InvokeEvents) chooses which half-axis fires.
    for (const auto& entry : kAnalogNames)
    {
        if (binding == entry.name)
        {
            InputBindingKey key = {};
            key.source_type = InputSourceType::Libretro;
            key.source_index = *port;
            key.source_subtype = InputSubclass::ControllerAxis;
            key.data = AnalogKeyIndex(entry.data);
            key.modifier = AnalogKeyPositive(entry.data) ? InputModifier::None : InputModifier::Negate;
            return key;
        }
    }

    // SP5.5: rumble motors. Naming + motor_index convention matches XInput /
    // SDL sources (LargeMotor = strong = data 0; SmallMotor = weak = data 1).
    if (binding == "LargeMotor")
        return MakeGenericControllerMotorKey(InputSourceType::Libretro, *port, 0);
    if (binding == "SmallMotor")
        return MakeGenericControllerMotorKey(InputSourceType::Libretro, *port, 1);

    return std::nullopt;
}

TinyString LibretroInputSource::ConvertKeyToString(InputBindingKey key, bool /*display*/, bool /*migration*/)
{
    if (key.source_type != InputSourceType::Libretro)
        return TinyString();

    if (key.source_subtype == InputSubclass::ControllerButton && key.data < 16)
    {
        TinyString result;
        result.format("Libretro-{}/{}", static_cast<u32>(key.source_index), kDigitalNames[key.data]);
        return result;
    }

    if (key.source_subtype == InputSubclass::ControllerAxis)
    {
        // Reverse of ParseKeyString's analog encoding: re-pack
        // (data, modifier) into the bit-packed kAnalogNames lookup key.
        const u32 data_encoded = AnalogKeyData(key.data, key.modifier != InputModifier::Negate);
        for (const auto& entry : kAnalogNames)
        {
            if (entry.data == data_encoded)
            {
                TinyString result;
                result.format("Libretro-{}/{}", static_cast<u32>(key.source_index), entry.name);
                return result;
            }
        }
    }

    if (key.source_subtype == InputSubclass::ControllerMotor)
    {
        const char* name = (key.data == 0) ? "LargeMotor" : "SmallMotor";
        TinyString result;
        result.format("Libretro-{}/{}", static_cast<u32>(key.source_index), name);
        return result;
    }

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
    // Two motors per port. Both ports are always "connected" from PCSX2's
    // perspective — libretro doesn't surface hotplug events, RetroNest
    // funnels controller state into ports 0 and 1 regardless. If the host
    // doesn't actually support rumble, set_rumble_state stays null and
    // UpdateMotorState no-ops; PCSX2's rumble writes are then harmless.
    std::vector<InputBindingKey> ret;
    ret.reserve(NUM_PORTS * 2);
    for (u32 port = 0; port < NUM_PORTS; ++port)
    {
        ret.push_back(MakeGenericControllerMotorKey(InputSourceType::Libretro, port, 0)); // Large
        ret.push_back(MakeGenericControllerMotorKey(InputSourceType::Libretro, port, 1)); // Small
    }
    return ret;
}

bool LibretroInputSource::GetGenericBindingMapping(
    const std::string_view device, InputManager::GenericInputBindingMapping* mapping)
{
    const auto port = ParsePortFromDevice(device);
    if (!port.has_value())
        return false;

    const std::string prefix = "Libretro-" + std::to_string(*port) + "/";

    // Digital bindings.
    mapping->emplace_back(GenericInputBinding::DPadUp,    prefix + "Up");
    mapping->emplace_back(GenericInputBinding::DPadRight, prefix + "Right");
    mapping->emplace_back(GenericInputBinding::DPadDown,  prefix + "Down");
    mapping->emplace_back(GenericInputBinding::DPadLeft,  prefix + "Left");
    mapping->emplace_back(GenericInputBinding::Cross,     prefix + "Cross");
    mapping->emplace_back(GenericInputBinding::Circle,    prefix + "Circle");
    mapping->emplace_back(GenericInputBinding::Square,    prefix + "Square");
    mapping->emplace_back(GenericInputBinding::Triangle,  prefix + "Triangle");
    mapping->emplace_back(GenericInputBinding::L1,        prefix + "L1");
    mapping->emplace_back(GenericInputBinding::R1,        prefix + "R1");
    mapping->emplace_back(GenericInputBinding::L2,        prefix + "+L2");
    mapping->emplace_back(GenericInputBinding::R2,        prefix + "+R2");
    mapping->emplace_back(GenericInputBinding::L3,        prefix + "L3");
    mapping->emplace_back(GenericInputBinding::R3,        prefix + "R3");
    mapping->emplace_back(GenericInputBinding::Start,     prefix + "Start");
    mapping->emplace_back(GenericInputBinding::Select,    prefix + "Select");
    // Analog stick half-axes.
    mapping->emplace_back(GenericInputBinding::LeftStickUp,    prefix + "-LeftY");
    mapping->emplace_back(GenericInputBinding::LeftStickDown,  prefix + "+LeftY");
    mapping->emplace_back(GenericInputBinding::LeftStickLeft,  prefix + "-LeftX");
    mapping->emplace_back(GenericInputBinding::LeftStickRight, prefix + "+LeftX");
    mapping->emplace_back(GenericInputBinding::RightStickUp,    prefix + "-RightY");
    mapping->emplace_back(GenericInputBinding::RightStickDown,  prefix + "+RightY");
    mapping->emplace_back(GenericInputBinding::RightStickLeft,  prefix + "-RightX");
    mapping->emplace_back(GenericInputBinding::RightStickRight, prefix + "+RightX");
    // SP5.5: rumble motor mappings — same names XInput / SDL sources use.
    mapping->emplace_back(GenericInputBinding::LargeMotor, prefix + "LargeMotor");
    mapping->emplace_back(GenericInputBinding::SmallMotor, prefix + "SmallMotor");
    return true;
}

InputLayout LibretroInputSource::GetControllerLayout(u32 /*index*/)
{
    return InputLayout::Playstation;
}

void LibretroInputSource::UpdateMotorState(InputBindingKey key, float intensity)
{
    // SP5.5: route into RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE's
    // set_rumble_state. Silently no-op if the host opted out (interface
    // not advertised at retro_init time → g_frontend.set_rumble_state
    // stays null) or if the key is malformed.
    if (!g_frontend.set_rumble_state)
        return;
    if (key.source_type != InputSourceType::Libretro)
        return;
    if (key.source_subtype != InputSubclass::ControllerMotor)
        return;
    if (key.source_index >= NUM_PORTS)
        return;

    // Convention matches XInput / SDL sources: data 0 = LargeMotor (strong);
    // any non-zero data = SmallMotor (weak).
    const retro_rumble_effect effect = (key.data == 0) ? RETRO_RUMBLE_STRONG : RETRO_RUMBLE_WEAK;
    const uint16_t strength = static_cast<uint16_t>(std::clamp(intensity, 0.0f, 1.0f) * 65535.0f);
    g_frontend.set_rumble_state(key.source_index, effect, strength);
}

void LibretroInputSource::PollPort(u32 port)
{
    // Digital: query all 16 RETRO_DEVICE_ID_JOYPAD_* bits.
    uint16_t new_digital = 0;
    for (u32 i = 0; i < NUM_DIGITAL; ++i)
    {
        const int16_t v = g_frontend.input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, i);
        if (v)
            new_digital |= (1u << i);
    }
    EmitDigitalEdges(port, new_digital);

    // Analog: 4 stick axes + 2 analog triggers.
    std::array<int16_t, NUM_ANALOG> new_analog{};
    new_analog[ANALOG_LX] = g_frontend.input_state_cb(port, RETRO_DEVICE_ANALOG,
        RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);
    new_analog[ANALOG_LY] = g_frontend.input_state_cb(port, RETRO_DEVICE_ANALOG,
        RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y);
    new_analog[ANALOG_RX] = g_frontend.input_state_cb(port, RETRO_DEVICE_ANALOG,
        RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X);
    new_analog[ANALOG_RY] = g_frontend.input_state_cb(port, RETRO_DEVICE_ANALOG,
        RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y);
    new_analog[ANALOG_L2] = g_frontend.input_state_cb(port, RETRO_DEVICE_ANALOG,
        RETRO_DEVICE_INDEX_ANALOG_BUTTON, RETRO_DEVICE_ID_JOYPAD_L2);
    new_analog[ANALOG_R2] = g_frontend.input_state_cb(port, RETRO_DEVICE_ANALOG,
        RETRO_DEVICE_INDEX_ANALOG_BUTTON, RETRO_DEVICE_ID_JOYPAD_R2);
    EmitAnalogEdges(port, new_analog);
}

void LibretroInputSource::EmitDigitalEdges(u32 port, uint16_t new_digital)
{
    auto& cached = m_ports[port].prev_digital;
    const uint16_t changed = cached ^ new_digital;
    if (changed == 0)
        return;

    for (u32 i = 0; i < NUM_DIGITAL; ++i)
    {
        const uint16_t mask = 1u << i;
        if (!(changed & mask))
            continue;

        const float value = (new_digital & mask) ? 1.0f : 0.0f;
        const InputBindingKey key = InputSource::MakeGenericControllerButtonKey(
            InputSourceType::Libretro, port, static_cast<s32>(i));
        InputManager::InvokeEvents(key, value);

        if (!m_first_event_logged)
        {
            FrontendLog(RETRO_LOG_INFO,
                "LibretroInputSource first event: port=%u key=Libretro-%u/%s value=%.3f",
                port, port, kDigitalNames[i], static_cast<double>(value));
            m_first_event_logged = true;
        }
    }

    cached = new_digital;
}

void LibretroInputSource::EmitAnalogEdges(u32 port, const std::array<int16_t, NUM_ANALOG>& new_analog)
{
    auto& cached = m_ports[port].prev_analog;

    for (u32 i = 0; i < NUM_ANALOG; ++i)
    {
        const int16_t v_new = new_analog[i];
        const int16_t v_old = cached[i];
        if (std::abs(static_cast<int>(v_new) - static_cast<int>(v_old)) < ANALOG_THRESHOLD)
            continue;

        // Normalize int16 to float in [-1.0, 1.0] using the same asymmetric
        // divisor PCSX2's SDLInputSource uses (NormalizeS16). Maps -32768 to
        // exactly -1.0 and 32767 to exactly 1.0 without needing a clamp.
        // For trigger axes (L2, R2) the value is already in [0, 32767];
        // the v_new < 0 branch is never taken for them.
        const float value = static_cast<float>(v_new) /
            (v_new < 0 ? 32768.0f : 32767.0f);

        const InputBindingKey key = InputSource::MakeGenericControllerAxisKey(
            InputSourceType::Libretro, port, static_cast<s32>(i));
        InputManager::InvokeEvents(key, value);

        cached[i] = v_new;

        if (!m_first_event_logged && std::abs(value) > 0.01f)
        {
            FrontendLog(RETRO_LOG_INFO,
                "LibretroInputSource first event: port=%u analog_idx=%u value=%.3f",
                port, i, static_cast<double>(value));
            m_first_event_logged = true;
        }
    }
}

} // namespace Pcsx2Libretro
