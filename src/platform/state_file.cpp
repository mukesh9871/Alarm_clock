#include "platform/state_file.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <system_error>

namespace ac {

namespace fs = std::filesystem;

StateFile::StateFile(fs::path path) : path_(std::move(path)) {}

std::optional<fs::file_time_type> StateFile::mtime() const {
    std::error_code ec;
    const auto t = fs::last_write_time(path_, ec);
    if (ec) return std::nullopt;
    return t;
}

StateFile::LoadResult StateFile::load() {
    known_mtime_ = mtime();
    std::ifstream in(path_, std::ios::binary);
    if (!in) return {};  // first run

    std::ostringstream buf;
    buf << in.rdbuf();
    in.close();

    try {
        return {state_from_json(buf.str()), std::nullopt};
    } catch (const StateFormatError& e) {
        // Keep the broken file for the user to inspect rather than silently overwriting it.
        fs::path backup;
        for (int n = 1;; ++n) {
            backup = path_;
            backup += ".corrupt-" + std::to_string(n);
            if (!fs::exists(backup)) break;
        }
        std::error_code ec;
        fs::rename(path_, backup, ec);
        known_mtime_.reset();
        return {SavedState{}, "Could not read " + path_.string() + " (" + e.what() + "). It was moved to " +
                                  backup.string() + " and a fresh alarm list was started."};
    }
}

void StateFile::save(const SavedState& state) {
    if (path_.has_parent_path()) fs::create_directories(path_.parent_path());

    fs::path tmp = path_;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) throw std::runtime_error("cannot write " + tmp.string());
        out << to_json(state);
        out.flush();
        if (!out) throw std::runtime_error("failed writing " + tmp.string());
    }
    fs::rename(tmp, path_);  // replaces atomically on all supported platforms
    known_mtime_ = mtime();
}

bool StateFile::changed_on_disk() const { return mtime() != known_mtime_; }

}  // namespace ac
