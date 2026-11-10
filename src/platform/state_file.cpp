#include "platform/state_file.hpp"

#include <fstream>
#include <random>
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

StateFile::LoadResult StateFile::load(OnCorrupt on_corrupt) {
    known_mtime_ = mtime();
    protect_existing_ = false;
    std::ifstream in(path_, std::ios::binary);
    if (!in) return {};  // first run

    std::ostringstream buf;
    buf << in.rdbuf();
    in.close();

    try {
        return {state_from_json(buf.str()), std::nullopt};
    } catch (const StateFormatError& e) {
        const std::string what = "Could not read " + path_.string() + " (" + e.what() + ").";
        if (on_corrupt == OnCorrupt::Leave) {
            protect_existing_ = true;
            return {SavedState{}, what + " It was left as is; changes won't be saved until it is fixed or removed.",
                    true};
        }

        // Keep the broken file for the user to inspect rather than silently overwriting it.
        fs::path backup;
        for (int n = 1;; ++n) {
            backup = path_;
            backup += ".corrupt-" + std::to_string(n);
            if (!fs::exists(backup)) break;
        }
        std::error_code ec;
        fs::rename(path_, backup, ec);
        if (!ec) {
            known_mtime_.reset();
            return {SavedState{}, what + " It was moved to " + backup.string() + " and a fresh alarm list was started.",
                    true};
        }
        // Renaming fails on Windows while another program has the file open; a copy may still work.
        std::error_code copy_ec;
        if (fs::copy_file(path_, backup, copy_ec)) {
            return {SavedState{}, what + " It was copied to " + backup.string() + " and a fresh alarm list was started.",
                    true};
        }
        protect_existing_ = true;
        return {SavedState{},
                what + " It could not be backed up (" + ec.message() +
                    "), so it won't be overwritten. Fix or remove it to save alarms again.",
                true};
    }
}

void StateFile::save(const SavedState& state) {
    if (protect_existing_ && fs::exists(path_)) {
        throw std::runtime_error("not overwriting " + path_.string() + ": it can't be read and has no backup");
    }
    if (path_.has_parent_path()) fs::create_directories(path_.parent_path());

    // A unique temp name, so two processes saving at once can't write into the same file.
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    fs::path tmp = path_;
    tmp += ".tmp-" + std::to_string(rng());
    try {
        {
            std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
            if (!out) throw std::runtime_error("cannot write " + tmp.string());
            out << to_json(state);
            out.flush();
            if (!out) throw std::runtime_error("failed writing " + tmp.string());
        }
        fs::rename(tmp, path_);  // replaces atomically on all supported platforms
    } catch (...) {
        std::error_code ec;
        fs::remove(tmp, ec);
        throw;
    }
    protect_existing_ = false;
    known_mtime_ = mtime();
}

bool StateFile::changed_on_disk() const { return mtime() != known_mtime_; }

}  // namespace ac
