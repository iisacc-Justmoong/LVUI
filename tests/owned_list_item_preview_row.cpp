#include <LVUI/ListItemPreviewRow.h>
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
    ListItemPreviewRowConfiguration c;
    auto size=ListItemPreviewRow::intrinsic_size(c);
    require(size&&size->width==400&&size->height==129,"Resource default extent changed");
    auto g=ListItemPreviewRow::geometry(c,400,129);
    require(g&&g->preview.x==12&&g->preview.y==12&&g->label.x==68&&g->label.y==16.5&&g->description.y==33.5&&
            g->more.x==316&&g->more.y==25&&g->metadata[0].y==72&&g->status.y==100.5&&g->actions.x==236,
            "Resource header, metadata, or actions changed");
    c.preview_width=70;c.preview_height=90;size=ListItemPreviewRow::intrinsic_size(c);
    g=ListItemPreviewRow::geometry(c,400,size?size->height:0);
    require(size&&size->height==171&&g&&g->preview.width==70&&g->preview.height==90&&g->label.x==90&&
            separated(g->preview,g->label),"Rectangular preview did not reserve its natural dimensions");
    for(int w=0;w<=80;++w)for(int h=0;h<=80;++h){
        g=ListItemPreviewRow::geometry(c,w,h);
        require(g&&inside(g->preview,w,h)&&inside(g->label,w,h)&&inside(g->more,w,h)&&
                separated(g->preview,g->label)&&separated(g->preview,g->more),
                "A rectangular preview escaped or overlapped a narrow row");
    }
    c.preview_width=std::numeric_limits<double>::infinity();
    require(!ListItemPreviewRow::intrinsic_size(c),"An invalid preview width was accepted");
    c.preview_width.reset();c.preview_height.reset();
    c.variant=ListItemPreviewVariant::media;size=ListItemPreviewRow::intrinsic_size(c);
    require(size&&size->width==400&&size->height==113,"Media default extent changed");
    g=ListItemPreviewRow::geometry(c,400,113);
    require(g&&g->preview.x==12&&g->label.x==68&&g->label.y==22&&g->description.y==39&&
            g->duration.x==316&&g->duration.y==23&&g->media_detail.y==38&&g->segments.x==12&&
            g->segments.y==72&&g->status.y==81&&g->actions.x==236&&g->actions.y==75.5,
            "Media preview, detail, segments, or actions changed");
    c.show_preview=false;c.show_description=false;c.show_segments=false;c.show_primary_action=false;c.show_secondary_action=false;
    size=ListItemPreviewRow::intrinsic_size(c);
    require(size&&size->height==73,"Hidden media controls retained their height");
    for(auto variant:{ListItemPreviewVariant::resource,ListItemPreviewVariant::media}){
        c={};c.variant=variant;
        for(int w=0;w<=400;++w)for(int h:{0,10,60,100,129}){
            g=ListItemPreviewRow::geometry(c,w,h);require(bool(g),"A valid preview host was rejected");
            for(auto r:{g->preview,g->label,g->description,g->more,g->duration,g->media_detail,g->metadata[0],g->metadata[1],g->metadata[2],g->segments,g->status,g->actions})
                require(inside(r,w,h),"A preview item escaped its host");
            require(separated(g->preview,g->label)&&separated(g->label,g->more)&&separated(g->label,g->duration)&&
                separated(g->metadata[0],g->metadata[1])&&separated(g->metadata[1],g->metadata[2])&&
                separated(g->segments,g->status)&&separated(g->status,g->actions),"Preview controls overlapped at a narrow size");
        }
    }
    c.preview_size=std::numeric_limits<double>::infinity();require(!ListItemPreviewRow::intrinsic_size(c),"Invalid preview size was accepted");
    c={};require(!ListItemPreviewRow::geometry(c,-1,20),"Invalid host extent was accepted");
}
