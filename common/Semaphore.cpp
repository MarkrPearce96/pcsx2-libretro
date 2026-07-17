// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "common/Threading.h"

#include <chrono>
#include <thread>
#include "common/Assertions.h"
#include "common/HostSys.h"

#ifdef _WIN32
#include "common/RedtapeWindows.h"
#endif

#include <limits>

// --------------------------------------------------------------------------------------
//  Semaphore Implementations
// --------------------------------------------------------------------------------------

bool Threading::WorkSema::CheckForWork()
{
	s32 value = m_state.load(std::memory_order_relaxed);
	pxAssert(!IsDead(value));

	// we want to switch to the running state, but preserve the waiting empty bit for RUNNING_N -> RUNNING_0
	// otherwise, we clear the waiting flag (since we're notifying the waiter that we're empty below)
	while (!m_state.compare_exchange_weak(value,
		IsReadyForSleep(value) ? STATE_RUNNING_0 : (value & STATE_FLAG_WAITING_EMPTY),
		std::memory_order_acq_rel, std::memory_order_relaxed))
	{
	}

	// if we're not empty, we have work to do
	if (!IsReadyForSleep(value))
		return true;

	// this means we're empty, so notify any waiters
	if (value & STATE_FLAG_WAITING_EMPTY)
		m_empty_sema.Post();

	// no work to do
	return false;
}

void Threading::WorkSema::WaitForWork()
{
	// State change:
	// SLEEPING, SPINNING: This is the worker thread and it's clearly not asleep or spinning, so these states should be impossible
	// RUNNING_0: Change state to SLEEPING, wake up thread if WAITING_EMPTY
	// RUNNING_N: Change state to RUNNING_0 (and preserve WAITING_EMPTY flag)
	s32 value = m_state.load(std::memory_order_relaxed);
	pxAssert(!IsDead(value));
	while (!m_state.compare_exchange_weak(value, NextStateWaitForWork(value), std::memory_order_acq_rel, std::memory_order_relaxed))
		;
	if (IsReadyForSleep(value))
	{
		if (value & STATE_FLAG_WAITING_EMPTY)
			m_empty_sema.Post();
		m_sema.Wait();
		// Acknowledge any additional work added between wake up request and getting here
		m_state.fetch_and(STATE_FLAG_WAITING_EMPTY, std::memory_order_acquire);
	}
}

void Threading::WorkSema::WaitForWorkWithSpin()
{
	s32 value = m_state.load(std::memory_order_relaxed);
	pxAssert(!IsDead(value));
	while (IsReadyForSleep(value))
	{
		if (m_state.compare_exchange_weak(value, STATE_SPINNING, std::memory_order_release, std::memory_order_relaxed))
		{
			if (value & STATE_FLAG_WAITING_EMPTY)
				m_empty_sema.Post();
			value = STATE_SPINNING;
			break;
		}
	}
	u32 waited = 0;
	while (value < 0)
	{
		if (waited > SPIN_TIME_NS)
		{
			if (!m_state.compare_exchange_weak(value, STATE_SLEEPING, std::memory_order_relaxed))
				continue;
			m_sema.Wait();
			break;
		}
		waited += ShortSpin();
		value = m_state.load(std::memory_order_relaxed);
	}
	// Clear back to STATE_RUNNING_0 (but preserve waiting empty flag)
	m_state.fetch_and(STATE_FLAG_WAITING_EMPTY, std::memory_order_acquire);
}

bool Threading::WorkSema::WaitForEmpty()
{
	s32 value = m_state.load(std::memory_order_acquire);
	while (true)
	{
		if (value < 0)
			return !IsDead(value); // STATE_SLEEPING or STATE_SPINNING, queue is empty!
		// Note: We technically only need memory_order_acquire on *failure* (because that's when we could leave without sleeping), but libstdc++ still asserts on failure < success
		if (m_state.compare_exchange_weak(value, value | STATE_FLAG_WAITING_EMPTY, std::memory_order_acquire))
			break;
	}
	pxAssertMsg(!(value & STATE_FLAG_WAITING_EMPTY), "Multiple threads attempted to wait for empty (not currently supported)");
	// SP10 (libretro fork): self-healing wait — bounded sleeps with a state
	// recheck instead of one unbounded m_empty_sema.Wait(). Two field
	// deadlocks on this exact wait (MTGS 2026-07-03, then MTVU the same day
	// with the Reset() handoff fix already deployed) proved the wake-up can
	// be lost through an interleaving we could not fully pin down even with
	// live samples; with this loop a lost wake costs ~1 ms instead of
	// wedging VM shutdown forever. Exit conditions:
	//  - TryWait succeeds: the normal handoff worked.
	//  - state < 0: the worker is asleep/spinning, i.e. the queue IS empty —
	//    the condition we are waiting for holds. The worker posts BEFORE its
	//    sleep-CAS becomes observable-with-post (CAS -> post -> sleep), so
	//    give the in-flight post a moment, then drain any stale count so a
	//    FUTURE WaitForEmpty cannot consume it and return early.
	while (!m_empty_sema.TryWait())
	{
		const s32 cur = m_state.load(std::memory_order_acquire);
		if (cur < 0)
		{
			std::this_thread::sleep_for(std::chrono::microseconds(100));
			while (m_empty_sema.TryWait())
				;
			return !IsDead(cur);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	return !IsDead(m_state.load(std::memory_order_relaxed));
}

bool Threading::WorkSema::WaitForEmptyWithSpin()
{
	s32 value = m_state.load(std::memory_order_acquire);
	u32 waited = 0;
	while (true)
	{
		if (value < 0)
			return !IsDead(value); // STATE_SLEEPING or STATE_SPINNING, queue is empty!
		if (waited > SPIN_TIME_NS && m_state.compare_exchange_weak(value, value | STATE_FLAG_WAITING_EMPTY, std::memory_order_acquire))
			break;
		waited += ShortSpin();
		value = m_state.load(std::memory_order_acquire);
	}
	pxAssertMsg(!(value & STATE_FLAG_WAITING_EMPTY), "Multiple threads attempted to wait for empty (not currently supported)");
	// SP10: same self-healing wait as WaitForEmpty() above.
	while (!m_empty_sema.TryWait())
	{
		const s32 cur = m_state.load(std::memory_order_acquire);
		if (cur < 0)
		{
			std::this_thread::sleep_for(std::chrono::microseconds(100));
			while (m_empty_sema.TryWait())
				;
			return !IsDead(cur);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	return !IsDead(m_state.load(std::memory_order_relaxed));
}

void Threading::WorkSema::Kill()
{
	s32 value = m_state.exchange(std::numeric_limits<s32>::min(), std::memory_order_release);
	if (value & STATE_FLAG_WAITING_EMPTY)
		m_empty_sema.Post();
}

void Threading::WorkSema::Reset()
{
	// SP10 (libretro fork): preserve a pending empty-waiter across Reset.
	// The raw `m_state = STATE_RUNNING_0` store erased STATE_FLAG_WAITING_EMPTY
	// without posting m_empty_sema — the ONLY primitive that could drop the
	// flag without a post (Kill posts, WaitForWork's sleep transition posts).
	// Field deadlock (deterministic, God of War quit-from-menu, sampled
	// 2026-07-03): MTGS's close/reopen cycle (ThreadEntryPoint calls Reset
	// after GSclose) raced VMManager::Shutdown's WaitGS -> WaitForEmpty; the
	// waiter's flag was clobbered, the GS thread then slept without posting,
	// and the CPU thread waited on m_empty_sema forever. Handing off the flag
	// here wakes the waiter, which re-checks state and proceeds.
	const s32 old = m_state.exchange(STATE_RUNNING_0, std::memory_order_acq_rel);
	if (old & STATE_FLAG_WAITING_EMPTY)
		m_empty_sema.Post();
}

void Threading::UserspaceSemaphore::WaitWithSpin()
{
	// See header for rationale. The peek-and-CAS spin path is the win — when
	// the producer is about to Post (within ~50µs), we acquire in user space
	// and skip the futex syscall entirely.
	int32_t counter = m_counter.load(std::memory_order_relaxed);
	u32 waited = 0;
	while (true)
	{
		while (counter > 0)
		{
			if (m_counter.compare_exchange_weak(counter, counter - 1,
				std::memory_order_acquire, std::memory_order_relaxed))
			{
				return;
			}
		}
		if (waited >= SPIN_TIME_NS)
			break;
		waited += ShortSpin();
		counter = m_counter.load(std::memory_order_relaxed);
	}
	// Spin window expired — block in the kernel (same as plain Wait()).
	if (m_counter.fetch_sub(1, std::memory_order_acquire) <= 0)
		m_sema.Wait();
}

#if !defined(__APPLE__) // macOS implementations are in DarwinThreads

Threading::KernelSemaphore::KernelSemaphore()
{
#ifdef _WIN32
	m_sema = CreateSemaphore(nullptr, 0, LONG_MAX, nullptr);
#else
	sem_init(&m_sema, false, 0);
#endif
}

Threading::KernelSemaphore::~KernelSemaphore()
{
#ifdef _WIN32
	CloseHandle(m_sema);
#else
	sem_destroy(&m_sema);
#endif
}

void Threading::KernelSemaphore::Post()
{
#ifdef _WIN32
	ReleaseSemaphore(m_sema, 1, nullptr);
#else
	sem_post(&m_sema);
#endif
}

void Threading::KernelSemaphore::Wait()
{
#ifdef _WIN32
	WaitForSingleObject(m_sema, INFINITE);
#else
	sem_wait(&m_sema);
#endif
}

bool Threading::KernelSemaphore::TryWait()
{
#ifdef _WIN32
	return WaitForSingleObject(m_sema, 0) == WAIT_OBJECT_0;
#else
	return sem_trywait(&m_sema) == 0;
#endif
}

#endif
