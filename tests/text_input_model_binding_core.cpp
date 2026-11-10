#include <LVUI/TextInput.h>
#include <concepts>
template<class P>struct Model{using Policy=P;};
int main(){using B=lvui::TextInputBinding<Model>;static_assert(std::same_as<B::Policy,lvui::TextInputModelPolicy>);}
