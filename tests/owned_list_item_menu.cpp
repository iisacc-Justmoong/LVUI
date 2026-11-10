#include <LVUI/ListItemMenu.h>
#include <iiGUI/ListItemMenuModel.h>
namespace lvui {using ListItemMenu=ListItemMenuBinding<ii::gui::BasicListItemMenu>;}

#include <any>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main(){
    using namespace lvui;
    auto payload=std::make_shared<const std::any>(std::string("asset-id"));
    ListItemMenuEntry leaf;leaf.label="Open";leaf.shortcut="Ctrl+O";leaf.icon_name="generaladd";leaf.payload=payload;
    ListItemMenuEntry divider;divider.divider=true;
    ListItemMenuEntry branch;branch.label="Export";branch.children={leaf};
    std::vector<ListItemMenuEntry> options{leaf,divider,branch};
    require(ListItemMenu::validate_options(options),"Rich menu rejected valid entries");
    require(std::any_cast<std::string>(*options[2].children[0].payload)=="asset-id","Menu data was not preserved");
    auto bad=options;bad[1].children={leaf};require(!ListItemMenu::validate_options(bad),"Divider accepted a submenu");
    bad=options;bad[2].has_child_items=false;require(!ListItemMenu::validate_options(bad),"Menu accepted children while child items are disabled");
    bad=options;bad[1].shortcut="Ctrl+D";require(!ListItemMenu::validate_options(bad),"Divider accepted an ignored shortcut");
    bad=options;bad[0].label=std::string("bad\xff",4);require(!ListItemMenu::validate_options(bad),"Menu accepted invalid text");
    bad=options;bad[0].icon_name=std::string(257,'x');require(!ListItemMenu::validate_options(bad),"Menu accepted an oversized icon name");
    bad=options;bad[0].state=static_cast<MenuEntryState>(-1);require(!ListItemMenu::validate_options(bad),"Menu accepted an invalid state");
    bad=options;bad[0].direction=static_cast<MenuDirection>(-1);require(!ListItemMenu::validate_options(bad),"Menu accepted an invalid direction");
    require(ListItemMenu::validate_options(std::vector<ListItemMenuEntry>(400,leaf))&&
            !ListItemMenu::validate_options(std::vector<ListItemMenuEntry>(401,leaf)),"Menu entry limit was not enforced");
    ListItemMenuEntry deep;deep.label="Deep";
    for(int i=0;i<15;++i){ListItemMenuEntry parent;parent.label="Parent";parent.children.push_back(std::move(deep));deep=std::move(parent);}
    require(ListItemMenu::validate_options({deep}),"Menu rejected its maximum supported depth");
    ListItemMenuEntry parent;parent.label="Parent";parent.children.push_back(std::move(deep));
    require(!ListItemMenu::validate_options({parent}),"Menu accepted excessive nesting");
    bad=options;bad[0].label=std::string(8193,'a');require(!ListItemMenu::validate_options(bad),"Menu accepted oversized text");
    ListItemMenuEntry wide;wide.label=std::string(8192,'a');std::vector<ListItemMenuEntry> budget(128,wide);
    require(ListItemMenu::validate_options(budget),"Menu rejected its text budget boundary");
    budget.push_back(wide);require(!ListItemMenu::validate_options(budget),"Menu accepted an oversized text budget");
}
