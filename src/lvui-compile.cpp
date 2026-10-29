#include <LVUI/Compiler.h>
#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    lvui::NativeCompileOptions options;
    std::filesystem::path input_file;
    std::filesystem::path style_file;
    bool emit_styles = false;
    std::string style_namespace = "lvui_style";
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--emit-styles") {
            emit_styles = true;
            continue;
        }
        if ((arg == "--name" || arg == "--include" || arg == "--framework" || arg == "--style-mode" ||
             arg == "--style-file" || arg == "--namespace") &&
            i + 1 < argc) {
            if (arg == "--style-file")
                style_file = argv[++i];
            else if (arg == "--namespace")
                style_namespace = argv[++i];
            else if (arg == "--name")
                options.name = argv[++i];
            else if (arg == "--framework")
                options.framework = argv[++i];
            else if (arg == "--style-mode") {
                const std::string choice = argv[++i];
                const auto equal = choice.find('=');
                if (equal == 0 || equal == choice.npos || equal + 1 == choice.size() ||
                    !options.style_tokens.modes.emplace(choice.substr(0, equal), choice.substr(equal + 1))
                         .second) {
                    std::cerr << "--style-mode requires one unique Collection=Mode\n";
                    return 2;
                }
            } else
                options.includes.emplace_back(argv[++i]);
        } else if (!arg.starts_with('-') && input_file.empty())
            input_file = argv[i];
        else {
            std::cerr << "Usage: lvui-compile [--name Name] [--include relative/header.h] [--framework "
                         "iiGUI] [--style-mode Collection=Mode] [--style-file Theme.lvui] file.lvui\n"
                         "       lvui-compile --emit-styles [--namespace App.Style] Theme.lvui\n";
            return 2;
        }
    }
    if (input_file.empty()) { std::cerr << "An LVUI source file is required\n"; return 2; }
    std::ifstream input(input_file, std::ios::binary);
    if (!input) { std::cerr << "Cannot open LVUI source: " << input_file.string() << '\n'; return 2; }
    std::string source(1024 * 1024 + 1, '\0');
    input.read(source.data(), static_cast<std::streamsize>(source.size()));
    source.resize(static_cast<std::size_t>(input.gcount()));
    if (input.bad()) { std::cerr << "Cannot read LVUI source\n"; return 2; }
    if (!style_file.empty()) {
        std::ifstream styles(style_file, std::ios::binary);
        if (!styles) {
            std::cerr << "Cannot open style profile\n";
            return 2;
        }
        std::string profile(1024 * 1024 + 1, '\0');
        styles.read(profile.data(), profile.size());
        profile.resize(styles.gcount());
        if (styles.bad()) {
            std::cerr << "Cannot read style profile\n";
            return 2;
        }
        auto parsed = lvui::compile_style_profile(profile, options.style_tokens);
        if (!parsed) {
            const auto &e = parsed.error();
            std::cerr << style_file.string() << ':' << e.line << ':' << e.column << ": " << e.message << '\n';
            return 1;
        }
        options.style_tokens = std::move(*parsed);
    }
    lvui::NativeCompileResult result;
    if (emit_styles) {
        auto profile = lvui::compile_style_profile(source, options.style_tokens);
        if (!profile) {
            const auto &e = profile.error();
            std::cerr << input_file.string() << ':' << e.line << ':' << e.column << ": " << e.message << '\n';
            return 1;
        }
        result = lvui::compile_style_header(*profile, style_namespace);
    } else
        result = lvui::compile_native(source, options);
    if (!result) {
        const auto& e = result.error();
        std::cerr << input_file.string() << ':' << e.line << ':' << e.column << ": " << e.message << '\n'; return 1;
    }
    // A build tool emits to stdout. It creates no application/user files.
    std::cout << *result;
    if (!std::cout) { std::cerr << "Cannot write generated C++\n"; return 2; }
}
