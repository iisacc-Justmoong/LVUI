#include <LVUI/Tab.h>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool near(double a,double b){return std::abs(a-b)<1e-8;}
}
int main(){
    lvui::Tab tab;lvui::TabConfiguration config;config.text="Overview";config.icon_name="home";config.badge="3";
    require(tab.set_configuration(config),"Valid tab was rejected");
    auto size=tab.geometry(140,32,52,13,8,13);
    require(size&&near(size->icon.width,18)&&near(size->label.width,52)&&near(size->badge.width,8)&&size->underline.height==0,
            "Tab content geometry is incorrect");
    config.selected=true;require(tab.set_configuration(config),"Selected tab was rejected");
    size=tab.geometry(140,32,52,13,8,13);
    require(size&&near(size->underline.y,30)&&near(size->underline.height,2)&&tab.visual({}).underline,"Selected underline is incorrect");
    config.style=lvui::TabStyle::surface;require(tab.set_configuration(config),"Surface tab was rejected");
    require(tab.visual({}).background==config.palette.surface_selected&&!tab.visual({}).underline,"Surface selection is incorrect");
    require(tab.visual({.hovered=true,.pressed=true}).background==config.palette.pressed,"Pressed state did not override hover");
    config.enabled=false;require(tab.set_configuration(config),"Disabled tab was rejected");
    require(tab.visual({.hovered=true,.focused=true}).text==config.palette.disabled_text&&!tab.visual({.focused=true}).focused,
            "Disabled tab retained an interactive appearance");
    auto bad=config;bad.icon_name="  ";require(!tab.set_configuration(bad)&&tab.configuration()==config,"Invalid icon changed tab state");
    bad=config;bad.height=std::numeric_limits<double>::quiet_NaN();require(!tab.set_configuration(bad),"Invalid geometry was accepted");
    require(!tab.geometry(-1,32,52,13,8,13),"Invalid extent was accepted");

    lvui::TabBar bar;require(bar.set_items({true,false,true}),"Valid tabs were rejected");
    require(bar.enabled(0)&&!bar.enabled(1)&&!bar.enabled(3),"Tab enabled lookup is incorrect");
    require(near(*bar.equal_tab_width(400),(400.-8)/3)&&near(*bar.equal_tab_width(100),120),"Equal tab width is incorrect");
    require(bar.focus_target(0,lvui::TabFocusMove::next)==2&&bar.focus_target(0,lvui::TabFocusMove::previous)==2&&
            bar.focus_target(2,lvui::TabFocusMove::first)==0&&bar.focus_target(0,lvui::TabFocusMove::last)==2,
            "Keyboard focus did not skip disabled tabs or wrap");
    require(bar.activate(1)==lvui::TabActivation::rejected&&bar.configuration().current_index==0,"Disabled tab changed selection");
    require(bar.activate(2)==lvui::TabActivation::selection_changed&&bar.configuration().current_index==2&&
            bar.activate(2)==lvui::TabActivation::invoked,"Tab selection or repeated activation is incorrect");
    auto bar_config=bar.configuration();bar_config.auto_select=false;require(bar.set_configuration(bar_config),"Manual selection was rejected");
    require(bar.activate(0)==lvui::TabActivation::invoked&&bar.configuration().current_index==2,"Manual activation changed selection");
    require(bar.set_items({false,false})&&!bar.focus_target(0,lvui::TabFocusMove::next),"All-disabled tabs accepted focus movement");
    auto invalid=bar_config;invalid.current_index=4096;require(!bar.set_configuration(invalid)&&bar.configuration()==bar_config,"Invalid tab bar changed state");
    require(!bar.set_items(std::vector<bool>(4097,true))&&bar.count()==2,"Too many tabs changed state");
}
