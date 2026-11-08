include(FetchContent)

# Every dependency is pinned to a release tarball and verified by SHA-256.

# miniaudio: single-header, cross-platform audio playback (replaces Windows-only winmm).
FetchContent_Declare(miniaudio
  URL      https://github.com/mackron/miniaudio/archive/refs/tags/0.11.21.tar.gz
  URL_HASH SHA256=6afb5c231613d2fab4f1c668b7243ff9a7d6d78a7f5a2692c133f026fe508506
  SOURCE_SUBDIR do-not-add-subdirectory  # we only want the header
  DOWNLOAD_EXTRACT_TIMESTAMP ON
)
FetchContent_MakeAvailable(miniaudio)

add_library(miniaudio STATIC ${CMAKE_CURRENT_LIST_DIR}/miniaudio_impl.c)
target_include_directories(miniaudio SYSTEM PUBLIC ${miniaudio_SOURCE_DIR})
target_compile_definitions(miniaudio PUBLIC MA_NO_ENCODING MA_NO_GENERATION)
find_package(Threads REQUIRED)
target_link_libraries(miniaudio PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
if(UNIX AND NOT APPLE)
  target_link_libraries(miniaudio PUBLIC m)
endif()

# nlohmann/json: alarm file format.
FetchContent_Declare(nlohmann_json
  URL      https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
  DOWNLOAD_EXTRACT_TIMESTAMP ON
)
FetchContent_MakeAvailable(nlohmann_json)

# FTXUI: full-screen terminal UI.
set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FTXUI_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(FTXUI_QUIET ON CACHE BOOL "" FORCE)
FetchContent_Declare(ftxui
  URL      https://github.com/ArthurSonzogni/FTXUI/archive/refs/tags/v5.0.0.tar.gz
  URL_HASH SHA256=a2991cb222c944aee14397965d9f6b050245da849d8c5da7c72d112de2786b5b
  DOWNLOAD_EXTRACT_TIMESTAMP ON
)
FetchContent_MakeAvailable(ftxui)

if(ALARM_BUILD_TESTS)
  FetchContent_Declare(Catch2
    URL      https://github.com/catchorg/Catch2/archive/refs/tags/v3.7.1.tar.gz
    URL_HASH SHA256=c991b247a1a0d7bb9c39aa35faf0fe9e19764213f28ffba3109388e62ee0269c
    DOWNLOAD_EXTRACT_TIMESTAMP ON
  )
  FetchContent_MakeAvailable(Catch2)
endif()
