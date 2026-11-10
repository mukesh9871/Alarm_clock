#pragma once

#include <chrono>
#include <filesystem>
#include <memory>

namespace ac {

// Non-blocking alarm sound playback.
class AudioPlayer {
public:
    virtual ~AudioPlayer() = default;

    // Starts looping `file`, ramping volume from silent to full over `fade_in`.
    // If the file can't be played (missing, no audio device), falls back to a
    // repeating beep so the alarm is never silent. Returns false if it fell back.
    virtual bool start(const std::filesystem::path& file, std::chrono::milliseconds fade_in) = 0;
    virtual void stop() = 0;
    [[nodiscard]] virtual bool is_playing() const = 0;
};

std::unique_ptr<AudioPlayer> make_audio_player();

}  // namespace ac
