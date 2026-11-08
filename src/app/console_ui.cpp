#include "app/console_ui.hpp"

#include "app/commands.hpp"
#include "core/time_format.hpp"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace ac {

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

void print_notices(const std::vector<Notice>& notices) {
    for (const auto& n : notices) {
        switch (n.level) {
            case Notice::Level::Alarm:
                std::cout << "\n*** " << n.text << " ***   [Enter] stop   [s] snooze\n";
                break;
            case Notice::Level::Warning:
                std::cout << "\n! " << n.text << '\n';
                break;
            case Notice::Level::Info:
                std::cout << "\n" << n.text << '\n';
                break;
        }
    }
}

std::string status_line(const AlarmSession& s) {
    const auto now = s.now();
    const auto local = s.time_zone().to_local(now);
    std::string line = "  " + format_date(local) + "  " + format_clock(local, s.settings().hour_format);
    const auto& svc = s.service();
    if (!svc.ringing().empty()) {
        line += "   RINGING: " + s.describe(svc.ringing().front());
    } else if (auto next = svc.next_due()) {
        line += "   next: " + s.describe(next->first) + (svc.is_snoozed(next->first) ? " (snoozed)" : "") + " in " +
                format_duration(next->second - now);
    } else {
        line += "   no alarms set";
    }
    return line;
}

}  // namespace

int run_console(AlarmSession& session) {
    print_notices(session.start());
    std::cout << "Commands: [Enter] stop alarm  [s] snooze  [l] list alarms  [q] quit\n"
                 "Manage alarms from another terminal with 'alarm_clock add/edit/remove' - changes load automatically.\n\n";

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

    std::size_t last_len = 0;
    for (bool running = true; running;) {
        print_notices(session.tick());

        std::string line = status_line(session);
        const std::size_t len = line.size();
        if (len < last_len) line.append(last_len - len, ' ');
        last_len = len;
        std::cout << '\r' << line << std::flush;

        // Sleep until the next whole second, waking early for input.
        const auto wait = std::chrono::ceil<std::chrono::milliseconds>(
            std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()) + 1s - std::chrono::system_clock::now());
        auto cmd = input.pop_for(std::max(wait, 1ms));
        if (!cmd) continue;

        if (*cmd == "q" || *cmd == "Q") {
            running = false;
        } else if (*cmd == "s" || *cmd == "S") {
            if (!session.service().ringing().empty()) {
                session.snooze();
                std::cout << "\nSnoozed.\n";
            }
        } else if (*cmd == "l" || *cmd == "L") {
            std::cout << "\n";
            const auto& svc = session.service();
            print_alarms(svc.alarms(), session.settings().hour_format, session.now(), session.time_zone(), std::cout,
                         [&svc](const Alarm& a) { return NextRing{svc.next_fire(a.id), svc.is_snoozed(a.id)}; });
        } else if (!session.service().ringing().empty()) {
            session.dismiss();
            std::cout << "\nAlarm stopped.\n";
        }
    }

    session.shutdown();
    std::cout << "\nBye.\n";
    reader.join();
    return 0;
}

}  // namespace ac
