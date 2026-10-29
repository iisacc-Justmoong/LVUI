#include <LVUI/Compiler.h>
#include <LVUI/Language.h>
#include <iostream>
int main() {
    constexpr auto source = R"lvui(
import class Domain.Model
import document Domain.Model
import navigation lvui.native.CollectionNavigation
let Body(backend: Model, title: String = "Native", content: View = Label(), action: () -> Void = backend.increment): View = VStack {
    Label(text: title)
    Label(text: backend.title)
    PushButton(backend.increment)
    PushButton(backend.makeHandler())
    PushButton(action)
    content()
}
let Sequence(title: String): [View] = [[Label(text: title)], Label(text: title)]
let SequencePanel(content: [View] = []): View = HStack { content() }
let format: (Int) -> String = document.format
let dynamic: (Int) -> String = document.makeFormatter()
@Static let frozen: (Int) -> String = dynamic
var editable: (Int) -> String = document.format
var ink: Color = .primary
let Paint(ink: Color = .primary): View = Label(color: ink)
let Shared(title: String, fallback: String = title): View = HStack {
    Label(text: title)
    Label(text: fallback)
}
let Formatted(render: (Int) -> String, title: String = render(document.count)): View = Label(text: title)
Window { VStack {
    Label(text: format(document.count))
    Label(text: frozen(document.count))
    Label(text: editable(document.count))
    Body(backend: document)
    Paint(ink: ink)
    VStack(spacing: document.count, padding: document.padding, width: document.width, height: document.height,
           minimumWidth: document.minimumWidth, alignment: .leading) { Label(text: "Native layout") }
    Shared(title: document.title)
    Formatted(render: document.makeFormatter())
    SequencePanel(content: Sequence(title: document.title))
    SequencePanel()
    for (item in document.items, key: item.id) { Label(text: item.title) }
    for (item in document.items, key: item.id, itemHeight: 80, itemWidth: 150,
         measureHeight: true, measureRevision: document.layoutRevision,
         navigation: navigation, selectionMode: .multiple,
         onActivate: document.open) { Label(text: item.title) }
} }
)lvui";
    auto document = lvui::parse_view(source);
    if (!document || !document->bindings()[0].parameters || document->bindings()[0].parameters->size() != 4 ||
        !document->bindings()[0].parameters->back().callable || document->bindings()[1].type!="[View]" ||
        !document->bindings()[2].parameters || document->bindings()[2].parameters->front().type!="[View]" ||
        document->root().fragment) return 1;
    auto generated = lvui::compile_native(source);
    if (!generated || generated->find("object).increment(std::forward") == generated->npos ||
        generated->find("return (scope->backend_0);") == generated->npos ||
        generated->find("parse_view") != generated->npos || generated->find("set_keyed_content") == generated->npos ||
        generated->find("virtual_collection_source") == generated->npos || generated->find("set_virtual_scroll_content") == generated->npos ||
        generated->find("CollectionWidgetNavigation") == generated->npos || generated->find("object).open(std::forward") == generated->npos ||
        generated->find("lvui::native::returned_callback(") == generated->npos ||
        generated->find("factories.create<::Domain::Model>") == generated->npos || generated->find("parent->backend_factories") == generated->npos ||
        generated->find("invoke_callback<void()>") == generated->npos || generated->find("invoke_callback<std::string(std::int64_t)>") == generated->npos ||
        generated->find("snapshot_callback_value") == generated->npos || generated->find("set_editable_native(") == generated->npos ||
        generated->find("FunctionVariable<std::string(std::int64_t)>") == generated->npos ||
        generated->find("widgets.hstack(") == generated->npos || generated->find("component_input(scope,") == generated->npos ||
        generated->find("component_storage(scope,") == generated->npos || generated->find("set_stack_layout(") == generated->npos ||
        generated->find("NativeWidgetLayout.h") == generated->npos) return 2;
    auto window=lvui::compile_native("import vm Model\nWindow(title: vm.title, width: vm.width) { Label() }");
    if(!window||window->find("window_properties()")==window->npos||window->find("NativeWindowValues.h")==window->npos)return 100;
    auto object=lvui::compile_native("import class Domain.Record\nimport vm Model\nlet Details(item: Record, again: Record = item): View = Label(text: again.title)\nWindow { Details(item: vm.record) }");
    if(!object||object->find("object_input<::Domain::Record>")==object->npos||object->find("NativeObjectInputs.h")==object->npos)return 101;
    auto method=lvui::compile_native("import vm Model\nWindow { Label(text: vm.selected.format(2)) }");
    if(!method||method->find("Methods::query(context")==method->npos)return 102;
    std::cout << "Relocated LVUI component compiler consumer passed\n";
}
