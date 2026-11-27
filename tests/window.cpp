#include <LVUI/Compiler.h>
#include <LVUI/WindowAccent.h>
#include <iiSDKCore/Core/Text/Number.h>
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
    const auto purple=lvui::figma_defaults::accent_purple_default;
    const auto expected="result.primary = ii::gui::WidgetColor{"+ii::core::format_decimal(purple.red)+", "+
                        ii::core::format_decimal(purple.green)+", "+ii::core::format_decimal(purple.blue)+", 1}";
    check(fixed.find(expected)!=fixed.npos,
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
    auto extras=lvui::compile_native("Window(accent: .purple) { VStack {\nProgressBar(currentValue: 50)\nColorPickerButton()\nAlertButton(tone: .primary)\nToolbarButton(tone: .primary)\nAlert(primaryText: \"OK\")\n} }",options);
    if(!extras)throw std::runtime_error(extras.error().message);
    check(extras->find("value.style.fill = ii::gui::progress_color(lvui::StyleColor{0.125, 0.25, 0.5, 1})")!=extras->npos&&
          extras->find("value.configuration.palette.focus = lvui::StyleColor{0.125, 0.25, 0.5, 1}")!=extras->npos&&
          extras->find("value.palette.primary = lvui::StyleColor{0.125, 0.25, 0.5, 1}")!=extras->npos&&
          extras->find("value.configuration.palette.primary = lvui::StyleColor{0.125, 0.25, 0.5, 1}")!=extras->npos&&
          extras->find("value.configuration.primary_color = lvui::StyleColor{0.125, 0.25, 0.5, 1}")!=extras->npos,
          "Window accent extras ignored the overridden token");
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
void safe_area(){
    constexpr auto marker = "@lvui/window-safe-area";
    const auto ordinary = generate("Window { VStack { Label() } }");
    check(ordinary.find(marker)!=ordinary.npos && ordinary.find(".insets = ii::gui::Insets{16, 0, 16, 0}")!=ordinary.npos,
          "A window did not receive the default horizontal content insets");
    for(auto source:{"Window { Label() }.ignoreSafeArea", "Window { Label() }.ignoreSafeArea()",
                     "ApplicationWindow { VStack { Label() } }.ignoreSafeArea()",
                     "AppBootstrapWindow { Label() }.ignoreSafeArea()",
                     "AppShell(visible: true) { Label() }.ignoreSafeArea()",
                     "Window { VStack { Label() } }\n.ignoreSafeArea()",
                     "Application { Scene { Label() }.ignoreSafeArea() }"}) {
        const auto code = generate(source);
        check(code.find(marker)==code.npos,"ignoreSafeArea did not remove the default window insets");
    }
    const auto scenes = generate("Application { Window { Label() }\nWindow { Label() }.ignoreSafeArea }");
    const auto first = scenes.find(marker);
    check(first!=scenes.npos && scenes.find(marker,first+1)==scenes.npos,
          "Window content insets leaked between application scenes");
    for(auto source:{"Window { Label() }.ignoreSafeArea(1)", "Window { Label() }.ignoreSafeArea.ignoreSafeArea",
                     "Window { VStack { Label() }.ignoreSafeArea }", "Window { Label().ignoreSafeArea() }",
                     "Window { VStack { Label().ignoreSafeArea } }", "Window { Label().unknownModifier }",
                     "Application { Scene { Label() } }.ignoreSafeArea()"}) {
        auto code=lvui::compile_native(source);
        check(!code && code.error().line>0,"An unsupported or misplaced safe-area modifier was accepted");
    }
    lvui::NativeCompileOptions options;
    options.view_factory_only = true;
    auto embedded = lvui::compile_native("VStack { Label() }", options);
    check(embedded && embedded->find(marker)==embedded->npos,
          "An embedded view factory gained application window insets");
    auto surface_window = lvui::compile_native("Window { Label() }", options);
    check(surface_window && surface_window->find(marker)!=surface_window->npos,
          "An explicit window lost its insets on an externally supplied surface");
    const auto chrome = generate("Window(windowChrome: .custom, titleBar: HStack(id: \"chrome\", height: 32) {}) { VStack(id: \"content\") {} }");
    check(chrome.find("@lvui/window-chrome-shell")!=chrome.npos && chrome.find(marker)!=chrome.npos,
          "Custom title bar and padded content were not separated");
    check(!lvui::compile_native("Window(windowChrome: .system, titleBar: HStack()) { Label() }"),
          "A custom title bar was accepted with system chrome");
    check(!lvui::compile_native("Window(windowChrome: .custom, titleBar: 42) { Label() }"),
          "A title bar accepted a non-View value");
    const auto hidden = generate("ApplicationWindow(titleBarVisible: false) { Label() }");
    check(hidden.find(".chrome = ii::gui::WindowChrome::custom")!=hidden.npos &&
          hidden.find("@lvui/window-chrome-shell")==hidden.npos,
          "An optional system title bar could not be omitted");
    const auto shown = generate("ApplicationWindow(titleBarVisible: true) { Label() }");
    check(shown.find("WindowChrome::custom")==shown.npos,"The visible system title bar changed its default chrome");
    const auto explicitShown = generate("Window(titleBarVisible: true, windowChrome: .system) { Label() }");
    check(explicitShown.find("WindowChrome::custom")==explicitShown.npos,"A matching visible system title bar was rejected");
    const auto explicitHidden = generate("Window(titleBarVisible: false, windowChrome: .custom) { Label() }");
    check(explicitHidden.find(".chrome = ii::gui::WindowChrome::custom")!=explicitHidden.npos,
          "A matching hidden system title bar was rejected");
    const auto replaced = generate("ApplicationWindow(titleBarVisible: false, titleBar: HStack(id: \"chrome\") {}) { Label() }");
    check(replaced.find(".chrome = ii::gui::WindowChrome::custom")!=replaced.npos &&
          replaced.find("@lvui/window-chrome-shell")!=replaced.npos,
          "An omitted system title bar could not be replaced by a custom View");
    for (auto source:{"Window(titleBarVisible: 1) { Label() }",
                      "import vm Model\nWindow(titleBarVisible: vm.visible) { Label() }",
                      "Window(titleBarVisible: false, windowChrome: .system) { Label() }",
                      "Window(titleBarVisible: true, windowChrome: .custom) { Label() }"})
        check(!lvui::compile_native(source),"Invalid or conflicting title-bar visibility was accepted");
    const auto hiddenScene=generate("Application { Scene(titleBarVisible: false) { Label() } }");
    check(hiddenScene.find(".chrome = ii::gui::WindowChrome::custom")!=hiddenScene.npos,
          "Application scenes could not omit the system title bar");
    const auto named_title = generate("const Header: View = HStack(id: \"chrome\") {}\nWindow(windowChrome: .custom, titleBar: Header) { Label() }");
    check(named_title.find("@lvui/window-chrome-shell")!=named_title.npos,
          "A named title bar View was not mounted");
    const auto custom_scene = generate("Application { Scene(windowChrome: .custom, titleBar: HStack() {}) { Label() } }");
    check(custom_scene.find("@lvui/window-chrome-shell")!=custom_scene.npos && custom_scene.find(marker)!=custom_scene.npos,
          "Application scene lost its separate title bar or safe area");
    const auto full_width = generate("Window(windowChrome: .custom, titleBar: HStack() {}) { Label() }.ignoreSafeArea");
    check(full_width.find("@lvui/window-chrome-shell")!=full_width.npos && full_width.find(marker)==full_width.npos,
          "Ignoring content insets changed the title bar or kept the default safe area");
    check(!lvui::compile_native("Window { VStack(id: \"@lvui/window-safe-area\") {} }"),
          "A user id collided with the generated safe-area widget");
}

// A window has no implicit title bar; each form must opt in to one explicitly.
void chrome_defaults(){
    constexpr auto custom=".chrome = ii::gui::WindowChrome::custom";
    constexpr auto system=".chrome = ii::gui::WindowChrome::system";
    constexpr auto bar="@lvui/window-chrome-shell";
    for(auto source:{"ApplicationWindow { Label() }", "Window { Label() }", "AppBootstrapWindow { Label() }",
                     "AppShell(visible: true) { Label() }", "Application { Scene { Label() } }", "Label()"}){
        const auto code=generate(source);
        check(code.find(custom)!=code.npos&&code.find(bar)==code.npos,
              "A default window created a system or custom title bar");
    }
    for(auto source:{"ApplicationWindow(titleBarVisible: true) { Label() }",
                     "Window(windowChrome: .system) { Label() }",
                     "Application { Scene(titleBarVisible: true) { Label() } }"}){
        const auto code=generate(source);
        check(code.find(system)!=code.npos&&code.find(custom)==code.npos&&code.find(bar)==code.npos,
              "An explicitly requested system title bar was not selected");
    }
    const auto provided=generate("ApplicationWindow(titleBar: HStack(id: \"header\") {}) { Label() }");
    check(provided.find(custom)!=provided.npos&&provided.find(bar)!=provided.npos&&provided.find("header")!=provided.npos,
          "An explicitly provided custom title bar was not mounted");
    const auto scenes=generate("Application { Scene(id: \"main\") { Label() }\nScene(id: \"secondary\", titleBarVisible: true) { Label() } }");
    check(scenes.find(custom)!=scenes.npos&&scenes.find(system)!=scenes.npos,
          "Window scenes did not retain independent chrome choices");
    for(auto source:{"ApplicationWindow(titleBarVisible: true, titleBar: HStack()) { Label() }",
                     "Window(windowChrome: .system, titleBar: HStack()) { Label() }"})
        check(!lvui::compile_native(source),"A custom title bar was combined with system chrome");
}
int main(int argc,char** argv){try{std::string name=argc>1?argv[1]:"dynamic";if(name=="dynamic")dynamic();else if(name=="accent")accent();else if(name=="constants")constants();else if(name=="diagnostics")diagnostics();else if(name=="safe_area")safe_area();else if(name=="chrome_defaults")chrome_defaults();else throw std::runtime_error("Unknown Window test");std::cout<<"Native Window "<<name<<" passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
