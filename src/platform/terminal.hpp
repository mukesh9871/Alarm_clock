#pragma once

namespace ac {

// True when the stream is attached to an interactive terminal (not a pipe, file or /dev/null).
[[nodiscard]] bool stdin_is_terminal();
[[nodiscard]] bool stdout_is_terminal();

// Notices requests to stop the program - Ctrl+C, SIGTERM and, on Windows, closing the
// console window or logging off - so a loop can shut down cleanly instead of being killed.
// Only one may exist at a time; the previous handling is restored when it is destroyed.
class StopRequests {
public:
    StopRequests();
    ~StopRequests();
    StopRequests(const StopRequests&) = delete;
    StopRequests& operator=(const StopRequests&) = delete;

    [[nodiscard]] static bool requested();
    // Call after cleaning up. A closing console window waits (briefly) for this.
    static void cleanup_done();

private:
    void (*prev_int_)(int) = nullptr;
    void (*prev_term_)(int) = nullptr;
};

}  // namespace ac
