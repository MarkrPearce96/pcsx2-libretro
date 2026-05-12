// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// pcsx2-libretro — save state entry points (SP6.5).
//
// Task 1 lands the skeleton: pause-stable handshake helpers
// (lifted verbatim from reverted commit 2eddc63de) plus stub
// entry points that still return 0 / false. Tasks 2-4 fill in
// the libzip plumbing and the upstream load call.

#include "LibretroSaveState.h"
#include "LibretroFrontend.h"   // FrontendLog, g_frontend

#include "pcsx2/VMManager.h"
#include "pcsx2/SaveState.h"    // SaveState_DownloadState, ArchiveEntryList, SaveState_UnzipFromMemory
#include "common/Error.h"

#include <zip.h>                // libzip

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace Pcsx2Libretro
{

namespace
{

// Probe-once: cache the serialized size on first call while VM is
// Running, return that constant forever. 0 means "VM not yet ready,
// frontend should retry on next call". Reset in
// ResetSerializeSizeCache() (called from retro_unload_game).
std::atomic<size_t> g_serialize_size{0};

// RETRONEST_STATE_TRACE: env-gated tracing. Zero overhead when unset.
// Same pattern as IsStateTraceEnabled() in LibretroFrontend.cpp
// (which already covers the retro_reset boundary from SP6 Task 5).
// SP6.5 adds five more boundaries inside SerializeSize / Serialize /
// Unserialize — defined here so this translation unit owns its
// cached-bool independently of the frontend file.
bool IsStateTraceEnabled()
{
    static const bool s_enabled = (std::getenv("RETRONEST_STATE_TRACE") != nullptr);
    return s_enabled;
}

// A libzip source backed by a std::vector<u8>. Write-only — Task 4
// uses zip_open_buffer_managed (from common/ZipHelpers.h) directly
// for the read path via the upstream SaveState_UnzipFromMemory call,
// so we don't need full read/seek/write callback dispatch in this
// sink for now.
//
// Lifetime: caller constructs a MemoryWriteSink, calls
// AcquireSource() once (which returns a zip_source_t* whose
// ownership transfers to the zip_t* on successful
// zip_open_from_source), then after zip_close the caller takes
// sink.bytes by reference.
//
// On zip_open_from_source FAILURE the source is not transferred;
// the caller must zip_source_free(...) the returned pointer to
// avoid a leak. Standard libzip ownership contract.
struct MemoryWriteSink
{
    std::vector<u8> bytes;
    size_t cursor = 0;   // libzip writes/seeks within bytes

    // libzip callback. cmd is one of the ZIP_SOURCE_* opcodes.
    static zip_int64_t Callback(void* userdata, void* data, zip_uint64_t length, zip_source_cmd_t cmd)
    {
        MemoryWriteSink* self = static_cast<MemoryWriteSink*>(userdata);
        switch (cmd)
        {
        case ZIP_SOURCE_OPEN:
            self->cursor = 0;
            return 0;
        case ZIP_SOURCE_READ:
        {
            if (self->cursor >= self->bytes.size()) return 0;
            const zip_uint64_t avail = self->bytes.size() - self->cursor;
            const zip_uint64_t n = (length < avail) ? length : avail;
            std::memcpy(data, self->bytes.data() + self->cursor, n);
            self->cursor += n;
            return static_cast<zip_int64_t>(n);
        }
        case ZIP_SOURCE_CLOSE:
            return 0;
        case ZIP_SOURCE_STAT:
        {
            zip_stat_t* st = static_cast<zip_stat_t*>(data);
            zip_stat_init(st);
            st->size = self->bytes.size();
            st->valid = ZIP_STAT_SIZE;
            return sizeof(*st);
        }
        case ZIP_SOURCE_ERROR:
        {
            int* errs = static_cast<int*>(data);
            errs[0] = errs[1] = 0;
            return 2 * sizeof(int);
        }
        case ZIP_SOURCE_FREE:
            // MemoryWriteSink is stack-owned by the caller; nothing to free.
            return 0;
        case ZIP_SOURCE_BEGIN_WRITE:
            self->bytes.clear();
            self->cursor = 0;
            return 0;
        case ZIP_SOURCE_WRITE:
        {
            if (self->cursor + length > self->bytes.size())
                self->bytes.resize(self->cursor + length);
            std::memcpy(self->bytes.data() + self->cursor, data, length);
            self->cursor += length;
            return static_cast<zip_int64_t>(length);
        }
        case ZIP_SOURCE_COMMIT_WRITE:
            // Truncate any tail past cursor (in case write-then-shrink happened).
            self->bytes.resize(self->cursor);
            return 0;
        case ZIP_SOURCE_ROLLBACK_WRITE:
            self->bytes.clear();
            self->cursor = 0;
            return 0;
        case ZIP_SOURCE_REMOVE:
            self->bytes.clear();
            self->cursor = 0;
            return 0;
        case ZIP_SOURCE_SEEK:
        {
            zip_int64_t off = zip_source_seek_compute_offset(self->cursor, self->bytes.size(), data, length, nullptr);
            if (off < 0) return -1;
            self->cursor = static_cast<size_t>(off);
            return 0;
        }
        case ZIP_SOURCE_SEEK_WRITE:
        {
            zip_int64_t off = zip_source_seek_compute_offset(self->cursor, self->bytes.size(), data, length, nullptr);
            if (off < 0) return -1;
            self->cursor = static_cast<size_t>(off);
            return 0;
        }
        case ZIP_SOURCE_TELL:
            return static_cast<zip_int64_t>(self->cursor);
        case ZIP_SOURCE_TELL_WRITE:
            return static_cast<zip_int64_t>(self->cursor);
        case ZIP_SOURCE_SUPPORTS:
            return zip_source_make_command_bitmap(
                ZIP_SOURCE_OPEN, ZIP_SOURCE_READ, ZIP_SOURCE_CLOSE, ZIP_SOURCE_STAT,
                ZIP_SOURCE_ERROR, ZIP_SOURCE_FREE, ZIP_SOURCE_SEEK, ZIP_SOURCE_TELL,
                ZIP_SOURCE_BEGIN_WRITE, ZIP_SOURCE_WRITE, ZIP_SOURCE_COMMIT_WRITE,
                ZIP_SOURCE_ROLLBACK_WRITE, ZIP_SOURCE_SEEK_WRITE, ZIP_SOURCE_TELL_WRITE,
                ZIP_SOURCE_REMOVE, ZIP_SOURCE_SUPPORTS, -1);
        default:
            return -1;
        }
    }

    zip_source_t* AcquireSource()
    {
        zip_error_t ze = {};
        zip_source_t* zs = zip_source_function_create(&MemoryWriteSink::Callback, this, &ze);
        if (!zs)
        {
            FrontendLog(RETRO_LOG_WARN, "MemoryWriteSink::AcquireSource: %s", zip_error_strerror(&ze));
        }
        return zs;
    }
};

// Drives SaveState_DownloadState, then walks the returned
// ArchiveEntryList writing each entry into an in-memory zip with
// forced ZIP_CM_STORE compression. Returns true on success and
// leaves the finalized zip bytes in sink.bytes. Returns false and
// leaves sink.bytes in unspecified state on any failure (caller
// should bail).
//
// Pre-condition: caller has paused the VM (WaitForVmPaused).
bool BuildZipIntoSink(MemoryWriteSink& sink)
{
    Error err;
    std::unique_ptr<ArchiveEntryList> srclist = SaveState_DownloadState(&err);
    if (!srclist)
    {
        FrontendLog(RETRO_LOG_WARN,
            "BuildZipIntoSink: SaveState_DownloadState failed: %s",
            err.GetDescription().c_str());
        return false;
    }

    zip_source_t* src = sink.AcquireSource();
    if (!src) return false;

    zip_error_t ze = {};
    zip_t* zf = zip_open_from_source(src, ZIP_CREATE | ZIP_TRUNCATE, &ze);
    if (!zf)
    {
        FrontendLog(RETRO_LOG_WARN,
            "BuildZipIntoSink: zip_open_from_source: %s", zip_error_strerror(&ze));
        zip_source_free(src);   // not transferred when zip_open_from_source fails
        return false;
    }
    // src ownership now belongs to zf — do NOT zip_source_free below.

    // Write each ArchiveEntry as a ZIP_CM_STORE file entry. Buffer
    // slices come from srclist's contiguous VmStateBuffer; we hand
    // libzip a non-owning view (freep=0) since srclist outlives the
    // call.
    bool ok = true;
    const u8* base = srclist->GetBuffer().data();
    for (uint i = 0; i < srclist->GetLength(); ++i)
    {
        const ArchiveEntry& e = (*srclist)[i];

        zip_source_t* es = zip_source_buffer(zf, base + e.GetDataIndex(), e.GetDataSize(), /*freep=*/0);
        if (!es)
        {
            FrontendLog(RETRO_LOG_WARN,
                "BuildZipIntoSink: zip_source_buffer failed for %s",
                e.GetFilename().c_str());
            ok = false;
            break;
        }

        const s64 fi = zip_file_add(zf, e.GetFilename().c_str(), es, ZIP_FL_ENC_UTF_8);
        if (fi < 0)
        {
            FrontendLog(RETRO_LOG_WARN,
                "BuildZipIntoSink: zip_file_add failed for %s: %s",
                e.GetFilename().c_str(), zip_strerror(zf));
            zip_source_free(es);   // not transferred on failure
            ok = false;
            break;
        }
        // es now owned by zf.

        if (zip_set_file_compression(zf, fi, ZIP_CM_STORE, 0) != 0)
        {
            FrontendLog(RETRO_LOG_WARN,
                "BuildZipIntoSink: zip_set_file_compression failed for %s: %s",
                e.GetFilename().c_str(), zip_strerror(zf));
            ok = false;
            break;
        }
    }

    if (!ok)
    {
        zip_discard(zf);   // also frees src
        return false;
    }

    // The version-indicator entry that SaveState_UnzipFromZip
    // The version-indicator entry that SaveState_UnzipFromZip
    // expects (CheckVersion reads "PCSX2 Savestate Version.id").
    // Format matches SaveState_AddToZip at pcsx2/SaveState.cpp:976-1010.
    //
    // Heap-allocate with freep=1 so libzip owns the buffer until
    // zip_close finalizes the archive. Matches upstream's pattern.
    //
    // Cross-validate at code-review time:
    //   - STATE_PCSX2_VERSION_SIZE: pcsx2/SaveState.cpp:342 (=32)
    //   - EntryFilename_StateVersion: pcsx2/SaveState.cpp:339
    //   - g_SaveVersion: pcsx2/SaveState.h:29 (public)
    {
        constexpr u32 kVersionSize = 32;
        struct VersionIndicator
        {
            u32 save_version;
            char version[kVersionSize];
        };

        VersionIndicator* vi = static_cast<VersionIndicator*>(std::malloc(sizeof(VersionIndicator)));
        if (!vi)
        {
            FrontendLog(RETRO_LOG_WARN, "BuildZipIntoSink: malloc version indicator failed");
            zip_discard(zf);
            return false;
        }
        vi->save_version = g_SaveVersion;
        std::strncpy(vi->version, "libretro", kVersionSize - 1);
        vi->version[kVersionSize - 1] = 0;

        zip_source_t* vsrc = zip_source_buffer(zf, vi, sizeof(*vi), /*freep=*/1);
        if (!vsrc)
        {
            FrontendLog(RETRO_LOG_WARN, "BuildZipIntoSink: version source: %s", zip_strerror(zf));
            std::free(vi);   // libzip didn't take ownership (freep semantics on failure)
            zip_discard(zf);
            return false;
        }
        // vi ownership now belongs to vsrc (freep=1 → libzip will free).

        const s64 fi = zip_file_add(zf, "PCSX2 Savestate Version.id", vsrc, ZIP_FL_ENC_UTF_8);
        if (fi < 0)
        {
            FrontendLog(RETRO_LOG_WARN, "BuildZipIntoSink: version zip_file_add: %s", zip_strerror(zf));
            zip_source_free(vsrc);   // also frees vi via the registered free function
            zip_discard(zf);
            return false;
        }
        // vsrc (and transitively vi) now owned by zf.

        if (zip_set_file_compression(zf, fi, ZIP_CM_STORE, 0) != 0)
        {
            FrontendLog(RETRO_LOG_WARN, "BuildZipIntoSink: version zip_set_file_compression: %s", zip_strerror(zf));
            zip_discard(zf);
            return false;
        }
    }

    if (zip_close(zf) != 0)
    {
        FrontendLog(RETRO_LOG_WARN, "BuildZipIntoSink: zip_close failed: %s", zip_strerror(zf));
        zip_discard(zf);
        return false;
    }
    // zip_close on success has freed zf and finalized sink.bytes.

    return true;
}

} // namespace

VMState WaitForVmPaused()
{
    using namespace std::chrono_literals;
    const VMState prev = VMManager::GetState();
    if (prev != VMState::Running)
    {
        // Already paused / not running. No handshake needed.
        return prev;
    }
    VMManager::SetPaused(true);
    const auto start = std::chrono::steady_clock::now();
    const auto deadline = start + 200ms;
    while (std::chrono::steady_clock::now() < deadline)
    {
        const VMState s = VMManager::GetState();
        if (s == VMState::Paused)
        {
            if (IsStateTraceEnabled())
            {
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start).count();
                FrontendLog(RETRO_LOG_INFO,
                    "[STATE_TRACE] WaitForVmPaused: paused in %lldms", static_cast<long long>(elapsed));
            }
            return prev;
        }
        if (s == VMState::Shutdown) return VMState::Shutdown;
        std::this_thread::sleep_for(1ms);
    }
    FrontendLog(RETRO_LOG_WARN,
        "WaitForVmPaused: 200 ms deadline exceeded — VMState=%d",
        static_cast<int>(VMManager::GetState()));
    return VMState::Shutdown;  // bail
}

void ResumeVm(VMState prev_state)
{
    if (prev_state == VMState::Running &&
        VMManager::GetState() == VMState::Paused)
    {
        VMManager::SetPaused(false);
    }
}

void ResetSerializeSizeCache()
{
    g_serialize_size.store(0);
}

size_t SerializeSize()
{
    // Probe-once: build a scratch zip, cache its size, return that
    // constant forever for this game. Returns 0 pre-Running so the
    // frontend retries on the next call (spec-legal).
    //
    // Deterministic by construction: every entry is ZIP_CM_STORE, so
    // output size = sum(entry_bytes) + sum(local_file_headers) +
    // central_directory + EOCD — fixed per (build, game).
    const size_t cached = g_serialize_size.load();
    if (cached != 0) return cached;
    if (!VMManager::HasValidVM()) return 0;
    if (VMManager::GetState() != VMState::Running) return 0;

    if (IsStateTraceEnabled())
        FrontendLog(RETRO_LOG_INFO, "[STATE_TRACE] SerializeSize: probe start");

    const auto t0 = std::chrono::steady_clock::now();
    const VMState prev = WaitForVmPaused();
    if (prev == VMState::Shutdown)
    {
        FrontendLog(RETRO_LOG_WARN, "SerializeSize: pause handshake failed");
        return 0;
    }

    size_t probed = 0;
    {
        MemoryWriteSink sink;
        if (BuildZipIntoSink(sink))
            probed = sink.bytes.size();
    }

    ResumeVm(prev);

    if (probed == 0) return 0;
    g_serialize_size.store(probed);

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count();
    FrontendLog(RETRO_LOG_INFO,
        "SerializeSize: probed=%zu bytes in %lldms (cached)",
        probed, static_cast<long long>(elapsed));
    return probed;
}

bool Serialize(void* dst, size_t len)
{
    if (!dst) return false;
    const size_t expected = g_serialize_size.load();
    if (expected == 0) return false;          // probe hasn't run yet
    if (len < expected) return false;         // frontend allocation bug
    if (!VMManager::HasValidVM()) return false;

    if (IsStateTraceEnabled())
        FrontendLog(RETRO_LOG_INFO, "[STATE_TRACE] Serialize: start len=%zu", len);

    const auto t0 = std::chrono::steady_clock::now();
    const VMState prev = WaitForVmPaused();
    if (prev == VMState::Shutdown) return false;

    bool ok = false;
    {
        MemoryWriteSink sink;
        if (BuildZipIntoSink(sink))
        {
            if (sink.bytes.size() > len)
            {
                FrontendLog(RETRO_LOG_ERROR,
                    "Serialize: produced %zu bytes but caller buffer is %zu — "
                    "probe-once assumption violated; not writing",
                    sink.bytes.size(), len);
            }
            else
            {
                std::memcpy(dst, sink.bytes.data(), sink.bytes.size());
                if (sink.bytes.size() < len)
                {
                    std::memset(static_cast<u8*>(dst) + sink.bytes.size(), 0,
                                len - sink.bytes.size());
                }
                ok = true;
            }
        }
    }

    ResumeVm(prev);

    if (IsStateTraceEnabled())
    {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0).count();
        FrontendLog(RETRO_LOG_INFO, "[STATE_TRACE] Serialize: done ok=%d in %lldms",
            ok ? 1 : 0, static_cast<long long>(elapsed));
    }
    return ok;
}

bool Unserialize(const void* /*src*/, size_t /*len*/) { return false; }

} // namespace Pcsx2Libretro
