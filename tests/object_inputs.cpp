#include <LVUI/Compiler.h>
#include <iostream>
#include <stdexcept>
void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
std::string generate(std::string_view source){auto code=lvui::compile_native(source);if(!code)throw std::runtime_error(code.error().message);return *code;}
void values(){
    auto code=generate("import class Domain.Record\nimport vm Model\nlet Details(item: Record): View = VStack { Label(text: item.title)\nLabel(text: item.count) }\nWindow { Details(item: vm.record) }");
    check(code.find("object_input<::Domain::Record>")!=code.npos&&code.find("ReadOnlyValue<input_type_")!=code.npos,
        "A named C++ value input did not receive typed activation storage");
}
void forwarding(){
    auto code=generate("import class Domain.Record\nimport vm Model\nlet Leaf(item: Record): View = Label(text: item.title)\nlet Pair(item: Record, again: Record = item): View = VStack { Leaf(item: item)\nLeaf(item: again) }\nWindow { Pair(item: vm.makeRecord(vm.count)) }");
    check(code.find("component_input(scope")!=code.npos&&code.find("object_input<::Domain::Record>")!=code.npos,
        "A forwarded native input lost its shared caller or dependent default");
}
void roots(){
    for(auto source:{
        "import class Domain.Record\nimport vm Model\nlet App(item: Record): View = Window { Label(text: item.title) }\nApp(item: vm.record)",
        "import class Domain.Record\nimport vm Model\nlet Maybe(item: Record): View = if (vm.visible) { Label(text: item.title) }\nWindow { Maybe(item: vm.record) }",
        "import class Domain.Record\nimport vm Model\nlet Row(item: Record): View = Label(text: item.title)\nWindow { VStack { for (row in vm.rows, key: row.id) { Row(item: row) } } }"})
        check(!generate(source).empty(),"A native input lost root, conditional or row composition");
}
void diagnostics(){
    for(auto source:{
        "let Details(item: Missing): View = Label()\nWindow { Details(item: 3) }",
        "import class Domain.Record\nlet Details(item: Record): View = Label()\nWindow { Details() }",
        "import class Domain.Record\nlet Details(item: Record): View = Label()\nWindow { Details(item: unknown) }",
        "import class Domain.Record\nimport vm Wrong\nlet Details(item: Record): View = Label()\nWindow { Details(item: vm) }"}){
        auto code=lvui::compile_native(source);check(!code&&code.error().line>0&&code.error().column>0,"An invalid native object input lost its source diagnostic");
    }
}
int main(int argc,char** argv){try{std::string name=argc>1?argv[1]:"values";if(name=="values")values();else if(name=="forwarding")forwarding();else if(name=="roots")roots();else if(name=="diagnostics")diagnostics();else throw std::runtime_error("Unknown native object input test");std::cout<<"Native object inputs "<<name<<" passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
