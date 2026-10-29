#pragma once

#include "StyleTokens.h"
#include <iiGUI/NativeWidgets.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>

namespace lvui::native {
// Bound already-validated UTF-8 at a scalar boundary for native display.
[[nodiscard]] inline std::string native_display_text(std::string_view value,std::size_t limit=8192) {
    auto length=std::min(limit,value.size());
    while(length<value.size() && length && (static_cast<unsigned char>(value[length])&0xc0)==0x80)--length;
    return std::string(value.substr(0,length));
}
[[nodiscard]] inline ii::gui::Color native_encoded_color(StyleColor value) noexcept {
    const auto channel=[](double c){return c<=.04045?c/12.92:std::pow((c+.055)/1.055,2.4);};
    return {float(channel(value.red)),float(channel(value.green)),float(channel(value.blue)),float(value.alpha)};
}
} // namespace lvui::native
