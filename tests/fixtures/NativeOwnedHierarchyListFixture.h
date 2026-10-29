#pragma once
#include <LVUI/Hierarchy.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct NativeOwnedHierarchyListModel final {
    std::shared_ptr<lvui::HierarchyMemory> source = std::make_shared<lvui::HierarchyMemory>(
        std::vector<lvui::HierarchyEntry>{{.key="root", .label="Root", .id=42},
                                          {.key="child", .parent="root", .label="Child"},
                                          {.key="other", .label="Other"}});
    ii::core::Observable<std::shared_ptr<lvui::HierarchyController>> controller{
        std::make_shared<lvui::HierarchyController>(source)};
    ii::core::Observable<std::shared_ptr<lvui::HierarchyEditor>> editor{source};
    ii::core::Observable<bool> editable{true};
    ii::core::Observable<std::int64_t> revision{0};
    ii::core::Observable<double> width{240};
    ii::core::Observable<bool> keyboard{true};
    int activations = 0, expansions = 0, moves = 0;
    std::string active_key; std::int64_t active_id = -1; std::size_t active_index = 0;
    void activated(std::string key, std::int64_t id, std::size_t index) {
        ++activations; active_key = std::move(key); active_id = id; active_index = index;
    }
    void expanded(std::string key, bool value) { if (key == "root" && value) ++expansions; }
    void moved(const lvui::HierarchyMovePlan&) { ++moves; revision.set(std::int64_t(source->snapshot().value()->revision)); }
};
