#include "debugger.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

void require(bool value, const std::string &message) {
  if (!value)
    throw std::runtime_error(message);
}
DebuggerSnapshot wait_for(Debugger &debugger, const std::string &state) {
  for (int i = 0; i < 150; ++i) {
    auto snapshot = debugger.snapshot();
    if (snapshot.status.starts_with(state))
      return snapshot;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  throw std::runtime_error("Timed out waiting for " + state + ": " +
                           debugger.snapshot().status);
}
int main(int argc, char **argv) {
  try {
    require(argc == 2, "Expected debug-target path");
    Debugger debugger;
    debugger.load(argv[1]);
    auto result = debugger.command("breakpoint set -n add_score");
    require(result.find("Breakpoint 1") != std::string::npos, result);
    result = debugger.command("run");
    require(result.find("error:") == std::string::npos, result);
    auto stopped = wait_for(debugger, "stopped");
    require(stopped.file.ends_with("debug_target.cpp") && stopped.line > 0,
            "No source location: " + stopped.status + " file=" + stopped.file +
                " line=" + std::to_string(stopped.line) + "\n" +
                debugger.command("bt"));
    require(stopped.breakpoints.count(stopped.line),
            "Missing breakpoint marker");
    result = debugger.command("frame variable total points");
    require(result.find("total = 0") != std::string::npos &&
                result.find("points = 10") != std::string::npos,
            result);
    debugger.command("next");
    auto stepped = wait_for(debugger, "stopped");
    require(stepped.line != stopped.line, "next did not advance source line");
    result = debugger.command("frame variable updated");
    require(result.find("updated = 10") != std::string::npos, result);
    require(debugger.command("bt").find("main") != std::string::npos,
            "Missing main stack frame");
    debugger.command("breakpoint disable 1");
    require(debugger.snapshot().breakpoints.empty(),
            "Disabled breakpoint still marked");
    debugger.command("continue");
    wait_for(debugger, "exited");
    auto output = debugger.output();
    require(output.find("Final score: 150") != std::string::npos, output);
    require(debugger.snapshot().status.find("exit code 0") != std::string::npos,
            "Unexpected exit status");
    require(debugger.command("not-a-command").find("error:") !=
                std::string::npos,
            "Missing command error");
    debugger.command("run -- --wait");
    wait_for(debugger, "running");
    debugger.command("process interrupt");
    wait_for(debugger, "stopped");
    debugger.command("continue");
    wait_for(debugger, "running");
    debugger.command("process kill");
    wait_for(debugger, "exited");
    std::cout << "Breakpoint, source, variables, next, stack, output, exit, "
                 "interrupt and errors passed\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
