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
#include <thread>

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
    //
    // SP3.6 NOTE: do NOT call Cpu->ExitExecution() from this non-CPU
    // thread to nudge Execute. When the EE is using the interpreter
    // (which is the active path in this build per crash diagnostics),
    // the dispatch is intSafeExitExecution which does fastjmp_jmp from
    // the caller's frame in the eeEventTestIsActive==false branch —
    // that's a longjmp into the CPU thread's saved jmp buffer using
    // OUR thread's stack/registers, which is undefined behavior and
    // produces a SIGBUS shortly after. The recompiler's variant
    // (recSafeExitExecution) IS safe (just writes a flag), but PCSX2
    // doesn't expose a thread-safe "request exit" API that works
    // across both interpreter and recompiler dispatch paths. SP3.6
    // attempt 1 (calling Cpu->ExitExecution unconditionally) was
    // verified to crash via this exact path.
    m_stop_requested.store(true, std::memory_order_release);

    // SP6.5 Task 4.6: nudge Cpu->Execute to return via the SP6.5-proven
    // pause path so emu.Join() doesn't block indefinitely on a busy EE
    // (which was the SP3.6 "Quit hangs UI worst-case" symptom — user
    // had to Cmd+Q RetroNest from the dock to recover; UX-visibly,
    // Save & Exit wrote the .resume file but never transitioned UI
    // to home because GameSession::finished was blocked on this Join).
    //
    // VMManager::SetPaused(true) routes through VMManager::SetState
    // which is thread-safe from any thread — SP6.5 task 3 verified
    // this live, calling SetPaused from the libretro worker thread
    // 5+ times mid-session for save state with no crash. The pause
    // request flows through PCSX2's CPU dispatch in a way that
    // properly exits Execute on the CPU thread itself, instead of
    // longjmping from the caller's stack like Cpu->ExitExecution does.
    //
    // The EmuThread loop sees state=Paused, sleeps 1 ms, wakes, sees
    // m_stop_requested true at the top of the loop, sets Stopping,
    // breaks. emu.Join() unblocks within ~1 frame.
    if (VMManager::HasValidVM())
        VMManager::SetPaused(true);
}

void EmuThread::Join()
{
    if (!m_thread.joinable()) return;

    // Wait for the CPU thread to exit naturally. The graceful path is:
    // m_stop_requested is polled between Execute() iterations; CPU thread
    // calls SetState(Stopping) on itself; Execute returns at the next
    // safe checkpoint; ThreadFunc finishes Shutdown + signals m_thread_done.
    // For a running game Execute returns every frame, so the wait is
    // ~one frame.
    //
    // SP3.6 (parked — three attempts, all fail):
    //
    //   1. Call Cpu->ExitExecution from non-CPU thread to nudge Execute.
    //      → Crashes immediately. With EE on interpreter (active in this
    //      build), Cpu->ExitExecution dispatches to intSafeExitExecution
    //      whose !eeEventTestIsActive branch does fastjmp_jmp from the
    //      caller's frame — undefined behavior across threads.
    //
    //   2. Same wait + indefinite m_thread.join().
    //      → No crash on quit click, but Execute may never return (e.g.
    //      R&C 2's memory-card busy-wait screen). UI freezes; if the
    //      user clicks the menu again RetroNest's GameSession cleanup
    //      races and SIGSEGVs in LibretroMetalItem::~LibretroMetalItem.
    //
    //   3. Bounded wait then m_thread.detach().
    //      → retro_unload_game returns, but RetroNest's CoreRuntime then
    //      runs retro_deinit + m_loader.close() (dlclose). The detached
    //      EmuThread is still executing inside the now-unmapped dylib
    //      pages → SIGBUS shortly after.
    //
    // The "JIT race in IOP memory" attribution in the SP3.5 spec was
    // incorrect — it's actually an interpreter-mode longjmp-from-wrong-
    // thread bug. The proper upstream fix is for PCSX2 to expose a
    // thread-safe "request exit" API that works for both recompiler and
    // interpreter dispatch. Until then, the chosen behaviour here is
    // attempt 2: indefinite wait. Quit hangs the UI in the worst case
    // (user must Cmd+Q RetroNest from the dock to recover) but does not
    // crash on the quit click itself, which is strictly better than the
    // pre-attempt baseline that crashed via the forced-fallback SetState.
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
    // (gsrunner/Main.cpp:950-957): set Running, then loop Execute. The
    // loop is necessary because VMManager::Execute returns on any
    // non-Running transition (Resetting, Paused — see VMManager.cpp:2756
    // → Cpu->Execute), not only terminal states.
    //
    // SP3.5 Phase 2: check m_stop_requested BEFORE each Execute call.
    // If set, transition to Stopping from this thread (the only thread
    // safe to call Cpu->ExitExecution from). Execute returns naturally
    // at frame boundaries, so the maximum delay from RequestShutdown to
    // graceful stop is ~one frame (~16 ms at 60 Hz). Matches pcsx2-qt's
    // EmuThread::run() pattern where shutdownVM is queued to the emu
    // thread via QMetaObject::invokeMethod(Qt::QueuedConnection).
    //
    // SP6.5 prep: the loop exit condition was `state == Running`, which
    // mistreated VMState::Paused (set by SP6.5's WaitForVmPaused
    // save-state handshake) as a shutdown signal — Cpu->Execute returns
    // on pause, loop exit fires, VMManager::Shutdown runs, and the save
    // never gets to write through a live VM. Loop now exits only on
    // Stopping/Shutdown; Paused sleeps briefly then re-checks (the
    // save-state caller flips state back to Running and the next Execute
    // resumes the EE thread). Resetting is treated like Running — re-
    // enter Execute, which handles the reset internally and returns once
    // state stabilizes.
    VMManager::SetState(VMState::Running);
    while (true)
    {
        if (m_stop_requested.load(std::memory_order_acquire))
        {
            VMManager::SetState(VMState::Stopping);
            break;
        }
        const VMState s = VMManager::GetState();
        if (s == VMState::Stopping || s == VMState::Shutdown)
            break;
        if (s == VMState::Paused)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
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
