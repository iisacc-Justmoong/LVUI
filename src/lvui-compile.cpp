#include <LVUI/Compiler.h>
#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    lvui::NativeCompileOptions options;
    std::filesystem::path input_file;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if ((arg == "--name" || arg == "--include" || arg=="--framework") && i + 1 < argc) {
            if (arg == "--name") options.name = argv[++i]; else if(arg=="--framework") options.framework=argv[++i]; else options.includes.emplace_back(argv[++i]);
        } else if (!arg.starts_with('-') && input_file.empty()) input_file = argv[i];
        else { std::cerr << "Usage: lvui-compile [--name Name] [--include relative/header.h] [--framework iiGUI|LVRS] file.lvui\n"; return 2; }
    }
    if (input_file.empty()) { std::cerr << "An LVUI source file is required\n"; return 2; }
    std::ifstream input(input_file, std::ios::binary);
    if (!input) { std::cerr << "Cannot open LVUI source: " << input_file.string() << '\n'; return 2; }
    std::string source(1024 * 1024 + 1, '\0');
    input.read(source.data(), static_cast<std::streamsize>(source.size()));
    source.resize(static_cast<std::size_t>(input.gcount()));
    if (input.bad()) { std::cerr << "Cannot read LVUI source\n"; return 2; }
    auto result = lvui::compile_native(source, options);
    if (!result) {
        const auto& e = result.error();
        std::cerr << input_file.string() << ':' << e.line << ':' << e.column << ": " << e.message << '\n'; return 1;
    }
    // A build tool emits to stdout. It creates no application/user files.
    std::cout << *result;
    if (!std::cout) { std::cerr << "Cannot write generated C++\n"; return 2; }
}
