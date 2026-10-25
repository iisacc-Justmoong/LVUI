#pragma once
#include <expected>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace lvui {
// Authored, encoded sRGB. Keep alpha independent of the RGB channels.
struct StyleColor final {
    double red = 0, green = 0, blue = 0, alpha = 1;
    friend bool operator==(const StyleColor &, const StyleColor &) = default;
};
using StyleTokenValue = std::variant<double, StyleColor, std::string, bool>;
enum class StyleTokenType { number, color, string, boolean };
struct StyleTokenAlias final {
    std::string id;
};
struct StyleTokenModeValue final {
    std::string mode_id;
    std::variant<StyleTokenValue, StyleTokenAlias> value;
};
struct StyleTokenMode final {
    std::string id, name;
};
struct StyleTokenCollection final {
    std::string id, key, name, default_mode_id;
    std::vector<StyleTokenMode> modes;
    bool hidden = false;
};
struct StyleToken final {
    std::string id, key, name, collection_id, css_name, description;
    StyleTokenType type;
    std::vector<StyleTokenModeValue> values;
    std::vector<std::string> scopes;
    bool hidden = false;
    std::string selector{}; // Color/Metric member; empty for non-style scalar tokens.
};
struct StyleTokenOptions final {
    // Collection name/id -> mode name/id. Unselected collections use their own
    // default.
    std::map<std::string, std::string, std::less<>> modes;
    // CSS name, Figma id/key, or Collection/Name -> typed construction override.
    std::map<std::string, StyleTokenValue, std::less<>> overrides;
    // App-specific Color/Metric members. Builtin changes belong in overrides.
    std::map<std::string, StyleTokenValue, std::less<>> members;
};
enum class StyleTokenErrorCode {
    unknown_token,
    unknown_mode,
    wrong_type,
    alias_cycle,
    invalid_value,
    invalid_syntax
};
struct StyleTokenError final {
    StyleTokenErrorCode code;
    std::string message;
};
template <class T> using StyleTokenResult = std::expected<T, StyleTokenError>;
class StyleTokenCatalog final {
  public:
    StyleTokenCatalog(std::vector<StyleTokenCollection>, std::vector<StyleToken>);
    [[nodiscard]] std::span<const StyleTokenCollection> collections() const noexcept { return collections_; }
    [[nodiscard]] std::span<const StyleToken> tokens() const noexcept { return tokens_; }
    [[nodiscard]] const StyleToken *find(std::string_view) const noexcept;
    [[nodiscard]] StyleTokenResult<void> validate(const StyleTokenOptions & = {}) const;
    [[nodiscard]] StyleTokenResult<StyleTokenValue> resolve(std::string_view,
                                                            const StyleTokenOptions & = {}) const;
    [[nodiscard]] StyleTokenResult<StyleTokenValue> select(std::string_view,
                                                           const StyleTokenOptions & = {}) const;
    [[nodiscard]] StyleTokenResult<double> number(std::string_view, const StyleTokenOptions & = {}) const;
    [[nodiscard]] StyleTokenResult<StyleColor> color(std::string_view, const StyleTokenOptions & = {}) const;
    [[nodiscard]] StyleTokenResult<std::string> string(std::string_view,
                                                       const StyleTokenOptions & = {}) const;
    [[nodiscard]] StyleTokenResult<bool> boolean(std::string_view, const StyleTokenOptions & = {}) const;
    // A non-var value returns nullopt. Unknown names may use a typed CSS
    // fallback.
    [[nodiscard]] StyleTokenResult<std::optional<StyleTokenValue>>
    css_value(std::string_view, const StyleTokenOptions & = {}) const;
    [[nodiscard]] StyleTokenResult<std::optional<StyleTokenValue>>
    style_value(std::string_view, StyleTokenType context, const StyleTokenOptions & = {}) const;

  private:
    std::vector<StyleTokenCollection> collections_;
    std::vector<StyleToken> tokens_;
    std::map<std::string, std::size_t, std::less<>> names_;
    StyleTokenResult<void> validate_options(const StyleTokenOptions &) const;
    StyleTokenResult<StyleTokenValue> resolve_impl(const StyleToken &, const StyleTokenOptions &,
                                                   std::vector<std::string_view> &) const;
    StyleTokenResult<std::optional<StyleTokenValue>> css_impl(std::string_view, const StyleTokenOptions &,
                                                              unsigned) const;
};
// Checked-in Figma snapshot, no LVRS/Qt/filesystem/network dependency.
[[nodiscard]] const StyleTokenCatalog &default_style_tokens();
[[nodiscard]] StyleTokenType style_token_type(const StyleTokenValue &) noexcept;
[[nodiscard]] bool valid_style_token_value(const StyleTokenValue &) noexcept;
[[nodiscard]] StyleColor composite_srgb(StyleColor foreground, StyleColor background);
[[nodiscard]] StyleTokenResult<StyleColor> parse_style_color(std::string_view);
} // namespace lvui
