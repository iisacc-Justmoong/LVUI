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
void roots(){
    auto plain=compile("VStack { var count: Int = 0\nLabel(text: count) }");
    check(plain.find("value_count")!=plain.npos&&plain.find("static ii::core::Result<void> build")!=plain.npos,"A standalone View did not produce a mountable activation");
    auto markup=compile("<VStack><Label text=\"Embedded\"/></VStack>");
    check(markup.find("Embedded")!=markup.npos,"Standalone markup View failed");
    auto extra=lvui::compile_native("<Label/><Label/>");
    check(!extra&&extra.error().message.find("exactly one root")!=std::string::npos,"Multiple markup roots lost their diagnostic");
    auto parameter=compile("let Badge(text: String): View = Label(text: text)\nBadge(text: \"Standalone\")");
    check(parameter.find("Standalone")!=parameter.npos,"Standalone parameterized View failed");
    auto branch=compile("if (true) { Label(text: \"Visible\") }");
    check(branch.find("Visible")!=branch.npos,"A conditional root was not compiled");
    check(branch.find("widgets.vstack(")==branch.npos,"A conditional root introduced an extra layout");
    auto repeated=compile("for (item in [1, 2, 3], key: item) { Label(text: item) }");
    check(repeated.find("RepeatedView_")!=repeated.npos,"A repeated root was not compiled");
    auto sequence=compile("let Items: [View] = [Label(text: \"One\"), Label(text: \"Two\")]\nItems()");
    check(sequence.find(".stack = {.spacing = 0,")!=sequence.npos,"A structural root added visual spacing");
    auto bootstrap=compile("AppBootstrapWindow { Label(text: \"Boot\") }");
    check(bootstrap.find("Boot")!=bootstrap.npos,"Bootstrap Window root was not compiled");
    auto shell=compile("AppShell(visible: true) { Label(text: \"Shell\") }");
    check(shell.find("Shell")!=shell.npos,"Visible AppShell root was not compiled");
    check(compile("<AppBootstrapWindow><Label>Boot</Label></AppBootstrapWindow>").find("Boot")!=std::string::npos,"Markup bootstrap root was not compiled");
    auto scenes=compile("Application {\nWindow { Label() }\nAppShell(id: \"later\") { Label() }\nAppBootstrapWindow(id: \"boot\", visible: false) { Label() }\n}");
    check(scenes.find("SceneDescriptor")!=scenes.npos,"Window compatibility types were not admitted as Application scenes");
    check(!lvui::compile_native("AppShell { Label() }"),"An initially hidden standalone AppShell was silently opened");
    check(!lvui::compile_native("import vm Model\nAppBootstrapWindow(visible: vm.visible) { Label() }"),"Reactive Window visibility was silently ignored");
    check(!lvui::compile_native("AppBootstrapWindow(visible: false) { Label() }"),"An initially hidden standalone bootstrap Window was opened");
    check(!lvui::compile_native("Application { AppShell { Label() } }"),"An initially hidden primary Window was accepted");
    check(!lvui::compile_native("Application { Window { Label() }\nAppShell(id: \"hidden\", visible: false, initiallyOpen: true) { Label() } }"),"Conflicting Window visibility inputs were accepted");
    check(!lvui::compile_native("AppShell(visible: 1) { Label() }"),"Non-Boolean Window visibility was accepted");
    check(!lvui::compile_native("Scene { Label() }"),"A standalone Scene was accepted");
}
void selector(){
    auto generated=compile("import vm Model\nListItemSelector(options: vm.options, currentIndex: vm.index, enabled: vm.enabled, onCurrentIndexChanged: vm.changed)");
    check(generated.find("NativeListItemSelector::create")!=generated.npos&&generated.find("ListItemSelectorInput")!=generated.npos,"Standalone selector did not use the owned selector");
    auto strings=compile("ListItemSelector(options: [\"One\", \"Two\"], currentIndex: 1)");
    check(strings.find("std::vector<std::string>")!=strings.npos,"Static selector options were not generated");
    for(auto source:{"ListItemSelector(options: [1])", "ListItemSelector(width: 1)", "ListItemSelector(currentIndex: -1)", "ListItemSelector { Label() }"})
        check(!lvui::compile_native(source),"Invalid selector declaration was accepted");
}
void diagnostics(){for(auto source:{"Window { NativeView(source: \"file.iisc\") }","Window { NativeView(width: -1) }","Window { NativeView { Label(text: \"Child\") } }","Window { NativeView(unknown: true) }"}){auto r=lvui::compile_native(source);check(!r&&r.error().line>0&&r.error().column>0,"Invalid custom visual input lost its source diagnostic");}}
}
int main(int argc,char** argv){try{std::string n=argc>1?argv[1]:"frameworks";if(n=="frameworks")frameworks();else if(n=="bindings")bindings();else if(n=="empty")empty();else if(n=="isolation")isolation();else if(n=="roots")roots();else if(n=="selector")selector();else if(n=="diagnostics")diagnostics();else throw std::runtime_error("Unknown native view compiler case");std::cout<<"Native view compiler "<<n<<" passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
