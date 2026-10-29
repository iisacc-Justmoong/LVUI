#pragma once
#include "NativeValues.h"
#include <algorithm>
#include <array>

namespace lvui::native {
// The concrete Accent enum is supplied by the generated framework adapter.
// Other integer or enum types cannot silently become a valid accent.
template<class Accent> requires std::is_enum_v<Accent>
Accent window_accent(Accent value) {
    if(static_cast<unsigned long long>(value)>11)throw std::invalid_argument("Invalid Window accent");
    return value;
}
template<class Accent,String T> requires std::is_enum_v<Accent>
Accent window_accent(const T& value) {
    constexpr std::array<std::string_view,12> names={"blue","purple","rose","red","orange","yellow","green","gray","brown","cyan","teal","white"};
    const auto name=lvui::native::string_view(value);
    const auto found=std::find(names.begin(),names.end(),name);
    if(found==names.end())throw std::invalid_argument("Invalid Window accent");
    return static_cast<Accent>(found-names.begin());
}
} // namespace lvui::native
