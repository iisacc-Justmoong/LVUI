#include <LVUI/Compiler.h>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool value, const char* why) { if (!value) throw std::runtime_error(why); }
std::string compile(std::string_view source) {
    auto result = lvui::compile_native(source, {.name="Hello"});
    if (!result) throw std::runtime_error(result.error().message);
    return *result;
}
void bad(std::string_view source, std::string_view reason) {
    auto result = lvui::compile_native(source);
    check(!result, "Invalid native source accepted");
    if (result.error().message.find(reason) == std::string::npos)
        throw std::runtime_error("Unexpected diagnostic: " + result.error().message);
}
void generation() {
    const auto code = compile(R"(let greeting: String = "Hello World"
ApplicationWindow(title: greeting, width: 600, height: 360) {
    VStack(spacing: 12, padding: 24) {
        Label(id: "greeting", text: greeting, fontSize: 28)
        HStack { Label(text: "Native Cocoa") }
    }
})");
    check(code.find("struct Hello") != std::string::npos &&
          code.find("widgets.vstack(") != std::string::npos &&
          code.find("widgets.hstack(") != std::string::npos &&
          code.find("widgets.label(") != std::string::npos &&
          code.find("host.mount_widgets(") != std::string::npos, "Native calls missing");
    check(code.find("parse_view") == std::string::npos && code.find("LVUI/") == std::string::npos &&
          code.find("ApplicationWindow(") == std::string::npos, "Generated code embeds source or runtime compiler");
    check(code == compile(R"(let greeting: String = "Hello World"
ApplicationWindow(title: greeting, width: 600, height: 360) {
    VStack(spacing: 12, padding: 24) {
        Label(id: "greeting", text: greeting, fontSize: 28)
        HStack { Label(text: "Native Cocoa") }
    }
})"), "Output is not deterministic");
}
void validation() {
    bad("Label(text: \"Hello\")", "ApplicationWindow");
    check(bool(lvui::compile_native("ApplicationWindow { Avatar() }")), "Independent Avatar must compile without a framework declaration");
    bad("ApplicationWindow { Label(text: 42) }", "String");
    bad("ApplicationWindow { Label(text: \"x\", typo: 1) }", "Unknown input");
    bad("ApplicationWindow(width: 2) { Label(text: \"x\") }", "minimum");
    bad("ApplicationWindow { Label(text: \"x\", fontSize: -1) }", "range");
    bad("ApplicationWindow { Label(text: \"x\") { Label(text: \"y\") } }", "children");
    bad("ApplicationWindow { VStack { Label(id: \"same\", text: \"x\")\n Label(id: \"same\", text: \"y\") } }", "Duplicate widget id");
    check(bool(lvui::compile_native("var count: Int = 1\nApplicationWindow { Label(text: \"x\") }")),"Owned scalar var is not compiled");
    bad("ApplicationWindow { Label(text: unknown) }", "Unknown value");
    bad("ApplicationWindow { Label(text: \"x\")\n Label(text: \"y\") }", "one child");
    auto error = lvui::compile_native("ApplicationWindow {\n  Label(text: 4)\n}");
    check(!error && error.error().line == 2 && error.error().column == 9, "Semantic error lost source position");
    check(!lvui::compile_native("ApplicationWindow { Label(text: \"x\") }", {.name="Bad;code"}), "C++ name injection accepted");
    check(!lvui::compile_native("ApplicationWindow { Label(text: \"x\") }", {.includes={"bad\"\ncode"}}), "Include injection accepted");
}
void actions() {
    auto result = lvui::compile_native(R"(import vm Demo.Backend
ApplicationWindow { PushButton(vm.close, text: "Close", enabled: true) })",
        {.name="Actions", .includes={"Demo/Backend.h"}});
    check(bool(result), "Scoped backend callback failed");
    check(result->find("::Demo::Backend backend_0") != std::string::npos &&
          result->find("std::make_shared<Scope>(factories)") != std::string::npos &&
          result->find("return (scope->backend_0);") != std::string::npos &&
          result->find("object).close(std::forward") != std::string::npos &&
          result->find("Methods::reference(context,") != std::string::npos &&
          result->find("[[maybe_unused]] lvui::native::ReadContext& context") != std::string::npos, "Backend is not scope owned or callback is not compiled");
    check(result->find("scope_ = std::move(owner)")!=std::string::npos && result->find("scope_.reset()")!=std::string::npos,
          "Native C++ widget does not retain its backend until Core detaches it");
    auto returned=lvui::compile_native("import vm Demo\nApplicationWindow { PushButton(vm.makeHandler(), text: \"Close\") }");
    check(returned&&returned->find("lvui::native::returned_callback(")!=returned->npos&&returned->find("scope->backend_0.makeHandler()")!=returned->npos,
          "An explicit C++ callable factory was rejected or deferred until click");
    auto events=lvui::compile_native("import vm Backend\nWindow { VStack { AppHeader(onMenuClicked: vm.makeHandler())\nCard(onMenuRequested: vm.makeHandler())\nColorPicker(onAccepted: vm.makeHandler())\nPopover(onOpened: vm.makeHandler()) {}\nAlertButton(method: vm.makeHandler()) } }",{.framework="iiGUI"});
    check(events&&events->find("lvui::native::returned_callback(")!=events->npos,
          "A component-specific callback incorrectly rejected a C++ callable factory");
    bad("ApplicationWindow { PushButton(vm.close, text: \"Close\") }", "Unknown backend");
    auto bound=lvui::compile_native("import vm Demo\nApplicationWindow { Label(text: vm.name) }");
    check(bound && bound->find("native_bindings->bind(")!=std::string::npos && bound->find("scope->backend_0.name, scope->revision_0")!=std::string::npos,"Typed C++ property is not bound");
    auto variable=lvui::compile_native("var note: String = \"Ready\"\nApplicationWindow { Label(text: note) }");
    check(variable && variable->find("Observable<std::string> value_note")!=std::string::npos && variable->find("set_note(")!=std::string::npos,"Reactive scalar has no owned state/accessor");
    bad("import vm class\nApplicationWindow { Label(text: \"x\") }", "C++ identifier");
}
void escaping() {
    const auto code = compile(R"(ApplicationWindow { Label(text: "Hello \"Cocoa\"\n한글 \\ path") })");
    check(code.find("\\\"Cocoa\\\"") != std::string::npos && code.find("\\n") != std::string::npos &&
          code.find("\\355") != std::string::npos && code.find("\\\\ path") != std::string::npos,
          "UTF-8/string escaping is unsafe");
    auto eight = compile("ApplicationWindow { Label(text: \"x\", fontSize: 08) }");
    check(eight.find("font_size = 8") != std::string::npos, "LVUI number emitted as an invalid/octal C++ literal");
}
void native_widget() {
    auto code=compile("ApplicationWindow { Label(text: \"Hello\", style: .title) }");
    check(code.find("Hello() = default")!=std::string::npos && code.find("native_widget_")!=std::string::npos &&
          code.find("attach(ii::gui::Host&")!=std::string::npos && code.find("detach(ii::gui::Host&")!=std::string::npos,
          "AOT View is not an instantiable native C++ widget");
    check(code.find("ii::gui::WidgetAppearance{}")!=std::string::npos && code.find("result.styled = true")!=std::string::npos &&
          code.find("ii::gui::lvrs_appearance()") == std::string::npos && code.find("TextRole::title")!=std::string::npos &&
          code.find("font_size = 26")!=std::string::npos, "LVUI visual role is not compiled into native widgets");
    bad("ApplicationWindow { Label(style: .unknown) }", "text style");
}
void embedded_build(){
    const auto code=compile("import vm Backend\nWindow { VStack(id: \"page\") { Label(text: vm.title) } }");
    check(code.find("static ii::core::Result<void> build(ii::gui::WidgetBuilder&")!=code.npos&&code.find("widgets.vstack(native_parent,")!=code.npos,"AOT page has no build-only parent entry point");
    check(code.find("return construct(widgets, native_parent, configure, scope_owner, factories)")!=code.npos&&code.find("widgets.retain(scope)")!=code.npos&&code.find("widgets.retain(native_bindings)")!=code.npos,"Embedded page lost scope, construction inputs or binding ownership");
    check(code.find("factories.create<::Backend>(\"Backend\", \"vm\", this->backend_activation)")!=code.npos&&code.find("ScopeFactoryStorage(factories)")!=code.npos,
          "A native import lost its typed constructor or activation-owned dependencies");
    auto popup=lvui::compile_native("Window { Modal(open: true) {} }",{.framework="iiGUI"});
    check(popup&&popup->find("Embedded pages must open dialogs after activation")!=popup->npos,"Embedded eager dialog construction was silently admitted");
    auto scoped_popup=lvui::compile_native("import vm Backend\nWindow { VStack { Label(text: vm.title)\nModal(open: true) {} } }",{.framework="iiGUI"});
    check(scoped_popup&&scoped_popup->find("(void)configure; (void)scope_owner; (void)factories;")!=scoped_popup->npos,"A rejected eager dialog page left a newly added factory parameter unused");
}
void lvrs_router(){
    auto code=lvui::compile_native("import vm Backend\nWindow { PageRouter(id: \"router\", router: vm.router, width: 700, height: 400) }",{.framework="LVRS"});
    check(code&&code->find("controls.page_router(native_parent, \"router\", lvui::native::initial(scope->backend_0.router), {700, 400})")!=code->npos,"PageRouter controller was not lowered to native mounting");
    for(auto source:{"Window { PageRouter() }","Window { PageRouter(router: 1) }","import vm Backend\nWindow { PageRouter(router: vm.router, width: 0) }","import vm Backend\nWindow { PageRouter(router: vm.router) { Label() } }"})check(!lvui::compile_native(source,{.framework="LVRS"}),"Invalid PageRouter was accepted");
    auto independent=lvui::compile_native("import vm Backend\nWindow { PageRouter(router: vm.router) }");
    check(independent&&independent->find("lvui::native::NativePageRouter::create(")!=independent->npos&&independent->find("lvrs::")==independent->npos,"Independent PageRouter was not generated");
}
void lvrs_link(){
    auto code=lvui::compile_native(R"(import vm Backend
Window { Link(id: "link", text: vm.text, to: vm.href, params: vm.params, router: vm.router, navigator: vm.navigator, targetComponent: vm.page, replace: vm.replace, underline: vm.underline, linkColor: vm.color, hoverColor: "#FFFFFFE6", method: vm.log, methods: [vm.log, vm.after], onNavigated: vm.navigated) { HStack { Label(text: "Custom") } } })",{.framework="LVRS"});
    check(code&&code->find("lvrs::Link::create(widgets,")!=code->npos&&code->find("link.set_to(value)")!=code->npos&&code->find("link.set_params(value)")!=code->npos&&code->find("native_bindings->bind(")!=code->npos,"Link typed/reactive routing was not lowered");
    check(code->find("controls.hstack(*node_0")!=code->npos&&code->find(".methods = {lvui::native::returned_callback(")!=code->npos&&code->find(".role = ii::gui::ActionRole") == code->npos,"Link children or deferred typed methods were lost");
    for(auto source:{"Window { Link(href: \"/a\", to: \"/b\") }","Window { Link(params: 1) }","Window { Link(linkColor: \"bad\") }","Window { Link(tone: .bad) }","import vm Backend\nWindow { Link(methods: [1]) }","import vm Backend\nWindow { Link(onClick: 1) }"})check(!lvui::compile_native(source,{.framework="LVRS"}),"Invalid Link contract accepted");
    check(bool(lvui::compile_native("Window { Link() }")),"Independent Link was rejected");
}
void lvrs_controls() {
    auto result=lvui::compile_native(R"(import vm Backend
Window { VStack {
 Label(text: "Native")
 ZStack { Label(text: "Overlay") }
 Spacer(minLength: 4)
 LabelButton(vm.click, text: "Default", tone: .default)
 IconButton(vm.click, iconName: "add")
 HelpButton(vm.click)
 DropdownButton(vm.click, text: "Open")
 IconMenuButton(iconName: "nodesfolder")
 LabelMenuButton(text: "Menu")
 TabBar(onCurrentIndexChanged: vm.selected) {
   Tab(text: "Overview", iconName: "nodesfolder")
   Tab(text: "Details", iconName: "generaladd", badge: "2", tabStyle: .surface)
 }
 CheckBox(checked: true, onToggled: vm.checked)
 RadioButton(group: "g")
 ToggleSwitch(onToggled: vm.checked)
 InputField(onEdited: vm.edited, onSubmit: vm.submit)
 Slider(from: 0, to: 100, onMoved: vm.moved)
 ProgressBar(currentValue: 40)
} })",{.framework="LVRS"});
    check(bool(result),"LVRS native component generation failed");
    for(auto method:{"zstack","spacer","button","icon_button","help_button","dropdown_button","icon_menu_button","tab_bar","tab","check_box","radio_button","toggle_switch","input_field","slider"})
        check(result->find(std::string("controls.")+method+"(")!=std::string::npos,"LVRS component call missing");
    check(result->find("lvrs::ProgressBar::create(widgets,")!=result->npos,"LVRS progress lost its retained native model/control");
    check(result->find("#include <LVRS/Widgets.h>")!=std::string::npos && result->find("lvrs::Theme(lvrs::Accent::blue)")!=std::string::npos,"Native LVRS framework not selected");
    for(auto signature:{"bool value","std::string value","double value"}) check(result->find(signature)!=std::string::npos,"Typed backend event lost its argument");
    bad("ApplicationWindow { Slider(from: 1, to: 0) }","smaller");
    check(bool(lvui::compile_native("ApplicationWindow { ProgressBar(minimumValue: 2, maximumValue: 1) }")),"Reverse progress range was rejected");
    bad("ApplicationWindow { InputField(readOnly: 1) }","Bool");
    bad("ApplicationWindow { PushButton(tone: .bad) }","tone");
    check(!lvui::compile_native("ApplicationWindow { Label() }",{.framework="WebView"}),"Unknown framework accepted");
    auto green=lvui::compile_native("ApplicationWindow(accent: .green) { Label() }",{.framework="LVRS"});
    check(green && green->find("lvrs::Accent::green")!=std::string::npos,"LVRS accent was not compiled");
    for(auto source:{"ApplicationWindow { Tab(text: \"missing icon\") }", "ApplicationWindow { Tab(iconName: \"  \" ) }",
                     "ApplicationWindow { TabBar { Label() } }", "ApplicationWindow { TabBar(currentIndex: 1.5) }",
                     "ApplicationWindow { Tab(iconName: \"add\", tabStyle: .bad) }"})
        check(!lvui::compile_native(source,{.framework="LVRS"}),"invalid native tab accepted");
    bad("ApplicationWindow { IconButton() }","requires a native icon");
    bad("ApplicationWindow { PushButton(iconMode: true) }","requires a native icon");
    auto help=lvui::compile_native("ApplicationWindow { HelpButton() }");
    check(help&&help->find("lvui::help_button_configuration()")!=help->npos&&help->find("lvrs::")==help->npos,
          "Default HelpButton did not use LVUI's independent preset");
    auto dropdown=lvui::compile_native("ApplicationWindow { VStack { DropdownButton(text: \"Open\")\nIconMenuButton()\nLabelMenuButton(text: \"Menu\") } }");
    check(dropdown&&dropdown->find("lvui::native::NativeDropdownButton::create(")!=dropdown->npos&&dropdown->find("lvrs::")==dropdown->npos,
          "Default dropdown variants did not use LVUI's independent implementation");
}
void lvrs_compound() {
    const auto source=R"(import vm Backend
ApplicationWindow { VStack {
 ComboBox(vm.click, text: "Choice", tone: .borderless, arrow: .down, width: 120, onPressed: vm.press, onReleased: vm.release, onCanceled: vm.cancel)
 Stepper(onStepped: vm.step, onClick: vm.click, arrow: .upDown)
 LabelSegmentedControl(shapeStyle: .cylinder, horizontalPadding: 6, verticalPadding: 3.5, spacing: 2, borderWidth: 2, cornerRadius: 12) {
   LabelButton(text: "One")
   LabelButton(text: "Two")
 }
 IconSegmentedControl(forceBorderlessTone: false) { IconButton(iconName: "add") }
} })";
    auto code=lvui::compile_native(source,{.framework="LVRS"});
    if(!code) throw std::runtime_error(code.error().message);
    for(auto call:{"combo_box","stepper","label_segmented_control","icon_segmented_control"})
        check(code->find(std::string("controls.")+call+"(")!=code->npos,"Compound control was not compiled to C++");
    for(auto event:{"click","press","release","cancel","step"})
        check(code->find(std::string("object).")+event+"(std::forward")!=code->npos,"Compound callback missing");
    check(code->find("int value")!=code->npos && code->find("StepperArrow::down")!=code->npos && code->find(".vertical_padding = 3.5")!=code->npos,"Typed direction or authored metrics missing");
    for(auto source:{"ApplicationWindow { ComboBox(arrow: .left) }", "ApplicationWindow { ComboBox(tone: .destructive) }",
                     "ApplicationWindow { ComboBox { Label() } }", "ApplicationWindow { Stepper(onStepped: vm.step()) }",
                     "ApplicationWindow { Stepper(cornerRadius: -1) }", "ApplicationWindow { LabelSegmentedControl(currentIndex: 0) }",
                     "ApplicationWindow { IconSegmentedControl(shapeStyle: .triangle) }"})
        check(!lvui::compile_native(source,{.framework="LVRS"}),"Invalid compound contract accepted");
    auto combo=lvui::compile_native("ApplicationWindow { ComboBox() }");
    check(combo&&combo->find("lvui::native::NativeComboBox::create(")!=combo->npos&&
          combo->find("lvrs::")==combo->npos,"Default ComboBox did not use LVUI's independent implementation");
    auto segmented=lvui::compile_native("ApplicationWindow { LabelSegmentedControl { LabelButton(text: \"One\") } }");
    check(segmented&&segmented->find("lvui::native::NativeSegmentedControl::create(")!=segmented->npos&&
          segmented->find("weak_tone_registration_port()")!=segmented->npos&&segmented->find("lvrs::")==segmented->npos,
          "Default segmented control did not use LVUI's independent implementation");
    auto independent=lvui::compile_native("ApplicationWindow { Stepper() }");
    check(independent&&independent->find("lvui::native::NativeStepper::create(")!=independent->npos&&
          independent->find("lvrs::")==independent->npos,"Default Stepper did not use LVUI's independent implementation");
    bad("ApplicationWindow { Stepper(arrow: .left) }","arrow");
}
void lvrs_menus() {
    auto code=lvui::compile_native(R"(import vm Backend
ApplicationWindow { VStack {
 LabelButton(vm.open, text: "Open")
 Menu(id: "menu", itemWidth: 200, selectedIndex: 0, onItemTriggered: vm.triggered) {
   MenuItem(label: "Open", shortcut: "⌘O", iconName: "nodesfolder", hasChildItems: false, onClick: vm.open)
   MenuDivider(thickness: 1)
   MenuItem(label: "More", state: .selected, selectionDirection: .down)
 }
 ContextMenu(id: "context", compactItems: true, autoCloseOnTrigger: false, onOpened: vm.open, onClosed: vm.close) {
   ContextMenuItem(label: "Copy", key: "⌘C", showIconSlot: false, keepOpen: true)
   ContextMenuDivider(linePadding: 4)
   ContextMenuItem(label: "Inactive", state: .inactive)
 }
 MenuDivider(axis: "vertical", lineLength: 30)
} })",{.framework="LVRS"});
    if(!code) throw std::runtime_error(code.error().message);
    for(auto call:{"menu","context_menu","menu_item","context_menu_item","menu_divider","context_menu_divider"})
        check(code->find(std::string("controls.")+call+"(")!=code->npos,"Menu was not constructed by native C++");
    check(code->find("int value")!=code->npos && code->find("object).triggered(std::forward")!=code->npos,"Typed menu index event missing");
    check(code->find("MenuItemState::selected")!=code->npos && code->find("MenuDirection::down")!=code->npos,"Menu state or chevron direction missing");
    for(auto source:{"Window { Menu { Label() } }", "Window { MenuItem { Label() } }", "Window { MenuItem(state: .bad) }",
        "Window { MenuItem(key: \"a\", shortcut: \"b\") }", "Window { MenuItem(selectionDirection: .diagonal) }",
        "Window { MenuDivider(axis: \"diagonal\") }", "Window { Menu(selectedIndex: 1.5) }", "Window { Menu(menuOpacity: 2) }"})
        check(!lvui::compile_native(source,{.framework="LVRS"}),"Invalid menu contract accepted");
    auto independent_menu=lvui::compile_native("Window { Menu() }");
    check(independent_menu&&independent_menu->find("lvui::native::NativeMenu::create(")!=independent_menu->npos,"Default Menu did not use LVUI's independent implementation");
    auto strings=lvui::compile_native(R"(Window { VStack {
 ContextMenu(targetId: "later", items: ["Copy", "Paste"], showIconSlot: false)
 Label(id: "later", text: "Right click")
} })",{.framework="LVRS"});
    check(strings && strings->find("set_context_menu(")!=strings->npos && strings->find(".label = \"Copy\"")!=strings->npos && strings->find(".show_icon_slot = false")!=strings->npos,"Static entry model or forward context target failed");
    for(auto source:{"Window { Menu(items: [1]) }", "Window { Menu(items: [\"A\"]) { MenuItem() } }", "Window { ContextMenu(targetId: \"missing\") }"})
        check(!lvui::compile_native(source,{.framework="LVRS"}),"Invalid entry model or target accepted");
}
void lvrs_color_fields() {
    auto result=lvui::compile_native("import vm ColorBackend\nWindow { HStack { ColorPickerSurface(model: vm.color, field: .SaturationBrightness, width: 160, height: 120, onColorEdited: vm.edited)\nColorField(model: vm.color, field: .HueRail, width: 18) } }",{.framework="LVRS"});
    if(!result)throw std::runtime_error(result.error().message);
    check(result && result->find("scope->backend_0.color")!=result->npos && result->find("ii::gui::ColorField::saturation_brightness")!=result->npos && result->find("ii::gui::WidgetColor value")!=result->npos,"Native color model/field/typed action generation failed");
    for(auto source:{"Window { ColorPickerSurface() }","Window { ColorPickerSurface(model: vm.color) }","import vm Backend\nWindow { ColorPickerSurface(model: vm.color, field: .unknown) }","import vm Backend\nWindow { ColorPickerSurface(model: vm.color, width: 2048) }"})
        check(!lvui::compile_native(source,{.framework="LVRS"}),"Invalid color surface contract accepted");
    bad("Window { ColorPickerSurface() }","Color surface requires");
    auto independent=lvui::compile_native("import vm Backend\nWindow { ColorPickerSurface(model: vm.color, field: .Wheel) }");
    check(independent&&independent->find("lvui::native::NativeColorSurface::create(")!=independent->npos,
          "Default color surface did not use LVUI's independent implementation");
}
void lvrs_color_picker() {
    auto result=lvui::compile_native("import vm Backend\nWindow { ColorPicker(id: \"picker\", type: .CMYK, model: vm.color, recentColors: [\"#00E71F80\", \"FF0000\"], onColorEdited: vm.edited, onAccepted: vm.accepted, onCanceled: vm.canceled, onEyedropperRequested: vm.sample) }",{.framework="LVRS"});
    if(!result)throw std::runtime_error(result.error().message);
    check(result->find("controls.color_picker(")!=result->npos && result->find("lvrs::ColorPickerType::cmyk")!=result->npos && result->find("scope->backend_0.color")!=result->npos && result->find(".eyedropper_requested = lvui::native::returned_callback(")!=result->npos && result->find("std::vector<ii::gui::WidgetColor>")!=result->npos,"ColorPicker did not emit native composite/options/typed callbacks");
    check(bool(lvui::compile_native("Window { ColorPicker() }",{.framework="LVRS"})),"Default self-owned picker rejected");
    for(auto source:{"Window { ColorPicker(type: .Spectrum) }","Window { ColorPicker(width: 200) }","Window { ColorPicker(padding: 64) }","Window { ColorPicker(recentColors: [\"oops\"]) }","import vm Backend\nWindow { ColorPicker(model: vm.color(), onAccepted: vm.accepted) }","import vm Backend\nWindow { ColorPicker(onAccepted: 1) }"})check(!lvui::compile_native(source,{.framework="LVRS"}),"Invalid ColorPicker inputs accepted");
    auto owned=lvui::compile_native("Window { ColorPicker() }");
    check(owned&&owned->find("lvui::native::NativeColorPicker::create(")!=owned->npos&&owned->find("<LVRS/")==owned->npos,
          "Default color picker did not use LVUI's independent implementation");
}
void lvrs_color_button(){
    auto result=lvui::compile_native("import vm Backend\nWindow { HStack { ColorPickerButton(id: \"open\", model: vm.model, buttonSize: .Large, onClick: vm.open, onPressed: vm.pressed)\nColorPickerButton(currentColor: \"#00E71F80\", buttonSize: 22, showHueRing: false) } }",{.framework="LVRS"});
    if(!result)throw std::runtime_error(result.error().message);
    check(result->find("controls.color_picker_button(")!=result->npos&&result->find(".model = scope->backend_0.model")!=result->npos&&result->find("ColorPickerButtonSize::large")!=result->npos&&result->find("ColorPickerButtonSize::small")!=result->npos&&result->find(".show_hue_ring = false")!=result->npos&&result->find(".current_color = ii::gui::WidgetColor{")!=result->npos&&result->find(".pressed = lvui::native::returned_callback(")!=result->npos,"Native color button/model/options/actions not emitted");
    check(bool(lvui::compile_native("Window { ColorPickerButton() }",{.framework="LVRS"})),"Default ColorPickerButton rejected");
    for(auto source:{"Window { ColorPickerButton(buttonSize: .Huge) }","Window { ColorPickerButton(buttonSize: 23) }","Window { ColorPickerButton(currentColor: \"red\") }","import vm Backend\nWindow { ColorPickerButton(model: vm.model, currentColor: \"00E71F\") }","import vm Backend\nWindow { ColorPickerButton(model: vm.model()) }","import vm Backend\nWindow { ColorPickerButton(onClick: 1) }","Window { ColorPickerButton { Label() } }"})check(!lvui::compile_native(source,{.framework="LVRS"}),"Invalid color button inputs accepted");
    auto independent=lvui::compile_native("Window { ColorPickerButton() }");
    check(independent&&independent->find("lvui::native::NativeColorPickerButton::create(")!=independent->npos,"Default ColorPickerButton did not use LVUI's independent implementation");
}
void lvrs_alerts(){
    auto motion=lvui::compile_native("import vm Backend\nWindow { Alert(motion: vm.motion, motionEnabled: false, shadowEnabled: false, shadowBlurRadius: 24, shadowOffsetX: 2, shadowOffsetY: 10, shadowColor: \"#00000040\") {} }",{.framework="LVRS"});
    check(motion&&motion->find(".motion = lvui::native::initial(scope->backend_0.motion)")!=motion->npos&&motion->find(".motion_enabled = false")!=motion->npos&&motion->find(".shadow_enabled = false")!=motion->npos&&motion->find(".blur_radius = 24")!=motion->npos,"Native motion/shadow inputs were not compiled");
    for(auto source:{"import vm Backend\nWindow { Alert(motion: vm.motion()) {} }","Window { Alert(motion: missing.model) {} }","Window { Alert(motionEnabled: 1) {} }","Window { Alert(shadowBlurRadius: 65) {} }"})check(!lvui::compile_native(source,{.framework="LVRS"}),"Invalid motion/shadow inputs accepted");
    auto r=lvui::compile_native("import vm Backend\nWindow { VStack { AlertButton(text: \"Standalone\", method: vm.method, dialogStyle: true, onClick: vm.clicked)\nAlert(id: \"alert\", open: true, title: \"Save changes?\", description: \"A document\", buttonCount: 3, button1Text: \"Save\", button1Method: vm.method, button2Method: vm.other, imageSource: \"qrc:/qt/qml/LVRS/resources/images/alert-file-text.svg\", onPrimaryClicked: vm.clicked) {} } }",{.framework="LVRS"});
    if(!r)throw std::runtime_error(r.error().message);check(r->find("controls.alert(")!=r->npos&&r->find("controls.alert_button(")!=r->npos&&r->find("const lvrs::ButtonEvent&")!=r->npos&&r->find("std::invocable<H&, const lvrs::ButtonEvent&>")!=r->npos&&r->find(".builtin_icon = lvrs::AlertIcon::file_text")!=r->npos&&r->find("mounted->find(\"alert\")")!=r->npos,"Alert typed methods/asset/initial open missing");
    for(auto source:{"Window { Alert(button1Text: \"a\", primaryText: \"b\") {} }","Window { Alert(description: \"a\", message: \"b\") {} }","Window { Alert { Label() } }","Window { Alert(buttonCount: 2.5) {} }","Window { Alert(imageSource: \"https://example.com/icon.svg\") {} }","Window { Alert(shapeStyle: .wrong) {} }","import vm Backend\nWindow { AlertButton(method: 1) }"})check(!lvui::compile_native(source,{.framework="LVRS"}),"Invalid Alert alias/method/source accepted");
    auto independent_alert=lvui::compile_native("Window { Alert {} }");
    check(independent_alert&&independent_alert->find("lvui::native::NativeAlert::create(")!=independent_alert->npos,
          "Default Alert did not use LVUI's independent implementation");
    auto independent=lvui::compile_native("Window { AlertButton() }");
    check(independent&&independent->find("lvui::native::NativeAlertButton::create(")!=independent->npos,
          "Default AlertButton did not use LVUI's independent implementation");
}
void lvrs_table_view(){
    auto result=lvui::compile_native("import vm TableBackend\nWindow { Table(model: vm.model, width: 660, height: 280, selectionMode: .RangeSelection, sortingEnabled: true, structureControlsVisible: true, resizeHandlesVisible: true, clipboardEnabled: false, onCellInputEdited: vm.edited, onCellInputSubmitted: vm.submitted, onCellInputRejected: vm.rejected, onSelectionChanged: vm.selection, onRowsSorted: vm.sorted, onRangeValuesChanged: vm.range, onColumnResized: vm.column, onRowResized: vm.row) }",{.framework="LVRS"});
    check(bool(result),"Native Table view did not AOT compile");
    check(result->find("controls.table(")!=std::string::npos&&result->find("scope->backend_0.model")!=std::string::npos&&result->find("row, column, value")!=std::string::npos,"Table lost native model/member or typed callbacks");
    check(result->find(".structure_controls_visible = true")!=std::string::npos&&result->find(".clipboard_enabled = false")!=std::string::npos&&result->find("lvrs::TableRange range")!=std::string::npos&&result->find("double width")!=std::string::npos,"Table interaction options or callbacks were not lowered");
    for(auto source:{"Window { Table(model: 1) }","Window { Table(selectionMode: .bad) }","Window { Table(height: 10) }","Window { Table { Label() } }"})check(!lvui::compile_native(source,{.framework="LVRS"}),"Invalid native Table accepted");
    auto independent_table=lvui::compile_native("Window { Table() }");
    check(independent_table&&independent_table->find("lvui::native::NativeTable::create")!=independent_table->npos&&independent_table->find("lvrs::")==independent_table->npos,"Independent table generation failed");
    for(auto source:{"Window { Table(width: 32, structureControlsVisible: true) }","Window { Table(height: 26, structureControlsVisible: true) }"})check(!lvui::compile_native(source,{.framework="LVRS"}),"Structure gutter erased the Table viewport");
}
void lvrs_tooltip(){
    auto result=lvui::compile_native(R"(import vm TooltipBackend
Window { VStack {
 Tooltip(id: "help", targetId: "origin", text: vm.help, delay: vm.delay, preferredPlacement: vm.side, maximumWidth: vm.width, onOpened: vm.opened) { VStack { Label(text: vm.detail) } }
 LabelButton(id: "origin", text: "Help")
}})",{.framework="LVRS"});
    check(bool(result),"Native Tooltip AOT failed");
    for(auto token:{"lvrs::Tooltip::create(","lvrs::TooltipContentFactory","tooltip_milliseconds(","->set_target(*","tooltip.set_options(","scope->backend_0.detail"})
        check(result->find(token)!=result->npos,"Tooltip lost its target, retained content, checked duration or bindings");
    for(auto source:{"Window { Tooltip(targetId: \"missing\") }","Window { Tooltip(delay: -1) }","Window { Tooltip(preferredPlacement: .Center) }","import vm Backend\nWindow { Tooltip(targetId: \"x\", target: vm.target) }"})
        check(!lvui::compile_native(source,{.framework="LVRS"}),"Invalid Tooltip input silently accepted");
    auto independent=lvui::compile_native(R"(Window { VStack {
 Tooltip(targetId: "origin", text: "Help")
 LabelButton(id: "origin", text: "Help")
} })");
    check(independent&&independent->find("#include <LVUI/NativeTooltip.h>")!=independent->npos&&
          independent->find("lvui::native::NativeTooltip::create(")!=independent->npos&&
          independent->find("->set_target(")!=independent->npos&&
          independent->find("lvrs::")==independent->npos&&independent->find("<LVRS/")==independent->npos,
          "Default Tooltip did not use LVUI's independent implementation");
    bad("Window { Tooltip(scrollContent: true) }","not implemented by the independent runtime");
}
void lvrs_code_editor(){
    auto result=lvui::compile_native(R"(import vm EditorBackend
Window { VStack {
 CodeEditor(model: vm.document, snippetTitle: vm.title, snippetLanguage: "C++23", width: vm.width, readOnly: vm.locked, cursorPosition: vm.cursor, onDocumentEdited: vm.changed, onSubmitted: vm.submitted)
 CodeEditor(text: "Hello\n한글 😀", showSnippetHeader: false, textColor: "#ffffff", tabStopDistance: 64, editorHeight: 240)
}})",{.framework="LVRS"});
    check(result.has_value(),"Native CodeEditor AOT failed");
    for(auto token:{"lvrs::CodeEditor::create(","value.model = lvui::native::initial(scope->backend_0.document)","value.text = \"Hello\\n","editor.set_text(","editor.set_options(","editor.set_cursor_position(value)","const lvrs::TextDocumentChange& change"}){
        if(std::string_view(token)=="editor.set_text(")continue;
        check(result->find(token)!=result->npos,"CodeEditor AOT lost its scoped model, bindings or callbacks");
    }
    auto bound=lvui::compile_native("var source: String = \"Native\"\nWindow { CodeEditor(text: source) }",{.framework="LVRS"});
    check(bound&&bound->find("editor.set_text(ii::gui::binding_text(value))")!=bound->npos,"CodeEditor text binding reconstructed its view");
    for(auto source:{"Window { CodeEditor(readOnly: 1) }","Window { CodeEditor(textFormat: .RichText) }","Window { CodeEditor { Label() } }","Window { CodeEditor(height: 160, editorHeight: 200) }","import vm Backend\nWindow { CodeEditor(model: vm.document, text: \"ambiguous\") }"})
        check(!lvui::compile_native(source,{.framework="LVRS"}),"Invalid or unimplemented CodeEditor contract silently accepted");
    auto independent=lvui::compile_native("Window { CodeEditor() }");
    check(independent&&independent->find("lvui::native::NativeCodeEditor::create(")!=independent->npos,
          "Default CodeEditor did not use its independent implementation");
}
void lvrs_materials(){
    auto result=lvui::compile_native(R"(import vm Backend
Window { VStack {
 MaterialSurface(width: vm.width, height: 120, density: .Dense75, primaryColor: vm.accent, backdrop: .PreviousLayers, outlinePath: vm.path, motion: vm.motion) { Label(text: "Native") }
 PanelMaterial(tintOpacity: 0.25, blurEnabled: vm.blur, innerHighlightOffsetY: -1, shadowOpacity: 0)
 WindowMaterial(density: vm.density, backdropBaseColor: "#01020380", motionEnabled: vm.animated)
}})",{.framework="LVRS"});check(result.has_value(),"Native material generation failed");
    for(auto token:{"lvrs::MaterialSurface::create(","lvrs::PanelMaterial::create(","lvrs::WindowMaterial::create(","lvrs::MaterialDensity::dense75","lvrs::MaterialBackdrop::previous_layers","options->primary_color = value","options->outline_path = ii::gui::binding_text(value)","material.set_options(std::move(*options))"})check(result->find(token)!=result->npos,"Material AOT lost typed style, scope or bindings");
    for(auto source:{"Window { MaterialSurface(density: .Unknown) }","Window { PanelMaterial(tintOpacity: 2) }","Window { WindowMaterial(blurEnabled: 1) }","Window { MaterialSurface(width: -1) }","Window { MaterialSurface(backdropSource: \"canvas\") }","Window { MaterialSurface(outlinePath: 1) }"})check(!lvui::compile_native(source,{.framework="LVRS"}),"Invalid/unsupported material input silently accepted");
    check(bool(lvui::compile_native("Window { MaterialSurface {} }")),"Independent material was rejected");
}
void lvrs_media(){
    auto result=lvui::compile_native(R"(import vm MediaBackend
Window { HStack {
 Avatar(size: .Small, source: vm.image, width: vm.width, enabled: vm.enabled)
 Card(type: .File, size: .Large, detail: .Detailed, filename: vm.title, selected: vm.selected, previewSource: vm.image, onSelectedChanged: vm.selectedChanged) { Label(text: "Preview") }
 Card(type: .Project, rows: vm.rows, progress: vm.progress, showMenu: vm.enabled, actionText: "View", onMenuRequested: vm.menu, onActionTriggered: vm.open)
 Card(type: .Link, domain: "iisacc.com", previewComponent: vm.preview, iconComponent: vm.icon)
}})",{.framework="LVRS"});check(result.has_value(),"Native media code generation failed");
    for(auto token:{"lvrs::Avatar::create(","lvrs::Card::create(","lvrs::AvatarSize::small","lvrs::CardSize::large","lvrs::CardDetail::detailed","lvrs::CardType::project","options->preview_source = value","options->selected = value","component.set_preview_content(lvrs::card_component(value))","object).selectedChanged(std::forward",".progress = lvui::native::initial("})check(result->find(token)!=result->npos,"Media AOT lost typed native construction, retained scope or binding");
    for(auto source:{"Window { Card(type: .Unknown) }","Window { Card(size: 1) }","Window { Card(title: \"a\", filename: \"b\") }","Window { Card(previewSource: \"file.png\") }","Window { Avatar(source: \"avatar.png\") }","Window { Avatar(cache: true) }","Window { Card(asynchronous: true) }","Window { Card(rows: [1]) }","Window { Avatar(width: -1) }","Window { Card(showMenu: 1) }","import vm Backend\nWindow { Card(previewComponent: vm.preview) { Label(text: \"ambiguous\") } }","import vm Backend\nWindow { Card(onMenuRequested: 1) }","Window { Avatar { Label(text: \"invalid\") } }"})check(!lvui::compile_native(source,{.framework="LVRS"}),"Invalid or unimplemented media input was silently accepted");
    auto independent=lvui::compile_native("Window { Card {} }");
    check(independent&&independent->find("lvui::native::NativeCard::create(")!=independent->npos,"Independent Card must compile without LVRS");
    check(bool(lvui::compile_native("Window { Avatar {} }")),"Independent Avatar must compile without LVRS");
}
void lvrs_toolbars(){
    auto result=lvui::compile_native(R"(import vm Backend
Window { VStack {
 ListToolbar(icon1: vm.icon, icon2: "settings", buttonSize: vm.size, interactive: vm.enabled, onIconClicked: vm.iconClicked)
 ListFooter(button1: vm.first, onButtonClicked: vm.footerClicked)
 HierarchyToolbar(buttonItems: vm.items, distributeSpacing: vm.distribute, activeButtonId: vm.active, onButtonTriggered: vm.triggered) {
  ToolbarButton(buttonId: "manual", iconName: vm.icon, enabled: vm.enabled, onClick: vm.manualClicked)
 }
} })",{.framework="LVRS"});
    check(bool(result),result?"":result.error().message.c_str());
    for(auto token:{"lvrs::ListToolbar::create(","lvrs::ListFooter::create(","lvrs::HierarchyToolbar::create(","lvrs::ToolbarButton::create(","register_button(*toolbar_","component.set_items(lvrs::toolbar_items(value))","component.set_active_id(value)","options->icons[0]","options->buttons[0]","->normalize()"})check(result->find(token)!=result->npos,"Native toolbar generation lost slots, scope ownership or bindings");
    for(auto source:{"Window { ListToolbar(buttonSize: -1) }","Window { ListToolbar(interactive: 1) }","Window { ListFooter(button1: 1) }","Window { HierarchyToolbar(buttonItems: [1]) }","Window { ToolbarButton(buttonId: 1.5) }","Window { ToolbarButton { Label(text: \"wrong\") } }","import vm Backend\nWindow { ListToolbar(onIconClicked: 1) }"})check(!lvui::compile_native(source,{.framework="LVRS"}),"Invalid toolbar input silently accepted");
    auto independent=lvui::compile_native("Window { ListToolbar() }");check(independent&&independent->find("lvui::native::NativeListToolbar::create")!=independent->npos&&independent->find("lvrs::")==independent->npos,"List toolbar did not use its independent native control");
    auto hierarchy=lvui::compile_native("Window { HierarchyToolbar() }");check(hierarchy&&hierarchy->find("lvui::native::NativeHierarchyToolbar::create")!=hierarchy->npos&&hierarchy->find("lvrs::")==hierarchy->npos,"Hierarchy toolbar did not use its independent native control");
    bad("Window { HierarchyToolbar { ToolbarButton() } }","accepts buttonItems");
}
void lvrs_app_surfaces(){
    auto result=lvui::compile_native(R"(import vm SurfaceBackend
Window { VStack {
 AppHeader(id: "header", title: vm.title, subtitle: "Overview", compact: vm.compact,
           menuVisible: true, onMenuClicked: vm.menu, contentVerticalPadding: 10) {
     LabelButton(text: "Action", onClick: vm.action)
 }
 AppCard(id: "card", title: vm.cardTitle, subtitle: "Details", width: vm.width,
         backgroundColor: vm.color, shapeStyle: .cylinder) {
     VStack { InputField(text: "Retain me") }
 }
} })",{.framework="LVRS"});
    if(!result)throw std::runtime_error(result.error().message);
    for(auto token:{"lvrs::AppHeader::create(","lvrs::AppCard::create(","options->compact = value","options->width = value","surface.set_options(std::move(*options))","lvrs::AppCardShape::cylinder","object).menu(std::forward"})check(result->find(token)!=result->npos,"Native app surface generation lost typed inputs or bindings");
    for(auto source:{"Window { AppHeader(compact: 1) }","Window { AppCard(shapeStyle: .bad) }","Window { AppCard(width: -1) }","Window { AppHeader(backgroundColor: \"bad\") }","Window { AppCard(motionEnabled: true) }","import vm Backend\nWindow { AppHeader(onMenuClicked: 1) }"})check(!lvui::compile_native(source,{.framework="LVRS"}),"Invalid or unimplemented surface input was silently accepted");
    auto independent=lvui::compile_native("Window { AppCard {} }");check(independent&&independent->find("lvui::native::NativeAppCard::create(")!=independent->npos,"Independent AppCard was not generated");
    auto independent_header=lvui::compile_native("Window { AppHeader {} }");check(independent_header&&independent_header->find("lvui::native::NativeAppHeader::create(")!=independent_header->npos,"Independent AppHeader was not generated");
}
void lvrs_lists(){
    auto r=lvui::compile_native(R"(import vm EditorBackend
Window { AppCard(width: 900, height: 600) { HStack {
 Hierarchy(model: vm.layers, showChecks: true, onSelectionChanged: vm.select, onItemRenamed: vm.rename, onItemMoved: vm.move, onItemChecked: vm.check)
 EventListener(id: "canvas", onInput: vm.input, onResized: vm.resize)
 ListItem(text: "New", onClick: vm.create)
} } })",{.framework="LVRS"});
    if(!r)throw std::runtime_error(r.error().message);
    check(r->find("controls.hierarchy(")!=r->npos&&r->find("controls.event_listener(")!=r->npos&&r->find("lvrs::AppCard::create(")!=r->npos&&r->find("controls.list_item(")!=r->npos,"Native editor component call missing");
    check(r->find("configure(*scope)")<r->find("controls.hierarchy("),"Shared model is configured after native construction");
    check(r->find("WidgetInput& value")!=r->npos&&r->find("std::string key, std::string parent, std::string before")!=r->npos,"Editor callback types were lost");
}
void markup(){
    auto editor=lvui::compile_native(R"(<Window><InputField multiline={true} height={70}/></Window>)",{.framework="iiGUI"});
    check(editor&&editor->find("value.multiline = true; value.height = 70;")!=editor->npos,"Multiline editor height did not compile");
    auto result=lvui::compile_native(R"(<script>
import vm Backend
var caption: String = "Ready"
</script>
<ApplicationWindow title="Native">
  <div style="display: flex; flex-direction: column; gap: 12px; padding: 16px; align-items: stretch">
    <span text={vm.status}/>
    <div style="display: flex; flex-direction: row; gap: 8px; flex: 1">
      <Button on:click={vm.open}>Open &amp; edit</Button>
      <Label>{caption}</Label>
    </div>
  </div>
</ApplicationWindow>)");
    if(!result)throw std::runtime_error(result.error().message);
    check(result->find("native_bindings->bind(")!=result->npos&&result->find("set_caption")!=result->npos&&result->find("set_stack_layout")!=result->npos&&result->find(".flex = 1")!=result->npos&&result->find(".hstack(")!=result->npos&&result->find("Open & edit")!=result->npos,"Markup lost styles, events, entities or typed property binding");
    for(auto input:{"<Window><Label></Button></Window>","<Window><div style=\"position: absolute\"/></Window>","<Window><Label text=\"a\" text=\"b\"/></Window>","<script>alert(1)</script><Window><Label/></Window>","<Window><div><Label/>mixed</div></Window>","<Window><Label>&bogus;</Label></Window>"})check(!lvui::compile_native(input),"Invalid native markup was accepted");
}
void lvrs_tables(){
    auto r=lvui::compile_native(R"(import vm TableBackend
Window { VStack {
 TableHeader(columns: ["Name", "Count"], columnWidths: [300, 120], interactive: true, onColumnClicked: vm.sort)
 TableRow(cells: ["First", "2"], width: 420, cellWidth: 210, inputable: true, onCellInputEdited: vm.rowEdited)
 TableCellItem(text: "12", inputable: true, selected: true, current: true, valueType: "int", valueValidator: vm.validator, textStyle: .body, onInputEdited: vm.edited, onInputSubmitted: vm.submitted, onInputRejected: vm.rejected)
} })",{.framework="LVRS"});
    check(bool(r),r?"":"Native table generation failed");
    for(auto s:{"controls.table_cell_item(","controls.table_row(","controls.table_header(","scope->backend_0.validator","index, column","index, value","value, valueType"})check(r->find(s)!=std::string::npos,"Native table call or typed callback missing");
    for(auto s:{"Window { TableCellItem(inputable: 1) }","Window { TableCellItem(textStyle: .bad) }","Window { TableRow(cells: [1]) }","Window { TableHeader(columnWidths: [\"bad\"]) }","Window { TableCellItem { Label() } }","Window { TableRow(width: 0) }"})check(!lvui::compile_native(s,{.framework="LVRS"}),"Invalid native table accepted");
    auto independent=lvui::compile_native("Window { TableCellItem() }");
    check(independent&&independent->find("lvui::native::NativeTableCellItem::create(")!=independent->npos,"Independent TableCellItem must compile without LVRS");
}
void lvrs_sheet(){
    auto result=lvui::compile_native("import vm Backend\nWindow { Sheet(id: \"sheet\", open: true, title: \"Export\", presentation: .Mobile, detent: .medium, motion: vm.motion, topSafeInset: 40, bottomSafeInset: 34, animationDuration: 250, surfaceColor: \"#20304080\", onClosed: vm.closed) { VStack { InputField(id: \"editor\", onEdited: vm.edited) } } }",{.framework="LVRS"});
    if(!result)throw std::runtime_error(result.error().message);
    check(result->find("controls.sheet(")!=result->npos&&result->find("lvrs::SheetContentFactory")!=result->npos&&result->find("lvrs::SheetPresentation::mobile")!=result->npos&&result->find("lvrs::SheetDetent::medium")!=result->npos&&result->find("std::chrono::milliseconds(250)")!=result->npos&&result->find("mounted->find(\"sheet\")")!=result->npos,"Sheet retained factory, geometry, preferences or initial open missing");
    for(auto source:{"Window { Sheet(presentation: .bad) {} }","Window { Sheet(detent: .full) {} }","Window { Sheet(animationDuration: 1.5) {} }","Window { Sheet(scrollContent: 1) {} }","Window { Sheet(preferredWidth: -1) {} }","import vm Backend\nWindow { Sheet(motion: vm.motion()) {} }","Window { VStack { Label(id: \"duplicate\")\nSheet { Label(id: \"duplicate\") } } }"})check(!lvui::compile_native(source,{.framework="LVRS"}),"Invalid Sheet input or duplicate retained id accepted");
    check(bool(lvui::compile_native("Window { Sheet { Label(id: \"one\")\nLabel(id: \"two\") } }",{.framework="LVRS"})),"Sheet inline child views were narrowed to one child");
    auto independent=lvui::compile_native("Window { Sheet {} }");
    check(independent&&independent->find("lvui::native::NativeSheet::create(")!=independent->npos,
          "Default Sheet did not use LVUI's independent implementation");
}
void lvrs_modal(){
    auto motion=lvui::compile_native("import vm Backend\nWindow { Modal(motion: vm.motion, motionEnabled: false, frameColor: \"#ff000080\") {} }",{.framework="LVRS"});
    check(motion&&motion->find(".motion = lvui::native::initial(scope->backend_0.motion)")!=motion->npos&&motion->find(".motion_enabled = false")!=motion->npos,"Modal motion settings did not compile to scoped native members");
    for(auto source:{"import vm Backend\nWindow { Modal(motion: vm.motion()) {} }","Window { Modal(motion: missing.model) {} }","Window { Modal(motionEnabled: 1) {} }"})check(!lvui::compile_native(source,{.framework="LVRS"}),"Invalid Modal motion model accepted");
    auto result=lvui::compile_native("import vm Backend\nWindow { VStack { Label(id: \"same\", text: \"Outer\")\nModal(id: \"modal\", title: \"Native Modal\", desc: \"Description\", iconName: \"projectStructure\", buttonCount: 2, open: true, onPrimaryClicked: vm.apply, onCanceled: vm.canceled) { VStack { InputField(id: \"same\", onEdited: vm.edited) } } } }",{.framework="LVRS"});
    if(!result)throw std::runtime_error(result.error().message);
    check(result->find("controls.modal(")!=result->npos&&result->find("ModalContentFactory")!=result->npos&&result->find("ii::gui::Widget content")!=result->npos&&result->find("controls.input_field(content")==result->npos&&result->find("mounted->find(\"modal\")")!=result->npos,"Modal child factory or initial open was not compiled");
    check(result->find(".description = \"Description\"")!=result->npos&&result->find(".button_count = 2")!=result->npos&&result->find("object).edited(std::forward")!=result->npos,"Modal inputs or borrowed backend callback missing");
    for(auto source:{"Window { Modal(minWidth: 900, maxWidth: 400) {} }","Window { Modal { Label()\nLabel() } }","Window { Modal(shapeStyle: .bad) {} }","Window { Modal(frameColor: \"bad\") {} }","Window { Modal { Modal(open: true) {} } }","Window { Modal(desc: \"a\", description: \"b\") {} }","Window { VStack { Label(id: \"outer\")\nModal { ContextMenu(targetId: \"outer\") {} } } }"})check(!lvui::compile_native(source,{.framework="LVRS"}),"Invalid Modal or cross-scope binding accepted");
    auto independent=lvui::compile_native("Window { Modal(motionEnabled: false) {} }");
    check(independent&&independent->find("lvui::native::NativeModal::create(")!=independent->npos,
          "Default Modal did not use LVUI's independent implementation");
}
void editor_styles(){
    auto code=lvui::compile_native(R"(<script>
import vm Backend
</script>
<Window><div style="display: flex; flex-direction: column">
<IconButton icon={vm.hand} text="Pan" tone={.borderless} iconSize={16} height={28} horizontalPadding={4} cornerRadius={4}/>
<Slider thumbDiameter={12} trackHeight={4} rangeInset={6} height={12}/>
<Hierarchy model={vm.layers} rowHeight={20} backgroundColor="#242625" selectionColor="#24324d" selectionRadius={0} fontSize={11}/>
<ColorPickerButton model={vm.color} circular={true} borderOpacity={0.15} showHueRing={false}/>
</div></Window>)",{.framework="LVRS"});
    if(!code)throw std::runtime_error(code.error().message);
    for(auto expected:{".icon = lvui::native::initial(scope->backend_0.hand)", ".height = 28", ".horizontal_padding = 4", ".thumb_diameter = 12", ".height = 12", ".track_height = 4", ".range_inset = 6", ".selection_radius = 0", ".circular = true", ".border_opacity = 0.15", ".font_size = 11"})check(code->find(expected)!=code->npos,"Editor appearance contract lost during AOT");
    for(auto source:{"Window { Slider(thumbDiameter: 0) }","Window { Hierarchy(model: vm.layers, selectionRadius: -1) }","Window { IconButton(height: 0) }","Window { IconButton(icon: \"file.svg\") }"})check(!lvui::compile_native(source,{.framework="LVRS"}),"Invalid editor geometry or unscoped image accepted");
}
void lvrs_popover(){
    auto result=lvui::compile_native("import vm Backend\nWindow { VStack { LabelButton(text: \"Open\", onClick: vm.open)\nPopover(id: \"popup\", modal: false, surfaceColor: \"#141414\", clip: true, onOpened: vm.opened, onClosed: vm.closed) { VStack { InputField()\nColorPicker(model: vm.model) } } } }",{.framework="LVRS"});
    if(!result)throw std::runtime_error(result.error().message);
    check(result->find("controls.popover(")!=result->npos&&result->find(".clip_content = true")!=result->npos&&result->find("controls.color_picker(")!=result->npos&&result->find(".opened = lvui::native::returned_callback(")!=result->npos,"Native popover/content/lifetime callbacks not generated");
    for(auto source:{"Window { Popover(width: -1) {} }","Window { Popover(surfaceColor: \"invalid\") {} }","Window { Popover { Label()\nLabel() } }","import vm Backend\nWindow { Popover(onOpened: 1) {} }"})check(!lvui::compile_native(source,{.framework="LVRS"}),"Invalid popover contract accepted");
    auto independent=lvui::compile_native("Window { Popover {} }");
    check(independent&&independent->find("lvui::native::NativePopover::create(")!=independent->npos,"Default Popover did not use LVUI's independent implementation");
}
}
int main(int argc, char** argv) {
    try {
        std::string_view name = argc > 1 ? argv[1] : "generation";
        if (name == "generation") generation(); else if (name == "validation") validation();
        else if (name == "actions") actions(); else if (name == "escaping") escaping();
        else if (name == "native_widget") native_widget();
        else if (name == "embedded_build") embedded_build();
        else if (name == "lvrs_router") lvrs_router();
        else if (name == "lvrs_link") lvrs_link();
        else if (name == "lvrs_materials") lvrs_materials();
        else if (name == "lvrs_code_editor") lvrs_code_editor();
        else if (name == "lvrs_tooltip") lvrs_tooltip();
        else if (name == "lvrs_media") lvrs_media();
        else if (name == "lvrs_toolbars") lvrs_toolbars();
        else if (name == "lvrs_app_surfaces") lvrs_app_surfaces();
        else if (name == "lvrs_controls") lvrs_controls();
        else if (name == "lvrs_compound") lvrs_compound();
        else if (name == "lvrs_menus") lvrs_menus();
        else if (name == "lvrs_color_fields") lvrs_color_fields();
        else if (name == "lvrs_color_picker") lvrs_color_picker();
        else if (name == "editor_styles") editor_styles();
        else if (name == "lvrs_popover") lvrs_popover();
        else if (name == "lvrs_color_button") lvrs_color_button();
        else if (name == "lvrs_modal") lvrs_modal();
        else if (name == "lvrs_sheet") lvrs_sheet();
        else if (name == "lvrs_tables") lvrs_tables();
        else if (name == "lvrs_table_view") lvrs_table_view();
        else if (name == "markup") markup();
        else if (name == "lvrs_lists") lvrs_lists();
        else if (name == "lvrs_alerts") lvrs_alerts();
        else throw std::runtime_error("Unknown test");
        std::cout << "LVUI AOT " << name << " passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
