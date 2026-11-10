#include <LVUI/ListItemTask.h>
#include "list_item_layout_test_binding.h"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool inside(lvui::ListItemRect r,double w,double h){
    return r.x>=0&&r.y>=0&&r.width>=0&&r.height>=0&&r.x+r.width<=w+1e-8&&r.y+r.height<=h+1e-8;
}
bool separated(lvui::ListItemRect a,lvui::ListItemRect b){
    return a.width==0||a.height==0||b.width==0||b.height==0||
        a.x+a.width<=b.x+1e-8||b.x+b.width<=a.x+1e-8||a.y+a.height<=b.y+1e-8||b.y+b.height<=a.y+1e-8;
}
}
int main(){
    using namespace lvui;
    ListItemTaskConfiguration c;
    auto size=ListItemTask::intrinsic_size(c);
    require(size&&size->width==400&&size->height==135,"Task item default extent changed");
    auto g=ListItemTask::geometry(c,400,135);
    require(g&&g->selection.x==12&&g->selection.y==13&&g->label.x==38&&g->selector.x==291&&
            g->description.x==12&&g->description.y==44&&g->metadata[0].y==78&&
            g->status.y==106.5&&g->quantity_label.x==158&&g->quantity_stepper.x==210&&g->actions.x==236,
            "Task sections or their controls are not positioned independently");
    c.show_description=false;c.show_metadata=false;c.show_selection=false;c.show_selector=false;
    c.show_stepper=false;c.show_primary_action=false;c.show_secondary_action=false;
    size=ListItemTask::intrinsic_size(c);
    require(size&&size->height==67,"Hidden task sections retained their height");
    c={};
    for(int w=0;w<=400;++w)for(int h:{0,10,44,80,135}){
        g=ListItemTask::geometry(c,w,h);require(bool(g),"A valid narrow host was rejected");
        for(auto r:{g->selection,g->label,g->selector,g->description,g->metadata[0],g->metadata[1],g->metadata[2],
                    g->status,g->quantity_label,g->quantity_stepper,g->actions})
            require(inside(r,w,h),"A task control escaped its host");
        require(separated(g->selection,g->label)&&separated(g->label,g->selector)&&
                separated(g->metadata[0],g->metadata[1])&&separated(g->metadata[1],g->metadata[2])&&
                separated(g->status,g->quantity_label)&&separated(g->quantity_label,g->quantity_stepper)&&
                separated(g->quantity_stepper,g->actions),"Task controls overlapped at a narrow size");
    }
    c.section_spacing=0;c.stepper_size=0;c.show_stepper=false;c.show_primary_action=false;c.show_secondary_action=false;
    size=ListItemTask::intrinsic_size(c);g=ListItemTask::geometry(c,size->width,size->height);
    require(separated(g->metadata[0],g->quantity_label),"A zero-size stepper caused the quantity label to overlap metadata");
    c.selector_width=std::numeric_limits<double>::quiet_NaN();
    require(!ListItemTask::intrinsic_size(c),"Invalid selector width was accepted");
    c={};require(!ListItemTask::geometry(c,-1,20),"Invalid host extent was accepted");
}
