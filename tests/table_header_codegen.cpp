#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { TableHeader(columns: vm.columns, accessibilityText: vm.tableText, sort: vm.sort, columnWidths: [100, 120], columnAppearance: vm.appearance, defaultLeadingIcon: vm.iconName, defaultTrailingIcon: \"\", iconSource: vm.icons, iconRevision: vm.iconRevision, iconSize: 18, iconSpacing: 7, interactive: true, onColumnClicked: vm.clicked) }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto part:{"#include <LVUI/NativeTableHeader.h>","lvui_generated::detail_table_header::NativeTableHeader::create(","lvui::native::TableHeaderInput",
                   "lvui::native::table_header_columns(","lvui::native::table_header_widths(","value.text", "value.configuration.sort", "value.configuration.column_appearance", "value.configuration.default_leading_icon", "value.icon_source", "value.icon_revision", "component.set_options(value)","const lvui::TableColumn& column"})
        if(code->find(part)==code->npos)throw std::runtime_error(std::string("Independent table header lost ")+part);
    if(code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos||code->find("<Qt")!=code->npos)
        throw std::runtime_error("Default table header depends on legacy widgets");
    for(auto source:{"Window { TableHeader() }","Window { TableHeader(columns: [\"Name\", \"Count\"], columnWidths: [100, 120]) }"}){
        auto result=lvui::compile_native(source);if(!result||result->find("NativeTableHeader::create")==result->npos)throw std::runtime_error("Table header was not generated");
    }
    auto composed=lvui::compile_native("TableHeader(columns: [\"Name\"], cellContext: \"column\", cellDelegate: Label(text: column.text))");
    if(!composed||composed->find("NativeTableViews")==std::string::npos)throw std::runtime_error(composed?"Header View delegate was omitted":composed.error().message);
    for(auto source:{"Window { TableHeader(width: 0) }","Window { TableHeader(accessibilityText: \"bad\") }","Window { TableHeader(sort: \"bad\") }","Window { TableHeader(columnAppearance: \"bad\") }","Window { TableHeader(iconSource: \"bad\") }","Window { TableHeader(iconSize: 0.5) }","Window { TableHeader(iconSize: 257) }","Window { TableHeader(iconRevision: 1.5) }","Window { TableHeader(columns: [1]) }","Window { TableHeader(columnWidths: [\"bad\"]) }",
                     "Window { TableHeader(columnWidths: [-2]) }","Window { TableHeader { Label() } }",
                     "Window { TableHeader(cellContext: \"column\") }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid independent header input was accepted");
}
