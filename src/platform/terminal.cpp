#include "platform/terminal.hpp"

#include <atomic>
#include <csignal>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace ac {

namespace {
std::atomic<bool> g_stop_requested{false};
std::atomic<bool> g_cleanup_done{false};

extern "C" void on_stop_signal(int /*sig*/) { g_stop_requested = true; }

#ifdef _WIN32
// _isatty() is also true for character devices such as NUL, so ask for a console instead.
bool is_console(DWORD std_handle) {
    HANDLE h = GetStdHandle(std_handle);
    DWORD mode = 0;
    return h != nullptr && h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &mode) != 0;
}

BOOL WINAPI on_console_event(DWORD type) {
    g_stop_requested = true;
    if (type == CTRL_CLOSE_EVENT || type == CTRL_LOGOFF_EVENT || type == CTRL_SHUTDOWN_EVENT) {
        // Windows ends the process as soon as this returns: give the loop a moment to save
        // and cancel the wake timer (the system allows a few seconds).
        for (int i = 0; i < 60 && !g_cleanup_done; ++i) Sleep(50);
    }
    return TRUE;
}
#endif
}  // namespace

#ifdef _WIN32
bool stdin_is_terminal() { return is_console(STD_INPUT_HANDLE); }
bool stdout_is_terminal() { return is_console(STD_OUTPUT_HANDLE); }
#else
bool stdin_is_terminal() { return isatty(0) != 0; }
bool stdout_is_terminal() { return isatty(1) != 0; }
#endif

StopRequests::StopRequests() {
    g_stop_requested = false;
    g_cleanup_done = false;
#ifdef _WIN32
    // A process started in a new process group (as some launchers do) inherits "ignore
    // Ctrl+C"; turn normal handling back on, then handle it ourselves.
    SetConsoleCtrlHandler(nullptr, FALSE);
    SetConsoleCtrlHandler(on_console_event, TRUE);
#endif
    prev_int_ = std::signal(SIGINT, on_stop_signal);
    prev_term_ = std::signal(SIGTERM, on_stop_signal);
}

StopRequests::~StopRequests() {
    std::signal(SIGINT, prev_int_ == SIG_ERR ? SIG_DFL : prev_int_);
    std::signal(SIGTERM, prev_term_ == SIG_ERR ? SIG_DFL : prev_term_);
#ifdef _WIN32
    SetConsoleCtrlHandler(on_console_event, FALSE);
#endif
}

bool StopRequests::requested() { return g_stop_requested; }

void StopRequests::cleanup_done() { g_cleanup_done = true; }

}  // namespace ac
