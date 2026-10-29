#include <LVUI/ListItemSelector.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace {void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}}
int main(){
    using namespace lvui;
    ListItemSelector selector;require(selector.display_text()=="Option"&&!selector.choose(0),"Empty selector lost its placeholder");
    ListItemSelectorConfiguration input{.options={{"Draft",true},{"Disabled",false},{"Published",true}}};
    require(selector.set_configuration(input)&&selector.display_text()=="Draft","Selector did not accept its initial selection");
    require(!selector.choose(0)&&!selector.choose(1)&&!selector.choose(-1)&&!selector.choose(3),"Invalid selection produced an edit");
    require(selector.navigate(1)==2&&selector.display_text()=="Published"&&!selector.navigate(1),"Selector navigation did not skip disabled items or stop at the end");
    require(selector.navigate(-1)==0&&!selector.navigate(-1),"Selector navigation did not stop at the start");
    input.enabled=false;require(selector.set_configuration(input)&&!selector.choose(2)&&!selector.navigate(1),"Disabled selector changed its value");
    const auto previous=selector.configuration();input.selected_index=3;require(!selector.set_configuration(input)&&selector.configuration()==previous,"Invalid index replaced the configuration");
    input=previous;input.options.resize(401);require(!selector.set_configuration(input),"Oversized selector options were accepted");
    input=previous;input.options[0].text=std::string(8193,'x');require(!selector.set_configuration(input),"Oversized selector text was accepted");
    input=previous;input.options[0].text=std::string("\xc0\x80",2);require(!selector.set_configuration(input),"Invalid UTF-8 selector text was accepted");
}
