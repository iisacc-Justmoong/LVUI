#include <LVUI/Compiler.h>
#include <iostream>
#include <stdexcept>
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::string compile(std::string_view source){auto result=lvui::compile_native(source);if(!result)throw std::runtime_error(result.error().message);return *result;}
void receivers(){
    for(auto source:{
        "import vm Model\nWindow { Label(text: vm.selected.format(vm.amount)) }",
        "import vm Model\nlet current = vm.selected\nWindow { Label(text: current.format(vm.amount)) }",
        "import class Domain.Item\nimport vm Model\nlet Details(item: Item): View = Label(text: item.format(vm.amount))\nWindow { Details(item: vm.selected) }"}){
        auto code=compile(source);check(code.find("NativeMethods.h")!=code.npos&&code.find("Methods::query(context")!=code.npos,"An object method did not use a tracked native C++ query");
    }
}
void postfix(){
    for(auto source:{
        "import vm Model\nWindow { Label(text: (vm.selected).format(vm.amount)) }",
        "import vm Model\nWindow { Label(text: vm.rows[0].format(vm.amount)) }",
        "import vm Model\nWindow { Label(text: vm.selected.record().count) }"})check(!compile(source).empty(),"A postfix native method query lost its C++ receiver");
}
void tracking(){
    auto code=compile("import vm Model\nlet render: (Int) -> String = vm.format\nWindow { VStack { Label(text: vm.format(vm.amount))\nLabel(text: render(vm.amount)) } }");
    check(code.find("Methods::evaluate(context")!=code.npos&&code.find("NativeMethods.h")!=code.npos,"Direct methods and typed function calls did not collect internal C++ reads");
    check(!compile("import vm Model\nimport peer Model\nWindow { Label(text: vm.describe(peer)) }").empty(),"A native method rejected a scope-owned backend object argument");
}
void diagnostics(){
    for(auto source:{"Window { Label(text: missing.item.run()) }","import vm Model\nWindow { Label(text: vm.item._hidden()) }","Window { Label(text: missing()) }"}){auto result=lvui::compile_native(source);check(!result&&result.error().line>0&&result.error().column>0,"An invalid native receiver lost its source diagnostic");}
}
int main(int argc,char** argv){try{std::string name=argc>1?argv[1]:"receivers";if(name=="receivers")receivers();else if(name=="postfix")postfix();else if(name=="tracking")tracking();else if(name=="diagnostics")diagnostics();else throw std::runtime_error("Unknown method query test");std::cout<<"Native method queries "<<name<<" passed\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
