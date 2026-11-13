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

set(LLDB_ROOT "" CACHE PATH "LLDB/LLVM installation prefix")
find_path(LLDB_INCLUDE_DIR lldb/API/LLDB.h
  HINTS "${LLDB_ROOT}/include" /opt/homebrew/opt/lldb/include /usr/local/opt/lldb/include
        /opt/homebrew/opt/llvm/include /usr/local/opt/llvm/include
  PATHS /usr/lib/llvm-20/include /usr/lib/llvm-19/include /usr/lib/llvm-18/include /usr/lib/llvm-17/include /usr/lib/llvm-16/include /usr/lib/llvm-14/include)
find_library(LLDB_LIBRARY NAMES lldb liblldb
  HINTS "${LLDB_ROOT}/lib" /opt/homebrew/opt/lldb/lib /usr/local/opt/lldb/lib
        /opt/homebrew/opt/llvm/lib /usr/local/opt/llvm/lib
  PATHS /usr/lib/llvm-20/lib /usr/lib/llvm-19/lib /usr/lib/llvm-18/lib /usr/lib/llvm-17/lib /usr/lib/llvm-16/lib /usr/lib/llvm-14/lib)
if(NOT LLDB_INCLUDE_DIR OR NOT LLDB_LIBRARY)
  message(FATAL_ERROR "LLDB development files required. Install LLDB (brew install lldb) or liblldb-dev; set -DLLDB_ROOT=/path/to/lldb if needed.")
endif()

add_library(debugger src/debugger.cpp)
target_include_directories(debugger PUBLIC src "${LLDB_INCLUDE_DIR}")
target_link_libraries(debugger PUBLIC "${LLDB_LIBRARY}")
if(APPLE)
  # Standalone liblldb does not always discover Apple's signed debugserver.
  execute_process(COMMAND xcode-select -p OUTPUT_VARIABLE APPLE_DEVELOPER_DIR
                  OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  find_program(LLDB_DEBUGSERVER NAMES debugserver
    PATHS "${APPLE_DEVELOPER_DIR}/Library/PrivateFrameworks/LLDB.framework/Resources"
          "${APPLE_DEVELOPER_DIR}/../SharedFrameworks/LLDB.framework/Resources")
elseif(UNIX)
  # Embedded liblldb cannot reliably locate lldb-server relative to our executable.
  # Prefer the server beside the selected headers to keep LLDB versions matched.
  get_filename_component(LLDB_PREFIX "${LLDB_INCLUDE_DIR}" DIRECTORY)
  find_program(LLDB_DEBUGSERVER NAMES lldb-server
    HINTS "${LLDB_ROOT}/bin" "${LLDB_PREFIX}/bin" NO_DEFAULT_PATH)
  if(NOT LLDB_DEBUGSERVER)
    find_program(LLDB_DEBUGSERVER NAMES lldb-server)
  endif()
endif()
if(LLDB_DEBUGSERVER)
  target_compile_definitions(debugger PRIVATE LLDB_TUI_DEBUGSERVER="${LLDB_DEBUGSERVER}")
  message(STATUS "LLDB debug server: ${LLDB_DEBUGSERVER}")
else()
  message(WARNING "LLDB debug server not found; set LLDB_DEBUGSERVER or the runtime LLDB_DEBUGSERVER_PATH to enable process launch.")
endif()

add_executable(lldb-tui src/main.cpp)
target_include_directories(lldb-tui PRIVATE src)

target_link_libraries(lldb-tui PRIVATE cpptui::cpptui debugger)

find_package(Threads REQUIRED)
add_executable(debug-target examples/debug_target.cpp)
target_link_libraries(debug-target PRIVATE Threads::Threads)
if(MSVC)
  target_compile_options(debug-target PRIVATE /Od /Zi)
  target_link_options(debug-target PRIVATE /DEBUG)
else()
  target_compile_options(debug-target PRIVATE -O0 -g)
endif()

include(CTest)
if(BUILD_TESTING)
  add_executable(debugger-test tests/debugger_test.cpp)
  target_link_libraries(debugger-test PRIVATE debugger Threads::Threads)
  add_test(NAME debugger-integration COMMAND debugger-test $<TARGET_FILE:debug-target>)
  set_tests_properties(debugger-integration PROPERTIES TIMEOUT 30)
endif()
