#pragma once


#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

namespace lvui::native {
// Bound already-validated UTF-8 at a scalar boundary for native display.
[[nodiscard]] inline std::string native_display_text(std::string_view value,std::size_t limit=8192) {
    auto length=(std::min)(limit,value.size());
    while(length<value.size() && length && (static_cast<unsigned char>(value[length])&0xc0)==0x80)--length;
    return std::string(value.substr(0,length));
}
} // namespace lvui::native
