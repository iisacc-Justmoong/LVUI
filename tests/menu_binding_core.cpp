#include <LVUI/NativeMenu.h>
#include <concepts>
struct Widget{};struct Snapshot{};
template<class P>struct Model{using Policy=P;};template<class P>struct Host{using Policy=P;};
int main(){
    using Binding=lvui::native::MenuBinding<Model,Host,Widget,Snapshot>;
    static_assert(std::same_as<Binding::Policy::Model,Model<lvui::MenuModelPolicy>>);
    static_assert(std::same_as<Binding::Policy::Input,lvui::native::MenuInput>);
    static_assert(std::same_as<Binding::Policy::ItemPort,lvui::native::MenuItemPortValue<Widget,Snapshot>>);
    static_assert(std::same_as<Binding::Policy::SectionPort,lvui::native::MenuSectionPortValue<Widget,Snapshot>>);
}
