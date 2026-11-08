#pragma once

#include <filesystem>

namespace ac {

// Directory containing the running executable (falls back to the current directory).
[[nodiscard]] std::filesystem::path executable_dir();

// Bundled default alarm sound, shipped next to the executable.
[[nodiscard]] std::filesystem::path default_sound_path();

}  // namespace ac
