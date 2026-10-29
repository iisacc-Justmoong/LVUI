#pragma once
#include <cstddef>
#include <string_view>

namespace lvui {
// Accessible names are bounded, NUL-free UTF-8. This is deliberately independent
// of a host toolkit so a candidate can be checked before any host mutation.
[[nodiscard]] inline bool valid_accessibility_text(std::string_view text) noexcept {
    if (text.size() > 65536) return false;
    std::size_t index = 0;
    const auto byte = [&](std::size_t at) { return static_cast<unsigned char>(text[at]); };
    const auto continuation = [&](std::size_t at) { return at < text.size() && (byte(at) & 0xc0) == 0x80; };
    while (index < text.size()) {
        const auto lead = byte(index);
        if (lead > 0 && lead < 0x80) { ++index; continue; }
        if (lead >= 0xc2 && lead <= 0xdf && continuation(index + 1)) { index += 2; continue; }
        if (lead >= 0xe0 && lead <= 0xef && continuation(index + 1) && continuation(index + 2)) {
            const auto second = byte(index + 1);
            if ((lead != 0xe0 || second >= 0xa0) && (lead != 0xed || second <= 0x9f)) {
                index += 3; continue;
            }
        }
        if (lead >= 0xf0 && lead <= 0xf4 && continuation(index + 1) &&
            continuation(index + 2) && continuation(index + 3)) {
            const auto second = byte(index + 1);
            if ((lead != 0xf0 || second >= 0x90) && (lead != 0xf4 || second <= 0x8f)) {
                index += 4; continue;
            }
        }
        return false;
    }
    return true;
}
} // namespace lvui
