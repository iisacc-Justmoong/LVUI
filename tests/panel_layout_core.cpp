#include <LVUI/HierarchyPanel.h>
#include <LVUI/ListPanel.h>
#include <concepts>
template<class P>struct Model{using Policy=P;};
int main(){using H=lvui::PanelLayoutBinding<Model,lvui::HierarchyPanelConfiguration>;using L=lvui::PanelLayoutBinding<Model,lvui::ListPanelConfiguration>;static_assert(std::same_as<typename H::Policy::Configuration,lvui::HierarchyPanelConfiguration>);static_assert(std::same_as<typename L::Policy::Configuration,lvui::ListPanelConfiguration>);}
