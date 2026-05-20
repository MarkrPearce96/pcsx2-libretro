// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// pcsx2-libretro — save state entry points (SP6.5).
//
// Wraps PCSX2's canonical SaveState_DownloadState path in an in-memory
// libzip container so retro_serialize / retro_unserialize round-trip
// cleanly without disk I/O. Probe-once size strategy: cache the
// serialized buffer size on first call while VM is Running, return
// that constant thereafter (deterministic by construction because
// every zip entry is ZIP_CM_STORE-compressed).
//
// See:
//   - SP6.5 spec: docs/superpowers/specs/2026-05-12-pcsx2-libretro-savestate-sp65-design.md
//   - Reverted predecessor: git show 2eddc63de -- pcsx2-libretro/LibretroFrontend.cpp

#pragma once

#include "pcsx2/VMManager.h"   // VMState enum

#include <cstddef>

namespace Pcsx2Libretro
{

// Pauses the VM and waits for it to reach VMState::Paused. Returns
// the state observed BEFORE the pause was requested, so the caller
// can restore it (we don't want to leave a Paused VM Running, or
// vice-versa).
//
// Polls in 1 ms increments up to a 200 ms ceiling. PCSX2-Qt uses the
// same handshake for its "save state while running" UI; the EE
// thread reaches the next event-test typically within a single
// frame (~16 ms). 200 ms catches a deeply stalled MTGS or recompiler
// edge case without locking the host indefinitely — on timeout we
// log a WARN and return VMState::Shutdown so the caller can bail.
//
// Caller must call ResumeVm(prev_state) regardless of whether the
// serialize succeeded.
VMState WaitForVmPaused();

// Restores the VM to prev_state (the value returned by
// WaitForVmPaused). If prev was Running, un-pause. Otherwise leave
// as-is.
void ResumeVm(VMState prev_state);

// Reset the probe-once size cache. Called from retro_unload_game so
// the next game loaded re-probes (different game = different ELF =
// potentially different size).
void ResetSerializeSizeCache();

// Libretro entry-point implementations. LibretroFrontend.cpp's
// retro_serialize_size / retro_serialize / retro_unserialize call
// straight through to these.
size_t SerializeSize();
bool   Serialize(void* dst, size_t len);
bool   Unserialize(const void* src, size_t len);

// SerializeSize reports a padded length to give Serialize headroom
// for cross-call PCSX2 state-size variance (probe-vs-serialize
// delta of up to a few MB observed on some titles, e.g. Looney Tunes
// Space Race PAL). The buffer Serialize fills is [real_zip || zeros
// || sentinel(magic+size)] — the trailing zeros prevent libzip from
// finding the EOCD when the saved buffer is written to disk and
// later read by the cold-boot resume path.
//
// TrimPaddedSaveStateFile rewrites the file in place to strip the
// pad, leaving a strict prefix that's a valid zip. Tries the
// sentinel first; falls back to backward-scanning for the zip EOCD
// signature so files written by earlier builds (pad without
// sentinel) also self-heal.
//
// Returns true if the file was trimmed, false if it was already
// the right size or didn't look padded.
bool TrimPaddedSaveStateFile(const char* path);

} // namespace Pcsx2Libretro
