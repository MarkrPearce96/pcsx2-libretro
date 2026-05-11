// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// LibretroInputSource — InputSource subclass that reads libretro's
// retro_input_state_t (digital RETRO_DEVICE_JOYPAD + analog
// RETRO_DEVICE_ANALOG) and feeds events into PCSX2's PAD subsystem
// via InputManager::InvokeEvents.
//
// Polling is driven by InputManager::PollSources, which is called by
// VMManager once per frame. PollEvents calls g_frontend.input_poll_cb
// then queries g_frontend.input_state_cb for both ports (0, 1),
// diffs against cached state, and emits events only on changes.

#pragma once

#include "Input/InputSource.h"
#include "Input/InputManager.h"

#include <array>
#include <cstdint>

namespace Pcsx2Libretro
{

class LibretroInputSource final : public ::InputSource
{
public:
    LibretroInputSource();
    ~LibretroInputSource() override;

    bool Initialize(SettingsInterface& si, std::unique_lock<std::mutex>& settings_lock) override;
    void UpdateSettings(SettingsInterface& si, std::unique_lock<std::mutex>& settings_lock) override;
    bool ReloadDevices() override;
    void Shutdown() override;
    bool IsInitialized() override;

    void PollEvents() override;

    std::optional<InputBindingKey> ParseKeyString(const std::string_view device, const std::string_view binding) override;
    TinyString ConvertKeyToString(InputBindingKey key, bool display = false, bool migration = false) override;
    TinyString ConvertKeyToIcon(InputBindingKey key) override;

    std::vector<std::pair<std::string, std::string>> EnumerateDevices() override;
    std::vector<InputBindingKey> EnumerateMotors() override;
    bool GetGenericBindingMapping(const std::string_view device, InputManager::GenericInputBindingMapping* mapping) override;
    InputLayout GetControllerLayout(u32 index) override;
    void UpdateMotorState(InputBindingKey key, float intensity) override;

    static constexpr u32 NUM_PORTS = 2;
    static constexpr u32 NUM_DIGITAL = 16; // RETRO_DEVICE_ID_JOYPAD_B..R3 (0..15)
    static constexpr u32 NUM_ANALOG  = 6;  // L stick X/Y, R stick X/Y, L2, R2

    // Analog edge-detection threshold in raw int16 units (~0.2% of range).
    static constexpr int16_t ANALOG_THRESHOLD = 64;

private:
    bool m_initialized = false;

    // Per-port cached state used by PollEvents to detect edges.
    struct PortState
    {
        uint16_t prev_digital = 0;      // bit N = RETRO_DEVICE_ID_JOYPAD_N
        std::array<int16_t, NUM_ANALOG> prev_analog = {};
    };
    std::array<PortState, NUM_PORTS> m_ports = {};

    // One-shot diagnostic latch — first event fired anywhere logs to stderr.
    bool m_first_event_logged = false;

    // Helpers (implementation in .cpp).
    void PollPort(u32 port);
    void EmitDigitalEdges(u32 port, uint16_t new_digital);
    void EmitAnalogEdges(u32 port, const std::array<int16_t, NUM_ANALOG>& new_analog);

    // Index conventions for the analog cache:
    //   0 = LeftX, 1 = LeftY, 2 = RightX, 3 = RightY, 4 = L2, 5 = R2
    enum AnalogIndex : u32 { ANALOG_LX = 0, ANALOG_LY, ANALOG_RX, ANALOG_RY, ANALOG_L2, ANALOG_R2 };
};

} // namespace Pcsx2Libretro
