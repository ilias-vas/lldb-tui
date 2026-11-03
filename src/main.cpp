#include "cpptui.hpp"

#include <array>
#include <memory>
#include <string>

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

std::shared_ptr<Border> source_pane() {
    // Demo contents only; these will eventually come from the selected LLDB frame.
    constexpr std::array lines{
            "#include <iostream>",
            "",
            "int main() {",
            "    const int count = 5;",
            "    int total = 0;",
            "    for (int i = 0; i < count; ++i) {",
            "        total += i;",
            "    }",
            "    std::cout << total << '\\n';",
            "    return 0;",
            "}",
    };
    constexpr int stopped_line = 7;
    const auto&   theme = Theme::current();
    auto          source = std::make_shared<ScrollableVertical>();
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const int  number = static_cast<int>(i) + 1;
        const bool current = number == stopped_line;
        const bool breakpoint = current || number == 9;
        StyledText text;
        text.colored(current ? ">" : " ", theme.success)
                .colored(breakpoint ? "B " : "  ", theme.error)
                .colored((number < 10 ? " " : "") + std::to_string(number) + " | ", theme.secondary)
                .colored(lines[i], current ? theme.success : theme.foreground);
        auto row = label(text);
        if (current) row->bg_color = theme.panel_bg;
        source->add(row);
    }
    return pane("Source / demo.cpp", source);
}

std::shared_ptr<Border> console_pane() {
    auto        console = std::make_shared<Vertical>();
    const auto& theme = Theme::current();
    console->add(
            label(StyledText().colored("Demo session - LLDB is not connected.", theme.warning)));
    console->add(label("(lldb) breakpoint set --file demo.cpp --line 7"));
    console->add(label("Breakpoint 1: demo`main + 32 at demo.cpp:7"));
    console->add(label("(lldb) run"));
    console->add(label("Process 1234 stopped: thread #1, breakpoint 1.1"));
    console->add(std::make_shared<VerticalSpacer>());
    console->add(label(StyledText().colored_bold("(lldb) ", theme.primary).add("_")));
    return pane("Command console / preview", console);
}
}  // namespace

int main() {
    using namespace cpptui;
    Theme::set_theme(Theme::Nord());
    const auto& theme = Theme::current();

    App  app;
    auto root = std::make_shared<Vertical>();
    root->add(label(StyledText()
                            .colored_bold(" lldb-tui", theme.primary)
                            .add("  |  debugger layout preview")));
    root->add(source_pane());

    auto status = label(StyledText()
                                .colored_bold(" STOPPED ", theme.warning)
                                .add(" demo | thread #1 | main() | demo.cpp:7 | breakpoint 1.1"));
    status->bg_color = theme.panel_bg;
    root->add(status);

    auto console = console_pane();
    console->fixed_height = 10;
    root->add(console);
    root->add(label(StyledText()
                            .colored(" q / Ctrl+C", theme.primary)
                            .add(" quit | Mouse wheel scrolls source | > current | B breakpoint")));

    app.register_exit_key('q');
    app.run(root);
    return 0;
}
