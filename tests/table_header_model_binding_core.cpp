#include <LVUI/TableHeader.h>
#include <concepts>
struct Viewport{};
template<class P>struct Model{using Policy=P;};
int main(){using B=lvui::TableHeaderModelBinding<Model,Viewport>;static_assert(std::same_as<typename B::Policy::Configuration,lvui::TableHeaderConfiguration>);}
