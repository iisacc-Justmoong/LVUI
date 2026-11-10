#include <LVUI/HierarchyViewport.h>
#include <LVUI/NativeVirtualCollection.h>
#include <LVUI/NativeMeasuredCollection.h>
#include <LVUI/NativeCollectionNavigation.h>
#include <iiGUI/HierarchyViewport.h>
#include <iiGUI/VirtualCollectionPlanner.h>
#include <iiGUI/VirtualMeasurements.h>
#include <iiGUI/CollectionNavigation.h>
#include <memory>
#include <vector>
using HierarchyViewport=lvui::HierarchyViewportBinding<ii::gui::BasicHierarchyViewportSource>;
using Planner=lvui::native::VirtualCollectionPlannerBinding<ii::gui::BasicVirtualCollectionPlanner>;
using Measurements=lvui::native::VirtualMeasurementsBinding<ii::gui::BasicVirtualMeasurements,int,Planner>;
using Navigation=lvui::native::CollectionNavigationBinding<ii::gui::BasicCollectionNavigation>;
int main(){
    auto key=[](int value,std::size_t){return value;};
    auto hierarchy_source=std::make_shared<lvui::HierarchyMemory>(std::vector<lvui::HierarchyEntry>{
        {.key="first",.label="First"},{.key="second",.label="Second"}});
    lvui::HierarchyController hierarchy(hierarchy_source);
    auto resized=hierarchy.set_row_extent("first",40.);if(!resized||!*resized)return 201;
    auto projected=hierarchy.projection();if(!projected)return 202;
    auto indexed_hierarchy=HierarchyViewport::create(std::make_shared<const lvui::HierarchyProjection>(*projected));
    if(!indexed_hierarchy)return 203;
    auto hierarchy_frame=(*indexed_hierarchy)->frame({.width=100,.height=20,.offset=30,.row_height=20,.overscan=0});
    auto hierarchy_snapshot=hierarchy_source->snapshot();
    if(!hierarchy_frame||hierarchy_frame->content_height!=60||hierarchy_frame->rows.size()!=2||
       hierarchy_frame->rows[0].height!=40||hierarchy_frame->rows[1].y!=10||
       !hierarchy_snapshot||(*hierarchy_snapshot)->revision!=0)return 204;
    auto snapshot=lvui::native::virtual_collection_source(std::vector{1,2,3,4,5},key);
    auto virtual_frame=Planner::frame(*snapshot,{.item_height=40,.overscan=0},{.width=100,.height=40,.y=80});
    if(virtual_frame.collection.items.size()!=1||virtual_frame.collection.items[0].value!=3||virtual_frame.content_height!=200)return 7;
    auto indexed=lvui::native::virtual_collection_source(std::vector{1,2,3},key);
    Measurements measurements;
    auto measured=measurements.plan(indexed,{.item_height=40,.measure_height=true},{.width=100,.height=70});
    measurements.report(measured.measurement_generation,{{"1",100,10},{"2",100,60}});
    auto actual=measurements.plan(indexed,{.item_height=40,.measure_height=true},{.width=100,.height=70});
    if(actual.cells[1].y!=10||actual.content_height!=110)return 8;
    measurements.close();
    Navigation navigation;
    navigation.configure({.mode=lvui::native::CollectionSelectionMode::multiple});
    auto moved=navigation.command(*indexed,lvui::native::CollectionCommand::end);
    if(!moved.handled||navigation.state.get().current_key!="3")return 9;
    navigation.command(*indexed,lvui::native::CollectionCommand::previous,{}, {.range=true});
    if(navigation.state.get().selected_keys!=std::vector<std::string>{"2","3"})return 10;
    navigation.close();
    return 0;
}
