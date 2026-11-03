cmake_minimum_required (VERSION 3.22)

project(lldb-tui
  LANGUAGES CXX
  VERSION 1.0.0
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(cpptui
  GIT_REPOSITORY https://github.com/jonoton/cpp-tui.git
  # Pin the dependency so builds use a known version of the API.
  GIT_TAG        3690d52bdba0075b9dba6703891f7298a2a2de4b
  GIT_PROGRESS   TRUE
)
set(CPPTUI_BUILD_EXAMPLES OFF CACHE BOOL "Build cpp-tui examples")
FetchContent_MakeAvailable(cpptui)

add_executable(lldb-tui src/main.cpp)
target_include_directories(lldb-tui PRIVATE src)

target_link_libraries(lldb-tui PRIVATE cpptui::cpptui)
