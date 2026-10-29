#include <LVUI/TableRowView.h>

#include <cmath>
#include <limits>
#include <stdexcept>

namespace {void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}}
int main(){
    using namespace lvui;
    TableRowView row;auto c=row.configuration();
    require(c.width==717&&c.cell_width==234&&c.cell_height==24&&c.cells.size()==3,"Table row defaults changed");
    auto frame=row.layout(717,0);
    require(frame&&frame->cells.size()==3&&std::abs(frame->spacing-7.5)<1e-8&&frame->cells[1].x==241.5&&frame->cells[2].x==483,
            "Row did not distribute available space between cells");
    c.width=300;require(row.set_configuration(c),"Valid row was rejected");frame=row.layout(300,0);
    require(frame&&frame->spacing==0&&frame->cells.size()==2&&frame->cells[1].x==234,"Narrow row did not clip its visible cells");
    frame=row.layout(300,300);
    require(frame&&frame->offset==300&&frame->cells.size()==2&&frame->cells[0].index==1&&frame->cells[0].x==-66,
            "Row scrolling did not preserve full cell geometry");
    const auto previous=row.configuration();c.cells[1].key=c.cells[0].key;
    require(!row.set_configuration(c)&&row.configuration()==previous,"Duplicate cell keys changed the row");
    c=previous;c.cell_width=std::numeric_limits<double>::infinity();require(!row.set_configuration(c),"Invalid cell width was accepted");
    c=previous;c.cells[0].value.text=std::string("x\0y",3);require(!row.set_configuration(c),"Invalid cell text was accepted");
    c=previous;c.cells.clear();require(row.set_configuration(c)&&row.layout(100,0)->cells.empty(),"Empty row was rejected");
    require(!row.layout(-1,0)&&!row.layout(100,-1),"Invalid row viewport was accepted");
}
