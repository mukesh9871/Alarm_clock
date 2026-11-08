#include "app/cli.hpp"

#include <charconv>

namespace ac::cli {

namespace {

int parse_int(const std::string& text, const std::string& what, int lo, int hi) {
    int value = 0;
    const auto* end = text.data() + text.size();
    const auto [ptr, ec] = std::from_chars(text.data(), end, value);
    if (ec != std::errc{} || ptr != end || value < lo || value > hi) {
        throw UsageError(what + " must be a whole number from " + std::to_string(lo) + " to " + std::to_string(hi) +
                         ", got '" + text + "'");
    }
    return value;
}

TimeOfDay parse_time_arg(const std::string& text) {
    if (auto t = parse_time_of_day(text)) return *t;
    throw UsageError("'" + text + "' is not a valid time. Use HH:MM (24-hour) or add am/pm, e.g. 07:30 or 7:30pm");
}

class ArgReader {
public:
    explicit ArgReader(const std::vector<std::string>& args) : args_(args) {}
    [[nodiscard]] bool done() const { return pos_ >= args_.size(); }
    const std::string& next(const std::string& what) {
        if (done()) throw UsageError("missing " + what);
        return args_[pos_++];
    }
    void expect_done() const {
        if (!done()) throw UsageError("unexpected argument '" + args_[pos_] + "'");
    }

private:
    const std::vector<std::string>& args_;
    std::size_t pos_ = 0;
};

AlarmOptions parse_options(ArgReader& r) {
    AlarmOptions o;
    while (!r.done()) {
        const std::string flag = r.next("option");
        if (flag == "--time" || flag == "-t") {
            o.time = parse_time_arg(r.next("value for --time"));
        } else if (flag == "--label" || flag == "-l") {
            o.label = r.next("value for --label");
        } else if (flag == "--days" || flag == "-d") {
            const auto& v = r.next("value for --days");
            o.days = parse_days(v);
            if (!o.days) throw UsageError("invalid --days '" + v + "'. Use once, daily, weekdays, weekends or e.g. mon,wed,fri");
        } else if (flag == "--snooze") {
            o.snooze_minutes = parse_int(r.next("value for --snooze"), "--snooze", 1, 60);
        } else if (flag == "--sound") {
            o.sound = r.next("value for --sound");
        } else if (flag == "--fade") {
            o.fade_in_seconds = parse_int(r.next("value for --fade"), "--fade", 0, 600);
        } else {
            throw UsageError("unknown option '" + flag + "'");
        }
    }
    return o;
}

}  // namespace

Invocation parse(const std::vector<std::string>& all_args) {
    Invocation inv;

    // Global options come first.
    std::vector<std::string> args;
    for (std::size_t i = 0; i < all_args.size(); ++i) {
        if (all_args[i] == "--data") {
            if (i + 1 >= all_args.size()) throw UsageError("missing value for --data");
            inv.data_file = std::filesystem::path(all_args[++i]);
        } else {
            args.assign(all_args.begin() + static_cast<std::ptrdiff_t>(i), all_args.end());
            break;
        }
    }

    ArgReader r(args);
    if (r.done()) return inv;  // Run

    const std::string cmd = r.next("command");
    if (cmd == "run") {
        inv.command = Run{};
    } else if (cmd == "list" || cmd == "ls") {
        inv.command = List{};
    } else if (cmd == "add") {
        Add add;
        add.options.time = parse_time_arg(r.next("alarm time, e.g. 'add 07:30'"));
        const AlarmOptions rest = parse_options(r);
        if (rest.time) throw UsageError("give the time once, as the first argument to 'add'");
        add.options.label = rest.label;
        add.options.days = rest.days;
        add.options.snooze_minutes = rest.snooze_minutes;
        add.options.sound = rest.sound;
        add.options.fade_in_seconds = rest.fade_in_seconds;
        inv.command = add;
        return inv;
    } else if (cmd == "edit") {
        Edit edit;
        edit.id = parse_int(r.next("alarm id"), "alarm id", 1, 1'000'000);
        edit.options = parse_options(r);
        inv.command = edit;
        return inv;
    } else if (cmd == "remove" || cmd == "rm" || cmd == "delete") {
        inv.command = Remove{parse_int(r.next("alarm id"), "alarm id", 1, 1'000'000)};
    } else if (cmd == "enable" || cmd == "on") {
        inv.command = SetEnabled{parse_int(r.next("alarm id"), "alarm id", 1, 1'000'000), true};
    } else if (cmd == "disable" || cmd == "off") {
        inv.command = SetEnabled{parse_int(r.next("alarm id"), "alarm id", 1, 1'000'000), false};
    } else if (cmd == "set") {
        const std::string key = r.next("setting name");
        if (key != "hour-format") throw UsageError("unknown setting '" + key + "' (available: hour-format)");
        const std::string v = r.next("value for hour-format (12 or 24)");
        if (v == "12" || v == "12h") {
            inv.command = SetHourFormat{HourFormat::H12};
        } else if (v == "24" || v == "24h") {
            inv.command = SetHourFormat{HourFormat::H24};
        } else {
            throw UsageError("hour-format must be 12 or 24");
        }
    } else if (cmd == "help" || cmd == "--help" || cmd == "-h") {
        inv.command = Help{};
    } else if (cmd == "version" || cmd == "--version") {
        inv.command = Version{};
    } else {
        throw UsageError("unknown command '" + cmd + "'");
    }
    r.expect_done();
    return inv;
}

std::string usage() {
    return R"(Usage: alarm_clock [--data FILE] [COMMAND]

Commands:
  (none) | run              Show the clock and ring alarms
  list                      List alarms
  add TIME [options]        Add an alarm, e.g.  add 6:45 --days weekdays --label "Gym"
  edit ID [options]         Change an alarm,    e.g.  edit 2 --time 7:00
  remove ID                 Delete an alarm
  enable ID | disable ID    Turn an alarm on or off
  set hour-format 12|24     Choose 12- or 24-hour display
  help | version

Alarm options:
  -t, --time TIME           07:30, 19:05, 7:30pm, 12am ...
  -l, --label TEXT          Name shown when it rings
  -d, --days DAYS           once (default), daily, weekdays, weekends, or mon,wed,fri
      --snooze MINUTES      Snooze length, 1-60 (default 9)
      --sound FILE          WAV/MP3/FLAC to play (default: bundled sound)
      --fade SECONDS        Fade-in time, 0-600 (default 30)

Global options:
  --data FILE               Use this alarm file instead of the default location
)";
}

}  // namespace ac::cli
