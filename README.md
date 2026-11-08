# Alarm Clock

A reliable, cross-platform terminal alarm clock written in modern C++20.

```
╭──────────────────────────────────────────────────────────────────────────╮
│             ██████ ██████    ██████ ██████    ██████ ██████              │
│             ██  ██ ██     ██ ██  ██ ██  ██ ██ ██  ██ ██  ██              │
│             ██  ██ ██████    ██  ██ ██  ██    ██  ██ ██  ██              │
│             ██  ██ ██  ██ ██ ██  ██ ██  ██ ██ ██  ██ ██  ██              │
│             ██████ ██████    ██████ ██████    ██████ ██████              │
│                             Sat 10 Jan 2026                              │
│                      Next: 06:45 "Gym" in 2d 0h 45m                      │
├──────────────────────────────────────────────────────────────────────────┤
│ [on]  06:45     Weekdays              Gym                  in 2d 0h 45m  │
│ [off] 09:30     Once                  Brunch                             │
├──────────────────────────────────────────────────────────────────────────┤
│ a  add   e  edit   space  on/off   d  delete   t  12/24h   q  quit       │
╰──────────────────────────────────────────────────────────────────────────╯
```

- **Full-screen terminal UI** ([FTXUI](https://github.com/ArthurSonzogni/FTXUI)): big clock, alarm list,
  add/edit dialog, flashing ringing screen
- **Wakes the PC from sleep** for alarms (Windows) and keeps it awake while ringing
- **Multiple alarms** with labels, repeat days (weekdays, weekends, `mon,wed,fri`...) or one-shot
- **Math challenge** (optional, per alarm): solve a problem to stop the alarm, so you can't
  switch it off half-asleep; snooze still works
- **Bedtime calculator** based on 90-minute sleep cycles, plus a **countdown timer** and **stopwatch**
- **Snooze** (per-alarm length) and auto-snooze if nobody answers, giving up after 3 tries
- **Gentle wake-up**: the sound fades in; per-alarm custom WAV/MP3/FLAC
- **Never silently misses an alarm**: rings even if a tick was skipped; tells you about alarms
  missed while the computer slept or the app was closed
- Alarms are **saved** to a JSON file and edits from another terminal load automatically
- 12- or 24-hour display; accepts `07:30`, `19:05`, `7:30pm`, `12am` ...
- Correct across midnight and daylight-saving changes (unit-tested)
- Sound plays without freezing the clock (via [miniaudio](https://miniaud.io)); falls back to a beep
  if the sound file or audio device is unavailable

## Download

Prebuilt ZIPs for Windows, Linux and macOS are attached to each
[GitHub release](https://github.com/mukesh9871/Alarm_clock/releases). Unzip and run `alarm_clock`;
no installation or extra DLLs needed.

## Build

Requirements: a C++20 compiler (GCC 11+, Clang 14+, MSVC 2022), CMake 3.24+ and Ninja.
Dependencies (miniaudio, nlohmann/json, FTXUI, Catch2) are downloaded automatically and verified by SHA-256.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure   # run the unit tests
```

## Usage

```sh
alarm_clock add 6:45 --days weekdays --label "Gym"   # add alarms
alarm_clock add 9:30am --days sat,sun --snooze 15 --fade 60
alarm_clock list                                     # see them
alarm_clock edit 2 --time 10:00                      # change one
alarm_clock disable 2 / enable 2 / remove 2
alarm_clock add 6:00 --challenge math                 # must solve a sum to stop
alarm_clock set hour-format 12
alarm_clock bedtime 6:45                             # when to sleep to wake between cycles
alarm_clock timer 25m -l Pomodoro                    # countdown that rings
alarm_clock stopwatch                                # Enter = lap, q = stop
alarm_clock                                          # open the clock
alarm_clock run --plain                              # simple line output (also used when piped)
```

In the clock window:

| Key | Action |
|---|---|
| `a` / `e` or Enter | add / edit alarm (Enter in the form saves, Esc cancels) |
| `↑` `↓` | select alarm |
| Space | turn selected alarm on/off |
| `d` | delete (asks to confirm) |
| `t` | switch 12/24-hour |
| `b` | bedtime suggestions for the next alarm |
| `q` | quit |
| **While ringing:** `s` | snooze |
| **While ringing, math challenge:** digits, Enter | answer the problem to stop |
| **While ringing:** Enter / Space | stop |

Other keys are ignored while an alarm rings, so you can't quit by accident. Run `alarm_clock help`
for all command-line options.

### Waking from sleep (Windows)

While the clock is open, it asks Windows to wake the computer one minute before the next alarm,
and keeps it awake while an alarm rings. For this to work, **Allow wake timers** must be enabled in
*Control Panel → Power Options → Change plan settings → Advanced → Sleep*. The app warns you if
Windows refuses. It cannot wake a computer that is shut down or hibernating without wake support,
and on Linux/macOS waking from sleep is not supported (it needs root), but missed alarms are still
reported when the computer resumes.

Alarms are stored in `%APPDATA%\AlarmClock\alarms.json` (Windows),
`~/Library/Application Support/AlarmClock/alarms.json` (macOS) or
`~/.config/alarm-clock/alarms.json` (Linux). Use `--data FILE` to pick another file.
If the file is ever corrupted it is set aside as `alarms.json.corrupt-N`, never overwritten.

### Development options

```sh
cmake -S . -B build -DALARM_SANITIZE=address,undefined   # GCC/Clang sanitizers
cmake -S . -B build -DALARM_CLANG_TIDY=ON                 # lint while compiling (.clang-tidy)
cpack --config build/CPackConfig.cmake                    # build a ZIP package
```

CI builds and tests on Linux (GCC, Clang), Windows (MSVC, MinGW) and macOS, runs the tests under
ASan/UBSan, and runs clang-tidy. Pushing a `v*` tag publishes release ZIPs.

## Project layout

| Path | Contents |
|---|---|
| `src/core/` | Pure logic: scheduling, time zones, parsing/formatting. No I/O; fully unit-tested. |
| `src/platform/` | OS-specific bits: audio playback, file locations, alarm file storage, wake timers. |
| `src/app/` | CLI parsing, commands, the session that runs alarms, the terminal UI and plain console UI. |
| `tests/` | Catch2 unit tests, including a fake DST time zone for edge cases. |
| `assets/sounds/` | Bundled alarm sound, copied next to the executable at build time. |

## License

Apache 2.0. See [LICENSE](LICENSE).
