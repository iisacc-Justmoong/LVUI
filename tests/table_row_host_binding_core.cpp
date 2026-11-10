#include <LVUI/NativeTableRow.h>
#include <concepts>
struct Model{};struct Cell{};template<class P>struct Host{using Policy=P;};
int main(){using B=lvui::native::TableRowBinding<Host,Model,Cell>;
 static_assert(std::same_as<typename B::Policy::Cell,Cell>);static_assert(std::same_as<typename B::Policy::Input,lvui::native::TableRowInput>);}
