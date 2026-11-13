#include "cpptui.hpp"
#include "debugger.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {
using namespace cpptui;

std::shared_ptr<Paragraph> label(const StyledText& text) {
    auto row = std::make_shared<Paragraph>(text);
    row->fixed_height = 1;
    row->word_wrap = false;
    row->focusable = false;
    row->selectable = false;
    return row;
}

std::shared_ptr<Border> pane(const std::string& title, std::shared_ptr<Widget> content) {
    auto border = std::make_shared<Border>(BorderStyle::Single);
    border->set_title(" " + title + " ", Alignment::Left);
    border->add(content);
    return border;
}

class CommandInput : public Input {
 public:
    std::function<void(std::string)> submit;
    bool                             on_event(const Event& event) override {
        if (has_focus() && event.is_key_event()) {
            if (event.is_enter()) {
                auto command = get_value();
                if (!command.empty()) {
                    history.push_back(command);
                    index = history.size();
                    set_value("");
                    submit(command);
                }
                return true;
            }
            if (event.is_nav_up() || event.is_nav_down()) {
                if (event.is_nav_up() && index > 0) --index;
                if (event.is_nav_down() && index < history.size()) ++index;
                set_value(index < history.size() ? history[index] : "");
                return true;
            }
        }
        return Input::on_event(event);
    }

 private:
    std::vector<std::string> history;
    std::size_t              index = 0;
};
}  // namespace

int main(int argc, char** argv) {
    if (argc > 2 || (argc == 2 && std::string(argv[1]) == "--help")) {
        std::cout << "Usage: lldb-tui [executable]\n"
                     "Type LLDB commands; quit exits. Use run -- arguments for target arguments.\n";
        return argc > 2 ? 1 : 0;
    }
    try {
        Debugger    debugger;
        std::string welcome = argc == 2
                                      ? debugger.load(argv[1])
                                      : "Load a program with: target create /path/to/executable\n";
        Theme::set_theme(Theme::Nord());
        App  app;
        auto root = std::make_shared<Vertical>();
        root->add(label(StyledText()
                                .colored_bold(" lldb-tui", Theme::current().primary)
                                .add(" | LLDB debugger")));
        auto source = std::make_shared<ScrollableVertical>();
        auto source_border = pane("Source", source);
        root->add(source_border);
        auto status = label("Ready");
        status->bg_color = Theme::current().panel_bg;
        root->add(status);
        auto console = std::make_shared<ScrollableVertical>();
        auto console_border = pane("LLDB console", console);
        console_border->fixed_height = 9;
        root->add(console_border);
        auto prompt = std::make_shared<Horizontal>();
        prompt->fixed_height = 1;
        auto prefix = label("(lldb) ");
        prefix->fixed_width = 7;
        prompt->add(prefix);
        auto input = std::make_shared<CommandInput>();
        input->placeholder = "breakpoint set -n main, then run";
        prompt->add(input);
        root->add(prompt);
        root->add(label(" Enter command | Up/Down history | wheel scroll | quit / Ctrl+C exit"));

        std::string transcript;
        auto        append = [&](const std::string& text) {
            if (text.empty()) return;
            // Strip terminal control characters from inferior/debugger output.
            for (unsigned char ch : text) {
                if (ch == '\n' || ch == '\t' || ch >= 32) {
                    if (ch != 127 && ch != 27) transcript += static_cast<char>(ch);
                }
            }
            // Limit scrollback memory and rendering work.
            if (transcript.size() > 24000) {
                auto start = transcript.find('\n', transcript.size() - 20000);
                transcript.erase(0, start == std::string::npos ? transcript.size() - 20000
                                                               : start + 1);
            }
            console->clear_children();
            std::istringstream stream(transcript);
            std::string        line;
            while (std::getline(stream, line))
                console->add(label(line));
            console->scroll_offset =
                    std::max(0, static_cast<int>(console->get_children().size()) - console->height);
        };
        DebuggerSnapshot previous;
        bool             first = true;
        auto             refresh = [&] {
            append(debugger.output());
            auto state = debugger.snapshot();
            if (!first && state.status == previous.status && state.file == previous.file &&
                state.line == previous.line && state.breakpoints == previous.breakpoints)
                return;
            if (!first && state.status != previous.status) append("[" + state.status + "]\n");
            first = false;
            status->set_text(state.status);
            source_border->set_title(" " + (state.file.empty() ? "Source" : state.file) + " ",
                                     Alignment::Left);
            source->clear_children();
            std::ifstream file(state.file);
            std::string   line;
            unsigned      number = 0;
            if (!file) {
                source->add(
                        label(state.file.empty()
                                      ? "Source appears when the process stops in a source frame."
                                      : "Cannot read source file: " + state.file));
            } else {
                while (std::getline(file, line)) {
                    ++number;
                    bool       current = number == state.line;
                    StyledText text;
                    text.colored(current ? ">" : " ", Theme::current().success)
                            .colored(state.breakpoints.count(number) ? "B " : "  ",
                                     Theme::current().error)
                            .colored(std::to_string(number) + " | ", Theme::current().secondary)
                            .add(line);
                    auto row = label(text);
                    if (current) row->bg_color = Theme::current().panel_bg;
                    source->add(row);
                }
            }
            source->scroll_offset =
                    std::max(0, static_cast<int>(state.line) - std::max(1, source->height / 2));
            previous = state;
        };
        input->submit = [&](const std::string& command) {
            if (command == "quit" || command == "q" || command == "exit") {
                App::quit();
                return;
            }
            append("(lldb) " + command + "\n");
            auto result = debugger.command(command);
            append(result);
            if (!result.empty() && result.back() != '\n') append("\n");
            refresh();
        };
        append(welcome);
        refresh();
        // Input starts focused even before the first layout pass assigns dimensions.
        input->set_focus(true);
        app.add_timer(100, refresh);
        app.run(root);
    } catch (const std::exception& error) {
        std::cerr << "lldb-tui: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
