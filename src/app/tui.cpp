#include "app/tui.hpp"

#include "app/big_digits.hpp"
#include "core/days.hpp"
#include "core/sleep_cycles.hpp"
#include "core/time_format.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <charconv>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

namespace ac {

namespace {

using namespace ftxui;
using namespace std::chrono_literals;

constexpr std::array<const char*, 7> kDayNames{"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
constexpr std::size_t kMaxNotices = 6;

// Checkbox index (Mon-first) -> std::chrono::weekday encoding (Sun = 0).
std::chrono::weekday ui_weekday(std::size_t i) { return std::chrono::weekday{static_cast<unsigned>((i + 1) % 7)}; }

std::optional<int> parse_int(const std::string& s, int lo, int hi) {
    int v = 0;
    const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec != std::errc{} || ptr != s.data() + s.size() || v < lo || v > hi) return std::nullopt;
    return v;
}

// The add/edit dialog's text fields.
struct EditForm {
    int id = 0;  // 0 = new alarm
    std::string time, label, snooze, fade, sound;
    std::array<bool, 7> days{};
    bool math = false;
    std::string error;

    void load(const Alarm& a) {
        id = a.id;
        time = a.id ? format_time_of_day(a.time) : "";
        label = a.label;
        snooze = std::to_string(a.snooze_minutes);
        fade = std::to_string(a.fade_in_seconds);
        sound = a.sound;
        for (std::size_t i = 0; i < days.size(); ++i) days[i] = a.days.contains(ui_weekday(i));
        math = a.challenge == ChallengeKind::Math;
        error.clear();
    }

    // Returns the alarm, or sets `error` and returns nullopt.
    std::optional<Alarm> build(const Alarm& base) {
        Alarm a = base;
        const auto t = parse_time_of_day(time);
        if (!t) return fail("Time must look like 07:30, 19:05 or 7:30pm.");
        const auto sn = parse_int(snooze, 1, 60);
        if (!sn) return fail("Snooze must be 1-60 minutes.");
        const auto fd = parse_int(fade, 0, 600);
        if (!fd) return fail("Fade-in must be 0-600 seconds.");
        a.time = *t;
        a.label = label;
        a.snooze_minutes = *sn;
        a.fade_in_seconds = *fd;
        a.sound = sound;
        a.days = DaySet{};
        for (std::size_t i = 0; i < days.size(); ++i) {
            if (days[i]) a.days.add(ui_weekday(i));
        }
        a.challenge = math ? ChallengeKind::Math : ChallengeKind::None;
        if (schedule_changed(base, a)) a.enabled = true;
        return a;
    }

private:
    std::optional<Alarm> fail(std::string msg) {
        error = std::move(msg);
        return std::nullopt;
    }
};

Element notice_line(const Notice& n) {
    switch (n.level) {
        case Notice::Level::Alarm:
            return text(" * " + n.text) | bold | color(Color::RedLight);
        case Notice::Level::Warning:
            return text(" ! " + n.text) | color(Color::Yellow);
        case Notice::Level::Info:
            break;
    }
    return text("   " + n.text) | dim;
}

Element key_hint(const std::string& key, const std::string& what) {
    return hbox({text(" " + key + " ") | inverted, text(" " + what + "  ")});
}

class Tui {
public:
    explicit Tui(AlarmSession& session) : session_(session) {}

    std::string snapshot(int width, int height) {
        refresh();
        Element frame = render_main();
        if (ringing_) frame = dbox({frame, render_ringing()});
        auto screen = Screen::Create(Dimension::Fixed(width), Dimension::Fixed(height));
        Render(screen, frame);
        return screen.ToString();
    }

    int run() {
        push(session_.start());
        refresh();

        auto root = build();
        std::mutex m;
        std::condition_variable cv;
        bool stop = false;
        std::thread ticker([&] {
            std::unique_lock lock(m);
            while (!stop) {
                const auto now = std::chrono::system_clock::now();
                cv.wait_until(lock, std::chrono::floor<std::chrono::seconds>(now) + 1s, [&] { return stop; });
                if (stop) break;
                screen_.Post([this] {
                    push(session_.tick());
                    refresh();
                });
                screen_.PostEvent(Event::Custom);
            }
        });

        screen_.Loop(root);

        {
            std::lock_guard lock(m);
            stop = true;
        }
        cv.notify_all();
        ticker.join();
        session_.shutdown();
        return 0;
    }

private:
    void push(const std::vector<Notice>& notices) {
        for (const auto& n : notices) {
            notices_.push_back(n);
            if (notices_.size() > kMaxNotices) notices_.pop_front();
        }
    }

    void refresh() {
        alarms_ = session_.service().alarms();
        selected_ = std::clamp(selected_, 0, std::max(0, static_cast<int>(alarms_.size()) - 1));
        ringing_ = !session_.service().ringing().empty();
    }

    const Alarm* current() const {
        return alarms_.empty() ? nullptr : &alarms_[static_cast<std::size_t>(selected_)];
    }

    HourFormat fmt() const { return session_.settings().hour_format; }

    // ---------- rendering ----------

    Element render_clock() const {
        const auto now = session_.now();
        const auto local = session_.time_zone().to_local(now);
        const bool h12 = fmt() == HourFormat::H12;
        const int hour = h12 ? (local.hour % 12 == 0 ? 12 : local.hour % 12) : local.hour;
        char buf[16];
        std::snprintf(buf, sizeof buf, "%02d:%02d:%02d", hour, local.minute, local.second);

        Element big;
        if (Terminal::Size().dimx >= big_digits_width(buf) + 10) {
            Elements rows;
            for (const auto& r : big_digits(buf)) rows.push_back(text(r));
            big = hbox({vbox(rows) | color(Color::CyanLight),
                        h12 ? vbox({filler(), text(local.hour < 12 ? " AM" : " PM") | bold}) : emptyElement()});
        } else {
            big = text(format_clock(local, fmt())) | bold | color(Color::CyanLight);
        }

        std::string next_line = "No alarms set";
        const auto& svc = session_.service();
        if (auto next = svc.next_due()) {
            next_line = std::string(svc.is_snoozed(next->first) ? "Snoozed: " : "Next: ") + session_.describe(next->first) +
                        " in " + format_duration(next->second - now);
        }
        return vbox({
                   text(""),
                   big | hcenter,
                   text(""),
                   text(format_date(local)) | hcenter,
                   text(next_line) | dim | hcenter,
               }) |
               flex_shrink;
    }

    Element render_alarm_row(const Alarm& a, bool selected) const {
        const auto now = session_.now();
        const auto& svc = session_.service();
        std::string when;
        if (const auto next = svc.next_fire(a.id)) {
            when = (svc.is_snoozed(a.id) ? "snoozed, " : "in ") + format_duration(*next - now);
        } else if (const auto ringing = svc.ringing(); std::find(ringing.begin(), ringing.end(), a.id) != ringing.end()) {
            when = "RINGING";
        }
        Element row = hbox({
            text(a.enabled ? " [on]  " : " [off] ") | (a.enabled ? color(Color::GreenLight) : Decorator(dim)),
            text(format_time_of_day(a.time, fmt())) | bold | size(WIDTH, EQUAL, 10),
            text(format_days(a.days)) | size(WIDTH, EQUAL, 22),
            text(a.label + (a.challenge == ChallengeKind::Math ? " [math]" : "")) | flex,
            text(when + " ") | dim,
        });
        if (!a.enabled) row = row | dim;
        if (selected) row = row | inverted | focus;
        return row;
    }

    Element render_main() const {
        Elements rows;
        for (std::size_t i = 0; i < alarms_.size(); ++i) {
            rows.push_back(render_alarm_row(alarms_[i], static_cast<int>(i) == selected_));
        }
        if (rows.empty()) rows.push_back(text("No alarms yet - press 'a' to add one.") | dim | hcenter);

        Elements log;
        for (const auto& n : notices_) log.push_back(notice_line(n));
        if (session_.wake_supported() && !session_.wake_timer_ok()) {
            log.push_back(notice_line({Notice::Level::Warning,
                                       "Windows refused the wake timer: enable 'Allow wake timers' in Power Options."}));
        }

        Element footer;
        if (confirm_delete_) {
            footer = hbox({text(" Delete " + session_.describe(current() ? current()->id : 0) + "? ") | bold,
                           key_hint("y", "yes"), key_hint("any key", "no")});
        } else {
            footer = hbox({key_hint("a", "add"), key_hint("e", "edit"), key_hint("space", "on/off"),
                           key_hint("d", "delete"), key_hint("t", "12/24h"), key_hint("b", "bedtime"), key_hint("q", "quit")});
        }

        return vbox({
                   render_clock(),
                   separator(),
                   vbox(rows) | vscroll_indicator | frame | flex,
                   log.empty() ? emptyElement() : separator(),
                   vbox(log),
                   separator(),
                   footer,
               }) |
               borderRounded;
    }

    Element render_ringing() const {
        const auto ids = session_.service().ringing();
        std::string what = ids.empty() ? "" : session_.describe(ids.front());
        const bool flash = session_.now().time_since_epoch().count() % 2 == 0;
        auto title = text("  ALARM  ") | bold | (flash ? bgcolor(Color::Red) : bgcolor(Color::DarkRed)) |
                     color(Color::White);
        Elements body{title | hcenter, text(""), text(what) | bold | hcenter, text("")};
        if (const auto* p = session_.challenge()) {
            body.push_back(text("Solve to stop:  " + p->question) | hcenter);
            body.push_back(hbox({text("> "), text(answer_) | bold, text("_") | blink}) | hcenter);
            if (!challenge_msg_.empty()) body.push_back(text(challenge_msg_) | color(Color::Yellow) | hcenter);
            body.push_back(text(""));
            body.push_back(hbox({key_hint("0-9", "answer"), key_hint("Enter", "submit"), key_hint("s", "snooze")}) |
                           hcenter);
        } else {
            body.push_back(hbox({key_hint("s", "snooze"), key_hint("Enter", "stop")}) | hcenter);
        }
        return vbox(body) |
               borderDouble | size(WIDTH, GREATER_THAN, 40) | clear_under | center;
    }

    // ---------- components ----------

    Component build() {
        InputOption single;
        single.multiline = false;
        single.on_enter = [this] { save_form(); };  // Enter in any field saves

        auto in_time = Input(&form_.time, "07:30 or 7:30pm", single);
        auto in_label = Input(&form_.label, "e.g. Gym", single);
        auto in_snooze = Input(&form_.snooze, "9", single);
        auto in_fade = Input(&form_.fade, "30", single);
        auto in_sound = Input(&form_.sound, "bundled sound", single);
        Components day_boxes;
        for (std::size_t i = 0; i < form_.days.size(); ++i) day_boxes.push_back(Checkbox(kDayNames[i], &form_.days[i]));
        auto days = Container::Horizontal(day_boxes);
        auto math_box = Checkbox("Solve a math problem to stop", &form_.math);
        auto save = Button(" Save ", [this] { save_form(); }, ButtonOption::Ascii());
        auto cancel = Button(" Cancel ", [this] { show_form_ = false; }, ButtonOption::Ascii());
        time_input_ = in_time;

        auto form_container = Container::Vertical(
            {in_time, in_label, days, math_box, in_snooze, in_fade, in_sound, Container::Horizontal({save, cancel})});

        auto field = [](const char* name, const Component& c) {
            return hbox({text(name) | size(WIDTH, EQUAL, 12), c->Render() | flex});
        };
        auto form_view = Renderer(form_container, [=, this] {
            Elements day_els;
            for (const auto& d : day_boxes) day_els.push_back(d->Render());
            return window(text(form_.id ? " Edit alarm " : " New alarm ") | bold,
                          vbox({
                              field("Time", in_time),
                              field("Label", in_label),
                              hbox({text("Repeat") | size(WIDTH, EQUAL, 12), hbox(day_els)}),
                              text("            (no days ticked = rings once)") | dim,
                              hbox({text("Challenge") | size(WIDTH, EQUAL, 12), math_box->Render()}),
                              field("Snooze min", in_snooze),
                              field("Fade-in s", in_fade),
                              field("Sound file", in_sound),
                              form_.error.empty() ? emptyElement() : text(form_.error) | color(Color::Yellow),
                              text("Tab/arrows move, Enter saves, Esc cancels") | dim,
                              separator(),
                              hbox({filler(), save->Render(), text("  "), cancel->Render()}),
                          })) |
                   size(WIDTH, GREATER_THAN, 60) | clear_under;
        });

        auto ring_view = Renderer([this] { return render_ringing(); });
        auto main_view = Renderer([this] { return render_main(); });

        auto root = Modal(main_view, form_view, &show_form_);
        root = Modal(root, ring_view, &ringing_);
        return CatchEvent(root, [this](const Event& e) { return on_event(e); });
    }

    bool on_event(const Event& e) {
        if (e == Event::Custom) return false;

        if (ringing_) {
            if (e == Event::Character('s') || e == Event::Character('S')) {
                session_.snooze();
                push({{Notice::Level::Info, "Snoozed."}});
                answer_.clear();
                challenge_msg_.clear();
            } else if (session_.challenge()) {
                on_challenge_key(e);
            } else if (e == Event::Return || e == Event::Character('d') || e == Event::Character(' ')) {
                session_.dismiss();
                push({{Notice::Level::Info, "Alarm stopped."}});
            }
            refresh();
            return true;  // swallow everything else so a stray key can't quit while ringing
        }

        if (show_form_) {
            if (e == Event::Escape) {
                show_form_ = false;
                return true;
            }
            return false;  // let the form handle it
        }

        if (confirm_delete_) {
            confirm_delete_ = false;
            if (e == Event::Character('y') || e == Event::Character('Y')) {
                if (const Alarm* a = current()) {
                    const std::string name = session_.describe(a->id);
                    session_.remove_alarm(a->id);
                    push({{Notice::Level::Info, "Deleted " + name + "."}});
                }
                refresh();
            }
            return true;
        }

        if (e == Event::ArrowUp || e == Event::Character('k')) {
            selected_ = std::max(0, selected_ - 1);
        } else if (e == Event::ArrowDown || e == Event::Character('j')) {
            selected_ = std::min(static_cast<int>(alarms_.size()) - 1, selected_ + 1);
            selected_ = std::max(0, selected_);
        } else if (e == Event::Character('a')) {
            open_form(Alarm{});
        } else if (e == Event::Character('e') || e == Event::Return) {
            if (const Alarm* a = current()) open_form(*a);
        } else if (e == Event::Character(' ')) {
            if (const Alarm* a = current()) session_.toggle_alarm(a->id);
            refresh();
        } else if (e == Event::Character('d') || e == Event::Delete) {
            confirm_delete_ = current() != nullptr;
        } else if (e == Event::Character('b')) {
            show_bedtimes();
        } else if (e == Event::Character('t')) {
            session_.set_hour_format(fmt() == HourFormat::H24 ? HourFormat::H12 : HourFormat::H24);
        } else if (e == Event::Character('q') || e == Event::Escape) {
            screen_.Exit();
        } else {
            return false;
        }
        return true;
    }

    void on_challenge_key(const Event& e) {
        if (e.is_character() && e.character().size() == 1 && std::isdigit(static_cast<unsigned char>(e.character()[0]))) {
            if (answer_.size() < 6) answer_ += e.character();
        } else if (e == Event::Character('-') && answer_.empty()) {
            answer_ = "-";
        } else if (e == Event::Backspace && !answer_.empty()) {
            answer_.pop_back();
        } else if (e == Event::Return && !answer_.empty()) {
            const auto result = session_.dismiss(parse_int(answer_, -1'000'000, 1'000'000));
            answer_.clear();
            switch (result) {
                case AlarmSession::DismissResult::Dismissed:
                    challenge_msg_.clear();
                    push({{Notice::Level::Info, "Correct - alarm stopped. Good morning!"}});
                    break;
                case AlarmSession::DismissResult::WrongAnswer:
                    challenge_msg_ = "Not quite - here's a new one.";
                    break;
                case AlarmSession::DismissResult::NeedsAnswer:
                    challenge_msg_ = "Type a number, then press Enter.";
                    break;
                case AlarmSession::DismissResult::NotRinging:
                    challenge_msg_.clear();
                    break;
            }
        }
    }

    void show_bedtimes() {
        const auto now = session_.now();
        std::vector<SleepOption> options;
        std::string header;
        if (auto next = session_.service().next_due()) {
            const auto wake = session_.time_zone().to_local(next->second).time_of_day();
            options = bedtimes_for(wake);
            header = "Bedtimes for " + session_.describe(next->first) + " (90-min cycles):";
        } else {
            options = wake_times_for(session_.time_zone().to_local(now).time_of_day());
            header = "If you sleep now, wake at (90-min cycles):";
        }
        std::string line;
        for (const auto& o : options) {
            line += format_time_of_day(o.time, fmt()) + " (" + format_duration(o.sleep) + ")   ";
        }
        push({{Notice::Level::Info, header}, {Notice::Level::Info, line}});
    }

    void open_form(const Alarm& a) {
        form_base_ = a;
        form_.load(a);
        show_form_ = true;
        time_input_->TakeFocus();
    }

    void save_form() {
        auto alarm = form_.build(form_base_);
        if (!alarm) return;
        const int id = session_.save_alarm(*alarm);
        show_form_ = false;
        refresh();
        for (std::size_t i = 0; i < alarms_.size(); ++i) {
            if (alarms_[i].id == id) selected_ = static_cast<int>(i);
        }
        if (auto next = session_.service().next_fire(id)) {
            push({{Notice::Level::Info,
                   "Saved " + session_.describe(id) + " - rings in " + format_duration(*next - session_.now()) + "."}});
        }
    }

    AlarmSession& session_;
    ScreenInteractive screen_ = ScreenInteractive::Fullscreen();

    std::vector<Alarm> alarms_;
    int selected_ = 0;
    bool ringing_ = false;
    bool show_form_ = false;
    bool confirm_delete_ = false;
    EditForm form_;
    Alarm form_base_;
    Component time_input_;
    std::string answer_;
    std::string challenge_msg_;
    std::deque<Notice> notices_;
};

}  // namespace

int run_tui(AlarmSession& session) {
    Tui tui(session);
    return tui.run();
}

std::string render_tui_snapshot(AlarmSession& session, int width, int height) {
    Tui tui(session);
    return tui.snapshot(width, height);
}

}  // namespace ac
