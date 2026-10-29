#include <LVUI/Compiler.h>
#include <iostream>
#include <stdexcept>
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::string compile(std::string_view source,std::string framework="iiGUI"){auto result=lvui::compile_native(source,{.framework=std::move(framework)});if(!result)throw std::runtime_error(result.error().message);return *result;}
void frameworks(){for(auto framework:{"iiGUI"}){auto result=compile("import vm Model\nWindow { NativeView(source: vm.canvas, width: 640, height: 480) }",framework);check(result.find("ii::gui::NativeView::create")!=result.npos&&result.find("#include <iiGUI/NativeView.h>")!=result.npos,"C++ visual source did not enter the native builder");}}
void bindings(){auto result=compile("import vm Model\nWindow { NativeView(source: vm.current(), width: vm.width, height: vm.height, enabled: vm.enabled, clip: vm.clip) }");check(result.find("return ii::gui::NativeViewOptions{")!=result.npos&&result.find("component.set_options(value)")!=result.npos&&result.find("lvui::native::range(")!=result.npos,"Native source and dimensions did not share a retained update");}
void empty(){auto result=compile("Window { NativeView() }");check(result.find(".source = {}")!=result.npos&&result.find(".clip = true")!=result.npos,"Native asynchronous empty state was rejected");}
void isolation(){auto result=compile("Window { Label(text: \"Basic\") }");check(result.find("#include <iiGUI/NativeView.h>")==result.npos,"An unused C++ visual source became a dependency of every view");}
void diagnostics(){for(auto source:{"Window { NativeView(source: \"file.iisc\") }","Window { NativeView(width: -1) }","Window { NativeView { Label(text: \"Child\") } }","Window { NativeView(unknown: true) }"}){auto r=lvui::compile_native(source);check(!r&&r.error().line>0&&r.error().column>0,"Invalid custom visual input lost its source diagnostic");}}
}
int main(int argc,char** argv){try{std::string n=argc>1?argv[1]:"frameworks";if(n=="frameworks")frameworks();else if(n=="bindings")bindings();else if(n=="empty")empty();else if(n=="isolation")isolation();else if(n=="diagnostics")diagnostics();else throw std::runtime_error("Unknown native view compiler case");std::cout<<"Native view compiler "<<n<<" passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
