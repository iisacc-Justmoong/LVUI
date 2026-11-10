#include <LVUI/Compiler.h>

#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
std::string compile(std::string_view source) {
    auto result = lvui::compile_native(source, {.name = "FigmaContract"});
    if (!result) throw std::runtime_error(result.error().message);
    return *result;
}

void same(std::string_view figma, std::string_view canonical) {
    if (compile(figma) != compile(canonical))
        throw std::runtime_error("Figma component declaration differs from its canonical native view");
}

void reject(std::string_view source) {
    if (lvui::compile_native(source)) throw std::runtime_error("Conflicting Figma inputs were accepted");
}
}

int main() {
    try {
        same("Window { PushButton { type: LabelButton, kind: accent } }",
             "Window { LabelButton(tone: .primary) }");
        same("Window { PushButton { kind: default } }",
             "Window { PushButton(tone: .default) }");
        same("Window { Slider { type: Filled, size: Mini } }",
             "Window { Slider(type: .filled, size: .mini) }");
        same("Window { Checkbox { state: On } }", "Window { CheckBox(checked: true) }");
        same("Window { Radio { state: Off } }", "Window { RadioButton(checked: false) }");
        same("Window { Toggle { state: On } }", "Window { ToggleSwitch(checked: true) }");
        same("Window { CheckBox() }", "Window { CheckBox(checked: true) }");
        same("Window { RadioButton() }", "Window { RadioButton(checked: true) }");
        same("Window { ToggleSwitch() }", "Window { ToggleSwitch(checked: true) }");
        same("Window { Slider() }", "Window { Slider(size: .mini) }");
        same("Window { TextField() }", "Window { InputField() }");
        same("Window { InputField() }", "Window { InputField(surface: .rounded) }");
        same("Window { GeneralSearch() }", "Window { SearchField() }");
        same("Window { SmallList() }", "Window { List() }");
        same("Window { Icon(name: \"projectStructure\") }", "Window { Image(iconName: \"projectStructure\") }");
        same("Window { Material { type: Window } }", "Window { WindowMaterial() }");
        same("Window { Card { type: Folder } }", "Window { Card(type: .Folder) }");
        same("Window { Card() }", "Window { Card(size: .Small) }");
        same("Window { Avatar() }", "Window { Avatar(size: .Large) }");
        same("Window { Avatar { size: Small } }", "Window { Avatar(size: .Small) }");
        same("Window { ColorPickerButton() }", "Window { ColorPickerButton(buttonSize: .Small) }");
        same("Window { ColorPickerButton { size: Large } }", "Window { ColorPickerButton(buttonSize: .Large) }");
        same("Window { Alert() }", "Window { Alert(buttonCount: 2) }");
        same("Window { Alert { amount: 3 } }", "Window { Alert(buttonCount: 3) }");
        same("Window { ListItem { type: Navigation, kind: Selected } }",
             "Window { ListItem(type: .Navigation, selected: true, enabled: true) }");
        same("Window { TextField { style: Cylinder, mode: Password } }",
             "Window { InputField(surface: .cylinder, password: true) }");
        same("Window { Stepper { arrow: UpDown, tone: Primary } }",
             "Window { Stepper(arrow: .UpDown, tone: .Primary) }");
        same("Window { ComboBox { arrow: UpDown, tone: primary } }",
             "Window { ComboBox(arrow: .UpDown, tone: .primary) }");
        same("Window { Sheet { presentation: MobileLarge } }",
             "Window { Sheet(presentation: .mobile, detent: .large) }");
        same("Window { Sheet() }", "Window { Sheet(presentation: .mobile, detent: .fit) }");
        same("Window { Tab(iconName: \"home\") { style: Surface } }",
             "Window { Tab(iconName: \"home\", tabStyle: .surface) }");
        same("Window { Card { state: Selected } }", "Window { Card(displayState: .SelectedState) }");
        same("import vm Example\nWindow { ColorPickerPanel(model: vm.color) { type: RGB } }",
             "import vm Example\nWindow { ColorPicker(model: vm.color, type: .RGB) }");
        (void)compile("import vm Example\nWindow { ListItem(type: vm.item_type) }");
        reject("Window { PushButton(kind: accent, tone: .borderless) }");
        reject("Window { CheckBox(state: On, checked: false) }");
        std::cout << "Figma component contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
