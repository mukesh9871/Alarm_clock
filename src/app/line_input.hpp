#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace ac {

// Reads stdin lines on a background thread so a UI loop can wait with a timeout.
// "q"/"Q" is delivered as "q".
//
// End of input means "quit" only when typed at a terminal (Ctrl+D / Ctrl+Z). When stdin is
// a pipe, a file or /dev/null (scheduled tasks, nohup), it just means no more input: the
// caller keeps running and can check closed().
//
// A thread blocked in std::getline can't be interrupted portably, so the reader owns its
// state through a shared_ptr and is detached on destruction; it ends with the process.
class LineInput {
public:
    explicit LineInput(bool eof_quits) : state_(std::make_shared<State>()) {
        std::thread([state = state_, eof_quits] {
            std::string line;
            while (std::getline(std::cin, line)) {
                // Keep reading after "q": the UI may refuse to quit (e.g. while an alarm rings).
                state->push(line == "Q" ? "q" : std::move(line));
            }
            state->closed = true;
            if (eof_quits) state->push("q");
        }).detach();
    }

    // True once stdin has reached end of input.
    [[nodiscard]] bool closed() const { return state_->closed; }

    std::optional<std::string> pop_for(std::chrono::milliseconds timeout) {
        std::unique_lock lock(state_->mutex);
        if (!state_->cv.wait_for(lock, timeout, [this] { return !state_->lines.empty(); })) return std::nullopt;
        std::string line = std::move(state_->lines.front());
        state_->lines.pop_front();
        return line;
    }

    // Milliseconds until the next whole second of the system clock (at least 1).
    static std::chrono::milliseconds until_next_second() {
        using namespace std::chrono;
        const auto now = system_clock::now();
        return std::max(ceil<milliseconds>(floor<seconds>(now) + seconds{1} - now), milliseconds{1});
    }

private:
    struct State {
        std::mutex mutex;
        std::condition_variable cv;
        std::deque<std::string> lines;
        std::atomic<bool> closed{false};

        void push(std::string line) {
            {
                std::lock_guard lock(mutex);
                lines.push_back(std::move(line));
            }
            cv.notify_one();
        }
    };
    std::shared_ptr<State> state_;
};

}  // namespace ac
