#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main() {
    auto code = lvui::compile_native("import vm Model\nWindow { Table(model: vm.controller, editor: vm.editor, orderEditor: vm.order, sorter: vm.sorter, structureEditor: vm.structure, editable: true, clipboardEnabled: vm.clipboard, sortingEnabled: true, columnResizeEnabled: true, rowResizeEnabled: true, modelRevision: vm.revision, width: vm.width, onSelectionChanged: vm.selected, onCellInputSubmitted: vm.submitted, onRangeValuesChanged: vm.rangeChanged, onRowsSorted: vm.sorted, onStructureChanged: vm.structured, onColumnResized: vm.columnResized) }");
    if (!code) throw std::runtime_error(code.error().message);
    for (auto part : {"#include <LVUI/NativeTable.h>", "lvui::native::NativeTable::create(",
                      "lvui::native::TableInput", "component.set_options(value)", "std::optional<lvui::TableSelection>", "lvui::native::TableCellEvent", "const lvui::TableRange& value", "value.clipboard_enabled", "value.order_editor", "lvui::TableSortOrder order", "value.structure_editor", "const lvui::TableStructureEdit& value"})
        if (code->find(part) == code->npos) throw std::runtime_error(std::string("Independent table lost ") + part);
    if (code->find("lvrs::") != code->npos || code->find("<LVRS/") != code->npos || code->find("<Qt") != code->npos)
        throw std::runtime_error("Default table depends on legacy widgets");
    auto empty = lvui::compile_native("Window { Table() }");
    if (!empty || empty->find("std::make_shared<lvui::TableMemory>()") == empty->npos)
        throw std::runtime_error("An empty table did not own its default source");
    for (auto source : {"Window { Table(width: 0) }", "Window { Table(height: 24, headerHeight: 24) }",
                        "Window { Table(minColumnWidth: 200, fallbackCellWidth: 100) }", "Window { Table(modelRevision: 1.5) }",
                        "Window { Table(editable: true) }", "Window { Table(sortingEnabled: true) }", "Window { Table(columnResizeEnabled: true) }",
                        "import vm Model\nWindow { Table(model: vm.model, controller: vm.controller) }"})
        if (lvui::compile_native(source)) throw std::runtime_error("An invalid or unsupported independent table input was accepted");
}
