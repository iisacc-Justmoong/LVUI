#pragma once
#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lvui {
// Encoded sRGB; the renderer converts to its working color space.
struct Color final { float red = 0, green = 0, blue = 0, alpha = 1; };
enum class Unit { automatic, pixels, percent };
struct Length final { Unit unit = Unit::automatic; float value = 0; };
enum class Direction { column, row };
enum class Alignment { left, center, right };
struct Insets final { float top = 0, right = 0, bottom = 0, left = 0; };
struct Style final {
    Length width, height;
    Insets padding;
    float gap = 0, radius = 0;
    Direction direction = Direction::column;
    std::optional<Color> background, color;
    std::optional<float> font_size;
    std::optional<unsigned> font_weight;
    std::optional<Alignment> text_align;
};
enum class Kind { container, text };
struct Node final {
    Kind kind = Kind::container;
    std::string tag, id, text;
    Style style;
    std::vector<Node> children;
};
struct Diagnostic final {
    std::size_t offset = 0, line = 1, column = 1;
    std::string message;
};
class Document;
using ParseResult = std::expected<Document, Diagnostic>;
[[nodiscard]] ParseResult parse(std::string_view source);
// A parsed value owns all strings and exposes an immutable tree. No source lifetime dependency.
class Document final {
public:
    [[nodiscard]] const Node& root() const noexcept { return root_; }
private:
    explicit Document(Node root) : root_(std::move(root)) {}
    friend ParseResult parse(std::string_view);
    Node root_;
};
// UTF-8 input <=1 MiB, <=4096 elements and <=64 levels (including root).
// Errors use zero-based byte offsets and one-based Unicode-scalar line/column positions.
} // namespace lvui
