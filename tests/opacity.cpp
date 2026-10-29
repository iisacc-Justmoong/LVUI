#include <LVUI/Compiler.h>
#include <iostream>
#include <stdexcept>
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::string compile(std::string_view source,std::string framework="iiGUI"){
    auto result=lvui::compile_native(source,{.framework=framework});
    if(!result)throw std::runtime_error(std::to_string(result.error().line)+":"+std::to_string(result.error().column)+" "+result.error().message);
    return *result;
}
void values(){auto code=compile("Window { VStack(opacity: 0.5) {\nLabel(text: \"Half\", opacity: 0)\nPushButton(text: \"Action\", opacity: 1)\n} }");for(auto value:{"set_opacity(0.5)","set_opacity(0)","set_opacity(1)"})check(code.find(value)!=code.npos,"A constant opacity was dropped");}
void reactive(){auto code=compile("import vm Model\nWindow { VStack(opacity: vm.alpha) { Label(opacity: vm.amount()) } }");check(code.find("set_opacity(value)")!=code.npos&&code.find(", 0, 1)")!=code.npos&&code.find(".amount()")!=code.npos,"Reactive opacity bypassed the native binding or C++ call boundary");}
void shared(){auto code=compile("import vm Model\nlet Content(alpha: Double): View = HStack(opacity: alpha) { Label(opacity: alpha) }\nWindow { Content(alpha: vm.amount()) }");const auto call=code.find(".amount()");check(code.find("set_opacity(value)")!=code.npos&&call!=code.npos&&code.find(".amount()",call+1)==code.npos,"Opacity evaluated a component input separately for each consumer");}
void fixed(){auto code=compile("import vm Model\n@Static let initial: Double = vm.alpha\nWindow { Label(opacity: initial) }");check(code.find("fixed_initial")!=code.npos&&code.find("set_opacity")!=code.npos,"Static opacity lost its activation snapshot");}
void components(){for(auto view:{"Label()","PushButton()","InputField()","CheckBox()","Slider()","ProgressBar()","HierarchyItem()","HierarchyToolbar()","ToolbarButton()","HStack()","ZStack()"}){
    auto source=std::string(view);source.insert(source.size()-1,"opacity: 0.4");auto code=compile("Window { "+source+" }");check(code.find("set_opacity(0.4)")!=code.npos,"An original native component omitted inherited opacity");
}}
void framework(){auto code=compile("import vm Model\nWindow { VStack(opacity: vm.alpha) { Label(opacity: 0.4) } }","iiGUI");check(code.find("set_opacity(value)")!=code.npos&&code.find("set_opacity(0.4)")!=code.npos&&code.find("lvrs::")==code.npos,"Common opacity requires an LVRS runtime");}
void delegate(){
    auto code=compile("import vm Model\nWindow { HierarchyList(opacity: vm.alpha) }");
    check(code.find("set_opacity(value)")!=code.npos, "Hierarchy opacity lost native binding");
    auto unsupported=lvui::compile_native("import vm Model\nWindow { HierarchyList(itemDelegate: HierarchyItem()) }");
    check(!unsupported&&unsupported.error().line>0, "Unsupported hierarchy delegate lacked a diagnostic");
}
void diagnostics(){for(auto source:{"Window { Label(opacity: -0.1) }","Window { Label(opacity: 1.1) }","Window { Label(opacity: true) }","Window { Label(opacity: \"half\") }","Window(opacity: 0.5) {}","let alpha: String = \"half\"\nWindow { Label(opacity: alpha) }"}){auto result=lvui::compile_native(source,{.framework="iiGUI"});check(!result&&result.error().line>0,"Invalid opacity escaped its source diagnostic");}}
}
int main(int argc,char** argv){try{const std::string name=argc>1?argv[1]:"values";if(name=="values")values();else if(name=="reactive")reactive();else if(name=="shared")shared();else if(name=="fixed")fixed();else if(name=="components")components();else if(name=="framework")framework();else if(name=="delegate")delegate();else if(name=="diagnostics")diagnostics();else throw std::runtime_error("Unknown opacity compiler case");std::cout<<"Opacity compiler "<<name<<" passed\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
