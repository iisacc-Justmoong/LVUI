#include <LVUI/Compiler.h>
#include <LVUI/Language.h>
#include <iostream>
#include <stdexcept>
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::string compile(std::string_view source,std::string framework="iiGUI"){
    auto result=lvui::compile_native(source,{.name="SequenceTest",.framework=std::move(framework)});
    if(!result)throw std::runtime_error(result.error().message);return *result;
}
void syntax(){
    auto parsed=lvui::parse_view("let Pair(title: String): [View] = [Label(text: title), Label()]\nlet Box(content: [View] = []): View = HStack { content() }\nWindow { Box(content: Pair(title: \"One\")) }");
    check(parsed&&parsed->bindings().front().type=="[View]"&&parsed->bindings().front().parameters&&parsed->bindings().at(1).parameters->front().type=="[View]",
          "A typed View sequence lost its declaration or parameter metadata");
    auto inferred=lvui::parse_view("let parts: [View] = [Label()]\nlet copy = parts\nWindow { VStack { copy() } }");
    check(inferred&&inferred->bindings().at(1).type=="[View]","A View sequence alias lost its type");
    auto nested=lvui::parse_view("let parts = [[Label(text: \"Nested\")]]\nWindow { VStack { parts() } }");
    check(nested&&nested->bindings().front().type=="[View]","A nested visual list did not infer View sequence type");
    check(!lvui::parse_view("var parts: [View] = []\nWindow { Label() }"),"A mutable View sequence silently bypassed replacement ownership");
    check(!lvui::parse_view("@Static let parts: [View] = []\nWindow { Label() }"),"A Static View sequence silently claimed deep snapshots");
}
void composition(){
    const auto code=compile("let Pair(title: String): [View] = [Label(id: \"first\", text: title), Label(id: \"second\", text: title)]\nlet Box(content: [View] = []): View = HStack(id: \"container\") { content() }\nWindow { Box(content: Pair(title: \"Native\")) }");
    check(code.find("Native")!=code.npos&&code.find("widgets.hstack(")!=code.npos,"A View sequence did not emit its children");
    check(code.find("widgets.vstack(")==code.npos&&code.find("widgets.content_host(")==code.npos,"A static View sequence added an unwanted layout or dynamic host");
    const auto nested=compile("let item: View = Label(text: \"One\")\nlet first: [View] = [item, Label(text: \"Two\")]\nlet second: [View] = [first, Label(text: \"Three\")]\nlet copy = second\nWindow { VStack { copy()\ncopy() } }");
    check(nested.find("Three")!=nested.npos,"Nested sequences or repeated aliases were rejected");
    check(!compile("let Empty(content: [View] = []): View = VStack { content() }\nWindow { Empty() }").empty(),"An empty sequence default was rejected");
    check(!compile("let empty: [View] = []\nWindow { empty()\nLabel(text: \"Only root\") }").empty(),"An empty sequence consumed the single root slot");
    check(!compile("import vm Backend\nlet entries = [[Label(text: \"Nested\")], if(vm.ready) { Label(text: \"Conditional\") }]\nWindow { VStack { entries() } }").empty(),"An inline conditional View could not be used in a nested sequence");
}
void typing(){
    const auto tabs=compile("let entries: [View] = [Tab(iconName: \"generaladd\", text: \"One\"), Tab(iconName: \"generaladd\", text: \"Two\")]\nlet Bar(content: [View]): View = TabBar { content() }\nWindow { Bar(content: entries) }","iiGUI");
    check(tabs.find("lvui::native::NativeTab::create(")!=tabs.npos,"Typed Tab children could not pass through a sequence");
    for(const auto source:{
        "let entries: [View] = [Label()]\nWindow { TabBar { entries() } }",
        "let entries: [View] = [Label(), Label()]\nWindow { Modal { entries() } }",
        "let entries: [View] = [Label()]\nWindow { PushButton { entries() } }",
        "let entries: [View] = [Label()]\nWindow { Menu { entries() } }"})
        check(!lvui::compile_native(source,{.framework="iiGUI"}),"A View sequence bypassed its receiving component contract");
    check(!lvui::compile_native("let entries: [View] = [Label(), Label()]\nWindow { entries() }"),"A sequence bypassed the single window root contract");
}
void capture(){
    const auto code=compile("import vm Backend\nlet entries: [View] = [Label(text: vm.title), PushButton(vm.increment)]\nlet Wrapper(content: [View]): View = VStack { import vm Private\ncontent() }\nWindow { Wrapper(content: entries) }");
    check(code.find("scope->parent->backend_0.title")!=code.npos&&code.find("object).increment(std::forward")!=code.npos,
          "A View sequence lost its definition environment under a shadowing import");
    const auto parameters=compile("import vm Backend\nlet Heading(title: String): View = Label(text: title)\nlet Pair(content: View, title: String): [View] = [content(title: title), Label(text: title)]\nlet Forward(body: [View]): View = VStack { body() }\nWindow { Forward(body: Pair(content: Heading, title: vm.title)) }");
    check(parameters.find("backend_0.title")!=parameters.npos,"A parameterized sequence lost its View factory or reactive argument");
    const auto function=compile("import vm Backend\nlet Cell(title: String): View = Label(text: title)\nlet Pair(format: (Int) -> String = vm.makeFormatter()): [View] = [Cell(title: format(vm.count))]\nWindow { VStack { Pair() } }");
    check(function.find("read_function(context")!=function.npos,"Structural inspection tried to use an unmaterialized native function field");
    const auto override=compile("let Default: [View] = [Label(text: \"Default\")]\nlet Frame(content: [View]): View = VStack { content() }\nlet alias: View = Frame(content: Default)\nWindow { let Local: [View] = [Label(text: \"Caller\"), Label(text: \"Second\")]\nalias(content: Local) }");
    check(override.find("Caller")!=override.npos&&override.find("Second")!=override.npos,"A named View sequence override lost its caller definition");
    check(!compile("let Frame(onContent: [View]): View = VStack { onContent() }\nlet alias: View = Frame(onContent: [Label()])\nWindow { alias(onContent: []) }").empty(),
          "An empty View sequence was mistaken for an event because of its parameter name");
}
void diagnostics(){
    for(const auto source:{
        "let bad: [View] = [1]\nWindow { Label() }",
        "let scalar: String = \"bad\"\nlet bad: [View] = [scalar]\nWindow { Label() }",
        "let entries: [View] = [Label()]\nlet bad: View = entries\nWindow { VStack { bad() } }",
        "let entry: View = Label()\nlet bad: [View] = entry\nWindow { VStack { bad() } }",
        "let Box(content: [View]): View = VStack { content() }\nWindow { Box(content: Label()) }",
        "let Box(content: View): View = VStack { content() }\nWindow { Box(content: [Label()]) }",
        "let entries: [View] = [Label()]\nWindow { VStack { entries(text: \"bad\") } }",
        "let entries: [View] = [Label()]\nWindow { VStack { entries() { Label() } } }",
        "let entries: [View] = [Label(), Label()]\nimport vm Backend\nWindow { VStack { if(vm.ready) { entries() } } }"}){
        auto result=lvui::compile_native(source);check(!result&&result.error().line>0,"An invalid sequence was accepted or lacked a source position");
    }
    auto empty=lvui::compile_native("let Empty(count: Int): [View] = []\nWindow { Modal { Empty(count: \"bad\") } }",{.framework="iiGUI"});
    check(!empty&&empty.error().message.find("scalar type")!=std::string::npos,
          "An empty popup sequence skipped its parameter type validation");
    check(!compile("let Empty(count: Int): [View] = []\nWindow { Modal { Empty(count: 1) } }","iiGUI").empty(),
          "A valid empty popup sequence was rejected");
}
void limits(){
    std::string source="let part0: [View] = []\n";
    for(int i=1;i<19;++i)source+="let part"+std::to_string(i)+": [View] = [part"+std::to_string(i-1)+", part"+std::to_string(i-1)+"]\n";
    source+="Window { VStack { part18() } }\n";
    auto result=lvui::compile_native(source);check(!result&&result.error().message.find("limit")!=std::string::npos,"An empty sequence expansion could grow without a finite limit");
}
}
int main(int argc,char** argv){try{const std::string name=argc>1?argv[1]:"syntax";
    if(name=="syntax")syntax();else if(name=="composition")composition();else if(name=="typing")typing();else if(name=="capture")capture();else if(name=="diagnostics")diagnostics();else if(name=="limits")limits();else throw std::runtime_error("Unknown View sequence test");
    std::cout<<"View sequence "<<name<<" passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
