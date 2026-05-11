// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// LibretroAudioStream — PCSX2 AudioStream subclass that routes SPU2 output
// to libretro's retro_audio_sample_batch_t callback.
//
// SPU2 produces 64-frame stereo float chunks via WriteChunk on the EE/SPU2
// thread (inherited base behavior — runs SoundTouch stretch + writes to the
// lock-free ring buffer). LibretroFrontend's retro_run drains the ring on
// the main thread by calling DrainToLibretroCallback, which converts float
// to int16 stereo and pushes via the supplied callback.
//
// Lifetime: instances are owned by SPU2 (s_output_stream). Constructor sets
// the s_active_stream singleton; destructor clears it. retro_run checks
// ActiveStream() before draining so it cleanly skips when no VM is running.

#pragma once

#include "pcsx2/Host/AudioStream.h"
#include "libretro.h"

namespace Pcsx2Libretro
{

class LibretroAudioStream final : public ::AudioStream
{
public:
    LibretroAudioStream(u32 sample_rate, const AudioStreamParameters& parameters);
    ~LibretroAudioStream() override;

    // Drains up to max_frames stereo frames from the ring buffer, converts
    // float to int16, and calls cb with the result. Safe to call when the
    // ring is empty (no-op). Caller must guarantee cb is non-null.
    //
    // Returns the number of stereo frames the libretro frontend accepted.
    // Any unaccepted frames are re-queued for the next call via an internal
    // staging buffer.
    u32 DrainToLibretroCallback(retro_audio_sample_batch_t cb, u32 max_frames);

    // Returns the live LibretroAudioStream if SPU2 currently owns one,
    // or nullptr otherwise. retro_run uses this to decide whether to drain.
    static LibretroAudioStream* ActiveStream();

private:
    // Used by DrainToLibretroCallback to retry frames the frontend didn't
    // consume on the previous call.
    static constexpr u32 MAX_FRAMES_PER_DRAIN = 2048;

    u32 m_pending_frames = 0;
    int16_t m_pending_buffer[MAX_FRAMES_PER_DRAIN * 2] = {};

    // One-shot diagnostic latch.
    bool m_first_drain_logged = false;
};

} // namespace Pcsx2Libretro

// Free function declared in pcsx2/Host/AudioStream.cpp via extern; defined
// in LibretroAudioStream.cpp. Lives outside the namespace because that's
// the signature AudioStream.cpp expects.
std::unique_ptr<AudioStream> CreateLibretroAudioStream(u32 sample_rate,
    const AudioStreamParameters& parameters, bool stretch_enabled, Error* error);
