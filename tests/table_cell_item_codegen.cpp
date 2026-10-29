#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { TableCellItem(text: vm.text, valueType: \"int\", valueValidator: vm.validator, inputable: true, onInputEdited: vm.edited, onInputSubmitted: vm.submitted, onInputRejected: vm.rejected) }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto part:{"#include <LVUI/NativeTableCellItem.h>","lvui::native::NativeTableCellItem::create(","lvui::native::TableCellItemInput",
                   "component.set_options(value)","std::string value, std::string valueType"})
        if(code->find(part)==code->npos)throw std::runtime_error(std::string("Independent table cell lost ")+part);
    if(code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos||code->find("<Qt")!=code->npos)
        throw std::runtime_error("Default table cell depends on legacy widgets");
    auto empty=lvui::compile_native("Window { TableCellItem() }");
    if(!empty||empty->find("NativeTableCellItem::create")==empty->npos)throw std::runtime_error("Default table cell was not generated");
    for(auto source:{"Window { TableCellItem(width: 0) }","Window { TableCellItem(inputable: 1) }",
                     "Window { TableCellItem(textStyle: .bad) }","Window { TableCellItem(valueValidator: 1) }",
                     "Window { TableCellItem { Label() } }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid independent table cell input was accepted");
}
