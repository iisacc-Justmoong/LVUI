#include <LVUI/Compiler.h>
#include <iostream>
#include <stdexcept>
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::string compile(std::string_view source){auto result=lvui::compile_native(source);if(!result)throw std::runtime_error(result.error().message);return *result;}
void receivers(){
    for(auto source:{
        "import vm Model\nWindow { PushButton(vm.selected.act) }",
        "import vm Model\nlet current = vm.selected\nWindow { PushButton(current.act) }",
        "import class Domain.Item\nimport vm Model\nlet Action(item: Item): View = PushButton(item.act)\nWindow { Action(item: vm.selected) }"}){
        auto code=compile(source);check(code.find("Methods::reference(")!=code.npos&&code.find("returned_callback(")!=code.npos,"An object method reference did not select a scoped native callable");
    }
}
void postfix(){
    for(auto source:{"import vm Model\nWindow { PushButton((vm.selected).act) }",
        "import vm Model\nWindow { PushButton(vm.rows[0].act) }",
        "import vm Model\nWindow { PushButton(vm.current().act) }"})
        check(compile(source).find("Methods::reference(")!=std::string::npos,"A postfix method reference lost its receiver");
}
void typed(){
    for(auto source:{
        "import vm Model\nlet render: () -> String = vm.selected.describe\nWindow { Label(text: render()) }",
        "import vm Model\n@Static let run: () -> Void = vm.selected.act\nWindow { PushButton(run) }",
        "import vm Model\nvar run: () -> Void = vm.selected.act\nWindow { PushButton(run) }",
        "import vm Model\nlet Action(run: () -> Void, again: () -> Void = run): View = VStack { PushButton(run)\nPushButton(again) }\nWindow { Action(run: vm.selected.act) }"})
        check(compile(source).find("Methods::reference(")!=std::string::npos,"A typed method reference lost native value selection");
}
void factories(){
    for(auto source:{"import vm Model\nWindow { PushButton(vm.selected.makeHandler()) }",
        "import vm Model\nWindow { PushButton(vm.rows[0].makeHandler()) }"})
        check(compile(source).find("returned_callback(")!=std::string::npos,"A nested callable factory was rejected");
}
void diagnostics(){
    for(auto source:{"Window { PushButton(missing.selected.act) }","import vm Model\nWindow { PushButton(vm.selected._private) }",
        "import vm Model\nWindow { PushButton(vm.rows[0].__private) }"}){
        auto result=lvui::compile_native(source);check(!result&&result.error().line>0&&result.error().column>0,"An invalid deferred receiver lost its source diagnostic");
    }
}
int main(int argc,char** argv){try{std::string name=argc>1?argv[1]:"receivers";if(name=="receivers")receivers();else if(name=="postfix")postfix();else if(name=="typed")typed();else if(name=="factories")factories();else if(name=="diagnostics")diagnostics();else throw std::runtime_error("Unknown object method reference test");std::cout<<"Object method reference "<<name<<" passed\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
