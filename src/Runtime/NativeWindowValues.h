#pragma once
#include <LVUI/NativeValues.h>
#include <LVUI/WindowAccent.h>
#include <type_traits>

namespace lvui::native {
// Only LVUI's own enum or an exact name may select a Window accent.
inline WindowAccent window_accent(WindowAccent value) {
    if(!valid_window_accent(value))throw std::invalid_argument("Invalid Window accent");
    return value;
}
template<String T> WindowAccent window_accent(const T& value) {
    const auto parsed=parse_window_accent(lvui::native::string_view(value));
    if(!parsed)throw std::invalid_argument("Invalid Window accent");
    return *parsed;
}

// Preserve the existing explicit conversion helper for application code.
// The enum parameter is intentionally non-deduced: generated Window inputs
// accept only WindowAccent or a name unless the caller explicitly converts.
template<class Accent> requires std::is_enum_v<Accent>
Accent window_accent(std::type_identity_t<Accent> value) {
    if(static_cast<unsigned long long>(value)>=window_accent_names.size())
        throw std::invalid_argument("Invalid Window accent");
    return value;
}
template<class Accent,String T> requires std::is_enum_v<Accent>
Accent window_accent(const T& value) {
    return static_cast<Accent>(window_accent(value));
}
} // namespace lvui::native
