cmake_minimum_required(VERSION 3.24)
project(AlarmClock VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ALARM_BUILD_TESTS "Build unit tests" ON)

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
  src/core/schedule.cpp
  src/core/time_format.cpp
  src/core/time_zone.cpp
)
target_include_directories(alarm_core PUBLIC src)
target_link_libraries(alarm_core PRIVATE alarm_warnings)

# ---- Platform layer: audio, paths ----
add_library(alarm_platform STATIC
  src/platform/audio_player.cpp
  src/platform/paths.cpp
)
target_include_directories(alarm_platform PUBLIC src)
target_link_libraries(alarm_platform PUBLIC alarm_core PRIVATE alarm_warnings miniaudio)

# ---- Application ----
add_executable(alarm_clock src/app/main.cpp)
target_link_libraries(alarm_clock PRIVATE alarm_core alarm_platform alarm_warnings)

add_custom_command(TARGET alarm_clock POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/assets/sounds $<TARGET_FILE_DIR:alarm_clock>/sounds)

install(TARGETS alarm_clock RUNTIME DESTINATION .)
install(DIRECTORY assets/sounds DESTINATION .)

if(ALARM_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
