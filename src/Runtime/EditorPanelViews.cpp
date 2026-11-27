#include "EditorPanelViews.h"

#include <algorithm>
#include <array>
#include <string>

namespace lvui {
namespace {
// These are LVUI declarations, not a second renderer or an application model.
// Labels, values, actions, choices and validation are explicitly app supplied;
// the schema View selects the controls without asking the app to write them.
std::string definitions() {
    std::string source = R"LVUI(
import class lvui.EditorAutoFormModel
const EditorTextControl(label: String, value: String, enabled: Bool, onEdited: (String) -> Void): View = VStack {
    Label(text: label)
    InputField(text: value, accessibilityName: label, enabled: enabled, onEdited: onEdited)
}
const EditorToggleControl(label: String, checked: Bool, enabled: Bool, onToggled: (Bool) -> Void): View = CheckBox(text: label, accessibilityName: label, checked: checked, enabled: enabled, onToggled: onToggled)
const EditorRangeControl(label: String, value: Double, minimum: Double, maximum: Double, enabled: Bool, onMoved: (Double) -> Void): View = VStack {
    Label(text: label)
    Slider(value: value, from: minimum, to: maximum, accessibilityName: label, enabled: enabled, onMoved: onMoved)
}
const EditorChoiceControl(label: String, selector: View): View = VStack {
    Label(text: label)
    selector()
}
const EditorAutoFields(model: EditorAutoFormModel): View = VStack {
    for (field in model.fields, key: field.id, id: "editor-fields") {
        VStack {
            if (field.visible) {
                VStack {
                    if (field.boolean) {
                        CheckBox(text: field.label, accessibilityName: field.label, enabled: field.enabled,
                                 checked: field.checked, onToggled: field.booleanEdited)
                    } else if (field.choice) {
                        VStack {
                            Label(text: field.label)
                            ListItemSelector(options: field.options, currentIndex: field.selected, placeholder: "",
                                accessibilityName: field.label, enabled: field.enabled, onCurrentIndexChanged: field.choiceEdited)
                        }
                    } else if (field.bounded) {
                        VStack {
                            Label(text: field.label)
                            Slider(value: field.amount, from: field.minimum, to: field.maximum,
                                accessibilityName: field.label, enabled: field.enabled, onMoved: field.numberEdited)
                        }
                    } else {
                        VStack {
                            Label(text: field.label)
                            InputField(text: field.text, accessibilityName: field.label, enabled: field.enabled,
                                onEdited: field.textEdited)
                        }
                    }
                    if (field.hasError) { Label(text: field.error) }
                }
            }
        }
    }
}
const EditorPanelSurface(title: String, fields: [View], preview: [View], applyLabel: String, cancelLabel: String,
    applyEnabled: Bool, onApply: () -> Void, onCancel: () -> Void): View = VStack(spacing: Metric.space12, padding: Metric.space16, backgroundColor: Color.surfacePanel, cornerRadius: Metric.radius8) {
        Label(text: title, fontSize: Metric.textHeader, fontWeight: Metric.weightSemiBold, color: Color.text.body)
        preview()
        fields()
        HStack(spacing: Metric.space8) {
            PushButton(text: cancelLabel, onClick: onCancel)
            PushButton(text: applyLabel, enabled: applyEnabled, onClick: onApply)
        }
    }
)LVUI";
    for (const auto name : std::array{"MenuCommandParameterForm", "NonDestructivePreviewPanel", "MaskRefinementControls",
            "LayerContentOptionsPanel", "FilterGalleryPanel", "ColorProfilePicker", "SoftProofSettingsPanel",
            "TimelineEditOptionsPanel", "ExportFormatOptionsPanel"}) {
        source += "const "; source += name;
        source += R"LVUI((title: String, fields: [View], preview: [View], applyLabel: String, cancelLabel: String,
    applyEnabled: Bool, onApply: () -> Void, onCancel: () -> Void): View = EditorPanelSurface(title: title, fields: fields, preview: preview, applyLabel: applyLabel, cancelLabel: cancelLabel,
                       applyEnabled: applyEnabled, onApply: onApply, onCancel: onCancel)
)LVUI";
    }
    return source;
}
}

NativeCompileResult compile_editor_panel_views(std::string_view source, const NativeCompileOptions& options) {
    auto prefix = definitions();
    const auto prefix_size = prefix.size();
    const auto prefix_lines = static_cast<std::size_t>(std::count(prefix.begin(), prefix.end(), '\n'));
    prefix.append(source);
    auto result = compile_native(prefix, options);
    if (!result) {
        auto diagnostic = result.error();
        if (diagnostic.offset < prefix_size || diagnostic.line <= prefix_lines) {
            diagnostic = {0, 1, 1, "Editor panel declarations at " + std::to_string(diagnostic.line) + ":" +
                std::to_string(diagnostic.column) + ": " + diagnostic.message};
        } else {
            diagnostic.offset -= prefix_size;
            diagnostic.line -= prefix_lines;
        }
        return std::unexpected(std::move(diagnostic));
    }
    return result;
}
} // namespace lvui
