#include <LVUI/TextInput.h>
#include <type_traits>
int main(){static_assert(std::is_abstract_v<lvui::TextInputBoundary>);lvui::TextInputRange range{2,3};return range.location==2&&range.length==3?0:1;}
