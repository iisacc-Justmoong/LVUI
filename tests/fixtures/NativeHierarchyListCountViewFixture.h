#pragma once
#include <LVUI/Hierarchy.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct NativeHierarchyListCountViewModel final {
    static std::vector<lvui::HierarchyEntry> entries() {
        std::vector<lvui::HierarchyEntry> result{{.key="one",.label="One",.count=1},
                                                 {.key="two",.label="Two",.count=2}};
        for(int n=0;n<30;++n)result.push_back({.key="row"+std::to_string(n),.label="Row",.count=n});
        return result;
    }
    std::shared_ptr<lvui::HierarchyMemory> source=std::make_shared<lvui::HierarchyMemory>(entries());
    ii::core::Observable<std::shared_ptr<lvui::HierarchyController>> controller{std::make_shared<lvui::HierarchyController>(source)};
    ii::core::Observable<std::int64_t> revision{0};
    ii::core::Observable<std::int64_t> view_revision{0};
    ii::core::Observable<std::string> badge{"Badge"};
};
