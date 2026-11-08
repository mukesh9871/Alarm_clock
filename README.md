# Alarm Clock

A reliable, cross-platform console alarm clock written in modern C++20.

- Live clock that updates in place
- Alarm sound loops and fades in without freezing the clock (via [miniaudio](https://miniaud.io));
  falls back to a beep if the sound file or audio device is unavailable
- Never silently misses an alarm: if the computer was busy or briefly suspended, the alarm still rings;
  if it was asleep for a long time, you're told it was missed
- Correct across midnight and daylight-saving changes (unit-tested)
- Accepts `07:30`, `19:05`, `7:30pm`, `12am`, ...

## Build

Requirements: a C++20 compiler (GCC 11+, Clang 14+, MSVC 2022), CMake 3.24+ and Ninja.
Dependencies (miniaudio, Catch2) are downloaded automatically and verified by SHA-256.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure   # run the unit tests
```

## Run

```sh
./build/alarm_clock
```

Answer `y`, enter a time, and leave it running. Press **Enter** to stop a ringing alarm,
type **q** + Enter to quit.

## Project layout

| Path | Contents |
|---|---|
| `src/core/` | Pure logic: scheduling, time zones, parsing/formatting. No I/O; fully unit-tested. |
| `src/platform/` | OS-specific bits: audio playback, executable paths. |
| `src/app/` | The console application. |
| `tests/` | Catch2 unit tests, including a fake DST time zone for edge cases. |
| `assets/sounds/` | Bundled alarm sound, copied next to the executable at build time. |

## License

Apache 2.0. See [LICENSE](LICENSE).
