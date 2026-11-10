#include "FigmaComponents.h"

#include <algorithm>
#include <array>
#include <map>
#include <string_view>
#include <tuple>

namespace lvui::detail {
namespace {
struct FigmaDefault { std::string_view node, property, value; };
constexpr FigmaDefault figma_defaults[] = {
#define LVUI_FIGMA_DEFAULT(node, property, value) {node, property, value},
#include "FigmaDefaults.def"
#undef LVUI_FIGMA_DEFAULT
};
std::string_view figma_default(std::string_view node, std::string_view property) {
    const auto key = std::pair(node, property);
    const auto found = std::ranges::lower_bound(figma_defaults, key, {}, [](const FigmaDefault& value) {
        return std::pair(value.node, value.property);
    });
    if (found == std::end(figma_defaults) || found->node != node || found->property != property)
        throw Diagnostic{0, 1, 1, "Missing Figma component default"};
    return found->value;
}
[[noreturn]] void error(SourcePosition at, std::string message) {
    throw Diagnostic{at.offset, at.line, at.column, std::move(message)};
}
Input* input(View& view, std::string_view name) {
    const auto it = std::ranges::find(view.inputs, name, &Input::name);
    return it == view.inputs.end() ? nullptr : &*it;
}
void add(View& view, std::string name, ValueKind kind, std::string text) {
    view.inputs.push_back({std::move(name), Value{kind, std::move(text), {}, view.position}, view.position});
}
void rename(View& view, std::string_view from, std::string_view to) {
    if (auto* item = input(view, from)) {
        if (input(view, to)) error(item->position, "Conflicting component inputs '" + std::string(from) + "' and '" + std::string(to) + "'");
        item->name = to;
    }
}
bool choice(Input& item, std::initializer_list<std::string_view> choices,
            const std::function<bool(const Value&)>& is_binding) {
    auto& value = item.value;
    if (value.kind == ValueKind::reference && !is_binding(value) &&
        std::ranges::find(choices, value.text) != choices.end()) {
        value.kind = ValueKind::enumeration;
        value.text.insert(value.text.begin(), '.');
        return true;
    }
    return false;
}
std::string_view choice_name(const Value& value) {
    if (value.kind == ValueKind::enumeration && value.text.starts_with('.'))
        return std::string_view(value.text).substr(1);
    return {};
}
void select_component(View& view, std::string_view name) {
    auto* keyword = find_keyword(name);
    if (!keyword || keyword->kind != KeywordKind::view) error(view.position, "Unknown native component '" + std::string(name) + "'");
    view.component = keyword;
}
void state_boolean(Input& item, const std::function<bool(const Value&)>& is_binding) {
    choice(item, {"On", "Off"}, is_binding);
    const auto value = choice_name(item.value);
    if (value == "On" || value == "Off") {
        item.value.kind = ValueKind::boolean;
        item.value.text = value == "On" ? "true" : "false";
    }
}
}

bool is_figma_component(std::string_view name) noexcept {
    constexpr std::string_view names[]{
        "Checkbox", "Radio", "Toggle", "TextField", "GeneralSearch", "SmallList", "ColorPickerPanel",
        "LabelSegementedControl", "IconSegementedControl", "Icon", "Material", "PushButton",
        "DropdownButton", "LabelButton", "IconButton", "LabelMenuButton", "IconMenuButton",
        "CheckBox", "RadioButton", "ToggleSwitch", "Slider", "Card", "ListItem", "InputField",
        "ColorPicker", "ColorPickerButton", "Avatar", "Stepper", "ComboBox", "Tab", "Sheet", "Alert",
        "IconSegmentedControl"
    };
    return std::ranges::find(names, name) != std::end(names);
}

bool normalize_figma_component(View& view, const std::function<bool(const Value&)>& is_binding) {
    if (!view.component) return false;
    const auto original = std::string(view.component->spelling);
    const std::map<std::string_view, std::string_view, std::less<>> aliases{
        {"Checkbox", "CheckBox"}, {"Radio", "RadioButton"}, {"Toggle", "ToggleSwitch"},
        {"TextField", "InputField"}, {"GeneralSearch", "SearchField"}, {"SmallList", "List"},
        {"ColorPickerPanel", "ColorPicker"}, {"LabelSegementedControl", "LabelSegmentedControl"},
        {"IconSegementedControl", "IconSegmentedControl"}, {"Icon", "Image"}
    };
    bool changed = false;
    if (const auto alias = aliases.find(original); alias != aliases.end()) {
        select_component(view, alias->second);
        changed = true;
    }
    auto kind = std::string(view.component->spelling);
    if (original == "Icon") rename(view, "name", "iconName");

    if (kind == "Material") {
        std::string selected = "Panel";
        if (auto* type = input(view, "type")) {
            choice(*type, {"Panel", "Window", "Surface"}, is_binding);
            selected = std::string(choice_name(type->value));
            if (selected != "Panel" && selected != "Window" && selected != "Surface")
                error(type->position, "Figma Material type must be Panel, Window or Surface");
            view.inputs.erase(std::ranges::find(view.inputs, std::string("type"), &Input::name));
        }
        select_component(view, selected == "Panel" ? "PanelMaterial" : selected == "Window" ? "WindowMaterial" : "MaterialSurface");
        kind = view.component->spelling;
        changed = true;
    }

    if (kind == "PushButton" || kind == "DropdownButton") {
        const bool dropdown = kind == "DropdownButton";
        std::string selected(figma_default(dropdown ? "700:337" : "44:599", "Type"));
        if (auto* type = input(view, "type")) {
            choice(*type, {"LabelButton", "IconButton", "LabelMenuButton", "IconMenuButton"}, is_binding);
            selected = std::string(choice_name(type->value));
            if ((!dropdown && selected != "LabelButton" && selected != "IconButton") ||
                (dropdown && selected != "LabelMenuButton" && selected != "IconMenuButton"))
                error(type->position, "Figma button type must be a supported static component variant");
            view.inputs.erase(std::ranges::find(view.inputs, std::string("type"), &Input::name));
        }
        select_component(view, selected);
        kind = selected;
        changed = true;
    }
    const bool button = kind == "LabelButton" || kind == "IconButton" ||
                        kind == "LabelMenuButton" || kind == "IconMenuButton";
    if (button) {
        if (input(view, "kind")) { rename(view, "kind", "tone"); changed = true; }
        if (auto* tone = input(view, "tone")) {
            if (choice(*tone, {"default", "accent", "borderless", "destructive", "disabled", "primary"}, is_binding)) changed = true;
            if (choice_name(tone->value) == "accent") { tone->value.text = ".primary"; changed = true; }
        } else {
            const auto node = kind == "LabelButton" ? "44:599" : kind == "IconButton" ? "1243:1226" : "700:337";
            auto default_tone = std::string(figma_default(node, "Kind"));
            if (default_tone == "accent") default_tone = "primary";
            add(view, "tone", ValueKind::enumeration, "." + default_tone); changed = true;
        }
        if ((kind == "LabelButton" || kind == "LabelMenuButton") && !input(view, "text")) {
            add(view, "text", ValueKind::string, std::string(figma_default(kind == "LabelButton" ? "44:599" : "700:337", "Label"))); changed = true;
        }
        if ((kind == "IconButton" || kind == "IconMenuButton") && !input(view, "iconName") && !input(view, "icon")) {
            add(view, "iconName", ValueKind::string, std::string(figma_default(kind == "IconButton" ? "1243:1226" : "700:337", "Icon"))); changed = true;
        }
    }
    if (kind == "CheckBox" || kind == "RadioButton" || kind == "ToggleSwitch") {
        if (auto* state = input(view, "state")) { state_boolean(*state, is_binding); changed = true; }
        if (input(view, "state")) { rename(view, "state", "checked"); changed = true; }
        if (!input(view, "checked")) {
            const auto node = kind == "CheckBox" ? "44:724" : kind == "RadioButton" ? "44:630" : "111:379";
            const auto state = figma_default(node, "State");
            if (state != "On" && state != "Off") error(view.position, "Invalid Figma selection default");
            add(view, "checked", ValueKind::boolean, state == "On" ? "true" : "false"); changed = true;
        }
        if (auto* available = input(view, "available")) {
            if (available->value.kind == ValueKind::reference && !is_binding(available->value) &&
                (available->value.text == "True" || available->value.text == "False")) {
                available->value.kind = ValueKind::boolean;
                available->value.text = available->value.text == "True" ? "true" : "false";
                changed = true;
            }
            rename(view, "available", "enabled");
            changed = true;
        }
    }
    if (kind == "Slider") {
        if (auto* type = input(view, "type"))
            changed |= choice(*type, {"Default", "CenterBiased", "Ticks", "CenterBiasedTicks", "Filled", "MinMaxLabels", "Segmented"}, is_binding);
        if (auto* size = input(view, "size")) {
            changed |= choice(*size, {"Mini", "Small", "Regular", "Large"}, is_binding);
            const auto value = choice_name(size->value);
            if (value == "Mini" || value == "Small" || value == "Regular" || value == "Large") {
                size->value.text[1] = char(size->value.text[1] - 'A' + 'a'); changed = true;
            }
        } else {
            auto default_size = std::string(figma_default("879:6541", "Size"));
            if (default_size != "Mini" && default_size != "Small" && default_size != "Regular" && default_size != "Large")
                error(view.position, "Invalid Figma slider size default");
            default_size[0] = char(default_size[0] - 'A' + 'a');
            add(view, "size", ValueKind::enumeration, "." + default_size); changed = true;
        }
    }
    if (kind == "Card") {
        if (auto* type = input(view, "type"))
            changed |= choice(*type, {"File", "FilePreview", "Folder", "Project", "Device", "Model", "Member", "Link"}, is_binding);
        for (const auto name : {"size", "detail"}) if (auto* prop = input(view, name))
            changed |= choice(*prop, {"Small", "Medium", "Large", "Brief", "Detailed"}, is_binding);
        if (!input(view, "size")) {
            add(view, "size", ValueKind::enumeration, "." + std::string(figma_default("852:997", "Size"))); changed = true;
        }
        if (auto* state = input(view, "state")) {
            choice(*state, {"Default", "Hover", "Selected"}, is_binding);
            const auto selected = choice_name(state->value);
            if (selected == "Default" || selected == "Hover" || selected == "Selected")
                state->value.text = "." + std::string(selected) + "State";
            rename(view, "state", "displayState"); changed = true;
        }
    }
    if (kind == "ListItem") {
        if (auto* type = input(view, "type")) changed |= choice(*type,
            {"Mini", "Navigation", "Detail", "Toggle", "Checkable", "Action", "ActionGroup", "Stepper", "Select", "InlineEdit", "DetailActions", "DetailQuantity", "DetailSettings", "Resource", "Media", "Task", "Form"}, is_binding);
        if (auto* prop = input(view, "kind")) {
            choice(*prop, {"Default", "Selected", "Disabled"}, is_binding);
            const auto selected = choice_name(prop->value);
            const auto at = prop->position;
            if (selected != "Default" && selected != "Selected" && selected != "Disabled")
                error(at, "Figma ListItem kind must be Default, Selected or Disabled");
            const bool active = selected == "Selected", enabled = selected != "Disabled";
            if (input(view, "selected") || input(view, "enabled")) error(at, "Figma ListItem kind conflicts with selected or enabled");
            view.inputs.erase(std::ranges::find(view.inputs, std::string("kind"), &Input::name));
            add(view, "selected", ValueKind::boolean, active ? "true" : "false");
            add(view, "enabled", ValueKind::boolean, enabled ? "true" : "false");
            changed = true;
        }
    }
    if (kind == "InputField" || (kind == "SearchField" && original == "TextField")) {
        if (auto* mode = input(view, "mode")) {
            choice(*mode, {"Text", "Search", "Password"}, is_binding);
            const std::string selected(choice_name(mode->value));
            const auto at = mode->position;
            if (selected != "Text" && selected != "Search" && selected != "Password")
                error(at, "Figma TextField mode must be Text, Search or Password");
            view.inputs.erase(std::ranges::find(view.inputs, std::string("mode"), &Input::name));
            if (selected == "Password") {
                if (input(view, "password")) error(at, "TextField mode conflicts with password");
                add(view, "password", ValueKind::boolean, "true");
            } else if (selected == "Search") { select_component(view, "SearchField"); kind = "SearchField"; }
            changed = true;
        }
        if (auto* style = input(view, "style")) {
            choice(*style, {"Rounded", "Cylinder", "Inline"}, is_binding);
            const auto selected = choice_name(style->value);
            if (selected == "Rounded" || selected == "Cylinder" || selected == "Inline")
                style->value.text[1] = char(style->value.text[1] - 'A' + 'a');
            rename(view, "style", "surface"); changed = true;
        }
        if (!input(view, "surface")) {
            auto surface = std::string(figma_default("114:179", "Style"));
            if (surface != "Rounded" && surface != "Cylinder" && surface != "Inline")
                error(view.position, "Invalid Figma text field style default");
            surface[0] = char(surface[0] - 'A' + 'a');
            add(view, "surface", ValueKind::enumeration, "." + surface); changed = true;
        }
        if (auto* state = input(view, "state")) {
            choice(*state, {"Enabled", "Disabled"}, is_binding);
            const auto selected = choice_name(state->value);
            const auto at = state->position;
            if (selected != "Enabled" && selected != "Disabled")
                error(at, "TextField interaction state is driven by input events");
            const bool enabled = selected == "Enabled";
            if (input(view, "enabled")) error(at, "TextField state conflicts with enabled");
            view.inputs.erase(std::ranges::find(view.inputs, std::string("state"), &Input::name));
            add(view, "enabled", ValueKind::boolean, enabled ? "true" : "false"); changed = true;
        }
        if (!input(view, "placeholder")) {
            add(view, "placeholder", ValueKind::string, std::string(figma_default("114:179", "Text"))); changed = true;
        }
    }
    if (kind == "ColorPicker") {
        if (auto* type = input(view, "type"))
            changed |= choice(*type, {"Wheel", "HueSaturation", "SaturationBrightness", "Grayscale", "RGB", "CMYK"}, is_binding);
    }
    if (kind == "ColorPickerButton" || kind == "Avatar") {
        const auto field = kind == "Avatar" ? "size" : "buttonSize";
        if (kind == "ColorPickerButton" && input(view, "size")) { rename(view, "size", field); changed = true; }
        if (auto* size = input(view, field)) changed |= choice(*size, {"Small", "Medium", "Large"}, is_binding);
        else {
            add(view, field, ValueKind::enumeration,
                "." + std::string(figma_default(kind == "Avatar" ? "1530:19" : "892:62", "Size")));
            changed = true;
        }
    }
    if (kind == "IconSegmentedControl") {
        if (auto* orientation = input(view, "orientation")) {
            changed |= choice(*orientation, {"Horizontal", "Vertical"}, is_binding);
            const auto selected = choice_name(orientation->value);
            if (selected == "Horizontal" || selected == "Vertical") {
                orientation->value.text[1] = char(orientation->value.text[1] - 'A' + 'a'); changed = true;
            }
        }
    }
    if (kind == "Stepper" || kind == "ComboBox") {
        if (auto* arrow = input(view, "arrow")) changed |= choice(*arrow, {"Up", "Down", "UpDown"}, is_binding);
        if (auto* tone = input(view, "tone")) changed |= choice(*tone, {"primary", "Primary", "borderless", "Borderless"}, is_binding);
    }
    if (kind == "Tab") {
        if (auto* style = input(view, "style")) {
            choice(*style, {"Underline", "Surface"}, is_binding);
            const auto selected = choice_name(style->value);
            if (selected == "Underline" || selected == "Surface") style->value.text[1] = char(style->value.text[1] - 'A' + 'a');
            rename(view, "style", "tabStyle"); changed = true;
        }
    }
    if (kind == "Sheet") {
        if (auto* presentation = input(view, "presentation")) {
            choice(*presentation, {"MobileFit", "MobileMedium", "MobileLarge", "DesktopCenter"}, is_binding);
            const std::string selected(choice_name(presentation->value));
            if (selected == "MobileFit" || selected == "MobileMedium" || selected == "MobileLarge" || selected == "DesktopCenter") {
                if (input(view, "detent")) error(presentation->position, "Figma Sheet presentation conflicts with detent");
                presentation->value.text = selected == "DesktopCenter" ? ".desktop" : ".mobile";
                add(view, "detent", ValueKind::enumeration,
                    selected == "MobileMedium" ? ".medium" : selected == "MobileLarge" ? ".large" : ".fit");
                changed = true;
            }
        } else {
            const auto selected = figma_default("921:14595", "Presentation");
            if (selected != "Mobile Fit" && selected != "Mobile Medium" && selected != "Mobile Large" && selected != "Desktop Center")
                error(view.position, "Invalid Figma sheet presentation default");
            add(view, "presentation", ValueKind::enumeration, selected == "Desktop Center" ? ".desktop" : ".mobile");
            if (!input(view, "detent")) add(view, "detent", ValueKind::enumeration,
                selected == "Mobile Medium" ? ".medium" : selected == "Mobile Large" ? ".large" : ".fit");
            changed = true;
        }
    }
    if (kind == "Alert") {
        if (input(view, "amount")) { rename(view, "amount", "buttonCount"); changed = true; }
        const bool authored_actions = input(view, "primaryText") || input(view, "secondaryText") || input(view, "tertiaryText") ||
            input(view, "button1Text") || input(view, "button2Text") || input(view, "button3Text") || !view.children.empty();
        if (!input(view, "buttonCount") && !authored_actions) {
            add(view, "buttonCount", ValueKind::number, std::string(figma_default("106:283", "Amount"))); changed = true;
        }
    }
    return changed;
}
}
