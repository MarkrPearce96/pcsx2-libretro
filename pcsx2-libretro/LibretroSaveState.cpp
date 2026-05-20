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
#include "CoreResources.h"      // kWaitPausedDeadline, kPostInitSettle

#include "pcsx2/VMManager.h"
#include "pcsx2/SaveState.h"    // SaveState_DownloadState, ArchiveEntryList, SaveState_UnzipFromMemory
#include "pcsx2/MTGS.h"         // MTGS::WaitGS — cross-cycle thread-sync workaround
#include "pcsx2/MTVU.h"         // vu1Thread.WaitVU — cross-cycle thread-sync workaround
#include "pcsx2/Config.h"       // THREAD_VU1
#include "common/Error.h"

#include <zip.h>                // libzip

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <unistd.h>             // truncate

namespace Pcsx2Libretro
{

namespace
{

// Probe-once: cache the serialized size on first call while VM is
// Running, return that constant forever. 0 means "VM not yet ready,
// frontend should retry on next call". Reset in
// ResetSerializeSizeCache() (called from retro_unload_game).
std::atomic<size_t> g_serialize_size{0};

// Tail sentinel: the last 16 bytes of a padded save-state buffer are
// 8 bytes of magic followed by little-endian uint64_t actual_zip_size.
// Lets readers know where the real zip ends so the trailing zero pad
// can be trimmed before libzip is asked to parse the buffer/file.
constexpr char     kPadMagic[8]  = {'P','C','S','X','S','I','Z','E'};
constexpr size_t   kSentinelSize = sizeof(kPadMagic) + sizeof(uint64_t);

void WriteSentinel(uint8_t* dst, size_t len, uint64_t actual_zip_size)
{
    if (len < kSentinelSize) return;
    std::memcpy(dst + len - kSentinelSize, kPadMagic, sizeof(kPadMagic));
    std::memcpy(dst + len - sizeof(uint64_t), &actual_zip_size,
                sizeof(actual_zip_size));
}

bool TryReadSentinel(const uint8_t* src, size_t len, uint64_t* out)
{
    if (len < kSentinelSize) return false;
    if (std::memcmp(src + len - kSentinelSize, kPadMagic,
                    sizeof(kPadMagic)) != 0)
        return false;
    uint64_t stored = 0;
    std::memcpy(&stored, src + len - sizeof(uint64_t), sizeof(stored));
    if (stored == 0 || stored > len - kSentinelSize) return false;
    *out = stored;
    return true;
}

// Backward-scan for the ZIP End-Of-Central-Directory signature
// (0x06054b50). Returns the offset just past the EOCD record
// (including its comment) — i.e. the true end of the zip blob —
// or 0 if not found / not validated.
//
// Used as a fallback when the tail sentinel is absent (e.g. files
// written by an earlier build that padded with zeros but didn't
// emit a sentinel). The scan walks the whole buffer because we
// don't know how large the pad is.
size_t FindZipEnd(const uint8_t* buf, size_t len)
{
    constexpr uint32_t kEocdSig = 0x06054b50;
    if (len < 22) return 0;
    size_t off = len - 22;
    while (true)
    {
        const uint32_t sig =
            uint32_t(buf[off])           |
            uint32_t(buf[off + 1]) <<  8 |
            uint32_t(buf[off + 2]) << 16 |
            uint32_t(buf[off + 3]) << 24;
        if (sig == kEocdSig)
        {
            const uint16_t comment_len =
                uint16_t(buf[off + 20]) |
                uint16_t(buf[off + 21]) << 8;
            const size_t end = off + 22 + size_t(comment_len);
            // Sanity: the central-dir-offset field should point inside the
            // buffer too. (Bytes [off+16..off+20], little-endian uint32.)
            const uint32_t cd_off =
                uint32_t(buf[off + 16])       |
                uint32_t(buf[off + 17]) <<  8 |
                uint32_t(buf[off + 18]) << 16 |
                uint32_t(buf[off + 19]) << 24;
            if (end <= len && cd_off < off)
                return end;
        }
        if (off == 0) break;
        --off;
    }
    return 0;
}

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

        // Mirror upstream SaveState_AddToZip (pcsx2/SaveState.cpp:1016):
        // zero-byte entries (e.g. optional SavestateEntry_USB when USB
        // emulation is off) are skipped so the zip doesn't carry empty
        // files that SysState_ComponentFreezeIn would then read as a
        // zero-byte zip_fread on load.
        if (e.GetDataSize() == 0)
            continue;

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
    const auto deadline = start + CoreResources::kWaitPausedDeadline;
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
        "WaitForVmPaused: %lld ms deadline exceeded — VMState=%d",
        static_cast<long long>(CoreResources::kWaitPausedDeadline.count()),
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
    // Probe + headroom pad. The probe is NOT byte-stable across the VM's
    // execution: at least one PCSX2 component's freeze output (suspected
    // GS surface cache / VU dynamic state) grows by tens-to-hundreds of
    // kilobytes between calls. Observed live on Looney Tunes Space Race
    // PAL — probe=48,361,703, actual serialize=48,543,463 (+178 KB), with
    // Serialize() refusing to overflow the caller buffer and the on-disk
    // zip ending up truncated/corrupt. Padding the reported size gives
    // headroom; the tail is zero-filled in Serialize() and the zip format
    // tolerates trailing zeros (EOCD self-locates).
    constexpr size_t kSerializeHeadroomBytes = 8 * 1024 * 1024;

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
    const size_t reported = probed + kSerializeHeadroomBytes;
    g_serialize_size.store(reported);

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count();
    FrontendLog(RETRO_LOG_INFO,
        "SerializeSize: probed=%zu bytes (+%zu pad = %zu reported) in %lldms (cached)",
        probed, kSerializeHeadroomBytes, reported, static_cast<long long>(elapsed));
    return reported;
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

    // Cross-cycle race workaround (matches EmuThread.cpp post-init).
    // Ensure pending GS / MTVU work is drained AND give the host enough time
    // to fully settle before snapshotting — the savestate's gsFreeze /
    // vuJITFreeze pull from the same globals these threads write to, and
    // capturing too eagerly produces a bad savestate that later thaws into a
    // freeze. See EmuThread.cpp + memory/ee_recompiler_freeze_pickup.md.
    MTGS::WaitGS();
    if (THREAD_VU1)
        vu1Thread.WaitVU();
    std::this_thread::sleep_for(CoreResources::kPostInitSettle);

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
                    // Tail sentinel lets readers (Unserialize and the cold-
                    // boot file-trim helper) recover the real zip end from
                    // a padded buffer/file. See WriteSentinel comment above.
                    WriteSentinel(static_cast<u8*>(dst), len, sink.bytes.size());
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

bool Unserialize(const void* src, size_t len)
{
    if (!src || len == 0) return false;
    if (!VMManager::HasValidVM()) return false;

    if (IsStateTraceEnabled())
        FrontendLog(RETRO_LOG_INFO, "[STATE_TRACE] Unserialize: start len=%zu", len);

    const auto t0 = std::chrono::steady_clock::now();
    const VMState prev = WaitForVmPaused();
    if (prev == VMState::Shutdown) return false;

    // Sentinel-aware trim: if the buffer was produced by our padded
    // Serialize path, strip the trailing zeros + sentinel before
    // handing it to libzip (which can't otherwise locate the EOCD).
    // Fall back to backward EOCD scan for files written by earlier
    // builds that padded without a sentinel.
    size_t effective_len = len;
    {
        uint64_t sentinel_size = 0;
        if (TryReadSentinel(static_cast<const uint8_t*>(src), len,
                            &sentinel_size))
        {
            effective_len = size_t(sentinel_size);
        }
        else
        {
            const size_t scanned =
                FindZipEnd(static_cast<const uint8_t*>(src), len);
            if (scanned != 0 && scanned < len) effective_len = scanned;
        }
    }

    Error err;
    const bool ok = SaveState_UnzipFromMemory(src, effective_len, &err);
    if (!ok)
    {
        // SaveState_UnzipFromZip already calls VMManager::Reset() on
        // mid-load failure. Pre-PreLoadPrep failures (CheckVersion,
        // missing entries) leave the VM untouched.
        FrontendLog(RETRO_LOG_WARN, "Unserialize: load failed: %s",
            err.GetDescription().c_str());
    }

    ResumeVm(prev);

    if (IsStateTraceEnabled())
    {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0).count();
        FrontendLog(RETRO_LOG_INFO, "[STATE_TRACE] Unserialize: done ok=%d in %lldms",
            ok ? 1 : 0, static_cast<long long>(elapsed));
    }
    return ok;
}

bool TrimPaddedSaveStateFile(const char* path)
{
    if (!path || !*path) return false;

    std::FILE* fp = std::fopen(path, "rb");
    if (!fp) return false;
    if (std::fseek(fp, 0, SEEK_END) != 0) { std::fclose(fp); return false; }
    const long file_size_raw = std::ftell(fp);
    if (file_size_raw <= long(kSentinelSize)) { std::fclose(fp); return false; }
    const size_t file_size = size_t(file_size_raw);

    size_t trim_to = 0;

    // Try sentinel first (faster, exact). TryReadSentinel's own bound
    // check is relative to its window, so revalidate against file_size.
    if (std::fseek(fp, long(file_size - kSentinelSize), SEEK_SET) == 0)
    {
        uint8_t tail[kSentinelSize];
        if (std::fread(tail, 1, kSentinelSize, fp) == kSentinelSize &&
            std::memcmp(tail, kPadMagic, sizeof(kPadMagic)) == 0)
        {
            uint64_t stored = 0;
            std::memcpy(&stored, tail + sizeof(kPadMagic), sizeof(stored));
            if (stored > 0 && stored < file_size) trim_to = size_t(stored);
        }
    }

    // Fallback: backward EOCD scan over the full file. Only kicks in for
    // files written by earlier builds that padded without a sentinel.
    if (trim_to == 0)
    {
        // Cap fallback to a sane ceiling so we don't slurp a bogus
        // multi-gigabyte file into memory. Real PCSX2 save states are
        // ~50 MB plus our 8 MB pad, so 256 MB is generous headroom.
        constexpr size_t kMaxScanBytes = 256 * 1024 * 1024;
        if (file_size > kMaxScanBytes) { std::fclose(fp); return false; }
        std::unique_ptr<uint8_t[]> buf(new(std::nothrow) uint8_t[file_size]);
        if (!buf) { std::fclose(fp); return false; }
        if (std::fseek(fp, 0, SEEK_SET) != 0) { std::fclose(fp); return false; }
        if (std::fread(buf.get(), 1, file_size, fp) != file_size)
        {
            std::fclose(fp); return false;
        }
        const size_t scanned = FindZipEnd(buf.get(), file_size);
        if (scanned != 0 && scanned < file_size) trim_to = scanned;
    }

    std::fclose(fp);

    if (trim_to == 0) return false;
    if (::truncate(path, off_t(trim_to)) != 0) return false;

    FrontendLog(RETRO_LOG_INFO,
        "TrimPaddedSaveStateFile: %s trimmed %zu -> %zu bytes",
        path, file_size, trim_to);
    return true;
}

} // namespace Pcsx2Libretro
