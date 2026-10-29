#include <LVUI/Compiler.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
void check(bool value, std::string_view message) {
    if (!value) throw std::runtime_error(std::string(message));
}

void independent_components() {
    for (std::string_view source : {
             "Window { Label(text: \"Hello\") }",
             "Window { VStack { PushButton(text: \"Open\")\nToggleSwitch()\nInputField()\nProgressBar() } }",
             "Window { HierarchyList() }",
             "Window { HierarchyItem(count: 2, countView: Label(text: \"Two\")) }",
             "Window { Table() }",
             "Window { Card(title: \"Project\") }",
         }) {
        const auto result = lvui::compile_native(source);
        if (!result) throw std::runtime_error(result.error().message);
        for (std::string_view forbidden : {"lvrs::", "<LVRS/", "<Qt", "QQml", "QQuick"})
            check(result->find(forbidden) == result->npos, "Generated code references another UI framework");
    }
}

void unsupported_frameworks() {
    for (std::string_view framework : {"LVRS", "unknown", ""}) {
        lvui::NativeCompileOptions options;
        options.framework = framework;
        const auto result = lvui::compile_native("Window { Label() }", options);
        check(!result, "Unsupported framework produced native code");
        check(result.error().message.find("iiGUI") != std::string::npos,
              "Unsupported framework lacks an actionable diagnostic");
    }
}
}

int main() {
    try {
        independent_components();
        unsupported_frameworks();
        std::cout << "Independent native backend passed\n";
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
