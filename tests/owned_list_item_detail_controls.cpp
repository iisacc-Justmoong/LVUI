#include <LVUI/ListItemDetailControls.h>
#include "list_item_layout_test_binding.h"

#include <array>
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
    ListItemDetailControlsConfiguration c;
    auto size=ListItemDetailControls::intrinsic_size(c);
    require(size&&size->width==400&&size->height==118,"Quantity item default extent changed");
    auto g=ListItemDetailControls::geometry(c,400,118);
    require(g&&g->label.x==12&&g->label.y==12&&g->description.y==29&&g->header_selector.x==291&&
            g->quantity_caption.y==56.5&&g->quantity_label.x==213&&g->quantity_stepper.x==265&&g->unit_selector.x==291&&
            g->toggle.x==12&&g->toggle.y==84&&g->status.y==89.5&&g->actions.x==316,
            "Quantity item lost its header, quantity row or footer layout");
    c.show_selector=false;c.show_unit_selector=false;c.show_stepper=false;c.show_toggle=false;c.show_primary_action=false;
    size=ListItemDetailControls::intrinsic_size(c);
    require(size&&size->height==105,"Hidden quantity controls retained their height");
    c={};c.variant=ListItemDetailControlVariant::settings;
    size=ListItemDetailControls::intrinsic_size(c);
    require(size&&size->width==400&&size->height==159,"Settings item default extent changed");
    g=ListItemDetailControls::geometry(c,400,159);
    require(g&&g->leading.x==12&&g->label.x==38&&g->toggle.x==350&&g->selector.x==291&&
            g->mode_caption.y==56.5&&g->segments.x==266&&g->scope_caption.y==93&&
            g->actions.x==236&&g->actions.y==125,"Settings item lost its selector, segments or action layout");
    c.leading_width=70;c.leading_height=60;size=ListItemDetailControls::intrinsic_size(c);
    g=ListItemDetailControls::geometry(c,400,size?size->height:0);
    require(size&&size->height==191&&g&&g->leading.width==70&&g->leading.height==60&&g->label.x==90&&
            separated(g->leading,g->label),"Detail settings lost the custom leading view's natural dimensions");
    for(int w=0;w<=80;++w)for(int h=0;h<=80;++h){
        g=ListItemDetailControls::geometry(c,w,h);
        require(g&&inside(g->leading,w,h)&&inside(g->label,w,h)&&inside(g->toggle,w,h)&&
                separated(g->leading,g->label)&&separated(g->leading,g->toggle),
                "A rectangular leading view escaped or overlapped a narrow settings row");
    }
    c.leading_width=std::numeric_limits<double>::quiet_NaN();
    require(!ListItemDetailControls::intrinsic_size(c),"An invalid leading width was accepted");
    c.leading_width.reset();c.leading_height.reset();
    c.show_description=false;c.show_selector=false;c.show_segments=false;c.show_toggle=false;
    c.show_primary_action=false;c.show_secondary_action=false;
    size=ListItemDetailControls::intrinsic_size(c);
    require(size&&size->height==111,"Hidden settings controls retained their height");
    for(auto variant:{ListItemDetailControlVariant::quantity,ListItemDetailControlVariant::settings}){
        c={};c.variant=variant;
        for(int w=0;w<=400;++w)for(int h:{0,10,40,80,159}){
            g=ListItemDetailControls::geometry(c,w,h);require(bool(g),"A valid narrow host was rejected");
            for(auto r:{g->leading,g->label,g->description,g->header_selector,g->toggle,g->quantity_caption,g->quantity_label,
                        g->quantity_stepper,g->unit_selector,g->mode_caption,g->selector,g->scope_caption,g->segments,g->status,g->actions})
                require(inside(r,w,h),"A detail control escaped its host");
            require(separated(g->label,g->leading)&&separated(g->label,g->header_selector)&&separated(g->label,g->toggle)&&
                    separated(g->quantity_caption,g->quantity_label)&&separated(g->quantity_label,g->quantity_stepper)&&
                    separated(g->quantity_stepper,g->unit_selector)&&separated(g->mode_caption,g->selector)&&
                    separated(g->scope_caption,g->segments)&&separated(g->status,g->actions),"Detail controls overlapped at a narrow size");
        }
    }
    c.segment_width=std::numeric_limits<double>::quiet_NaN();require(!ListItemDetailControls::intrinsic_size(c),"Invalid segment width was accepted");
    c={};require(!ListItemDetailControls::geometry(c,-1,20),"Invalid host extent was accepted");
}
