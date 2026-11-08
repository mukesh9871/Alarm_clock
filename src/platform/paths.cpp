#include "platform/paths.hpp"

#include <cstdlib>
#include <optional>
#include <system_error>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace ac {

namespace fs = std::filesystem;

namespace {

#ifdef _WIN32
std::optional<fs::path> env_path(const wchar_t* name) {
    const DWORD n = GetEnvironmentVariableW(name, nullptr, 0);
    if (n == 0) return std::nullopt;
    std::wstring value(n, L'\0');
    const DWORD written = GetEnvironmentVariableW(name, value.data(), n);
    value.resize(written);
    if (value.empty()) return std::nullopt;
    return fs::path(value);
}
#else
std::optional<fs::path> env_path(const char* name) {
    const char* v = std::getenv(name);
    if (v == nullptr || *v == '\0') return std::nullopt;
    return fs::path(v);
}
#endif

}  // namespace

fs::path executable_dir() {
#ifdef _WIN32
    std::vector<wchar_t> buf(MAX_PATH);
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
        if (n == 0) break;
        if (n < buf.size()) return fs::path(std::wstring(buf.data(), n)).parent_path();
        buf.resize(buf.size() * 2);
    }
#elif defined(__APPLE__)
    char buf[4096];
    uint32_t size = sizeof buf;
    if (_NSGetExecutablePath(buf, &size) == 0) return fs::path(buf).parent_path();
#else
    std::error_code ec;
    const fs::path p = fs::read_symlink("/proc/self/exe", ec);
    if (!ec) return p.parent_path();
#endif
    return fs::current_path();
}

fs::path default_sound_path() { return executable_dir() / "sounds" / "music1.wav"; }

fs::path resolve_sound(const std::string& sound) {
    if (sound.empty()) return default_sound_path();
    const fs::path p{std::u8string(sound.begin(), sound.end())};  // settings are UTF-8
    if (!p.has_parent_path() && !p.is_absolute()) return executable_dir() / "sounds" / p;
    return p;
}

fs::path default_state_path() {
#ifdef _WIN32
    const fs::path base = env_path(L"APPDATA").value_or(executable_dir());
    return base / "AlarmClock" / "alarms.json";
#elif defined(__APPLE__)
    const fs::path base = env_path("HOME").value_or(fs::current_path());
    return base / "Library" / "Application Support" / "AlarmClock" / "alarms.json";
#else
    fs::path base;
    if (auto xdg = env_path("XDG_CONFIG_HOME")) {
        base = *xdg;
    } else {
        base = env_path("HOME").value_or(fs::current_path()) / ".config";
    }
    return base / "alarm-clock" / "alarms.json";
#endif
}

}  // namespace ac
