# Alarm Clock

A console alarm clock written in C++.

> This is the original 2017 learning project. It is being modernised in phases
> (see the `modernize` branch history).

## Build

Requirements: Windows, a MinGW-w64 GCC toolchain, CMake 3.20+ and Ninja.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Run

```sh
./build/alarm_clock.exe
```

Answer `y` to set an alarm, then enter the hour (24-hour format) and minute.
The current time prints every second; when the alarm time is reached, `music1.wav`
plays. Press **Enter** to quit.

![Sample output](https://github.com/mukesh9871/Alarm_clock/assets/19510273/2f059fce-4b29-42a9-a65a-d3c12750af6d)

## License

Apache 2.0. See [LICENSE](LICENSE).
