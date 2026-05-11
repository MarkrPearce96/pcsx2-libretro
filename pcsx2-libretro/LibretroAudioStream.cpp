// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// LibretroAudioStream — implementation. Mirrors SDLAudioStream.cpp's
// shape; the meaningful difference is the consumer is on the main
// (retro_run) thread instead of an audio-device callback thread.

#include "pcsx2/PrecompiledHeader.h"

#include "LibretroAudioStream.h"
#include "LibretroFrontend.h"

#include "common/Assertions.h"
#include "common/Error.h"

#include <algorithm>
#include <atomic>
#include <cstring>

namespace Pcsx2Libretro
{
namespace
{
    // Singleton pointer set by ctor / cleared by dtor. Atomic because the
    // EE thread's WriteChunk path doesn't read it (it goes through SPU2's
    // own s_output_stream pointer), but retro_run on the main thread must
    // see a coherent value relative to ctor/dtor on the CPU thread.
    std::atomic<LibretroAudioStream*> s_active_stream{nullptr};
} // namespace

LibretroAudioStream::LibretroAudioStream(u32 sample_rate, const AudioStreamParameters& parameters)
    : AudioStream(sample_rate, parameters)
{
    // expansion_mode is forced to Disabled by Settings.cpp — verify here.
    // Use pxAssertRel (not pxAssert) so the check survives in Release builds:
    // a violated invariant here corrupts audio (multichannel frames pumped
    // into a stereo libretro callback) rather than crashing, so silent
    // failure in production is the worst outcome.
    pxAssertRel(parameters.expansion_mode == AudioExpansionMode::Disabled,
        "LibretroAudioStream requires expansion_mode == Disabled (stereo only)");

    // Stereo path uses StereoSampleReaderImpl directly. stretch_enabled is
    // wired through by CreateLibretroAudioStream below.
    BaseInitialize(&AudioStream::StereoSampleReaderImpl, /*stretch_enabled=*/false);

    LibretroAudioStream* expected = nullptr;
    if (!s_active_stream.compare_exchange_strong(expected, this))
    {
        // Should never happen — SPU2 destroys the previous stream before
        // creating the new one (spu2.cpp:117). Log loudly if it does.
        FrontendLog(RETRO_LOG_ERROR,
            "LibretroAudioStream: s_active_stream already set on construction (was %p)",
            static_cast<void*>(expected));
    }

    FrontendLog(RETRO_LOG_INFO,
        "LibretroAudioStream constructed: sample_rate=%u channels=%u",
        sample_rate, GetInternalChannels());
}

LibretroAudioStream::~LibretroAudioStream()
{
    LibretroAudioStream* expected = this;
    s_active_stream.compare_exchange_strong(expected, nullptr);
    FrontendLog(RETRO_LOG_INFO, "LibretroAudioStream destroyed");
}

LibretroAudioStream* LibretroAudioStream::ActiveStream()
{
    return s_active_stream.load(std::memory_order_acquire);
}

u32 LibretroAudioStream::DrainToLibretroCallback(retro_audio_sample_batch_t cb, u32 max_frames)
{
    if (!cb)
        return 0;

    max_frames = std::min(max_frames, MAX_FRAMES_PER_DRAIN);

    // Step 1: flush any frames the frontend rejected on the previous call.
    // m_pending_frames is non-zero only if the previous cb() returned less
    // than we passed it. We push those first so order is preserved.
    if (m_pending_frames > 0)
    {
        const size_t accepted = cb(m_pending_buffer.data(), m_pending_frames);
        if (accepted < m_pending_frames)
        {
            // Frontend still backed up. Shift unaccepted frames to the front.
            const u32 remaining = static_cast<u32>(m_pending_frames - accepted);
            std::memmove(m_pending_buffer.data(),
                         m_pending_buffer.data() + accepted * 2,
                         remaining * 2 * sizeof(int16_t));
            m_pending_frames = remaining;
            return 0; // Don't try to drain new frames while backed up.
        }
        m_pending_frames = 0;
    }

    // Step 2: read up to max_frames new frames from the ring buffer into a
    // float staging buffer, then convert to int16 stereo.
    const u32 available = std::min(GetBufferedFramesRelaxed(), max_frames);
    if (available == 0)
        return 0;

    float float_staging[MAX_FRAMES_PER_DRAIN * 2];
    ReadFrames(float_staging, available);

    int16_t int_staging[MAX_FRAMES_PER_DRAIN * 2];
    const u32 sample_count = available * 2; // stereo
    for (u32 i = 0; i < sample_count; ++i)
    {
        const float f = std::clamp(float_staging[i], -1.0f, 1.0f);
        int_staging[i] = static_cast<int16_t>(f * 32767.0f);
    }

    // Step 3: push to libretro.
    const size_t accepted = cb(int_staging, available);

    // Step 4: stash any unaccepted tail for the next drain.
    if (accepted < available)
    {
        const u32 remaining = static_cast<u32>(available - accepted);
        std::memcpy(m_pending_buffer.data(),
                    int_staging + accepted * 2,
                    remaining * 2 * sizeof(int16_t));
        m_pending_frames = remaining;
    }

    if (!m_first_drain_logged)
    {
        FrontendLog(RETRO_LOG_INFO,
            "LibretroAudioStream first drain: %u frames pushed (frontend accepted %zu)",
            available, accepted);
        m_first_drain_logged = true;
    }

    return static_cast<u32>(accepted);
}

} // namespace Pcsx2Libretro

// Out-of-namespace factory matching the extern declaration in AudioStream.cpp.
std::unique_ptr<AudioStream> CreateLibretroAudioStream(u32 sample_rate,
    const AudioStreamParameters& parameters, bool stretch_enabled, Error* /*error*/)
{
    auto stream = std::make_unique<Pcsx2Libretro::LibretroAudioStream>(sample_rate, parameters);

    // BaseInitialize was called with stretch_enabled=false in the ctor.
    // If SPU2 wants stretch, re-init now via the public SetStretchEnabled.
    if (stretch_enabled)
        stream->SetStretchEnabled(true);

    return stream;
}
