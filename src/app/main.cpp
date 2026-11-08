// Console alarm clock: shows a live clock and rings a looping alarm sound.

#include "core/alarm_service.hpp"
#include "core/clock.hpp"
#include "core/time_format.hpp"
#include "core/time_zone.hpp"
#include "platform/audio_player.hpp"
#include "platform/paths.hpp"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <optional>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

namespace {

using namespace std::chrono_literals;

// Lines typed by the user, handed from the input thread to the main loop.
class InputQueue {
public:
    void push(std::string line) {
        {
            std::lock_guard lock(mutex_);
            lines_.push_back(std::move(line));
        }
        cv_.notify_one();
    }

    // Waits up to `timeout` for a line.
    std::optional<std::string> pop_for(std::chrono::milliseconds timeout) {
        std::unique_lock lock(mutex_);
        if (!cv_.wait_for(lock, timeout, [this] { return !lines_.empty(); })) return std::nullopt;
        std::string line = std::move(lines_.front());
        lines_.pop_front();
        return line;
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<std::string> lines_;
};

std::optional<ac::TimeOfDay> prompt_alarm_time() {
    std::cout << "Set an alarm? [y/N]: " << std::flush;
    std::string answer;
    if (!std::getline(std::cin, answer) || answer.empty() || (answer[0] != 'y' && answer[0] != 'Y')) {
        return std::nullopt;
    }
    for (;;) {
        std::cout << "Alarm time (e.g. 07:30, 19:05, 7:30pm): " << std::flush;
        std::string text;
        if (!std::getline(std::cin, text)) return std::nullopt;
        if (auto t = ac::parse_time_of_day(text)) return t;
        std::cout << "  '" << text << "' is not a valid time. Use HH:MM (24-hour) or add am/pm.\n";
    }
}

}  // namespace

int main() {
    ac::SystemClock clock;
    ac::SystemTimeZone tz;
    ac::AlarmService alarms(tz);
    auto audio = ac::make_audio_player();

    std::cout << "Alarm Clock\n";
    if (auto t = prompt_alarm_time()) {
        const auto now = clock.now();
        const int id = alarms.upsert(ac::Alarm{.time = *t}, now);
        const auto due = *alarms.next_fire(id);
        std::cout << "Alarm set for " << ac::format_time_of_day(*t) << " (in " << ac::format_duration(due - now)
                  << ").\n";
    }
    std::cout << "Press Enter to stop a ringing alarm, type q + Enter to quit.\n\n";

    InputQueue input;
    std::thread reader([&input] {
        std::string line;
        while (std::getline(std::cin, line)) {
            const bool quit = line == "q" || line == "Q";
            input.push(std::move(line));
            if (quit) return;
        }
        input.push("q");  // stdin closed
    });

    bool running = true;
    while (running) {
        const auto now = clock.now();

        for (const auto& ev : alarms.tick(now)) {
            const auto* a = alarms.find(ev.alarm_id);
            const std::string when = a ? ac::format_time_of_day(a->time) : "?";
            if (ev.kind == ac::AlarmEvent::Kind::Ring) {
                std::cout << "\n*** ALARM " << when << " *** (press Enter to stop)\n";
                if (!audio->start(ac::default_sound_path(), 10s)) {
                    std::cout << "(could not play " << ac::default_sound_path().string() << ", beeping instead)\n";
                }
            } else {
                std::cout << "\nMissed alarm " << when << " (" << ac::format_duration(ev.late) << " ago).\n";
            }
        }

        const auto local = tz.to_local(now);
        std::string status = "  " + ac::format_date(local) + "  " + ac::format_clock(local);
        if (auto next = alarms.next_due()) {
            status += "   next alarm in " + ac::format_duration(next->second - now);
        } else if (!alarms.ringing().empty()) {
            status += "   RINGING";
        }
        std::cout << '\r' << status << "      " << std::flush;

        // Sleep until the next whole second, waking early for input.
        const auto until_next_second = std::chrono::ceil<std::chrono::milliseconds>(
            (now + 1s) - std::chrono::system_clock::now());
        if (auto line = input.pop_for(std::max(until_next_second, 1ms))) {
            if (*line == "q" || *line == "Q") {
                running = false;
            } else if (!alarms.ringing().empty()) {
                for (int id : alarms.ringing()) alarms.dismiss(id, clock.now());
                audio->stop();
                std::cout << "\nAlarm stopped.\n";
            }
        }
    }

    audio->stop();
    std::cout << "\nBye.\n";
    reader.join();
    return 0;
}
