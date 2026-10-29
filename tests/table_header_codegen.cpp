#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { TableHeader(columns: vm.columns, columnWidths: [100, 120], interactive: true, onColumnClicked: vm.clicked) }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto part:{"#include <LVUI/NativeTableHeader.h>","lvui::native::NativeTableHeader::create(","lvui::native::TableHeaderInput",
                   "lvui::native::table_header_columns(","lvui::native::table_header_widths(","component.set_options(value)","const lvui::TableColumn& column"})
        if(code->find(part)==code->npos)throw std::runtime_error(std::string("Independent table header lost ")+part);
    if(code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos||code->find("<Qt")!=code->npos)
        throw std::runtime_error("Default table header depends on legacy widgets");
    for(auto source:{"Window { TableHeader() }","Window { TableHeader(columns: [\"Name\", \"Count\"], columnWidths: [100, 120]) }"}){
        auto result=lvui::compile_native(source);if(!result||result->find("NativeTableHeader::create")==result->npos)throw std::runtime_error("Table header was not generated");
    }
    for(auto source:{"Window { TableHeader(width: 0) }","Window { TableHeader(columns: [1]) }","Window { TableHeader(columnWidths: [\"bad\"]) }",
                     "Window { TableHeader(columnWidths: [-2]) }","Window { TableHeader { Label() } }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid independent header input was accepted");
}
