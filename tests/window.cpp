#include <LVUI/Compiler.h>
#include <iostream>
#include <stdexcept>
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::string generate(std::string_view source,std::string framework="iiGUI"){
    auto code=lvui::compile_native(source,{.framework=framework});
    if(!code)throw std::runtime_error(code.error().message);
    return *code;
}
void dynamic(){
    auto code=generate("import vm Model\nWindow(title: vm.title, width: vm.width, height: vm.height, minimumWidth: vm.minimumWidth, minimumHeight: vm.minimumHeight, backgroundColor: vm.color, fontFamily: vm.font) { InputField() }");
    check(code.find("window_properties()")!=code.npos&&code.find("target.update(value)")!=code.npos,"Window inputs did not use a retained native target");
    for(auto name:{"title","width","height","minimumWidth","minimumHeight","color","font"})
        check(code.find(std::string("backend_0.")+name)!=code.npos,"A Window input lost its native dependency");
}
void accent(){
    auto code=generate("import vm Model\nWindow(accent: vm.accent, backgroundColor: vm.color) { Label() }","LVRS");
    check(code.find("window_accent<lvrs::Accent>")!=code.npos,"Native LVRS accent values were not type checked");
}
void constants(){
    auto code=generate("Window(title: \"Static\", width: 700, height: 500, minimumWidth: 400, minimumHeight: 300, fontFamily: \"Pretendard\", backgroundColor: \"#151515\") { Label() }");
    check(code.find("LVUI/")==code.npos&&code.find("window_properties()")==code.npos,"Constant Window retained a runtime binding");
    check(code.find(".title = \"Static\"")!=code.npos&&code.find(".minimum_size = {400, 300}")!=code.npos,"Static host options changed");
}
void diagnostics(){
    for(auto source:{"var width: String = \"700\"\nWindow(width: width) { Label() }",
        "var title: Int = 4\nWindow(title: title) { Label() }",
        "var color: Bool = true\nWindow(backgroundColor: color) { Label() }",
        "var font: Bool = true\nWindow(fontFamily: font) { Label() }",
        "Window(fontFamily: \"\") { Label() }", "Window(width: 200, minimumWidth: 300) { Label() }"}){
        auto code=lvui::compile_native(source);
        check(!code&&code.error().line>0&&code.error().column>0,"Invalid Window input lost its source diagnostic");
    }
}
int main(int argc,char** argv){try{std::string name=argc>1?argv[1]:"dynamic";if(name=="dynamic")dynamic();else if(name=="accent")accent();else if(name=="constants")constants();else if(name=="diagnostics")diagnostics();else throw std::runtime_error("Unknown Window test");std::cout<<"Native Window "<<name<<" passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
