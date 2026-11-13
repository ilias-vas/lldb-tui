#include "debugger.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>

namespace {
std::string path(lldb::SBFileSpec file) {
    const auto* name = file.GetFilename();
    const auto* directory = file.GetDirectory();
    if (!name) return {};
    return (std::filesystem::path(directory ? directory : "") / name).string();
}
}  // namespace

Debugger::Debugger() {
#ifdef LLDB_TUI_DEBUGSERVER
    if (!std::getenv("LLDB_DEBUGSERVER_PATH"))
        setenv("LLDB_DEBUGSERVER_PATH", LLDB_TUI_DEBUGSERVER, 0);
#endif
    lldb::SBDebugger::Initialize();
    debugger_ = lldb::SBDebugger::Create(false);
    if (!debugger_.IsValid()) throw std::runtime_error("Could not initialize LLDB");
    debugger_.SetAsync(true);
    if (auto* file = std::tmpfile()) debugger_.SetOutputFileHandle(file, true);
    if (auto* file = std::tmpfile()) debugger_.SetErrorFileHandle(file, true);
    command("settings set auto-confirm true");
    command("settings set use-color false");
}

Debugger::~Debugger() {
    auto process = debugger_.GetSelectedTarget().GetProcess();
    if (process.IsValid() && process.GetState() != lldb::eStateExited &&
        process.GetState() != lldb::eStateDetached) {
        process.Kill();
    }
    lldb::SBDebugger::Destroy(debugger_);
    lldb::SBDebugger::Terminate();
}

std::string Debugger::load(const std::string& executable) {
    lldb::SBError error;
    auto target = debugger_.CreateTarget(executable.c_str(), nullptr, nullptr, true, error);
    if (!target.IsValid() || error.Fail())
        throw std::runtime_error(error.GetCString() ? error.GetCString() : "Cannot load target");
    return "Loaded " + executable + "\nSet a breakpoint, then type run.\n";
}

std::string Debugger::command(const std::string& command) {
    lldb::SBCommandReturnObject result;
    debugger_.GetCommandInterpreter().HandleCommand(command.c_str(), result);
    return std::string(result.GetOutput() ? result.GetOutput() : "") +
           (result.GetError() ? result.GetError() : "");
}

std::string Debugger::output() {
    auto        process = debugger_.GetSelectedTarget().GetProcess();
    std::string result;
    char        buffer[4096];
    for (int stream = 0; stream < 2; ++stream) {
        for (int i = 0; i < 16; ++i) {
            auto count = stream == 0 ? process.GetSTDOUT(buffer, sizeof(buffer))
                                     : process.GetSTDERR(buffer, sizeof(buffer));
            if (!count) break;
            result.append(buffer, count);
        }
    }
    return result;
}

DebuggerSnapshot Debugger::snapshot() {
    // Removing process events from the listener advances LLDB's public state.
    lldb::SBEvent event;
    auto          listener = debugger_.GetListener();
    while (listener.GetNextEvent(event)) {}
    DebuggerSnapshot result;
    auto             target = debugger_.GetSelectedTarget();
    auto             process = target.GetProcess();
    auto             state = process.GetState();
    const char*      name = lldb::SBDebugger::StateAsCString(state);
    result.status = name ? name : "no process";
    if (state == lldb::eStateExited) {
        result.status += " | exit code " + std::to_string(process.GetExitStatus());
    }
    if (state != lldb::eStateStopped && state != lldb::eStateCrashed &&
        state != lldb::eStateSuspended)
        return result;
    auto thread = process.GetSelectedThread();
    if (!thread.IsValid()) thread = process.GetThreadAtIndex(0);
    auto frame = thread.GetSelectedFrame();
    if (!frame.IsValid()) frame = thread.GetFrameAtIndex(0);
    if (!frame.IsValid()) {
        result.status = "stopping";
        return result;
    }
    auto entry = frame.GetLineEntry();
    result.file = path(entry.GetFileSpec());
    result.line = entry.GetLine();
    if (result.line == LLDB_INVALID_LINE_NUMBER) result.line = 0;
    result.status += " | thread #" + std::to_string(thread.GetIndexID());
    result.status += " | " + std::string(frame.GetFunctionName() ? frame.GetFunctionName() : "??");
    char reason[512] = {};
    thread.GetStopDescription(reason, sizeof(reason));
    result.status += " | " + std::string(reason);
    for (unsigned i = 0; i < target.GetNumBreakpoints(); ++i) {
        auto breakpoint = target.GetBreakpointAtIndex(i);
        if (!breakpoint.IsEnabled()) continue;
        for (unsigned j = 0; j < breakpoint.GetNumLocations(); ++j) {
            auto location = breakpoint.GetLocationAtIndex(j);
            auto line = location.GetAddress().GetLineEntry();
            if (location.IsEnabled() && path(line.GetFileSpec()) == result.file)
                result.breakpoints.insert(line.GetLine());
        }
    }
    return result;
}
