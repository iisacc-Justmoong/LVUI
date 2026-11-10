#include <LVUI/NativeCodeEditor.h>
#include <concepts>
template<class P>struct Host{using Policy=P;};
int main(){using B=lvui::native::CodeEditorBinding<Host>;static_assert(std::same_as<typename B::Policy::Input,lvui::native::CodeEditorInput>);}
