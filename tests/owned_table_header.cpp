#include <LVUI/TableHeader.h>

#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
}
int main(){
    using namespace lvui;
    TableHeader header;auto c=header.configuration();
    require(c.width==717&&c.row_height==24&&c.columns.size()==3,"Header defaults changed");
    auto frame=header.layout(717,0);
    require(frame&&frame->columns.size()==3&&std::abs(frame->columns[0].extent-239)<1e-7&&
            frame->columns[1].offset==239,"Flexible columns did not share the header width");
    c.columns={{"a","First",100},{"b","Second",0},{"c","Third",0}};
    c.width=300;c.minimum_column_width=32;
    require(header.set_configuration(c),"Valid header columns were rejected");
    frame=header.layout(300,0);
    require(frame&&frame->columns.size()==3&&frame->columns[0].extent==100&&frame->columns[1].extent==100,
            "Mixed fixed and flexible widths were not resolved");
    c.fallback_column_width=80;c.fit_flexible_columns=false;
    require(header.set_configuration(c),"Valid fallback width was rejected");
    frame=header.layout(100,70);
    require(frame&&frame->x==70&&frame->columns.size()==2&&frame->columns[0].offset==-70&&frame->columns[1].offset==30,
            "Scrolled header did not keep the viewport column geometry");
    require(header.hit(*frame,5)==0&&header.hit(*frame,50)==1&&!header.hit(*frame,100),"Header hit testing ignored clipped columns");
    c.columns[1].enabled=false;require(header.set_configuration(c),"Disabled column was rejected");
    frame=header.layout(100,70);require(frame&&header.hit(*frame,5)==0&&!header.hit(*frame,50),"Disabled header accepted activation");
    const auto previous=header.configuration();c.columns[2].key="a";
    require(!header.set_configuration(c)&&header.configuration()==previous,"Duplicate column changed the committed header");
    c=previous;c.minimum_column_width=0;require(!header.set_configuration(c),"Invalid minimum width was accepted");
    c=previous;c.columns[0].width=std::numeric_limits<double>::quiet_NaN();require(!header.set_configuration(c),"Invalid column width was accepted");
    c=previous;c.columns[0].title=std::string("bad\xC0\x80",5);require(!header.set_configuration(c),"Invalid UTF-8 header text was accepted");
    require(!header.layout(-1,0)&&!header.layout(100,-1),"Invalid header viewport was accepted");
}
