#include <LVUI/NativeTextEditor.h>
#include <concepts>
struct Field{};struct Affinity{};template<class P>struct Host{using Policy=P;};
int main(){using B=lvui::native::TextEditorBinding<Host,Field,Affinity>;
 static_assert(std::same_as<B::Policy::Field,Field>);
 static_assert(std::same_as<B::Policy::Configuration,lvui::TextEditorConfiguration>);
 static_assert(std::same_as<B::Policy::Snapshot,lvui::native::NativeInputFieldSnapshotValue<Affinity>>);}
