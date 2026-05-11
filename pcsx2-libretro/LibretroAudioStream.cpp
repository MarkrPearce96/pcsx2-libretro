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
    pxAssert(parameters.expansion_mode == AudioExpansionMode::Disabled);

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

u32 LibretroAudioStream::DrainToLibretroCallback(retro_audio_sample_batch_t /*cb*/, u32 /*max_frames*/)
{
    // Stub — implemented in Task 4. Returning 0 keeps the link valid and
    // produces silence (acceptable interim behavior).
    return 0;
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
