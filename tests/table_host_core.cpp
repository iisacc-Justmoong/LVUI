#include <LVUI/NativeTable.h>
#include <concepts>
struct Field{};struct Menu{};struct Item{};struct Header{};struct Group{};struct Builder{};struct Rect{};struct Viewport{};struct Image{};template<class P>struct Engine{using Policy=P;};
int main(){using E=lvui::native::TableBinding<Engine,Field,Menu,Item,Header,Group,Builder,Rect,Image,Viewport>;static_assert(std::same_as<typename E::Policy::Input,lvui::native::TableInput>);}
