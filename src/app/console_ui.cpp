#include "app/console_ui.hpp"

#include "app/commands.hpp"
#include "app/line_input.hpp"
#include "core/time_format.hpp"
#include "platform/terminal.hpp"

#include <charconv>
#include <csignal>
#include <iostream>
#include <string>

namespace ac {

namespace {

// Set by Ctrl+C / SIGTERM so the loop can shut down cleanly (save, cancel the wake timer).
volatile std::sig_atomic_t g_stop_requested = 0;

extern "C" void on_stop_signal(int /*sig*/) { g_stop_requested = 1; }

// Installs the stop handlers for the lifetime of the console loop.
class StopSignals {
public:
    StopSignals() {
        g_stop_requested = 0;
        prev_int_ = std::signal(SIGINT, on_stop_signal);
        prev_term_ = std::signal(SIGTERM, on_stop_signal);
    }
    ~StopSignals() {
        std::signal(SIGINT, prev_int_);
        std::signal(SIGTERM, prev_term_);
    }
    StopSignals(const StopSignals&) = delete;
    StopSignals& operator=(const StopSignals&) = delete;

    [[nodiscard]] static bool requested() { return g_stop_requested != 0; }

private:
    void (*prev_int_)(int) = SIG_DFL;
    void (*prev_term_)(int) = SIG_DFL;
};

void print_notices(const AlarmSession& s, const std::vector<Notice>& notices) {
    for (const auto& n : notices) {
        switch (n.level) {
            case Notice::Level::Alarm:
                std::cout << "\n*** " << n.text << " ***   ";
                if (const auto* p = s.challenge()) {
                    std::cout << "Solve to stop: " << p->question << "   [s] snooze\n";
                } else {
                    std::cout << "[Enter] stop   [s] snooze\n";
                }
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

std::optional<int> parse_answer(const std::string& text) {
    int v = 0;
    const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), v);
    if (ec != std::errc{} || ptr != text.data() + text.size()) return std::nullopt;
    return v;
}

}  // namespace

int run_console(AlarmSession& session) {
    print_notices(session, session.start());
    std::cout << "Commands: [Enter] stop alarm  [s] snooze  [l] list alarms  [q] quit\n"
                 "Manage alarms from another terminal with 'alarm_clock add/edit/remove' - changes load automatically.\n\n";

    const StopSignals signals;
    LineInput input(stdin_is_terminal());
    bool reported_closed = false;
    std::size_t last_len = 0;
    for (bool running = true; running && !StopSignals::requested();) {
        print_notices(session, session.tick());

        std::string line = status_line(session);
        const std::size_t len = line.size();
        if (len < last_len) line.append(last_len - len, ' ');
        last_len = len;
        std::cout << '\r' << line << std::flush;

        auto cmd = input.pop_for(LineInput::until_next_second());
        if (input.closed() && !reported_closed) {
            reported_closed = true;
            std::cout << "\nNo more input - the clock keeps running. Press Ctrl+C to stop it.\n";
        }
        if (!cmd) continue;
        const bool ringing = !session.service().ringing().empty();

        if (*cmd == "q") {
            // Like the full-screen UI: quitting must not silence an alarm (or skip its challenge).
            if (ringing) {
                std::cout << "\nStop or snooze the alarm first.\n";
            } else {
                running = false;
            }
        } else if (*cmd == "s" || *cmd == "S") {
            if (ringing) {
                session.snooze();
                std::cout << "\nSnoozed.\n";
            }
        } else if (*cmd == "l" || *cmd == "L") {
            std::cout << "\n";
            const auto& svc = session.service();
            print_alarms(svc.alarms(), session.settings().hour_format, session.now(), session.time_zone(), std::cout,
                         [&svc](const Alarm& a) { return NextRing{svc.next_fire(a.id), svc.is_snoozed(a.id)}; });
        } else if (ringing) {
            switch (session.dismiss(parse_answer(*cmd))) {
                case AlarmSession::DismissResult::Dismissed:
                    std::cout << "\nAlarm stopped.\n";
                    break;
                case AlarmSession::DismissResult::NeedsAnswer:
                    std::cout << "\nSolve to stop: " << session.challenge()->question << '\n';
                    break;
                case AlarmSession::DismissResult::WrongAnswer:
                    std::cout << "\nNot quite. Try this one: " << session.challenge()->question << '\n';
                    break;
                case AlarmSession::DismissResult::NotRinging:
                    break;
            }
        }
    }

    session.shutdown();
    std::cout << "\nBye.\n";
    return 0;
}

}  // namespace ac
