#include <LVUI/Compiler.h>
#include <LVUI/WindowAccent.h>
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
    auto fixed=generate("Window(accent: .purple) { PushButton(text: \"Save\") }");
    check(fixed.find(".527506")!=fixed.npos&&fixed.find(".291259")!=fixed.npos,
          "Static Window accent did not affect the generated appearance");
    auto reactive=generate("import vm Model\nWindow(accent: vm.accent) { PushButton(text: \"Save\") }");
    check(reactive.find("lvui::native::window_accent(")!=reactive.npos&&
          reactive.find("lvui::WindowAccent::purple")!=reactive.npos&&
          reactive.find("window_properties()")!=reactive.npos&&reactive.find("lvrs::")==reactive.npos,
          "Reactive Window accent did not use an independent scoped palette");
    for(const auto name:lvui::window_accent_names){
        auto code=generate("Window(accent: ."+std::string(name)+") { PushButton(text: \"Save\") }");
        check(code.find("result.primary = ii::gui::WidgetColor{")!=code.npos&&code.find("lvrs::")==code.npos,
              "A supported accent did not produce an independent appearance");
    }
    lvui::NativeCompileOptions options;
    options.style_tokens.overrides["Theme Colors/Accent/Purple/Default"]=lvui::StyleColor{.125,.25,.5,1};
    auto overridden=lvui::compile_native("Window(accent: .purple) { PushButton(text: \"Save\") }",options);
    check(overridden&&overridden->find("0.125, 0.25, 0.5, 1")!=overridden->npos,
          "Window accent ignored an application style override");
    for(auto source:{"Window(accent: .unknown) { Label() }", "Window(accent: 9) { Label() }"}){
        auto rejected=lvui::compile_native(source);
        check(!rejected&&rejected.error().line>0,"Invalid Window accent lacked a source diagnostic");
    }
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
