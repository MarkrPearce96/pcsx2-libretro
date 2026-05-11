// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+

#include "PrecompiledHeader.h"

#include "EmuThread.h"
#include "LibretroFrontend.h"

#include "common/Error.h"
#include "pcsx2/Host.h"
#include "pcsx2/VMManager.h"

#include "libretro.h"

#include <chrono>

namespace Pcsx2Libretro
{

namespace
{
    constexpr auto INIT_TIMEOUT = std::chrono::seconds(30);

    EmuThread g_emu_thread;
}

EmuThread& GetEmuThread()
{
    return g_emu_thread;
}

EmuThread::EmuThread() = default;

EmuThread::~EmuThread()
{
    if (m_thread.joinable())
    {
        RequestShutdown();
        Join();
    }
}

bool EmuThread::Start(const VMBootParameters& boot_params)
{
    if (m_thread.joinable())
    {
        FrontendLog(RETRO_LOG_ERROR, "EmuThread::Start called while thread already running");
        return false;
    }

    m_init_done.store(false);
    m_init_success.store(false);
    m_thread_started.store(false);

    m_thread = std::thread(&EmuThread::ThreadFunc, this, boot_params);

    // Wait for init handshake.
    std::unique_lock<std::mutex> lk(m_init_mutex);
    if (!m_init_cv.wait_for(lk, INIT_TIMEOUT, [this] { return m_init_done.load(); }))
    {
        FrontendLog(RETRO_LOG_ERROR, "EmuThread: VM init timed out after %llds",
                    static_cast<long long>(INIT_TIMEOUT.count()));
        // Thread is still running but we couldn't confirm init — request stop
        // via the safe flag path and join to clean up. (Same cross-thread
        // SetState concerns as RequestShutdown apply here.)
        m_stop_requested.store(true, std::memory_order_release);
        m_thread.join();
        return false;
    }

    return m_init_success.load();
}

void EmuThread::RequestShutdown()
{
    // SP3.5 Phase 2: do NOT call VMManager::SetState(VMState::Stopping)
    // directly. SetState invokes Cpu->ExitExecution() on the caller's
    // thread; ExitExecution manipulates JIT recompiler state, and doing
    // that from a non-CPU thread races PCSX2's IOP/EE recompilers,
    // deprotecting code pages while another thread is executing in them
    // (verified crash: instruction abort in JIT memory mid-shutdown).
    //
    // Just flip a flag. The emu thread polls it between Execute()
    // iterations and calls SetState(Stopping) from inside its own
    // ThreadFunc, mirroring the pcsx2-qt frontend's pattern of
    // QMetaObject::invokeMethod(..., Qt::QueuedConnection) when
    // shutdownVM is called from a non-emu thread.
    m_stop_requested.store(true, std::memory_order_release);
}

void EmuThread::Join()
{
    if (!m_thread.joinable()) return;

    // Wait briefly for the graceful path (flag-poll + SetState from the
    // CPU thread) to complete. VMManager::Execute() returns at natural
    // checkpoints; for a running game that's every frame, but the PS2
    // BIOS in an idle wait can leave the interpreter cycling on memory
    // reads for seconds before the next event-test yields. The grace
    // window keeps the common case clean.
    constexpr auto kGracefulWindow = std::chrono::seconds(3);
    constexpr auto kPollInterval   = std::chrono::milliseconds(50);
    const auto deadline = std::chrono::steady_clock::now() + kGracefulWindow;
    while (!m_thread_done.load(std::memory_order_acquire) &&
           std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(kPollInterval);
    }

    if (!m_thread_done.load(std::memory_order_acquire))
    {
        // Fallback: force Cpu->ExitExecution from this non-CPU thread.
        // SP3.5 Phase 2 noted this is racy with PCSX2's JIT recompiler
        // and CAN crash. We only take this path after the grace window
        // expired, so the failure mode degrades from "hang forever on
        // quit" to "crash sometimes on quit" — strictly better.
        FrontendLog(RETRO_LOG_WARN,
            "EmuThread: graceful shutdown timed out after %llds; forcing "
            "Cpu->ExitExecution. PCSX2 JIT race may cause a crash on this path.",
            static_cast<long long>(kGracefulWindow.count()));
        VMManager::SetState(VMState::Stopping);
    }

    m_thread.join();
}

bool EmuThread::IsRunning() const
{
    return m_thread.joinable() && m_init_success.load();
}

void EmuThread::ThreadFunc(VMBootParameters params)
{
    m_thread_started.store(true);

    // CPUThreadInitialize must precede any other VMManager call on the
    // CPU thread (gsrunner/Main.cpp:940).
    FrontendLog(RETRO_LOG_INFO, "EmuThread: calling CPUThreadInitialize");
    if (!VMManager::Internal::CPUThreadInitialize())
    {
        FrontendLog(RETRO_LOG_ERROR, "VMManager::Internal::CPUThreadInitialize failed");
        m_init_success.store(false);
        m_init_done.store(true);
        m_init_cv.notify_all();
        return;
    }
    FrontendLog(RETRO_LOG_INFO, "EmuThread: CPUThreadInitialize succeeded");

    // Apply settings after CPUThreadInitialize so the renderer / input-source
    // overrides actually take effect before VM Initialize. Matches gsrunner
    // pattern (gsrunner/Main.cpp:943).
    VMManager::ApplySettings();
    FrontendLog(RETRO_LOG_INFO, "EmuThread: calling VMManager::Initialize");

    Error err;
    const VMBootResult result = VMManager::Initialize(params, &err);
    if (result != VMBootResult::StartupSuccess)
    {
        FrontendLog(RETRO_LOG_ERROR, "VMManager::Initialize failed: %s",
                    err.GetDescription().c_str());
        VMManager::Internal::CPUThreadShutdown();
        m_init_success.store(false);
        m_init_done.store(true);
        m_init_cv.notify_all();
        return;
    }

    FrontendLog(RETRO_LOG_INFO, "VMManager::Initialize succeeded; entering Execute");
    m_init_success.store(true);
    m_init_done.store(true);
    m_init_cv.notify_all();

    // VMManager::Initialize leaves the VM in VMState::Paused. Without an
    // explicit SetState(Running), Cpu->Execute() returns immediately
    // because the state isn't Running. Mirrors gsrunner's pattern
    // (gsrunner/Main.cpp:950-957): set Running, then loop Execute while
    // the state stays Running. Execute can return for reasons other than
    // Stopping (e.g. Resetting), so the while-loop is necessary.
    //
    // SP3.5 Phase 2: check m_stop_requested BEFORE each Execute call.
    // If set, transition to Stopping from this thread (the only thread
    // safe to call Cpu->ExitExecution from). Execute returns naturally
    // at frame boundaries, so the maximum delay from RequestShutdown to
    // graceful stop is ~one frame (~16 ms at 60 Hz). Matches pcsx2-qt's
    // EmuThread::run() pattern where shutdownVM is queued to the emu
    // thread via QMetaObject::invokeMethod(Qt::QueuedConnection).
    VMManager::SetState(VMState::Running);
    while (VMManager::GetState() == VMState::Running)
    {
        if (m_stop_requested.load(std::memory_order_acquire))
        {
            VMManager::SetState(VMState::Stopping);
            break;
        }
        VMManager::Execute();
    }

    FrontendLog(RETRO_LOG_INFO, "VMManager::Execute returned; shutting down VM");
    VMManager::Shutdown(false);
    VMManager::Internal::CPUThreadShutdown();
    FrontendLog(RETRO_LOG_INFO, "EmuThread: clean exit");
    m_thread_done.store(true, std::memory_order_release);
}

} // namespace Pcsx2Libretro
