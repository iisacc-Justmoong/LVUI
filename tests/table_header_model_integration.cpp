#include <iiGUI/TableViewport.h>
#include <LVUI/TableHeader.h>
#include <iiGUI/TableHeaderModel.h>
using TableHeader=lvui::TableHeaderModelBinding<ii::gui::BasicTableHeader,lvui::TableViewportBinding<ii::gui::BasicTableViewportSource>>;

#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
}
int main(){
    using namespace lvui;
    TableHeader header;auto c=header.configuration();
    require(c.width==717&&c.row_height==24&&c.columns.size()==3&&c.default_leading_icon=="generalchevronDown"&&
            c.default_trailing_icon.empty()&&c.icon_size==18&&c.icon_spacing==7,"Header defaults changed");
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
    c=previous;c.sort=TableHeaderSort{"a",TableSortOrder::ascending};
    require(header.set_configuration(c)&&header.configuration().sort==c.sort,"Valid sort indication was rejected");
    const auto sorted=header.configuration();c.sort->column="missing";
    require(!header.set_configuration(c)&&header.configuration()==sorted,"Unknown sort column replaced the committed header");
    c=sorted;c.sort->order=static_cast<TableSortOrder>(99);
    require(!header.set_configuration(c)&&header.configuration()==sorted,"Invalid sort direction replaced the committed header");
    c=sorted;c.column_appearance={{.column="a",.padding=3.,.leading_icon="",.trailing_icon="generalchevronUp"}};
    require(header.set_configuration(c),"Valid column presentation was rejected");
    const auto styled=header.configuration();
    c.column_appearance.push_back(c.column_appearance.front());
    require(!header.set_configuration(c)&&header.configuration()==styled,"Duplicate presentation replaced the committed header");
    c=styled;c.column_appearance[0].column="missing";require(!header.set_configuration(c),"Unknown styled column was accepted");
    c=styled;c.column_appearance[0].padding=-1;require(!header.set_configuration(c),"Negative column padding was accepted");
    c=styled;c.column_appearance[0].padding=std::numeric_limits<double>::quiet_NaN();require(!header.set_configuration(c),"NaN column padding was accepted");
    c=styled;c.default_leading_icon=std::string("bad\xC0\x80",5);require(!header.set_configuration(c),"Invalid icon name was accepted");
    c=styled;c.icon_size=.5;require(!header.set_configuration(c),"Fractional subpixel icon was accepted");
    c=styled;c.icon_spacing=-1;require(!header.set_configuration(c),"Negative icon spacing was accepted");
    TableHeaderPresentation pending;pending.column_appearance={{.column="future",.padding=4.,.leading_icon="generalchevronDown"}};
    require(valid_table_header_presentation(pending),"A future full-table column style was rejected");
    pending.column_appearance.push_back(pending.column_appearance.front());
    require(!valid_table_header_presentation(pending),"Duplicate full-table styles were accepted");
    require(!header.layout(-1,0)&&!header.layout(100,-1),"Invalid header viewport was accepted");
}
