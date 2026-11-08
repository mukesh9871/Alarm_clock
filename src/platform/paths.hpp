#pragma once

#include <filesystem>
#include <string>

namespace ac {

// Directory containing the running executable (falls back to the current directory).
[[nodiscard]] std::filesystem::path executable_dir();

// Bundled default alarm sound, shipped next to the executable.
[[nodiscard]] std::filesystem::path default_sound_path();

// Resolves an alarm's sound setting: empty -> bundled default; a bare file name ->
// looked up in the bundled sounds folder; anything else is used as given.
[[nodiscard]] std::filesystem::path resolve_sound(const std::string& sound);

// Where alarms are stored by default:
//   Windows: %APPDATA%\AlarmClock\alarms.json
//   macOS:   ~/Library/Application Support/AlarmClock/alarms.json
//   Linux:   $XDG_CONFIG_HOME/alarm-clock/alarms.json (default ~/.config)
[[nodiscard]] std::filesystem::path default_state_path();

}  // namespace ac
