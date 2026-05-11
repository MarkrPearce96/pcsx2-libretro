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
        // and join to clean up.
        VMManager::SetState(VMState::Stopping);
        m_thread.join();
        return false;
    }

    return m_init_success.load();
}

void EmuThread::RequestShutdown()
{
    if (m_thread.joinable())
    {
        // Setting state to Stopping causes Cpu->Execute() to return,
        // which causes VMManager::Execute() to return, which lets the
        // thread function fall through to Shutdown + CPUThreadShutdown.
        VMManager::SetState(VMState::Stopping);
    }
}

void EmuThread::Join()
{
    if (m_thread.joinable())
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
    if (!VMManager::Internal::CPUThreadInitialize())
    {
        FrontendLog(RETRO_LOG_ERROR, "VMManager::Internal::CPUThreadInitialize failed");
        m_init_success.store(false);
        m_init_done.store(true);
        m_init_cv.notify_all();
        return;
    }

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

    // Blocks until VMManager::SetState(Stopping) is called from another
    // thread (typically from RequestShutdown above).
    VMManager::Execute();

    FrontendLog(RETRO_LOG_INFO, "VMManager::Execute returned; shutting down VM");
    VMManager::Shutdown(false);
    VMManager::Internal::CPUThreadShutdown();
    FrontendLog(RETRO_LOG_INFO, "EmuThread: clean exit");
}

} // namespace Pcsx2Libretro
