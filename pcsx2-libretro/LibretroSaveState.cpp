// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// pcsx2-libretro — save state entry points (SP6.5).
//
// Task 1 lands the skeleton: pause-stable handshake helpers
// (lifted verbatim from reverted commit 2eddc63de) plus stub
// entry points that still return 0 / false. Tasks 2-4 fill in
// the libzip plumbing and the upstream load call.

#include "LibretroSaveState.h"
#include "LibretroFrontend.h"   // FrontendLog

#include "pcsx2/VMManager.h"
#include "common/Error.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <thread>

namespace Pcsx2Libretro
{

namespace
{

// Probe-once: cache the serialized size on first call while VM is
// Running, return that constant forever. 0 means "VM not yet ready,
// frontend should retry on next call". Reset in
// ResetSerializeSizeCache() (called from retro_unload_game).
std::atomic<size_t> g_serialize_size{0};

// RETRONEST_STATE_TRACE: env-gated tracing. Zero overhead when unset.
// Same pattern as IsStateTraceEnabled() in LibretroFrontend.cpp
// (which already covers the retro_reset boundary from SP6 Task 5).
// SP6.5 adds five more boundaries inside SerializeSize / Serialize /
// Unserialize — defined here so this translation unit owns its
// cached-bool independently of the frontend file.
bool IsStateTraceEnabled()
{
    static const bool s_enabled = (std::getenv("RETRONEST_STATE_TRACE") != nullptr);
    return s_enabled;
}

} // namespace

VMState WaitForVmPaused()
{
    using namespace std::chrono_literals;
    const VMState prev = VMManager::GetState();
    if (prev != VMState::Running)
    {
        // Already paused / not running. No handshake needed.
        return prev;
    }
    VMManager::SetPaused(true);
    const auto start = std::chrono::steady_clock::now();
    const auto deadline = start + 200ms;
    while (std::chrono::steady_clock::now() < deadline)
    {
        const VMState s = VMManager::GetState();
        if (s == VMState::Paused)
        {
            if (IsStateTraceEnabled())
            {
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start).count();
                FrontendLog(RETRO_LOG_INFO,
                    "[STATE_TRACE] WaitForVmPaused: paused in %lldms", static_cast<long long>(elapsed));
            }
            return prev;
        }
        if (s == VMState::Shutdown) return VMState::Shutdown;
        std::this_thread::sleep_for(1ms);
    }
    FrontendLog(RETRO_LOG_WARN,
        "WaitForVmPaused: 200 ms deadline exceeded — VMState=%d",
        static_cast<int>(VMManager::GetState()));
    return VMState::Shutdown;  // bail
}

void ResumeVm(VMState prev_state)
{
    if (prev_state == VMState::Running &&
        VMManager::GetState() == VMState::Paused)
    {
        VMManager::SetPaused(false);
    }
}

void ResetSerializeSizeCache()
{
    g_serialize_size.store(0);
}

// Task 1 stubs. Tasks 2-4 fill in the real implementations.
size_t SerializeSize() { return 0; }
bool   Serialize(void* /*dst*/, size_t /*len*/)         { return false; }
bool   Unserialize(const void* /*src*/, size_t /*len*/) { return false; }

} // namespace Pcsx2Libretro
