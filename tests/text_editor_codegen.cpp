#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { TextEditor(id: \"notes\", text: vm.text, placeholder: vm.placeholder, width: vm.width, height: vm.height, minimumHeight: vm.minimumHeight, verticalPadding: vm.inset, enabled: vm.enabled, readOnly: vm.readOnly, onEdited: vm.edited, onContentHeightChanged: vm.measured) }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto token:{"#include <LVUI/NativeTextEditor.h>","#include <iiGUI/NativeTextEditorView.h>","lvui_generated::detail_text_editor::NativeTextEditor::create(","lvui::TextEditorConfiguration","minimum_height","content_height_changed","component.set_options(value)"})
        if(code->find(token)==code->npos)throw std::runtime_error(std::string("TextEditor codegen lost ")+token);
    for(auto forbidden:{"<Qt","<LVRS/","lvrs::"})
        if(code->find(forbidden)!=code->npos)throw std::runtime_error("TextEditor generated a legacy dependency");
    for(auto invalid:{"Window { TextEditor(width: 0) }", "Window { TextEditor(minimumHeight: -1) }",
                      "Window { TextEditor(filePath: \"notes.txt\") }", "Window { TextEditor(richText: true) }",
                      "Window { TextEditor() { Label(text: \"child\") } }"})
        if(lvui::compile_native(invalid))throw std::runtime_error("Invalid or unsupported TextEditor input was accepted");
}
