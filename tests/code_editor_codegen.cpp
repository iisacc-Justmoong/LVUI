#include <LVUI/Compiler.h>

#include <stdexcept>
#include <string>

int main() {
    auto generated = lvui::compile_native("import vm Model\nWindow { CodeEditor(model: vm.document, editor: vm.document, sourceRevision: vm.revision, width: vm.width, onDocumentEdited: vm.changed, onSelectionChanged: vm.selected) }");
    if (!generated) throw std::runtime_error(generated.error().message);
    for (auto part : {"#include <LVUI/NativeCodeEditor.h>", "lvui::native::NativeCodeEditor::create(",
                      "lvui::native::CodeEditorAppearance", "component.set_appearance(value)",
                      "component.sync_source()", "const lvui::CodeDocumentChange& change", "lvui::CodeSelection value"})
        if (generated->find(part) == generated->npos) throw std::runtime_error(std::string("Independent code editor lost ") + part);
    if (generated->find("lvrs::") != generated->npos || generated->find("<LVRS/") != generated->npos || generated->find("<Qt") != generated->npos)
        throw std::runtime_error("Default code editor depends on legacy widgets");
    auto owned = lvui::compile_native("Window { CodeEditor(text: \"Hello\", readOnly: true) }");
    if (!owned || owned->find("lvui::CodeMemoryDocument::create") == owned->npos)
        throw std::runtime_error("Code editor did not create its own initial document");
    for (auto source : {"import vm Model\nWindow { CodeEditor(model: vm.document, text: \"x\") }",
                        "import vm Model\nWindow { CodeEditor(editor: vm.document) }",
                        "Window { CodeEditor(height: 10, editorHeight: 20) }",
                        "Window { CodeEditor(lineNumbers: true) }", "Window { CodeEditor(model: 4) }",
                        "import vm Model\nWindow { CodeEditor(model: vm.document, readOnly: 1) }",
                        "import vm Model\nWindow { CodeEditor(model: vm.document, sourceRevision: -1) }",
                        "import vm Model\nWindow { CodeEditor(text: vm.text) }",
                        "import vm Model\nWindow { CodeEditor(readOnly: vm.readOnly) }",
                        "Window { CodeEditor { Text(\"x\") } }"})
        if (lvui::compile_native(source)) throw std::runtime_error("Invalid independent code editor input was accepted");
}
