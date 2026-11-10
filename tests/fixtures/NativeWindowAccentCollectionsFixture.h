#pragma once

#include <LVUI/Hierarchy.h>
#include <LVUI/HierarchyToolbar.h>
#include <LVUI/List.h>
#include <LVUI/ListFooter.h>
#include <LVUI/Table.h>
#include <LVUI/TableRowView.h>
#include <iiSDKCore/Core/Property/Observable.h>

#include <memory>
#include <string>
#include <vector>

struct NativeWindowAccentCollectionsModel final {
    ii::core::Observable<std::string> accent{"blue"};
    ii::core::Observable<double> width{1000};

    std::shared_ptr<lvui::HierarchyMemory> hierarchy_source = std::make_shared<lvui::HierarchyMemory>(
        std::vector<lvui::HierarchyEntry>{{.key="root", .label="Hierarchy row"},
                                          {.key="child", .parent="root", .label="Hierarchy child"}});
    ii::core::Observable<std::shared_ptr<lvui::HierarchyController>> hierarchy{
        std::make_shared<lvui::HierarchyController>(hierarchy_source)};

    std::shared_ptr<lvui::ListMemory> list_source = lvui::ListMemory::create(
        {{.key="one", .label="List row"}, {.key="two", .label="Other row"}}).value();
    ii::core::Observable<std::shared_ptr<lvui::ListController>> list{
        std::make_shared<lvui::ListController>(lvui::ListController::create(list_source).value())};
    ii::core::Observable<std::shared_ptr<lvui::ListEditor>> list_editor{list_source};

    std::shared_ptr<lvui::TableMemory> table_source = std::make_shared<lvui::TableMemory>(
        std::vector<lvui::TableColumn>{{.key="c", .title="Column", .width=100}},
        std::vector<lvui::TableRow>{{.key="r", .cells={{.text="Cell"}}}});
    ii::core::Observable<std::shared_ptr<lvui::TableController>> table{
        std::make_shared<lvui::TableController>(table_source)};
    ii::core::Observable<std::shared_ptr<lvui::TableEditor>> table_editor{table_source};
    ii::core::Observable<std::vector<lvui::TableRowCell>> cells{
        std::vector<lvui::TableRowCell>{{.key="cell", .value={.text="Row cell"}, .selected=true, .current=true}}};
    ii::core::Observable<std::vector<lvui::HierarchyToolbarItem>> tools{
        std::vector<lvui::HierarchyToolbarItem>{{.id=std::string("tool"), .text="Tool"}}};
    ii::core::Observable<lvui::ListFooterSlot> footer{lvui::ListFooterSlot{.text="Action"}};

    NativeWindowAccentCollectionsModel() {
        (void)hierarchy.get()->select("root").value();
        (void)hierarchy.get()->set_expanded("root", true).value();
        (void)list.get()->select("one").value();
        (void)table.get()->select({"r", "c"}).value();
    }
};
