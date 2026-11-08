add_executable(alarm_tests
  test_alarm_service.cpp
  test_schedule.cpp
  test_time_format.cpp
  test_time_zone.cpp
)
target_include_directories(alarm_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(alarm_tests PRIVATE alarm_core alarm_warnings Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(alarm_tests)
