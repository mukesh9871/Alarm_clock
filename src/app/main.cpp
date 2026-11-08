#include "app/cli.hpp"
#include "app/commands.hpp"
#include "app/console_ui.hpp"
#include "app/timers.hpp"
#include "app/tui.hpp"
#include "app/session.hpp"
#include "core/clock.hpp"
#include "core/time_zone.hpp"
#include "platform/audio_player.hpp"
#include "platform/paths.hpp"
#include "platform/power.hpp"
#include "platform/state_file.hpp"

#include <exception>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#define AC_ISATTY(fd) _isatty(fd)
#else
#include <unistd.h>
#define AC_ISATTY(fd) isatty(fd)
#endif

#ifndef ALARM_CLOCK_VERSION
#define ALARM_CLOCK_VERSION "dev"
#endif

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);  // labels may contain non-ASCII text
#endif
    try {
        const std::vector<std::string> args(argv + 1, argv + argc);
        const auto inv = ac::cli::parse(args);

        if (std::holds_alternative<ac::cli::Help>(inv.command)) {
            std::cout << ac::cli::usage();
            return 0;
        }
        if (std::holds_alternative<ac::cli::Version>(inv.command)) {
            std::cout << "alarm_clock " << ALARM_CLOCK_VERSION << '\n';
            return 0;
        }

        if (const auto* t = std::get_if<ac::cli::Timer>(&inv.command)) {
            auto audio = ac::make_audio_player();
            return ac::run_timer(t->duration, t->label, *audio, ac::default_sound_path());
        }
        if (std::holds_alternative<ac::cli::Stopwatch>(inv.command)) return ac::run_stopwatch();

        ac::SystemClock clock;
        ac::SystemTimeZone tz;
        ac::StateFile file(inv.data_file.value_or(ac::default_state_path()));

        if (!std::holds_alternative<ac::cli::Run>(inv.command)) {
            return ac::run_command(inv.command, file, clock, tz, std::cout, std::cerr);
        }

        auto audio = ac::make_audio_player();
        auto power = ac::make_power_manager();
        ac::AlarmSession session(file, clock, tz, *audio, ac::resolve_sound, power.get());

        // The full-screen UI needs a real terminal; fall back to plain output when piped.
        const bool interactive = AC_ISATTY(0) && AC_ISATTY(1);
        if (std::get<ac::cli::Run>(inv.command).plain || !interactive) return ac::run_console(session);
        return ac::run_tui(session);
    } catch (const ac::cli::UsageError& e) {
        std::cerr << "alarm_clock: " << e.what() << "\nRun 'alarm_clock help' for usage.\n";
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "alarm_clock: " << e.what() << '\n';
        return 1;
    }
}
