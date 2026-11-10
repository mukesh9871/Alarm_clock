#pragma once

#include "core/clock.hpp"
#include "platform/audio_player.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

namespace ac::test {

class FakeClock final : public Clock {
public:
    explicit FakeClock(TimePoint t) : t_(t) {}
    TimePoint now() const override { return t_; }
    void set(TimePoint t) { t_ = t; }
    void advance(std::chrono::seconds d) { t_ += d; }

private:
    TimePoint t_;
};

class FakeAudio final : public AudioPlayer {
public:
    bool start(const std::filesystem::path& file, std::chrono::milliseconds fade_in) override {
        started.push_back(file);
        last_fade = fade_in;
        playing = true;
        return succeed;
    }
    void stop() override { playing = false; }
    bool is_playing() const override { return playing; }

    std::vector<std::filesystem::path> started;
    std::chrono::milliseconds last_fade{0};
    bool playing = false;
    bool succeed = true;
};

// Gives a file a distinct, later timestamp, as another process writing it would. Needed
// because two writes within the filesystem's timestamp resolution can look identical.
inline void bump_mtime(const std::filesystem::path& p) {
    static int bumps = 0;
    std::filesystem::last_write_time(
        p, std::filesystem::file_time_type::clock::now() + std::chrono::seconds{5 + ++bumps});
}

// A unique temporary directory, deleted on destruction.
class TempDir {
public:
    TempDir() {
        std::random_device rd;
        path_ = std::filesystem::temp_directory_path() / ("alarm_clock_test_" + std::to_string(rd()) + std::to_string(rd()));
        std::filesystem::create_directories(path_);
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

}  // namespace ac::test
