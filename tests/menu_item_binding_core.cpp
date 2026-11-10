#include <LVUI/NativeMenuItem.h>
#include <concepts>
#include <stdexcept>
struct Image{};struct Decoder{using Image=::Image;};struct Widget{};struct Snapshot{};
template<class P>struct Model{using Policy=P;};template<class P>struct Host{using Policy=P;};
enum class State{normal,selected,inactive};enum class Direction{automatic,right,left,up,down};
int main(){
    using Binding=lvui::native::MenuItemBinding<Model,Host,Decoder,Widget,Snapshot>;
    static_assert(std::same_as<Binding::Policy::Model,Model<lvui::MenuItemModelPolicy>>);
    static_assert(std::same_as<Binding::Policy::Input,lvui::native::MenuItemInputValue<Image>>);
    static_assert(std::same_as<Binding::Policy::ItemPort,lvui::native::MenuItemPortValue<Widget,Snapshot>>);
    if(lvui::native::menu_item_state(State::selected)!=lvui::MenuEntryState::selected||
       lvui::native::menu_item_direction(Direction::down)!=lvui::MenuDirection::down||
       lvui::native::menu_item_direction(static_cast<Direction>(-1))==lvui::MenuDirection::down)
        throw std::runtime_error("Menu item binding conversion changed");
}
