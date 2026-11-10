#include <LVUI/MobileNavigation.h>
#include <LVUI/MobileNavigationMotion.h>
#include <concepts>
template<class P>struct Model{using Policy=P;};
int main(){using A=lvui::MobileNavigationModelBinding<Model>;using B=lvui::MobileNavigationMotionBinding<Model>;
 static_assert(std::same_as<typename A::Policy::Configuration,lvui::MobileNavigationConfiguration>);static_assert(std::same_as<typename B::Policy::Geometry,lvui::MobileIndicatorGeometry>);}
