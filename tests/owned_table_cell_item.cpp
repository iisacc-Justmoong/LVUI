#include <LVUI/TableCellItem.h>

#include <limits>
#include <memory>
#include <stdexcept>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Positive final:lvui::TableCellValueValidator {
    lvui::TableResult<void> validate(std::string_view text,std::string_view)const override {
        if(text=="yes")return {};return std::unexpected(lvui::TableError{lvui::TableErrorCode::invalid,"Rejected"});
    }
};
}
int main(){
    using namespace lvui;
    TableCellItem cell;auto c=cell.configuration();
    require(c.width==234&&c.height==24&&c.padding==8&&c.text=="Text", "Cell defaults changed");
    auto geometry=cell.geometry(234,24);
    require(geometry&&geometry->text.x==8&&geometry->text.width==218&&geometry->divider.x==0&&geometry->divider.width==1,
            "Cell content spacing or divider changed");
    auto narrow=cell.geometry(3,2);
    require(narrow&&narrow->text.width>=0&&narrow->text.x+narrow->text.width<=3&&narrow->divider.y+narrow->divider.height<=2,
            "Narrow cell escaped its bounds");
    c.value_type="int";require(cell.set_configuration(c)&&cell.accept("-14")&&!cell.accept("1.5")&&!cell.accept("9223372036854775808"),
                                 "Integer validation did not check the full value and range");
    c.value_type="number";require(cell.set_configuration(c)&&cell.accept("3.25")&&cell.accept("3.25e-2")&&!cell.accept("nan")&&
                                    !cell.accept("3x")&&!cell.accept(" 3")&&!cell.accept("3 ")&&!cell.accept("1e999"),
                                    "Number validation accepted invalid values");
    c.value_type="bool";require(cell.set_configuration(c)&&cell.accept("true")&&cell.accept("false")&&!cell.accept("yes"),
                                  "Boolean validation accepted unsupported values");
    c.value_type="custom";require(cell.set_configuration(c)&&!cell.accept("yes"),"Unrecognized type bypassed validation");
    auto validator=std::make_shared<Positive>();require(cell.accept("yes",validator)&&!cell.accept("no",validator),"Replaceable validator was ignored");
    c.selected=true;c.current=true;require(cell.set_configuration(c),"Selected cell configuration was rejected");
    auto visual=cell.visual();require(visual.selected&&visual.current,"Cell selection or current indicator was lost");
    const auto previous=cell.configuration();c.width=std::numeric_limits<double>::infinity();
    require(!cell.set_configuration(c)&&cell.configuration()==previous,"Invalid cell geometry replaced committed settings");
    c=previous;c.text=std::string("x\0y",3);require(!cell.set_configuration(c),"Invalid cell text was accepted");
    require(!cell.geometry(-1,10)&&!cell.geometry(10,std::numeric_limits<double>::quiet_NaN()),"Invalid cell extent was accepted");
}
