// SPDX-FileCopyrightText: 2026 Mark Pearce (RetroNest)
// SPDX-License-Identifier: GPL-3.0+
//
// pcsx2-libretro Host:: stubs.
//
// Implementations of every function in PCSX2's Host:: namespace that
// must be defined by a frontend. Behavior-compatible with the
// pcsx2-gsrunner/Main.cpp and tests/ctest/core/StubHost.cpp references.
//
// The skeleton phase doesn't run any PCSX2 code that actually calls into
// these stubs, but every Host:: symbol must resolve at link time or
// pcsx2_libretro.dylib won't build.
//
// Maintenance note: when rebasing onto a newer upstream, if pcsx2/
// declares a new Host::* function, the build will fail with an
// undefined-symbol error pointing at the new name. Add a stub here
// modeled on the equivalent stub in pcsx2-gsrunner/Main.cpp.

// pcsx2/PrecompiledHeader.h is the standard PCSX2 preamble — every
// translation unit in pcsx2/ and pcsx2-gsrunner/ uses this pattern.
#include "pcsx2/PrecompiledHeader.h"

#include "pcsx2/Achievements.h"
#include "pcsx2/GS.h"
#include "pcsx2/GameList.h"
#include "pcsx2/Host.h"
#include "pcsx2/ImGui/FullscreenUI.h"
#include "pcsx2/ImGui/ImGuiFullscreen.h"
#include "pcsx2/ImGui/ImGuiManager.h"
#include "pcsx2/Input/InputManager.h"
#include "pcsx2/VMManager.h"

#include "common/ProgressCallback.h"
#include "common/SmallString.h"
#include "common/WindowInfo.h"

#include "LibretroFrontend.h"
#include "Settings.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

// ----------------------------------------------------------------------------
// Frontend state singleton and logging helper.
// ----------------------------------------------------------------------------

namespace Pcsx2Libretro
{

FrontendState g_frontend{};

void FrontendLog(retro_log_level level, const char* fmt, ...)
{
    char buffer[2048];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    if (g_frontend.log_cb)
    {
        g_frontend.log_cb(level, "[pcsx2_libretro] %s\n", buffer);
    }
    else
    {
        std::fprintf(stderr, "[pcsx2_libretro] %s\n", buffer);
    }
}

} // namespace Pcsx2Libretro

// ----------------------------------------------------------------------------
// Host:: stubs — required by the PCSX2 static library.
// Implementations follow pcsx2-gsrunner/Main.cpp and
// tests/ctest/core/StubHost.cpp as the canonical reference.
//
// Settings layer access (GetSettingsInterface, GetSettingsLock,
// GetBase*SettingValue, SetBaseSettingsLayer, etc.) is intentionally
// NOT overridden here. pcsx2/Host.cpp provides a correct implementation
// via its internal LayeredSettingsInterface. Duplicating those symbols
// here caused the LAYER_BASE pointer to be set in our local state while
// GetBaseBoolSettingValue read from Host.cpp's LayeredSettingsInterface
// (always null → SIGSEGV in CPUThreadInitialize). Letting Host.cpp own
// the whole settings layer resolves the mismatch.
// ----------------------------------------------------------------------------

// ---------- Settings bookkeeping ----------

void Host::CommitBaseSettingChanges()
{
    // nothing to save in the skeleton
}

void Host::LoadSettings(SettingsInterface& si, std::unique_lock<std::mutex>& lock)
{
}

void Host::CheckForSettingsChanges(const Pcsx2Config& old_config)
{
}

bool Host::RequestResetSettings(bool folders, bool core, bool controllers, bool hotkeys, bool ui)
{
    return false;
}

void Host::SetDefaultUISettings(SettingsInterface& si)
{
}

// ---------- Localisation ----------

bool Host::LocaleCircleConfirm()
{
    return false;
}

// Pass-through translation: copy the source string verbatim (no i18n).
s32 Host::Internal::GetTranslatedStringImpl(
    const std::string_view context, const std::string_view msg, char* tbuf, size_t tbuf_space)
{
    if (msg.size() > tbuf_space)
        return -1;
    else if (msg.empty())
        return 0;

    std::memcpy(tbuf, msg.data(), msg.size());
    return static_cast<s32>(msg.size());
}

std::string Host::TranslatePluralToString(const char* context, const char* msg, const char* disambiguation, int count)
{
    TinyString count_str = TinyString::from_format("{}", count);

    std::string ret(msg);
    for (;;)
    {
        std::string::size_type pos = ret.find("%n");
        if (pos == std::string::npos)
            break;
        ret.replace(pos, pos + 2, count_str.view());
    }

    return ret;
}

int Host::LocaleSensitiveCompare(std::string_view lhs, std::string_view rhs)
{
    const int res = std::strncmp(lhs.data(), rhs.data(), std::min(lhs.size(), rhs.size()));
    if (res != 0)
        return res;
    return lhs.size() > rhs.size() ? 1 : (lhs.size() < rhs.size() ? -1 : 0);
}

// ---------- Progress callback ----------

std::unique_ptr<ProgressCallback> Host::CreateHostProgressCallback()
{
    return ProgressCallback::CreateNullProgressCallback();
}

// ---------- Async notifications ----------

void Host::ReportInfoAsync(const std::string_view title, const std::string_view message)
{
    if (!title.empty() && !message.empty())
        Pcsx2Libretro::FrontendLog(RETRO_LOG_INFO, "ReportInfoAsync: %.*s: %.*s",
            static_cast<int>(title.size()), title.data(),
            static_cast<int>(message.size()), message.data());
    else if (!message.empty())
        Pcsx2Libretro::FrontendLog(RETRO_LOG_INFO, "ReportInfoAsync: %.*s",
            static_cast<int>(message.size()), message.data());
}

void Host::ReportErrorAsync(const std::string_view title, const std::string_view message)
{
    if (!title.empty() && !message.empty())
        Pcsx2Libretro::FrontendLog(RETRO_LOG_ERROR, "ReportErrorAsync: %.*s: %.*s",
            static_cast<int>(title.size()), title.data(),
            static_cast<int>(message.size()), message.data());
    else if (!message.empty())
        Pcsx2Libretro::FrontendLog(RETRO_LOG_ERROR, "ReportErrorAsync: %.*s",
            static_cast<int>(message.size()), message.data());
}

// ---------- UI helpers (no-op in libretro) ----------

void Host::OpenURL(const std::string_view url)
{
}

bool Host::CopyTextToClipboard(const std::string_view text)
{
    return false;
}

void Host::BeginTextInput()
{
}

void Host::EndTextInput()
{
}

std::optional<WindowInfo> Host::GetTopLevelWindowInfo()
{
    return std::nullopt;
}

// ---------- Input device notifications ----------

void Host::OnInputDeviceConnected(const std::string_view identifier, const std::string_view device_name)
{
}

void Host::OnInputDeviceDisconnected(const InputBindingKey key, const std::string_view identifier)
{
}

void Host::SetMouseMode(bool relative_mode, bool hide_cursor)
{
}

void Host::SetMouseLock(bool state)
{
}

// ---------- Render window ----------
// No real window in the skeleton; these are never called until a VM boots.

std::optional<WindowInfo> Host::AcquireRenderWindow(bool recreate_window)
{
    // Return a surfaceless WindowInfo so PCSX2 can create a GS device without
    // a real display window. Metal/Vulkan on macOS supports surfaceless rendering
    // (off-screen). Returning nullopt causes the GS device creation to fail
    // even with the Null renderer.
    return WindowInfo{};  // default type == WindowInfo::Type::Surfaceless
}

void Host::ReleaseRenderWindow()
{
}

void Host::BeginPresentFrame()
{
}

void Host::RequestResizeHostDisplay(s32 width, s32 height)
{
}

// ---------- VM lifecycle ----------

void Host::OnVMStarting()
{
}

void Host::OnVMStarted()
{
}

void Host::OnVMDestroyed()
{
}

void Host::OnVMPaused()
{
}

void Host::OnVMResumed()
{
}

void Host::OnGameChanged(const std::string& title, const std::string& elf_override, const std::string& disc_path,
    const std::string& disc_serial, u32 disc_crc, u32 current_crc)
{
}

void Host::OnPerformanceMetricsUpdated()
{
}

void Host::OnSaveStateLoading(const std::string_view filename)
{
}

void Host::OnSaveStateLoaded(const std::string_view filename, bool was_successful)
{
}

void Host::OnSaveStateSaved(const std::string_view filename)
{
}

// ---------- Threading ----------
// In the skeleton no PCSX2 threads are running, so execute inline.

void Host::RunOnCPUThread(std::function<void()> function, bool block /* = false */)
{
    Pcsx2Libretro::FrontendLog(RETRO_LOG_WARN, "RunOnCPUThread called in skeleton — running inline");
    if (function)
        function();
}

void Host::RunOnGSThread(std::function<void()> function)
{
    Pcsx2Libretro::FrontendLog(RETRO_LOG_WARN, "RunOnGSThread called in skeleton — running inline");
    if (function)
        function();
}

void Host::PumpMessagesOnCPUThread()
{
}

// ---------- Game list ----------

void Host::RefreshGameListAsync(bool invalidate_cache)
{
}

void Host::CancelGameListRefresh()
{
}

// ---------- Fullscreen ----------

bool Host::IsFullscreen()
{
    return false;
}

void Host::SetFullscreen(bool enabled)
{
}

// ---------- Capture ----------

void Host::OnCaptureStarted(const std::string& filename)
{
}

void Host::OnCaptureStopped()
{
}

// ---------- Application exit ----------

void Host::RequestExitApplication(bool allow_confirm)
{
}

void Host::RequestExitBigPicture()
{
}

void Host::RequestVMShutdown(bool allow_confirm, bool allow_save_state, bool default_save_state)
{
    VMManager::SetState(VMState::Stopping);
}

// ---------- Achievements ----------

void Host::OnAchievementsLoginSuccess(const char* username, u32 points, u32 sc_points, u32 unread_messages)
{
}

void Host::OnAchievementsLoginRequested(Achievements::LoginRequestReason reason)
{
}

void Host::OnAchievementsHardcoreModeChanged(bool enabled)
{
}

void Host::OnAchievementsRefreshed()
{
}

// ---------- Miscellaneous UI dialogs ----------

void Host::OnCoverDownloaderOpenRequested()
{
}

void Host::OnCreateMemoryCardOpenRequested()
{
}

bool Host::InBatchMode()
{
    return false;
}

bool Host::InNoGUIMode()
{
    return false;
}

bool Host::ShouldPreferHostFileSelector()
{
    return false;
}

void Host::OpenHostFileSelectorAsync(std::string_view title, bool select_directory, FileSelectorCallback callback,
    FileSelectorFilters filters, std::string_view initial_directory)
{
    // No file dialog in the skeleton; immediately return an empty path.
    callback(std::string());
}

// ---------- InputManager host keyboard stubs ----------

std::optional<u32> InputManager::ConvertHostKeyboardStringToCode(const std::string_view str)
{
    return std::nullopt;
}

std::optional<std::string> InputManager::ConvertHostKeyboardCodeToString(u32 code)
{
    return std::nullopt;
}

const char* InputManager::ConvertHostKeyboardCodeToIcon(u32 code)
{
    return nullptr;
}

// ---------- Hotkey list (empty — no hotkeys in libretro) ----------

BEGIN_HOTKEY_LIST(g_host_hotkeys)
END_HOTKEY_LIST()
