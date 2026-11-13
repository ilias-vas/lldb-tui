#pragma once
#include <lldb/API/LLDB.h>
#include <set>
#include <string>

struct DebuggerSnapshot {
    std::string        status;
    std::string        file;
    unsigned           line = 0;
    std::set<unsigned> breakpoints;
};

class Debugger {
 public:
    Debugger();
    ~Debugger();
    Debugger(const Debugger&) = delete;
    Debugger&        operator=(const Debugger&) = delete;
    std::string      load(const std::string& executable);
    std::string      command(const std::string& command);
    std::string      output();
    DebuggerSnapshot snapshot();

 private:
    lldb::SBDebugger debugger_;
};
