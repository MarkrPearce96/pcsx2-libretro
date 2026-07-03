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

    // SP10: bounded variant for retro_unload_game. Polls m_thread_done until
    // deadline; on success joins (fast) and returns true. On timeout DETACHES
    // the wedged thread and returns false — the caller must then mark the
    // core wedged (retronest_shutdown_wedged) so the host skips retro_deinit
    // and dlclose, keeping this dylib mapped for the detached thread's sake
    // (the SP3.6 "attempt 3" SIGBUS happened precisely because the host
    // dlclosed under a detached thread; with the host contract it is safe).
    bool JoinWithTimeout(unsigned timeout_ms);

    // True between successful Start() and Join() (or thread exit).
    bool IsRunning() const;

    // Toggle the host-initiated pause marker. Set true when RetroNest
    // is about to enter a long pause via WaitForVmPaused; clear when
    // resuming. Suppresses the EmuThread watchdog so legitimate
    // user-driven pauses don't get terminated as fatal-state pauses.
    void SetHostInitiatedPause(bool b) { m_host_initiated_pause.store(b, std::memory_order_release); }

private:
    void ThreadFunc(VMBootParameters params);

    std::thread m_thread;
    std::mutex m_init_mutex;
    std::condition_variable m_init_cv;
    std::atomic<bool> m_init_done{false};
    std::atomic<bool> m_init_success{false};
    std::atomic<bool> m_thread_started{false};

    // SP3.5 Phase 2: VMManager::SetState(Stopping) calls Cpu->ExitExecution()
    // which manipulates JIT recompiler state. That's only safe to invoke from
    // the CPU thread itself. The Qt frontend enforces this with
    // QMetaObject::invokeMethod(Qt::QueuedConnection) from any non-emu thread;
    // we mirror it with an atomic request flag that the emu thread polls
    // between Execute() iterations and only THEN transitions to Stopping.
    // RequestShutdown() just flips this flag.
    std::atomic<bool> m_stop_requested{false};
    // Set by ThreadFunc just before returning. Join() polls this with a
    // bounded timeout so a stalled CPU thread (BIOS idle waits where the
    // interpreter doesn't yield naturally) doesn't hang the host forever.
    std::atomic<bool> m_thread_done{false};

    // Set by retronest_set_paused (host-initiated pause via in-game
    // menu). When true, the watchdog in the main loop skips its
    // 500ms / 1000ms timeout — host pauses can legitimately last
    // arbitrarily long (user looking at the menu) without indicating
    // a fatal R5900 exception. Reset by retronest_set_paused(false)
    // when the menu closes and by retro_unload_game on session end.
    std::atomic<bool> m_host_initiated_pause{false};
};

// Singleton accessor — declared here, defined in EmuThread.cpp.
EmuThread& GetEmuThread();

} // namespace Pcsx2Libretro
