cmake_minimum_required(VERSION 3.24)
project(AlarmClock VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ALARM_BUILD_TESTS "Build unit tests" ON)
option(ALARM_CLANG_TIDY "Run clang-tidy on the project's own code while compiling" OFF)
set(ALARM_SANITIZE "" CACHE STRING "Sanitizers to enable (GCC/Clang), e.g. address,undefined")

# Ship self-contained binaries: no compiler runtime DLLs needed next to the .exe.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
if(MINGW)
  string(APPEND CMAKE_EXE_LINKER_FLAGS " -static")
endif()

if(ALARM_SANITIZE)
  add_compile_options(-fsanitize=${ALARM_SANITIZE} -fno-omit-frame-pointer -fno-sanitize-recover=all)
  add_link_options(-fsanitize=${ALARM_SANITIZE})
endif()

include(cmake/Dependencies.cmake)

# Warnings for our own code only (third-party targets are left alone).
add_library(alarm_warnings INTERFACE)
if(MSVC)
  target_compile_options(alarm_warnings INTERFACE /W4 /permissive- /utf-8)
else()
  target_compile_options(alarm_warnings INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion)
endif()

# ---- Core library: pure logic, no I/O, fully unit-tested ----
add_library(alarm_core STATIC
  src/core/alarm_service.cpp
  src/core/challenge.cpp
  src/core/days.cpp
  src/core/duration.cpp
  src/core/saved_state.cpp
  src/core/schedule.cpp
  src/core/sleep_cycles.cpp
  src/core/time_format.cpp
  src/core/time_zone.cpp
)
target_include_directories(alarm_core PUBLIC src)
target_link_libraries(alarm_core PRIVATE alarm_warnings nlohmann_json::nlohmann_json)

# ---- Platform layer: audio, paths, files ----
add_library(alarm_platform STATIC
  src/platform/audio_player.cpp
  src/platform/paths.cpp
  src/platform/power.cpp
  src/platform/state_file.cpp
)
target_include_directories(alarm_platform PUBLIC src)
target_link_libraries(alarm_platform PUBLIC alarm_core PRIVATE alarm_warnings miniaudio)

# ---- Application logic (testable) and executable ----
add_library(alarm_app STATIC
  src/app/big_digits.cpp
  src/app/cli.cpp
  src/app/commands.cpp
  src/app/console_ui.cpp
  src/app/session.cpp
  src/app/timers.cpp
  src/app/tui.cpp
)
target_include_directories(alarm_app PUBLIC src)
target_link_libraries(alarm_app PUBLIC alarm_core alarm_platform PRIVATE alarm_warnings ftxui::component)

add_executable(alarm_clock src/app/main.cpp)
target_link_libraries(alarm_clock PRIVATE alarm_app alarm_warnings)
target_compile_definitions(alarm_clock PRIVATE ALARM_CLOCK_VERSION="${PROJECT_VERSION}")

add_custom_command(TARGET alarm_clock POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/assets/sounds $<TARGET_FILE_DIR:alarm_clock>/sounds)

if(ALARM_CLANG_TIDY)
  find_program(CLANG_TIDY_EXE NAMES clang-tidy REQUIRED)
  set_target_properties(alarm_core alarm_platform alarm_app alarm_clock PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
endif()

# ---- Install / package:  cmake --build build && cpack --config build/CPackConfig.cmake ----
install(TARGETS alarm_clock RUNTIME DESTINATION .)
install(DIRECTORY assets/sounds DESTINATION .)
install(FILES README.md LICENSE DESTINATION .)

set(CPACK_GENERATOR ZIP)
set(CPACK_PACKAGE_NAME alarm_clock)
set(CPACK_PACKAGE_VENDOR "Mukesh Kumar")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Reliable terminal alarm clock")
if(NOT ALARM_PACKAGE_SUFFIX)
  set(ALARM_PACKAGE_SUFFIX "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
endif()
set(CPACK_PACKAGE_FILE_NAME "alarm_clock-${PROJECT_VERSION}-${ALARM_PACKAGE_SUFFIX}")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
include(CPack)

if(ALARM_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
