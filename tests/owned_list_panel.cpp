#include <LVUI/ListPanel.h>

#include <limits>
#include <stdexcept>

namespace { void require(bool value,const char* message){if(!value)throw std::runtime_error(message);} }
int main(){
    using namespace lvui;ListPanelConfiguration input;
    auto normal=list_panel_geometry(input,170,140);
    require(normal && !normal->toolbar_visible && normal->list==PanelRegionRect{0,0,170,114} &&
            normal->footer==PanelRegionRect{0,114,170,26},"Flat list panel lost its default layout");
    input.toolbar_visible=true;auto full=list_panel_geometry(input,200,120);
    require(full && full->toolbar.height==30 && full->list==PanelRegionRect{0,30,200,64} && full->footer.y==94,
            "Flat list panel did not arrange its toolbar, list and footer");
    auto short_panel=list_panel_geometry(input,100,33);
    require(short_panel && short_panel->toolbar.height==30 && short_panel->footer.height==3 && !short_panel->list_visible,
            "Short flat list panel produced overlapping or negative regions");
    input.height=std::numeric_limits<double>::infinity();require(!list_panel_geometry(input,100,100),"Invalid flat list panel geometry was accepted");
}
