#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { TableRow(cells: vm.cells, inputable: true, onCellInputEdited: vm.edited, onCellInputSubmitted: vm.submitted, onCellInputRejected: vm.rejected) }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto part:{"#include <LVUI/NativeTableRow.h>","lvui::native::NativeTableRow::create(","lvui::native::TableRowInput",
                   "lvui::native::table_row_cells(","component.set_options(value)","std::size_t index, std::string value, std::string valueType"})
        if(code->find(part)==code->npos)throw std::runtime_error(std::string("Independent table row lost ")+part);
    if(code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos||code->find("<Qt")!=code->npos)
        throw std::runtime_error("Default table row depends on legacy widgets");
    for(auto source:{"Window { TableRow() }","Window { TableRow(cells: [\"First\", \"Second\"], cellWidth: 100) }"}){
        auto result=lvui::compile_native(source);if(!result||result->find("NativeTableRow::create")==result->npos)throw std::runtime_error("Table row was not generated");
    }
    for(auto source:{"Window { TableRow(width: 0) }","Window { TableRow(cells: [1]) }","Window { TableRow(cellWidth: -1) }",
                     "Window { TableRow(inputable: 1) }","Window { TableRow { Label() } }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid independent table row input was accepted");
}
