#pragma once

#include "core/days.hpp"
#include "core/time_format.hpp"

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace ac::cli {

// Fields that `add` / `edit` may set. Unset = keep existing / use default.
struct AlarmOptions {
    std::optional<TimeOfDay> time;
    std::optional<std::string> label;
    std::optional<DaySet> days;
    std::optional<int> snooze_minutes;
    std::optional<std::string> sound;
    std::optional<int> fade_in_seconds;
};

struct Run {
    bool plain = false;  // line-based output instead of the full-screen UI
};
struct List {};
struct Add { AlarmOptions options; };
struct Edit { int id; AlarmOptions options; };
struct Remove { int id; };
struct SetEnabled { int id; bool enabled; };
struct SetHourFormat { HourFormat format; };
struct Help {};
struct Version {};

using Command = std::variant<Run, List, Add, Edit, Remove, SetEnabled, SetHourFormat, Help, Version>;

struct Invocation {
    std::optional<std::filesystem::path> data_file;  // --data <file>
    Command command = Run{};
};

class UsageError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// `args` excludes argv[0]. Throws UsageError with a helpful message.
[[nodiscard]] Invocation parse(const std::vector<std::string>& args);

[[nodiscard]] std::string usage();

}  // namespace ac::cli
