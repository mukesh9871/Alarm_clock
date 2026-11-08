add_executable(alarm_tests
  test_alarm_service.cpp
  test_big_digits.cpp
  test_cli.cpp
  test_days.cpp
  test_saved_state.cpp
  test_schedule.cpp
  test_session.cpp
  test_time_format.cpp
  test_time_zone.cpp
  test_tui.cpp
)
target_include_directories(alarm_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(alarm_tests PRIVATE alarm_app alarm_warnings Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(alarm_tests)
