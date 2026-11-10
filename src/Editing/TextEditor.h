#pragma once
#include <LVUI/InputField.h>
#include <algorithm>
#include <cmath>
#include <utility>

namespace lvui {
// The text editor has its own public contract; the shared field configuration
// is an implementation port for the text service and its Figma material.
[[nodiscard]] inline InputFieldConfiguration text_editor_field_defaults() {
    InputFieldConfiguration value;
    value.multiline=true;
    value.wrap_enabled=true;
    value.surface=InputFieldSurface::rounded;
    value.reveal_caret_in_ancestors=true;
    value.accessibility_name="Text editor";
    return value;
}
struct TextEditorConfiguration final {
    InputFieldConfiguration field=text_editor_field_defaults();
    double minimum_height=1;
    friend bool operator==(const TextEditorConfiguration&,const TextEditorConfiguration&)=default;
};
[[nodiscard]] inline bool valid_text_editor_configuration(const TextEditorConfiguration& value) noexcept {
    return std::isfinite(value.minimum_height)&&value.minimum_height>=1&&value.minimum_height<=8192&&
        value.field.multiline&&!value.field.password&&!value.field.clear_button_visible&&
        valid_input_field_configuration(value.field);
}

} // namespace lvui
