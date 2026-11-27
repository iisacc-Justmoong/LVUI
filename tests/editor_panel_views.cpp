#include <LVUI/EditorPanelViews.h>

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {
void require(bool condition, const char* message) {
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}
}

int main() {
    auto automatic = lvui::compile_editor_panel_views(R"LVUI(
import vm AppModel
Window { MenuCommandParameterForm(title: vm.title,
    fields: [EditorAutoFields(model: vm.form)], preview: [],
    applyLabel: vm.applyLabel, cancelLabel: vm.cancelLabel, applyEnabled: vm.enabled,
    onApply: vm.apply, onCancel: vm.cancel) }
)LVUI");
    if (!automatic) std::cerr << automatic.error().line << ':' << automatic.error().column << ' ' << automatic.error().message << '\n';
    require(automatic.has_value(), "automatic fields could not be compiled from the app schema");
    require(automatic->find("NativeInputField") != std::string::npos &&
            automatic->find("NativeListItemSelector") != std::string::npos &&
            automatic->find("keyed_content") != std::string::npos,
            "automatic fields omitted typed controls or keyed repetition");
    for (const auto name : {"MenuCommandParameterForm", "NonDestructivePreviewPanel", "MaskRefinementControls",
            "LayerContentOptionsPanel", "FilterGalleryPanel", "ColorProfilePicker", "SoftProofSettingsPanel",
            "TimelineEditOptionsPanel", "ExportFormatOptionsPanel"}) {
        const auto source = std::string{"import vm AppModel\nWindow { "} + name + R"LVUI((title: vm.title,
            fields: [EditorTextControl(label: vm.label, value: vm.text, enabled: vm.enabled, onEdited: vm.edited),
                     EditorToggleControl(label: vm.label, checked: vm.checked, enabled: vm.enabled, onToggled: vm.toggled),
                     EditorRangeControl(label: vm.label, value: vm.amount, minimum: vm.minimum, maximum: vm.maximum,
                                        enabled: vm.enabled, onMoved: vm.moved),
                     EditorChoiceControl(label: vm.label, selector: ListItemSelector(options: vm.choices,
                                         currentIndex: vm.index, enabled: vm.enabled, onCurrentIndexChanged: vm.selected))],
            preview: [Label(text: vm.preview)], applyLabel: vm.applyLabel, cancelLabel: vm.cancelLabel,
            applyEnabled: vm.enabled, onApply: vm.apply, onCancel: vm.cancel) })LVUI";
        auto result = lvui::compile_editor_panel_views(source);
        if (!result) std::cerr << name << ": " << result.error().line << ':' << result.error().column << ' ' << result.error().message << '\n';
        require(result.has_value(), "a requested editor panel failed native compilation");
        require(result->find("NativeInputField") != std::string::npos && result->find("onApply") == std::string::npos,
                "editor panel did not expand into native controls and callbacks");
    }
    auto bad = lvui::compile_editor_panel_views("Window {\n  MissingEditorView()\n}\n");
    require(!bad && bad.error().line == 2, "editor prelude changed user diagnostic coordinates");
    return 0;
}
