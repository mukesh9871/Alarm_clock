# Alarm Clock

A reliable, cross-platform console alarm clock written in modern C++20.

- **Multiple alarms** with labels, repeat days (weekdays, weekends, `mon,wed,fri`...) or one-shot
- **Snooze** (per-alarm length) and auto-snooze if nobody answers, giving up after 3 tries
- **Gentle wake-up**: the sound fades in; per-alarm custom WAV/MP3/FLAC
- **Never silently misses an alarm**: rings even if a tick was skipped; tells you about alarms
  missed while the computer slept or the app was closed
- Alarms are **saved** to a JSON file and edits from another terminal load automatically
- 12- or 24-hour display; accepts `07:30`, `19:05`, `7:30pm`, `12am` ...
- Correct across midnight and daylight-saving changes (unit-tested)
- Sound plays without freezing the clock (via [miniaudio](https://miniaud.io)); falls back to a beep
  if the sound file or audio device is unavailable

## Build

Requirements: a C++20 compiler (GCC 11+, Clang 14+, MSVC 2022), CMake 3.24+ and Ninja.
Dependencies (miniaudio, nlohmann/json, Catch2) are downloaded automatically and verified by SHA-256.

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
alarm_clock set hour-format 12
alarm_clock                                          # run the clock
```

While the clock runs: **Enter** stops a ringing alarm, **s** snoozes, **l** lists alarms, **q** quits.
Run `alarm_clock help` for all options.

Alarms are stored in `%APPDATA%\AlarmClocklarms.json` (Windows),
`~/Library/Application Support/AlarmClock/alarms.json` (macOS) or
`~/.config/alarm-clock/alarms.json` (Linux). Use `--data FILE` to pick another file.
If the file is ever corrupted it is set aside as `alarms.json.corrupt-N`, never overwritten.

## Project layout

| Path | Contents |
|---|---|
| `src/core/` | Pure logic: scheduling, time zones, parsing/formatting. No I/O; fully unit-tested. |
| `src/platform/` | OS-specific bits: audio playback, file locations, alarm file storage. |
| `src/app/` | CLI parsing, commands, the session that runs alarms, and the console UI. |
| `tests/` | Catch2 unit tests, including a fake DST time zone for edge cases. |
| `assets/sounds/` | Bundled alarm sound, copied next to the executable at build time. |

## License

Apache 2.0. See [LICENSE](LICENSE).
