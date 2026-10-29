#include <LVUI/MenuItem.h>
#include <limits>
#include <stdexcept>
namespace {void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}}
int main(){
    lvui::MenuItem item;auto value=item.configuration();value.label="Open";value.shortcut="⌘O";
    require(item.set_configuration(value)&&item.width()==161&&item.height()==24,"Regular menu item defaults are incorrect");
    auto geometry=item.geometry(161,24,40,13,18,13);
    require(geometry&&geometry->icon.width==16&&geometry->chevron.width==16&&geometry->label.width==40&&geometry->shortcut.width==18,
            "Menu item layout is incorrect");
    require(item.visual({.hovered=true,.pressed=true}).background==value.palette.pressed,"Pressed menu item did not override hover");
    value.compact=true;value.state=lvui::MenuEntryState::selected;value.expanded=true;require(item.set_configuration(value),"Compact selection was rejected");
    require(item.width()==141&&item.height()==18&&item.direction()==lvui::MenuDirection::down&&item.visual({}).background==value.palette.selected,
            "Compact selection or automatic chevron direction is incorrect");
    value.state=lvui::MenuEntryState::inactive;require(item.set_configuration(value)&&!item.visual({.focused=true}).enabled&&!item.visual({.focused=true}).focused,
            "Inactive menu item retained interaction");
    value.key_visible=false;require(item.set_configuration(value)&&item.shortcut().empty(),"Hidden shortcut remained visible");
    value.key_visible=true;value.shortcut.clear();value.key_placeholder="Key";require(item.set_configuration(value)&&item.shortcut()=="Key","Shortcut placeholder was lost");
    auto narrow=item.geometry(10,10,40,13,18,13);require(narrow&&narrow->icon.width<=10&&narrow->label.width>=0&&narrow->chevron.width>=0,
            "Narrow menu item produced invalid geometry");
    auto invalid=value;invalid.icon_size=std::numeric_limits<double>::quiet_NaN();require(!item.set_configuration(invalid)&&item.configuration()==value,
            "Invalid menu item replaced valid state");
    require(!item.geometry(-1,24,40,13,18,13),"Invalid menu item extent was accepted");
}
