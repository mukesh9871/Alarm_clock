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
        std::optional<std::string> warning;  // set if the file was unreadable
        bool unreadable = false;             // the file exists but couldn't be parsed
    };

    // What to do with a file that exists but can't be parsed.
    enum class OnCorrupt {
        // Move it to *.corrupt-<n> (or copy it there) and start from an empty state.
        // If no backup can be made, saving is refused so the only copy isn't overwritten.
        BackUp,
        // Leave it alone and refuse to save over it until it changes (it may be mid-edit).
        Leave,
    };

    // Missing file -> empty state. Unreadable file -> empty state, unreadable = true.
    LoadResult load(OnCorrupt on_corrupt = OnCorrupt::BackUp);

    // Atomic: writes a uniquely named temp file then renames it over the target.
    // Throws on I/O failure, or if the current file is unreadable and must not be overwritten.
    void save(const SavedState& state);

    // True if the file was modified by someone else since our last load/save.
    [[nodiscard]] bool changed_on_disk() const;

    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
    [[nodiscard]] std::optional<std::filesystem::file_time_type> mtime() const;

    std::filesystem::path path_;
    std::optional<std::filesystem::file_time_type> known_mtime_;
    bool protect_existing_ = false;  // don't overwrite: unreadable and not backed up
};

}  // namespace ac
