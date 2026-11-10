#include <LVUI/TableCellItem.h>
#include <LVUI/TableRowView.h>
#include <concepts>
template<class P>struct Cell{using Policy=P;};template<class P>struct Row{using Policy=P;};
int main(){using C=lvui::TableCellItemModelBinding<Cell>;using R=lvui::TableRowViewModelBinding<Row,C>;
 static_assert(std::same_as<typename C::Policy::Configuration,lvui::TableCellItemConfiguration>);
 static_assert(std::same_as<typename R::Policy::CellModel,C>);}
