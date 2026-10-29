#pragma once
#include <LVUI/Hierarchy.h>
#include <LVUI/HierarchyToolbar.h>
#include <LVUI/ListFooter.h>
#include <iiSDKCore/Core/Property/Observable.h>

struct NativeOwnedHierarchyPanelModel final {
    std::shared_ptr<lvui::HierarchyMemory> source=std::make_shared<lvui::HierarchyMemory>(
        std::vector<lvui::HierarchyEntry>{{.key="root",.label="Root",.id=42},
            {.key="child",.parent="root",.label="Child"},{.key="other",.label="Other"}});
    ii::core::Observable<std::shared_ptr<lvui::HierarchyController>> controller{std::make_shared<lvui::HierarchyController>(source)};
    ii::core::Observable<std::shared_ptr<lvui::HierarchyEditor>> editor{source};
    ii::core::Observable<bool> editable{true};
    ii::core::Observable<std::int64_t> revision{0};
    ii::core::Observable<double> width{240};
    int activations=0,expansions=0,moves=0;
    void activated(std::string,std::int64_t,std::size_t){++activations;}
    void expanded(std::string,bool){++expansions;}
    void moved(const lvui::HierarchyMovePlan&){++moves;revision.set(static_cast<std::int64_t>(source->snapshot().value()->revision));}
    ii::core::Observable<std::vector<lvui::HierarchyToolbarItem>> tools{std::vector<lvui::HierarchyToolbarItem>{
        {.id=lvui::ToolbarButtonId{std::string("home")},.icon_name="home",.accessibility_name="Panel home"}}};
    ii::core::Observable<std::optional<lvui::ToolbarButtonId>> active{std::nullopt};
    ii::core::Observable<bool> footer{true};
    int tool_actions=0,footer_actions=0;
    void tool(const lvui::HierarchyToolbarSelection&) { ++tool_actions; }
    void footerClicked(std::size_t,const lvui::ListFooterSlot&) { ++footer_actions; }
};
