#include <LVUI/Compiler.h>
#include <LVUI/Language.h>
#include <iostream>
#include <stdexcept>
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::string compile(std::string_view source){auto result=lvui::compile_native(source);if(!result)throw std::runtime_error(result.error().message);return *result;}
void syntax(){
    auto parsed=lvui::parse_view("import vm Backend\nWindow { for (item in vm.items, key: item.id, index: ordinal, id: \"rows\", first: vm.first, count: vm.count) { Label(text: item.title) } }");
    check(bool(parsed),"A keyed repeated View was rejected");
    const auto code=compile("Window { for (item in [1, 2, 3], key: item) { Label(text: item) } }");
    check(code.find("set_keyed_content")!=code.npos&&code.find("collection_frame")!=code.npos,
          "Repetition did not compile into native keyed state and widgets");
    check(code.find("parse_view")==code.npos&&code.find("ValueKind")==code.npos,"Repetition shipped its source parser or AST");
    const auto declared=compile("let names = [\"A\", \"B\"]\nWindow { for (name in names, key: name) { Label(text: name) } }");
    check(declared.find("inferred_names")!=declared.npos&&declared.find("Mode::readonly")!=declared.npos,"An ordinary list declaration did not produce native reactive storage");
}
void scope(){
    const auto code=compile("import vm Backend\nlet Cell(title: String): View = Label(text: title)\nWindow { for (item in vm.items, key: item.id, index: position) { VStack { import vm LocalBackend\nCell(title: item.title)\nLabel(text: position)\nLabel(text: vm.title) } } }");
    check(code.find("row->state")!=code.npos&&code.find("row->revision")!=code.npos&&code.find("::LocalBackend backend_0")!=code.npos,
          "Repeated scope did not isolate its local import and row data");
    const auto nested=compile("import vm Backend\nWindow { for (row in vm.items, key: row.id) { for (child in row.children, key: child.id) { Label(text: child.title) } } }");
    check(nested.find("RepeatedView_1")!=nested.npos,"A nested repetition did not produce independent native row scopes");
    const auto shadowed=compile("Window { VStack { let item: String = \"Outer\"\nfor (item in [1,2], key: item) { Label(text: item) }\nLabel(text: item) } }");
    check(shadowed.find("Outer")!=shadowed.npos,"A repeated item escaped or removed its outer shadowed value");
}
void projection(){
    const auto code=compile("import vm Backend\nWindow { for (item in vm.items, key: item.id, first: vm.first, count: vm.count, spacing: 4, horizontal: true) { Label(text: item.title) } }");
    check(code.find(".first = ")!=code.npos&&code.find(".count = ")!=code.npos&&code.find(".horizontal = ")!=code.npos,
          "A repeated viewport or layout option was discarded");
    const auto automatic=compile("import vm Backend\nWindow { for (item in vm.items, key: item.id, itemHeight: 80, itemWidth: 150, overscan: 2, width: vm.width, height: 400) { Label(text: item.title) } }");
    check(automatic.find("measurements->plan")!=automatic.npos&&automatic.find("set_scroll_handler")!=automatic.npos&&
          automatic.find("set_virtual_scroll_content")!=automatic.npos,"Automatic virtualization did not generate native viewport planning and delivery");
    const auto measured=compile("import vm Backend\nlet Cell(hidden: Bool): View = if (hidden) { Label(text: \"scope->parent->literal\") } else { Label() }\nWindow { for (item in vm.items, key: item.id, itemHeight: 40, measureHeight: true) { VStack { Label(text: item.title)\nCell(hidden: vm.hidden)\nLabel(text: vm.caption()) } } }");
    const auto begin=measured.find("auto ambient = native_bindings->derive"),end=measured.find("auto ambient_state",begin);
    const auto ambient=measured.substr(begin,end-begin);
    check(ambient.find("scope->backend_0.hidden")!=ambient.npos&&ambient.find("scope->revision_0")!=ambient.npos&&
          ambient.find("scope->row")==ambient.npos&&ambient.find(".caption(")==ambient.npos,
          "Measured rows failed to capture ancestor properties and method revisions without borrowing row state or invoking methods twice");
    const auto wrapped=compile("import vm Backend\nlet Rows(hidden: Bool): View = for (item in vm.items, key: item.id, itemHeight: 40, measureHeight: true) { if (hidden) { VStack {} } else { Label(text: item.title) } }\nWindow { Rows(hidden: vm.hidden) }");
    const auto inherited=wrapped.substr(wrapped.find("auto ambient = native_bindings->derive"));
    check(inherited.substr(0,inherited.find("auto ambient_state")).find("scope->parent->backend_0.hidden")!=inherited.npos,
          "A measured repetition lost an ancestor dependency passed through a scalar component parameter");
}
void diagnostics(){
    for(auto source:{
        "Window { for (item in [1]) { Label() } }",
        "Window { for (item [1], key: item) { Label() } }",
        "Window { for (item in [1], key: item, key: item) { Label() } }",
        "Window { for (item in [1], key: item, index: item) { Label() } }",
        "Window { for (item in [1], key: item, count: -1) { Label() } }",
        "Window { for (item in [1], key: item, unknown: 2) { Label() } }",
        "Window { for (item in [1], key: item, itemHeight: 0) { Label() } }",
        "Window { for (item in [1], key: item, itemHeight: 20, first: 0) { Label() } }",
        "Window { for (item in [1], key: item, itemHeight: 20, count: 1) { Label() } }",
        "Window { for (item in [1], key: item, itemWidth: 80) { Label() } }",
        "Window { for (item in [1], key: item, itemHeight: 20, columns: 1.5) { Label() } }",
        "Window { for (item in [1], key: item, itemHeight: 20, horizontal: true) { Label() } }",
        "Window { for (item in [1], key: item, itemHeight: 20, itemWidth: 30, horizontal: true, columns: 2) { Label() } }",
        "Window { for (item in [1], key: item, itemHeight: 20, flex: 4097) { Label() } }",
        "Window { for (item in [1], key: item, measureHeight: true) { Label() } }",
        "Window { for (item in [1], key: item, itemHeight: 20, itemWidth: 20, measureHeight: true, horizontal: true) { Label() } }",
        "Window { for (item in [1], key: item, navigation: true) { Label() } }",
        "Window { for (item in [1], key: item, itemHeight: 20, navigation: true) { Label() } }",
        "Window { for (item in [1], key: item, itemHeight: 20, selectionMode: .multiple) { Label() } }",
        "import nav lvui.native.CollectionNavigation\nWindow { for (item in [1], key: item, itemHeight: 20, navigation: nav, selectionMode: .invalid) { Label() } }",
        "Window { for (item in [1], key: item) { Label()\nLabel() } }",
        "Window { for (item in [], key: item) { Label() } }",
        "let items = []\nWindow { Label() }",
        "let items: String = [1,2]\nWindow { Label() }",
        "let items = [1,2]\nlet copied = items\nlet wrong: String = copied\nWindow { Label() }",
        "Window { VStack { for (item in [1], key: item) { Label() }\nLabel(text: item) } }",
        "let Wrong: View = Label(text: item.title)\nimport vm Backend\nWindow { for (item in vm.items, key: item.id) { Wrong() } }",
        "for (item in [1], key: item) { Label() }"}) {
        auto result=lvui::compile_native(source);
        check(!result&&result.error().line>0&&result.error().column>0,"Invalid repetition was accepted without a source diagnostic");
    }
}
void navigation(){
    const auto code=compile("import vm Backend\nimport nav lvui.native.CollectionNavigation\nWindow { for (item in vm.items, key: item.id, itemHeight: 80, navigation: nav, selectionMode: .multiple, selectOnMove: false, navigationFilter: item.enabled, onActivate: vm.activate) { Label(text: nav.state.current_key) } }");
    check(code.find("CollectionWidgetNavigation")!=code.npos&&code.find("navigation->publish(frame)")!=code.npos&&
          code.find("CollectionSelectionMode::multiple")!=code.npos&&code.find("object).activate(std::forward")!=code.npos,
          "Navigated repetition omitted its native adapter, presentation, options or C++ endpoint");
}
}
int main(int argc,char** argv){try{
    const std::string name=argc>1?argv[1]:"syntax";
    if(name=="syntax")syntax();else if(name=="scope")scope();else if(name=="projection")projection();else if(name=="diagnostics")diagnostics();else if(name=="navigation")navigation();
    else throw std::runtime_error("Unknown repetition test");
    std::cout<<"LVUI repetition "<<name<<" passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
