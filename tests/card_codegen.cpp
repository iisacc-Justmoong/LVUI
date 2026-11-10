#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main() {
    auto code=lvui::compile_native("import vm Model\nWindow { Card(type: .Project, title: vm.title, selected: vm.selected, rows: vm.rows, onClick: vm.clicked, onSelectedChanged: vm.selection) }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto part:{"#include <LVUI/NativeCard.h>","lvui::native::CardBinding<", "iiGUI/NativeCardView.h","lvui::native::CardInput",
                   "lvui::CardType::project","component.set_options(value)","bool value"})
        if(code->find(part)==code->npos)throw std::runtime_error(std::string("Independent card lost ")+part);
    if(code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos||code->find("<Qt")!=code->npos)
        throw std::runtime_error("Default card depends on legacy widgets");
    for(auto source:{"Window { Card() }","Window { Card(type: .FilePreview) }","Window { Card(type: .Device, size: .Small, detail: .Detailed, displayState: .HoverState) }"}){
        auto result=lvui::compile_native(source);if(!result||result->find("CardBinding<")==result->npos)throw std::runtime_error("Independent card was not generated");
    }
    for(auto source:{"Window { Card(type: .Model) { VStack { Label(text: \"Details\") } } }",
                     "let Preview: View = Label(text: \"Preview\")\nWindow { Card(previewComponent: Preview) }",
                     "Window { Card(type: .Model, iconComponent: Label(text: \"Icon\")) }",
                     "Window { Card(previewComponent: Label(), previewSource: \"a.png\") }",
                     "Window { Card(type: .Model, iconComponent: Label(), iconName: \"home\") }",
                     "Window { Card(contentComponent: Label(text: \"Body\")) }"}){
        auto result=lvui::compile_native(source);
        if(!result||result->find("NativeCardViews")==result->npos)throw std::runtime_error(result?"Card View factory was omitted":result.error().message);
    }
    auto counter=lvui::compile_native(R"(Card(type: .Model, width: 256, height: 280, showMenu: false, showAction: false) {
    VStack {
        var count: Int = 0
        Label(text: count)
        PushButton(text: "+", onClick: { count = count + 1 })
    }
})");
    if(!counter||counter->find("NativeCardViews")==std::string::npos)throw std::runtime_error(counter?"Card counter View was omitted":counter.error().message);
    for(auto source:{"Window { Card(type: .Unknown) }","Window { Card(progress: 2) }","Window { Card(width: -1) }",
                     "Window { Card(title: \"A\", filename: \"B\") }","Window { Card(icon: 1, iconName: \"home\") }",
                     "Window { Card { Label()\nLabel() } }",
                     "Window { Card(contentComponent: Label(), previewComponent: Label()) }",
                     "Window { Card(contentComponent: Label()) { Label() } }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid independent card input was accepted");
}
