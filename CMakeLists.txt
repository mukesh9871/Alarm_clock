cmake_minimum_required(VERSION 3.20)
project(AlarmClock LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT WIN32)
  message(FATAL_ERROR "The current alarm.cpp uses Win32 APIs (windows.h, winmm) and only builds on Windows.")
endif()

add_executable(alarm_clock alarm.cpp)
target_link_libraries(alarm_clock PRIVATE winmm)

find_package(Threads REQUIRED)
target_link_libraries(alarm_clock PRIVATE Threads::Threads)

# The program loads music1.wav from the working directory; copy it next to the binary.
add_custom_command(TARGET alarm_clock POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_SOURCE_DIR}/music1.wav $<TARGET_FILE_DIR:alarm_clock>/music1.wav)
