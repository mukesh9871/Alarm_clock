#pragma once

#include "core/saved_state.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace ac {

// Reads/writes SavedState as JSON on disk.
class StateFile {
public:
    explicit StateFile(std::filesystem::path path);

    struct LoadResult {
        SavedState state;
        std::optional<std::string> warning;  // set if the file was unreadable and got backed up
    };

    // Missing file -> empty state. Corrupt file -> renamed to *.corrupt-<n> and empty state.
    LoadResult load();

    // Atomic: writes a temp file then renames it over the target. Throws on I/O failure.
    void save(const SavedState& state);

    // True if the file was modified by someone else since our last load/save.
    [[nodiscard]] bool changed_on_disk() const;

    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
    [[nodiscard]] std::optional<std::filesystem::file_time_type> mtime() const;

    std::filesystem::path path_;
    std::optional<std::filesystem::file_time_type> known_mtime_;
};

}  // namespace ac
