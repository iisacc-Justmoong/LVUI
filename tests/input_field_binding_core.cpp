#include <LVUI/NativeInputField.h>
#include <concepts>
struct Affinity{};template<class P>struct Model{using Policy=P;};template<class P>struct Material{using Policy=P;};template<class P>struct Host{using Policy=P;};
int main(){using B=lvui::native::InputFieldBinding<Host,Model,Material,Affinity>;
 static_assert(std::same_as<B::Policy::TextInput,Model<lvui::TextInputModelPolicy>>);
 static_assert(std::same_as<B::Policy::Configuration,lvui::InputFieldConfiguration>);
 static_assert(std::same_as<B::Policy::Snapshot,lvui::native::NativeInputFieldSnapshotValue<Affinity>>);}
