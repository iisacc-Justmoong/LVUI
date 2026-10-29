#pragma once

#include <LVUI/StyleTokens.h>

#include <cmath>
#include <cstdint>
#include <expected>
#include <string>

namespace lvui {

[[nodiscard]] inline bool valid_color_value(StyleColor value) noexcept {
    const auto channel = [](double value) {
        return std::isfinite(value) && value >= 0 && value <= 1;
    };
    return channel(value.red) && channel(value.green) && channel(value.blue) && channel(value.alpha);
}

struct ColorSourceError final {
    std::string message;
};
using ColorSourceResult = std::expected<StyleColor, ColorSourceError>;

// Increment the revision whenever the observed color changes. Readers may
// compare revisions around color() to reject a concurrent replacement.
class ColorSource {
public:
    virtual ~ColorSource() = default;
    [[nodiscard]] virtual ColorSourceResult color() const = 0;
    [[nodiscard]] virtual std::uint64_t revision() const noexcept = 0;
};

} // namespace lvui
