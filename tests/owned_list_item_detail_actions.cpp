#include <LVUI/ListItemDetailActions.h>
#include "list_item_layout_test_binding.h"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool inside(lvui::ListItemRect r,double w,double h){
    return r.x>=0&&r.y>=0&&r.width>=0&&r.height>=0&&r.x+r.width<=w+1e-8&&r.y+r.height<=h+1e-8;
}
}
int main(){
    using namespace lvui;
    ListItemDetailActionsConfiguration c;
    auto size=ListItemDetailActions::intrinsic_size(c);
    require(size&&size->width==400&&size->height==120,"Compound item default size changed");
    auto g=ListItemDetailActions::geometry(c,400,120);
    require(g&&g->leading.x==12&&g->leading.width==18&&g->label.x==38&&g->label.y==12&&
            g->description.y==29&&g->description.height==22&&g->bookmark.x==370&&
            g->metadata[0].y==63&&g->status.y==91.5&&g->actions.y==86&&g->actions.width==232,
            "Compound item geometry differs from its sections");
    c.leading_width=70;c.leading_height=60;size=ListItemDetailActions::intrinsic_size(c);
    g=ListItemDetailActions::geometry(c,400,size?size->height:0);
    require(size&&size->height==141&&g&&g->leading.width==70&&g->leading.height==60&&g->label.x==90,
            "Detail actions lost the custom leading view's natural dimensions");
    for(int w=0;w<=80;++w)for(int h=0;h<=80;++h){
        g=ListItemDetailActions::geometry(c,w,h);
        require(g&&inside(g->leading,w,h)&&inside(g->label,w,h)&&inside(g->bookmark,w,h)&&
                g->leading.x+g->leading.width<=g->label.x+1e-9,
                "A rectangular leading view escaped or overlapped a narrow detail row");
    }
    c.leading_height=std::numeric_limits<double>::infinity();
    require(!ListItemDetailActions::intrinsic_size(c),"An invalid leading height was accepted");
    c.leading_width.reset();c.leading_height.reset();
    c.show_metadata=false;size=ListItemDetailActions::intrinsic_size(c);
    require(size&&size->height==97,"Hiding metadata did not remove its height and gap");
    c.show_description=false;size=ListItemDetailActions::intrinsic_size(c);
    require(size&&size->height==76,"Hiding the description did not update intrinsic height");
    c.show_actions={false,false,false};size=ListItemDetailActions::intrinsic_size(c);
    require(size&&size->height==65,"A status-only footer retained button height");
    c={};c.width=300;c.height=75;size=ListItemDetailActions::intrinsic_size(c);
    require(size&&size->width==300&&size->height==75,"Explicit compound dimensions were ignored");
    for(int w=0;w<=80;++w)for(int h=0;h<=80;++h){
        g=ListItemDetailActions::geometry(c,w,h);
        require(g&&inside(g->leading,w,h)&&inside(g->bookmark,w,h)&&inside(g->label,w,h)&&
            inside(g->description,w,h)&&inside(g->status,w,h)&&inside(g->actions,w,h),"Compound geometry escaped its host");
        for(auto rect:g->metadata)require(inside(rect,w,h),"Metadata escaped its host");
    }
    c.action_width=std::numeric_limits<double>::quiet_NaN();
    require(!ListItemDetailActions::intrinsic_size(c),"Invalid geometry was accepted");
    c={};require(!ListItemDetailActions::geometry(c,-1,20),"Invalid host extent was accepted");
}
