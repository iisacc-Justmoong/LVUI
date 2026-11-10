#include <LVUI/MobileNavigation.h>
#include <iiGUI/MobileNavigationModel.h>
namespace lvui {using MobileNavigationModel=MobileNavigationModelBinding<ii::gui::BasicMobileNavigationModel>;inline bool valid_mobile_navigation(const MobileNavigationConfiguration& c){return MobileNavigationModel::validate(c);}inline MobileNavigationLayout mobile_navigation_layout(const MobileNavigationConfiguration& c,double w){return MobileNavigationModel::layout(c,w);}}
#include <cmath>
#include <limits>
#include <stdexcept>
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){
    lvui::MobileNavigationConfiguration c;c.items={{"a","Home","home","Home"},{"b","Tools","generaladd","Tools"}};
    c.search=lvui::MobileNavigationItem{"search","","generalsearch","Search"};c.current_index=0;
    check(lvui::valid_mobile_navigation(c),"Valid mobile navigation rejected");
    auto layout=lvui::mobile_navigation_layout(c,402);
    check(layout.height==88&&layout.tab_width==56&&layout.search_x==330&&layout.group_width==120,
          "Mobile navigation lost Figma geometry");
    auto narrow=lvui::mobile_navigation_layout(c,150);
    check(narrow.tab_width==48&&narrow.content_width>narrow.viewport_width,"Narrow tabs lost the touch size or overflow");
    c.items[1].key="a";check(!lvui::valid_mobile_navigation(c),"Duplicate tab keys accepted");
    c.items[1].key="b";c.search->accessibility_name.clear();check(!lvui::valid_mobile_navigation(c),"Unlabelled search accepted");
    c.search->accessibility_name="Search";c.bottom_safe_inset=std::numeric_limits<double>::infinity();
    check(!lvui::valid_mobile_navigation(c),"Invalid inset accepted");
    c.bottom_safe_inset=0;c.current_index=2;check(!lvui::valid_mobile_navigation(c),"Out-of-range selection accepted");
}
