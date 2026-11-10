#include <LVUI/AuthoredIcons.h>
#include "../src/Media/AuthoredIconData.h"
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
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
}

int main() try {
    const auto source = lvui::authored_icon_catalog();
    check(bool(source) && source->revision() == 1, "Authored source is unavailable");
    const auto names = lvui::authored_icon_names();
    check(names.size() == 2860 && std::ranges::is_sorted(names), "Authored inventory changed");
    std::size_t checked = 0;
    for (const auto name : names) {
        const auto path = std::filesystem::path(LVUI_AUTHORED_ICON_DIR) / (std::string(name) + ".svg");
        std::ifstream in(path, std::ios::binary);
        check(bool(in), "Authored SVG file is unavailable");
        const std::string expected((std::istreambuf_iterator<char>(in)), {});
        const auto actual = lvui::authored_icon_svg(name);
        if (!actual || *actual != expected)
            throw std::runtime_error("Authored SVG bytes changed: " + std::string(name));
        if (name != "projectStructure") {
            const auto resolved = source->resolve(name);
            if (!resolved || !*resolved || (*resolved)->svg != expected)
                throw std::runtime_error("Authored SVG lookup changed: " + std::string(name));
        }
        ++checked;
    }
    check(checked == names.size(), "Not all authored SVGs were verified");
    for (const auto& [alias_name, canonical_name] : lvui::detail::authored_resource_aliases()) {
        const auto alias_value = source->resolve(alias_name);
        const auto exact = lvui::authored_icon_svg(canonical_name);
        if (!alias_value || !*alias_value || !exact || (*alias_value)->svg != *exact)
            throw std::runtime_error("Resource alias changed: " + std::string(alias_name));
    }
    for (const auto& [alias_name, canonical_name] : lvui::detail::authored_name_aliases()) {
        const auto alias_value = source->resolve(alias_name);
        const auto canonical_value = source->resolve(canonical_name);
        if (!alias_value || !canonical_value || *alias_value != *canonical_value)
            throw std::runtime_error("Name alias changed: " + std::string(alias_name));
    }
    auto first = source->resolve("generaladd");
    auto alias = source->resolve(" /general/add.SVG ");
    auto qrc = source->resolve("qrc:/qt/qml/LVRS/resources/iconset/generaladd.svg");
    check(first && alias && qrc && *first == *alias && *first == *qrc, "Canonical icon alias did not share its asset");
    auto legacy = source->resolve("GroupByModule-1");
    auto canonical = source->resolve("GroupByModule");
    check(legacy && canonical && *legacy == *canonical, "Authored alias was not preserved");
    auto mixed = source->resolve("nodestest");
    auto original = source->resolve("nodesTest");
    check(mixed && original && *mixed == *original, "Case-sensitive legacy alias was not preserved");
    auto figma_javascript = source->resolve("javascript");
    auto authored_javascript = source->resolve("javaScript");
    check(figma_javascript && authored_javascript && *figma_javascript == *authored_javascript,
          "Figma javascript icon name was not resolved");
    auto project = source->resolve("projectStructure");
    auto normalized_project = source->resolve("generalprojectStructure");
    check(project && normalized_project && *project == *normalized_project,
          "Existing projectStructure name normalization was not preserved");
    auto large = source->resolve("hive-chart");
    check(large && (*large)->svg.size() > 65536, "Large authored SVG was truncated or rejected");
    auto missing = source->resolve("missing-icon-for-test");
    check(!missing && missing.error().code == lvui::IconErrorCode::not_found, "Unknown icon was accepted");
    check(!source->resolve("") && !source->resolve(std::string("a\0b", 3)), "Invalid icon name was accepted");
    std::atomic<bool> concurrent_ok{true};
    std::vector<std::thread> threads;
    for (int i = 0; i < 8; ++i) threads.emplace_back([&] {
        for (int j = 0; j < 50; ++j) {
            auto value = source->resolve("generaladd");
            if (!value || *value != *first) concurrent_ok = false;
        }
    });
    for (auto& thread : threads) thread.join();
    check(concurrent_ok, "Concurrent lookup changed immutable asset identity");
    std::cout << "AUTHORED_ICON_CATALOG_OK " << checked << '\n';
    return 0;
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
