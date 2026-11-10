#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main() {
    auto code = lvui::compile_native("import vm Model\nWindow { Table(model: vm.controller, accessibilityText: vm.tableText, headerColumnAppearance: vm.headers, headerIconSource: vm.icons, headerIconRevision: vm.iconRevision, headerDefaultLeadingIcon: vm.leading, headerDefaultTrailingIcon: \"\", headerIconSize: 18, headerIconSpacing: 7, editor: vm.editor, orderEditor: vm.order, sorter: vm.sorter, structureEditor: vm.structure, structureFactory: vm.factory, structureControlsVisible: true, addRowControlsVisible: vm.rows, addColumnControlsVisible: false, deleteContextMenuEnabled: vm.menu, selectOnContextClick: false, structureGutterWidth: 22, structureGutterHeight: 24, mergeEditor: vm.merge, editable: true, clipboardEnabled: vm.clipboard, sortingEnabled: true, columnResizeEnabled: true, rowResizeEnabled: true, modelRevision: vm.revision, width: vm.width, onSelectionChanged: vm.selected, onCellInputSubmitted: vm.submitted, onRangeValuesChanged: vm.rangeChanged, onRowsSorted: vm.sorted, onStructureChanged: vm.structured, onColumnResized: vm.columnResized, onMergeChanged: vm.merged) }");
    if (!code) throw std::runtime_error(code.error().message);
    for (auto part : {"#include <LVUI/NativeTable.h>", "detail_table::NativeTable::create(",
                      "lvui::native::TableInput", "component.set_options(value)", "value.text", "value.header_presentation.column_appearance", "value.header_icon_source", "value.header_icon_revision", "std::optional<lvui::TableSelection>", "lvui::native::TableCellEvent", "const lvui::TableRange& value", "value.clipboard_enabled", "value.order_editor", "lvui::TableSortOrder order", "value.structure_editor", "const lvui::TableStructureEdit& value", "value.merge_editor", "const lvui::TableMergeEdit& value", "value.structure_factory", "value.structure_controls_visible", "value.add_row_controls_visible", "value.add_column_controls_visible", "value.delete_context_menu_enabled", "value.select_on_context_click", "value.structure_gutter_width", "value.structure_gutter_height"})
        if (code->find(part) == code->npos) throw std::runtime_error(std::string("Independent table lost ") + part);
    if (code->find("lvrs::") != code->npos || code->find("<LVRS/") != code->npos || code->find("<Qt") != code->npos)
        throw std::runtime_error("Default table depends on legacy widgets");
    auto empty = lvui::compile_native("Window { Table() }");
    if (!empty || empty->find("std::make_shared<lvui::TableMemory>()") == empty->npos)
        throw std::runtime_error("An empty table did not own its default source");
    auto composed=lvui::compile_native("import vm Model\nWindow { Table(model: vm.controller, headerContext: \"heading\", headerDelegate: Label(text: heading.text), cellContext: \"cell\", cellDelegate: Label(text: cell.text)) }");
    if(!composed||composed->find("NativeTableViews")==std::string::npos||composed->find("TableViewContext")==std::string::npos)
        throw std::runtime_error(composed?"Table View delegates were omitted":composed.error().message);
    auto columns=lvui::compile_native("import vm Model\nWindow { Table(model: vm.controller, headerContext: \"heading\", cellContext: \"cell\", columnViews: [TableColumn(key: \"name\", header: Label(text: heading.text), cell: Label(text: cell.text)), TableColumn(key: \"size\", cell: Label(text: cell.text))]) }");
    if(!columns||columns->find(".headers = ")==std::string::npos||columns->find(".columns = ")==std::string::npos)
        throw std::runtime_error(columns?"Column-specific Views were omitted":columns.error().message);
    auto columnInputs=lvui::compile_native("Window { Table(columnViews: [TableColumn(key: \"name\", header: Label())]) }");
    auto columnBody=lvui::compile_native("Window { Table(columnViews: [TableColumn() { key: \"name\", header: Label() }]) }");
    if(!columnInputs||!columnBody||*columnInputs!=*columnBody)
        throw std::runtime_error("TableColumn constructor and View body produced different output");
    for (auto source : {"Window { Table(width: 0) }", "Window { Table(height: 24, headerHeight: 24) }",
                        "Window { Table(minColumnWidth: 200, fallbackCellWidth: 100) }", "Window { Table(modelRevision: 1.5) }",
                        "Window { Table(editable: true) }", "Window { Table(accessibilityText: \"bad\") }", "Window { Table(headerColumnAppearance: \"bad\") }", "Window { Table(headerIconSource: \"bad\") }", "Window { Table(headerIconSize: 0.5) }", "Window { Table(headerIconRevision: 1.5) }", "Window { Table(sortingEnabled: true) }", "Window { Table(columnResizeEnabled: true) }", "Window { Table(structureControlsVisible: true) }", "Window { Table(structureGutterWidth: 0) }",
                        "import vm Model\nWindow { Table(model: vm.model, controller: vm.controller) }",
                        "Window { Table(headerContext: \"item\") }", "Window { Table(cellContext: \"item\") }",
                        "Window { Table(cellContext: \"invalid-name\", cellDelegate: Label()) }",
                        "Window { Table(columnViews: [TableColumn(key: \"name\", cell: Label()), TableColumn(key: \"name\", cell: Label())]) }",
                        "Window { Table(columnViews: [TableColumn(key: \"name\")]) }",
                        "Window { Table(columnViews: [TableColumn(key: 7, cell: Label())]) }",
                        "Window { Table(columnViews: [TableColumn(key: \"name\", header: Label(), unknown: 1)]) }",
                        "Window { Table(columnViews: [TableColumn(key: \"name\", header: Label()) { key: \"other\" }]) }",
                        "Window { Table(columnViews: [{key: \"name\", cell: Label()}]) }"})
        if (lvui::compile_native(source)) throw std::runtime_error("An invalid or unsupported independent table input was accepted");
}
