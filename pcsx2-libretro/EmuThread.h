// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// pcsx2-libretro emu thread.
//
// Owns a std::thread that drives PCSX2's full VM lifecycle:
//   VMManager::Internal::CPUThreadInitialize
//   VMManager::Initialize (returns StartupSuccess or failure)
//   VMManager::Execute (blocking — runs until SetState(Stopping))
//   VMManager::Shutdown
//   VMManager::Internal::CPUThreadShutdown
//
// retro_load_game starts the thread synchronously: it waits (with
// a generous timeout) until Initialize has reported success or
// failure, so it can return a meaningful true/false to libretro.

#pragma once

#include "pcsx2/VMManager.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace Pcsx2Libretro
{

class EmuThread
{
public:
    EmuThread();
    ~EmuThread();

    EmuThread(const EmuThread&) = delete;
    EmuThread& operator=(const EmuThread&) = delete;

    // Spawns the thread and waits for VMManager::Initialize to complete
    // (success or failure). Returns true iff Initialize returned
    // StartupSuccess. After this returns true, the VM is in the Running
    // state on the emu thread.
    bool Start(const VMBootParameters& boot_params);

    // Asks the VM to stop. Returns immediately; the actual shutdown
    // happens on the emu thread. Call Join() to wait for it.
    void RequestShutdown();

    // Blocks until the emu thread has exited. Idempotent.
    void Join();

    // True between successful Start() and Join() (or thread exit).
    bool IsRunning() const;

private:
    void ThreadFunc(VMBootParameters params);

    std::thread m_thread;
    std::mutex m_init_mutex;
    std::condition_variable m_init_cv;
    std::atomic<bool> m_init_done{false};
    std::atomic<bool> m_init_success{false};
    std::atomic<bool> m_thread_started{false};
};

// Singleton accessor — declared here, defined in EmuThread.cpp.
EmuThread& GetEmuThread();

} // namespace Pcsx2Libretro
