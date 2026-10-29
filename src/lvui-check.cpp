#include <LVUI/Language.h>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
std::size_t count(const lvui::View& view) {
    std::size_t result = 1;
    for (const auto& child : view.children) result += count(child);
    return result;
}
}
int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--keywords") {
        for (const auto& word : lvui::keywords()) std::cout << word.spelling << '\n';
        return 0;
    }
    if (argc != 2) { std::cerr << "Usage: lvui-check <file.lvui> | --keywords\n"; return 2; }
    std::ifstream input(std::filesystem::path(argv[1]), std::ios::binary);
    if (!input) { std::cerr << "Unable to open the LVUI source\n"; return 2; }
    std::string source(1024 * 1024 + 1, '\0');
    input.read(source.data(), static_cast<std::streamsize>(source.size()));
    source.resize(static_cast<std::size_t>(input.gcount()));
    if (input.bad()) { std::cerr << "Unable to read the LVUI source\n"; return 2; }
    const auto first=source.find_first_not_of(" \t\r\n");
    auto result = first!=source.npos&&source[first]=='<'?lvui::parse_native_markup(source):lvui::parse_view(source);
    if (!result) {
        const auto& error = result.error();
        std::cerr << argv[1] << ':' << error.line << ':' << error.column << ": " << error.message << '\n';
        return 1;
    }
    std::cout << "LVUI component syntax valid: " << count(result->root()) << " Views; root "
              << (result->root().condition ? "if" : result->root().repetition ? "for" : result->root().component ? result->root().component->spelling : result->root().custom_component) << '\n';
    return 0;
}
