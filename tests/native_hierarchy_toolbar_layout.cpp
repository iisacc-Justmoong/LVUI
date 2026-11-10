#include <LVUI/NativeHierarchyToolbar.h>
#include <iiGUI/HierarchyToolbarLayout.h>
#include <iiGUI/ToolbarButtonModel.h>
using ToolbarButtonLayoutModel=ii::gui::BasicToolbarButton<lvui::native::ToolbarButtonModelPolicy>;
using ToolbarLayoutTest=lvui::native::HierarchyToolbarLayoutBinding<ii::gui::BasicHierarchyToolbarLayout,ToolbarButtonLayoutModel>;
#include <limits>
#include <stdexcept>
namespace {void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}}
int main(){
    lvui::HierarchyToolbarSnapshot snapshot;
    snapshot.items={{.icon_name="home"},{.visible=false},{.enabled=false},{.icon_name="generaladd"}};
    lvui::HierarchyToolbarAppearance input;
    auto result=ToolbarLayoutTest::geometry(snapshot,input,{0,0,0,0});
    require(result&&result->width==200&&result->height==26&&result->slots[0].x==8&&result->slots[1].width==0,
            "Hierarchy toolbar defaults changed");
    input.distribute_spacing=true;result=ToolbarLayoutTest::geometry(snapshot,input,{0,0,0,0});
    require(result&&result->slots[3].x+result->slots[3].width==192,"Hierarchy toolbar did not distribute spare space");
    input.width=30;result=ToolbarLayoutTest::geometry(snapshot,input,{0,0,0,0});
    require(result&&result->slots[3].x+result->slots[3].width<=22.0001,"Narrow hierarchy toolbar overflowed");
    input={};input.slot_padding=10;input.icon_text_spacing=5;snapshot.items[0].text="Open";
    result=ToolbarLayoutTest::geometry(snapshot,input,{30,0,0,0});
    require(result&&result->slots[0].width==63,"Hierarchy layout ignored supplied design metrics");
    require(!ToolbarLayoutTest::geometry(snapshot,input,{0,0,0}),"Mismatched widths were accepted");
    input.width=std::numeric_limits<double>::quiet_NaN();
    require(!ToolbarLayoutTest::validate(input),"Invalid hierarchy appearance was accepted");
}
