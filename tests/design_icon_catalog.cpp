#include <LVUI/AuthoredIcons.h>

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}

int main() try {
    const auto source = lvui::design_icon_catalog();
    const auto original = lvui::authored_icon_catalog();
    check(bool(source) && source->revision() == 1, "Design icon source is unavailable");
    const auto names = lvui::design_icon_names();
    check(names.size() == 101 && std::ranges::is_sorted(names), "Design icon inventory changed");
    for (const auto name : names) {
        const auto path = std::filesystem::path(LVUI_DESIGN_ICON_DIR) / (std::string(name) + ".svg");
        std::ifstream in(path, std::ios::binary);
        check(bool(in), "Design icon resource is unavailable");
        const std::string expected((std::istreambuf_iterator<char>(in)), {});
        const auto exact = lvui::design_icon_svg(name);
        const auto resolved = source->resolve(name);
        if (!exact || *exact != expected || !resolved || !*resolved || (*resolved)->svg != expected)
            throw std::runtime_error("Design icon differs from its resource: " + std::string(name));
        if (exact->find("viewBox=\"0 0 18 18\"") == std::string_view::npos)
            throw std::runtime_error("Design icon viewport changed: " + std::string(name));
    }
    const auto previous = lvui::design_icon_svg("previous-frame");
    check(previous && previous->find("M12.7748 3.91014L5.98306 8.43796") != std::string_view::npos,
          "The Figma previous-frame geometry changed");
    check(bool(lvui::design_icon_svg("avatar-placeholder")), "The LVUI avatar placeholder is unavailable");
    const auto direct = source->resolve("brush");
    const auto alias = source->resolve(" /brush.SVG ");
    const auto old_path = source->resolve("qrc:/qt/qml/LVRS/resources/iconset/brush.svg");
    const auto legacy = original->resolve("brush");
    check(direct && alias && old_path && legacy && *direct == *alias && *direct == *old_path,
          "Design icon aliases do not share the same asset");
    check((*direct)->svg != (*legacy)->svg, "The original icon snapshot was unexpectedly replaced");
    const auto fallback = source->resolve("checkboxCheckedDisabled");
    const auto legacy_fallback = original->resolve("checkboxCheckedDisabled");
    check(fallback && legacy_fallback && *fallback == *legacy_fallback,
          "A non-design icon did not use the original catalog");
    const auto missing = source->resolve("missing-icon-for-test");
    check(!missing && missing.error().code == lvui::IconErrorCode::not_found,
          "Unknown design icon was accepted");
    check(!source->resolve("") && !source->resolve(std::string("a\0b", 3)),
          "Invalid design icon name was accepted");
    std::atomic<bool> concurrent_ok{true};
    std::vector<std::thread> threads;
    for (int i = 0; i < 8; ++i) threads.emplace_back([&] {
        for (int j = 0; j < 50; ++j) {
            const auto value = source->resolve("brush");
            if (!value || *value != *direct) concurrent_ok = false;
        }
    });
    for (auto& thread : threads) thread.join();
    check(concurrent_ok, "Concurrent design icon lookup did not share its asset");
    std::cout << "DESIGN_ICON_CATALOG_OK " << names.size() << '\n';
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
