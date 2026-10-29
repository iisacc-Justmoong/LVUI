#include <LVUI/Compiler.h>
#include <iostream>
#include <stdexcept>
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
std::string compile(std::string_view source){auto r=lvui::compile_native(source);if(!r)throw std::runtime_error(r.error().message);return *r;}
void inputs(){
    check(!compile("var ink: Color = .window\nWindow(backgroundColor: ink) { Label() }").empty(),"A dynamic Window Color was rejected");
    auto code=compile("let Paint(ink: Color = .primary): View = Label(color: ink)\nWindow { VStack { Paint()\nPaint(ink: Color(0,1,0,1)) } }");
    check(code.find(".color = ii::gui::WidgetColor{")!=code.npos,"Typed Color default did not generate native color");
    auto nested=compile("import vm Backend\nlet Inner(ink: Color): View = Label(color: ink)\nlet Outer(ink: Color): View = Inner(ink: ink)\nWindow { Outer(ink: vm.color) }");
    check(nested.find("set_label_color")!=nested.npos,"Forwarded reactive Color lost its native setter");
    const auto link=lvui::compile_native("import vm Backend\nWindow { Link(linkColor: vm.ink) }",{.framework="iiGUI"});
    check(link&&link->find(".palette.link = lvui::native::link_color(lvui::native::color<ii::gui::WidgetColor>(")!=link->npos,"A Link Color input bypassed native color normalization");
}
void state(){
    auto code=compile("var ink: Color = .primary\nlet copy: Color = ink\n@Static let initial: Color = copy\nWindow { VStack { Label(color: copy)\nLabel(color: initial) } }");
    check(code.find("set_ink(ii::gui::WidgetColor value)")!=code.npos&&code.find("fixed_initial")!=code.npos,"Color state or Static storage was omitted");
    check(code.find("!scope_->active()")!=code.npos,"A generated scalar setter bypassed the native activation gate");
    auto dynamic=compile("import vm Backend\nlet ink: Color = vm.enabled ? .primary : .window\nlet channels: Color = Color(vm.red,.2,.3,1)\nWindow { Label(color: composite(ink, channels)) }");
    check(dynamic.find("composite_color<ii::gui::WidgetColor>")!=dynamic.npos,"Dynamic Color constructors or composition were not native");
    check(!compile("var ink: Color = .primary\nlet alpha: Double = ink.alpha\nWindow { Label(text: alpha) }").empty(),"Color channel read lost its reactive value owner");
    check(!compile("let ink: Color = Color(.2,.3,.4,.5)\nWindow { Slider(value: ink.alpha) }").empty(),"Constant Color channel read was rejected");
}
void capture(){
    auto code=compile("import vm Parent\nlet Panel(ink: Color): View = VStack { import vm Child\nLabel(color: ink) }\nWindow { Panel(ink: vm.color) }");
    check(code.find("parent->backend_0.color")!=code.npos,"Color parameter changed its receiver under a shadowed import");
    auto list=compile("import vm Backend\nlet Paint(ink: Color): [View] = [Label(color: ink)]\nWindow { VStack { Paint(ink: vm.color) } }");
    check(list.find("set_label_color")!=list.npos,"Color in a View sequence was not reactive");
}
void diagnostics(){
    for(auto source:{"let Paint(ink: Color): View = Label(color: ink)\nWindow { Paint(ink: 2) }","let x: Color = Color(2,0,0,1)\nWindow { Label() }","var n: Int = 2\nlet x: Color = n\nWindow { Label() }","let Paint(ink: Color): View = Label(color: ink)\nWindow { Paint() }","Window(backgroundColor: true) { Label() }"}){
        auto r=lvui::compile_native(source);check(!r&&r.error().line>0,"Invalid Color input was accepted or lost its source position");
    }
}
int main(int argc,char** argv){try{std::string s=argc>1?argv[1]:"inputs";if(s=="inputs")inputs();else if(s=="state")state();else if(s=="capture")capture();else if(s=="diagnostics")diagnostics();else throw std::runtime_error("Unknown color compiler test");std::cout<<"Color compiler "<<s<<" passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
