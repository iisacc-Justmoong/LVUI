#include <LVUI/NativeTableViewGroup.h>
#include <stdexcept>
struct Builder{};struct Rect{double x=0,y=0,width=0,height=0;};
int main(){
    using Policy=lvui::native::TableViewGroupPolicy<Builder,Rect>;
    lvui::native::TableViewSnapshot value;value.text="Initial";
    auto context=Policy::make_context(value);int updates=0;
    auto subscription=context->observe([&](const auto& next,std::uint64_t){if(next.text=="Changed")++updates;});
    value.text="Changed";Policy::publish(*context,value);
    if(updates!=1||context->get().text!="Changed")throw std::runtime_error("context publication failed");
    Policy::close(*context);bool closed=false;try{(void)context->get();}catch(const std::logic_error&){closed=true;}
    if(!closed)throw std::runtime_error("context did not close");
}
