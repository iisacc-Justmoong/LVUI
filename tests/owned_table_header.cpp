#include <LVUI/TableHeader.h>

#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
}
int main(){
    using namespace lvui;
    TableHeaderPresentation pending;pending.column_appearance={{.column="future",.padding=4.,.leading_icon="generalchevronDown"}};
    require(valid_table_header_presentation(pending),"A future full-table column style was rejected");
    pending.column_appearance.push_back(pending.column_appearance.front());
    require(!valid_table_header_presentation(pending),"Duplicate full-table styles were accepted");

}
