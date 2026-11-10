#include <LVUI/ListItemForm.h>
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
    ListItemFormConfiguration c;
    auto size=ListItemForm::intrinsic_size(c);
    require(size&&size->width==400&&size->height==170,"Form default extent changed");
    auto g=ListItemForm::geometry(c,400,170);
    require(g&&g->label.x==12&&g->label.y==12&&g->description.y==29&&g->selector.x==291&&g->selector.y==16&&
            g->field_label1.y==61.5&&g->input1.x==248&&g->input1.y==52&&g->field_label2.y==103.5&&
            g->input2.x==248&&g->input2.y==94&&g->toggle.x==12&&g->toggle.y==136&&g->status.y==141.5&&g->actions.x==236,
            "Form fields, header, or actions changed");
    c.show_description=false;c.show_selector=false;c.show_input1=false;c.show_input2=false;
    c.show_toggle=false;c.show_primary_action=false;c.show_secondary_action=false;
    size=ListItemForm::intrinsic_size(c);require(size&&size->height==106,"Hidden form controls retained their height");
    c={};
    for(int w=0;w<=400;++w)for(int h:{0,10,70,130,170}){
        g=ListItemForm::geometry(c,w,h);require(bool(g),"A valid narrow form was rejected");
        for(auto r:{g->label,g->description,g->selector,g->field_label1,g->input1,g->field_label2,g->input2,g->toggle,g->status,g->actions})
            require(inside(r,w,h),"A form control escaped its host");
        require(separated(g->label,g->selector)&&separated(g->field_label1,g->input1)&&separated(g->field_label2,g->input2)&&
                separated(g->toggle,g->status)&&separated(g->status,g->actions),"Form controls overlapped at a narrow size");
    }
    c.input_width=std::numeric_limits<double>::quiet_NaN();require(!ListItemForm::intrinsic_size(c),"Invalid input width was accepted");
    c={};require(!ListItemForm::geometry(c,-1,20),"Invalid host extent was accepted");
}
