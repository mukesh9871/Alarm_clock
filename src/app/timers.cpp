#include "app/timers.hpp"

#include "app/line_input.hpp"
#include "core/duration.hpp"

#include <iostream>
#include <vector>

namespace ac {

namespace {
using namespace std::chrono;
using namespace std::chrono_literals;

void overwrite_line(const std::string& text, std::size_t& last_len) {
    std::string padded = text;
    if (padded.size() < last_len) padded.append(last_len - padded.size(), ' ');
    last_len = text.size();
    std::cout << '\r' << padded << std::flush;
}
}  // namespace

int run_timer(seconds duration, const std::string& label, AudioPlayer& audio, const std::filesystem::path& sound) {
    const std::string name = label.empty() ? "Timer" : label;
    std::cout << name << ": " << format_hms(duration) << ".  [q] cancel\n";

    // steady_clock: a countdown must not jump if the wall clock is changed.
    const auto end = steady_clock::now() + duration;
    LineInput input;
    std::size_t last_len = 0;

    for (;;) {
        const auto left = ceil<seconds>(end - steady_clock::now());
        if (left <= 0s) break;
        overwrite_line("  " + format_hms(left) + " remaining", last_len);
        if (auto line = input.pop_for(LineInput::until_next_second()); line && *line == "q") {
            std::cout << "\nCancelled.\n";
            return 0;
        }
    }

    overwrite_line("  00:00", last_len);
    std::cout << "\n*** " << name << " finished! ***   [Enter] stop\n";
    if (!audio.start(sound, 0ms)) std::cout << "(could not play the sound - beeping instead)\n";
    input.pop_for(10min);  // ring for up to 10 minutes or until any input
    audio.stop();
    std::cout << "Stopped.\n";
    return 0;
}

int run_stopwatch() {
    std::cout << "Stopwatch running.  [Enter] lap  [q] stop\n";
    const auto start = steady_clock::now();
    auto lap_start = start;
    std::vector<milliseconds> laps;
    LineInput input;
    std::size_t last_len = 0;

    for (;;) {
        const auto now = steady_clock::now();
        overwrite_line("  " + format_hms_tenths(duration_cast<milliseconds>(now - start)), last_len);
        auto line = input.pop_for(100ms);
        if (!line) continue;
        const auto t = steady_clock::now();
        if (*line == "q") {
            std::cout << "\nTotal " << format_hms_tenths(duration_cast<milliseconds>(t - start)) << '\n';
            return 0;
        }
        laps.push_back(duration_cast<milliseconds>(t - lap_start));
        lap_start = t;
        last_len = 0;
        std::cout << "\r  Lap " << laps.size() << ": " << format_hms_tenths(laps.back()) << "   (total "
                  << format_hms_tenths(duration_cast<milliseconds>(t - start)) << ")\n";
    }
}

}  // namespace ac
