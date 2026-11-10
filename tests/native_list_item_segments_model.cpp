#include <LVUI/ListItemSegments.h>
#include <iiGUI/ListItemSegmentsModel.h>

#include <limits>
#include <stdexcept>

namespace {void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}}
int main(){
    using namespace lvui;
    ii::gui::BasicListItemSegments<ListItemSegmentsModelPolicy> item;auto c=item.configuration();auto size=item.intrinsic_size();
    require(size.width==114&&size.height==22&&c.options.size()==2&&c.selected_index==0,"Segment defaults are not tight-fitting");
    c.options={{"Original",true},{"Disabled",false},{"Edited",true}};require(item.set_configuration(c),"Valid segments were rejected");
    require(!item.choose(1)&&item.choose(2)==2&&!item.choose(2),"Segment selection did not honor disabled and unchanged entries");
    require(item.configuration().selected_index==2,"Segment selection was not retained");
    auto invalid=item.configuration();invalid.selected_index=3;
    require(!item.set_configuration(invalid)&&item.configuration().selected_index==2,"Invalid selection replaced the model");
    invalid=item.configuration();invalid.segment_width=std::numeric_limits<double>::infinity();require(!item.set_configuration(invalid),"Invalid segment width was accepted");
    invalid=item.configuration();invalid.options[0].text=std::string("bad\xff",4);require(!item.set_configuration(invalid),"Invalid option text was accepted");
    c=item.configuration();c.options.clear();c.selected_index=0;require(item.set_configuration(c)&&item.intrinsic_size().height==0&&!item.choose(0),"Empty segments changed their state");
    c.options.resize(400);require(!item.set_configuration(c),"An oversized segment row was accepted");
}
