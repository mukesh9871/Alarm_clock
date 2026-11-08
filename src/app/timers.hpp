#pragma once

#include "platform/audio_player.hpp"

#include <chrono>
#include <filesystem>
#include <string>

namespace ac {

// Countdown that rings `sound` when done. Enter stops the ring, q cancels.
int run_timer(std::chrono::seconds duration, const std::string& label, AudioPlayer& audio,
              const std::filesystem::path& sound);

// Stopwatch: Enter records a lap, q stops.
int run_stopwatch();

}  // namespace ac
