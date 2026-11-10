#include "platform/audio_player.hpp"

#include <miniaudio.h>

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace ac {

namespace {

// Last-resort alarm: a beep every second on its own thread.
class Beeper {
public:
    ~Beeper() { stop(); }

    void start() {
        stop();
        running_ = true;
        thread_ = std::thread([this] {
            std::unique_lock lock(mutex_);
            while (running_) {
#ifdef _WIN32
                lock.unlock();
                Beep(880, 300);
                lock.lock();
#else
                std::fputs("\a", stderr);
                std::fflush(stderr);
#endif
                cv_.wait_for(lock, std::chrono::milliseconds(700), [this] { return !running_; });
            }
        });
    }

    void stop() {
        {
            std::lock_guard lock(mutex_);
            running_ = false;
        }
        cv_.notify_all();
        if (thread_.joinable()) thread_.join();
    }

    [[nodiscard]] bool running() const {
        std::lock_guard lock(mutex_);
        return running_;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool running_ = false;
    std::thread thread_;
};

class MiniaudioPlayer final : public AudioPlayer {
public:
    ~MiniaudioPlayer() override {
        stop();
        if (engine_ready_) ma_engine_uninit(&engine_);
    }

    bool start(const std::filesystem::path& file, std::chrono::milliseconds fade_in) override {
        stop();
        if (ensure_engine() && load(file)) {
            ma_sound_set_looping(&sound_, MA_TRUE);
            ma_sound_set_fade_in_milliseconds(&sound_, 0.0f, 1.0f, static_cast<ma_uint64>(fade_in.count()));
            if (ma_sound_start(&sound_) == MA_SUCCESS) return true;
            unload();
        }
        beeper_.start();
        return false;
    }

    void stop() override {
        if (sound_ready_) {
            ma_sound_stop(&sound_);
            unload();
        }
        beeper_.stop();
    }

    [[nodiscard]] bool is_playing() const override { return sound_ready_ || beeper_.running(); }

private:
    bool ensure_engine() {
        if (!engine_ready_) engine_ready_ = ma_engine_init(nullptr, &engine_) == MA_SUCCESS;
        return engine_ready_;
    }

    bool load(const std::filesystem::path& file) {
        const ma_uint32 flags = MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION;
#ifdef _WIN32
        const ma_result r = ma_sound_init_from_file_w(&engine_, file.c_str(), flags, nullptr, nullptr, &sound_);
#else
        const ma_result r = ma_sound_init_from_file(&engine_, file.c_str(), flags, nullptr, nullptr, &sound_);
#endif
        sound_ready_ = r == MA_SUCCESS;
        return sound_ready_;
    }

    void unload() {
        ma_sound_uninit(&sound_);
        sound_ready_ = false;
    }

    ma_engine engine_{};
    ma_sound sound_{};
    bool engine_ready_ = false;
    bool sound_ready_ = false;
    Beeper beeper_;
};

}  // namespace

std::unique_ptr<AudioPlayer> make_audio_player() { return std::make_unique<MiniaudioPlayer>(); }

}  // namespace ac
