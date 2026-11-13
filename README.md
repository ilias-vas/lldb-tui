# lldb-tui

A terminal frontend for LLDB, inspired by GDB's TUI and built with
[cpp-tui](https://github.com/jonoton/cpp-tui). Uses LLDB's C++ API for real
breakpoints, stepping, variable inspection, stack navigation, and process control.

## Build

Requires CMake 3.22+, a C++20 compiler, and LLDB headers and library. On macOS,
install Apple's Command Line Tools (`xcode-select --install`) and LLVM
(`brew install llvm`). On Ubuntu, install `lldb liblldb-dev cmake g++`.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j 4
./build/lldb-tui ./build/debug-target
```

## Try the debugger

Pass your own executable as the first argument, or start without arguments and
use `target create /path/to/program` in the tui console. Compile your program with `-g -O0` for
useful source and variable information. Target arguments go in `run -- ...`.
Commands use the [LLDB command interpreter](https://lldb.llvm.org/cpp_reference/classlldb_1_1SBCommandInterpreter.html).

This first functional version captures target stdout/stderr but does not forward
interactive stdin to the target. It has no command completion or disassembly
pane yet. Frames without accessible debug source show an explanatory message.
Long synchronous commands (for example, an expression that never returns) can
block command entry. Debugging also requires your OS to permit debugger process
control; run locally outside a restrictive sandbox.
