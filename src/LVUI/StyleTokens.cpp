#include "StyleTokens.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <iiSDKCore/Core/Text/Number.h>
#include <iiSDKCore/Core/Text/Utf8.h>
#include <limits>
#include <type_traits>

namespace lvui {
namespace {
std::string_view trim(std::string_view value) {
    const auto start = value.find_first_not_of(" \t\r\n");
    return start == value.npos ? std::string_view{}
                               : value.substr(start, value.find_last_not_of(" \t\r\n") - start + 1);
}
auto failure(StyleTokenErrorCode code, std::string message) {
    return std::unexpected(StyleTokenError{code, std::move(message)});
}
template <class T> StyleTokenResult<T> typed(StyleTokenResult<StyleTokenValue> value) {
    if (!value)
        return std::unexpected(value.error());
    if (auto result = std::get_if<T>(&*value))
        return *result;
    return failure(StyleTokenErrorCode::wrong_type, "Style token has the wrong type");
}
} // namespace
StyleTokenType style_token_type(const StyleTokenValue &value) noexcept {
    return std::visit(
        [](const auto &item) {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, double>)
                return StyleTokenType::number;
            else if constexpr (std::is_same_v<T, StyleColor>)
                return StyleTokenType::color;
            else if constexpr (std::is_same_v<T, std::string>)
                return StyleTokenType::string;
            else
                return StyleTokenType::boolean;
        },
        value);
}
bool valid_style_token_value(const StyleTokenValue &value) noexcept {
    return std::visit(
        [](const auto &item) {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, double>)
                return std::isfinite(item);
            else if constexpr (std::is_same_v<T, StyleColor>)
                return std::ranges::all_of(std::array{item.red, item.green, item.blue, item.alpha},
                                           [](double v) { return std::isfinite(v) && v >= 0 && v <= 1; });
            else if constexpr (std::is_same_v<T, std::string>)
                return item.size() <= 4096 && item.find('\0') == item.npos && ii::core::valid_utf8(item);
            else
                return true;
        },
        value);
}
StyleTokenCatalog::StyleTokenCatalog(std::vector<StyleTokenCollection> collections,
                                     std::vector<StyleToken> tokens)
    : collections_(std::move(collections)), tokens_(std::move(tokens)) {
    const auto add = [&](const std::string &key, std::size_t index) {
        if (key.empty())
            return;
        const auto [it, inserted] = names_.emplace(key, index);
        if (!inserted && it->second != index)
            it->second = std::numeric_limits<std::size_t>::max();
    };
    for (std::size_t i = 0; i < tokens_.size(); ++i) {
        const auto &t = tokens_[i];
        add(t.id, i);
        add(t.key, i);
        add(t.css_name, i);
        add(t.selector, i);
        add(t.name, i);
        const auto c = std::ranges::find(collections_, t.collection_id, &StyleTokenCollection::id);
        if (c != collections_.end())
            add(c->name + "/" + t.name, i);
    }
}
const StyleToken *StyleTokenCatalog::find(std::string_view name) const noexcept {
    name = trim(name);
    if (name.starts_with("var(") && name.ends_with(')'))
        name = trim(name.substr(4, name.size() - 5));
    const auto found = names_.find(name);
    return found == names_.end() || found->second >= tokens_.size() ? nullptr : &tokens_[found->second];
}
StyleTokenResult<StyleTokenValue> StyleTokenCatalog::resolve_impl(const StyleToken &token,
                                                                  const StyleTokenOptions &options,
                                                                  std::vector<std::string_view> &path) const {
    if (path.size() >= 64 || std::ranges::find(path, token.id) != path.end())
        return failure(StyleTokenErrorCode::alias_cycle, "Style token alias cycle at " + token.id);
    path.push_back(token.id);
    struct Pop {
        std::vector<std::string_view> &path;
        ~Pop() { path.pop_back(); }
    } pop{path};
    std::optional<StyleTokenValue> overridden;
    for (const auto &[name, value] : options.overrides)
        if (find(name) == &token) {
            if (overridden)
                return failure(StyleTokenErrorCode::invalid_value,
                               "Duplicate style override for " + token.css_name);
            overridden = value;
        }
    if (overridden) {
        if (style_token_type(*overridden) != token.type)
            return failure(StyleTokenErrorCode::wrong_type,
                           "Style override type differs from " + token.css_name);
        if (!valid_style_token_value(*overridden))
            return failure(StyleTokenErrorCode::invalid_value,
                           "Invalid style override for " + token.css_name);
        return *overridden;
    }
    const auto collection = std::ranges::find(collections_, token.collection_id, &StyleTokenCollection::id);
    if (collection == collections_.end())
        return failure(StyleTokenErrorCode::invalid_value, "Unknown style collection " + token.collection_id);
    std::string mode = collection->default_mode_id;
    auto chosen = options.modes.find(collection->id);
    if (chosen == options.modes.end())
        chosen = options.modes.find(collection->name);
    if (chosen != options.modes.end()) {
        const auto m = std::ranges::find_if(collection->modes, [&](const auto &item) {
            return item.id == chosen->second || item.name == chosen->second;
        });
        if (m == collection->modes.end())
            return failure(StyleTokenErrorCode::unknown_mode,
                           "Unknown mode for " + collection->name + ": " + chosen->second);
        mode = m->id;
    }
    const auto value = std::ranges::find(token.values, mode, &StyleTokenModeValue::mode_id);
    if (value == token.values.end())
        return failure(StyleTokenErrorCode::unknown_mode, "Missing mode value for " + token.css_name);
    if (const auto alias = std::get_if<StyleTokenAlias>(&value->value)) {
        const auto target = find(alias->id);
        if (!target)
            return failure(StyleTokenErrorCode::unknown_token, "Missing style alias target " + alias->id);
        if (target->type != token.type)
            return failure(StyleTokenErrorCode::wrong_type, "Style alias type mismatch at " + token.css_name);
        return resolve_impl(*target, options, path);
    }
    const auto &result = std::get<StyleTokenValue>(value->value);
    if (style_token_type(result) != token.type || !valid_style_token_value(result))
        return failure(StyleTokenErrorCode::invalid_value,
                       "Invalid stored style value for " + token.css_name);
    return result;
}
StyleTokenResult<StyleTokenValue> StyleTokenCatalog::resolve(std::string_view name,
                                                             const StyleTokenOptions &options) const {
    auto valid = validate_options(options);
    if (!valid)
        return std::unexpected(valid.error());
    if (auto member = options.members.find(name); member != options.members.end())
        return member->second;
    const auto token = find(name);
    if (!token)
        return failure(StyleTokenErrorCode::unknown_token,
                       "Unknown or ambiguous style token '" + std::string(name) + "'");
    std::vector<std::string_view> path;
    return resolve_impl(*token, options, path);
}
StyleTokenResult<StyleTokenValue> StyleTokenCatalog::select(std::string_view name,
                                                            const StyleTokenOptions &options) const {
    name = trim(name);
    const bool color = name.starts_with("Color."), metric = name.starts_with("Metric.");
    if (!color && !metric)
        return failure(StyleTokenErrorCode::invalid_syntax, "Expected a Color or Metric member");
    if (options.members.contains(name))
        return resolve(name, options);
    const auto token = find(name);
    if (!token || token->selector != name)
        return failure(StyleTokenErrorCode::unknown_token,
                       "Unknown typed style member '" + std::string(name) + "'");
    if (token->type != (color ? StyleTokenType::color : StyleTokenType::number))
        return failure(StyleTokenErrorCode::wrong_type,
                       "Typed style member has the wrong domain: " + std::string(name));
    return resolve(name, options);
}
StyleTokenResult<void> StyleTokenCatalog::validate_options(const StyleTokenOptions &options) const {
    for (const auto &[name, value] : options.members) {
        const bool color = name.starts_with("Color."), metric = name.starts_with("Metric.");
        const auto leaf = std::string_view(name).substr(color ? 6 : metric ? 7 : 0);
        const auto alpha = [](char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
        };
        if ((!color && !metric) || leaf.empty() || !alpha(leaf.front()) ||
            !std::ranges::all_of(leaf, [&](char c) { return alpha(c) || (c >= '0' && c <= '9'); }) ||
            find(name))
            return failure(StyleTokenErrorCode::invalid_value,
                           "Invalid or builtin application style member " + name);
        if (style_token_type(value) != (color ? StyleTokenType::color : StyleTokenType::number) ||
            !valid_style_token_value(value))
            return failure(StyleTokenErrorCode::wrong_type, "Invalid application style member value " + name);
    }
    std::map<std::string, std::string, std::less<>> modes;
    for (const auto &[key, mode] : options.modes) {
        const auto c = std::ranges::find_if(
            collections_, [&](const auto &item) { return item.name == key || item.id == key; });
        if (c == collections_.end())
            return failure(StyleTokenErrorCode::unknown_mode, "Unknown style collection " + key);
        const auto m = std::ranges::find_if(
            c->modes, [&](const auto &item) { return item.name == mode || item.id == mode; });
        if (m == c->modes.end())
            return failure(StyleTokenErrorCode::unknown_mode,
                           "Unknown style collection/mode " + key + "/" + mode);
        const auto [found, inserted] = modes.emplace(c->id, m->id);
        if (!inserted && found->second != m->id)
            return failure(StyleTokenErrorCode::unknown_mode, "Conflicting style modes for " + c->name);
    }
    std::vector<const StyleToken *> overridden;
    for (const auto &[name, value] : options.overrides) {
        const auto token = find(name);
        if (!token)
            return failure(StyleTokenErrorCode::unknown_token, "Unknown style override '" + name + "'");
        if (std::ranges::find(overridden, token) != overridden.end())
            return failure(StyleTokenErrorCode::invalid_value,
                           "Duplicate style override for " + token->css_name);
        overridden.push_back(token);
        if (style_token_type(value) != token->type)
            return failure(StyleTokenErrorCode::wrong_type, "Style override type differs from " + name);
        if (!valid_style_token_value(value))
            return failure(StyleTokenErrorCode::invalid_value, "Invalid style override " + name);
    }
    return {};
}
StyleTokenResult<void> StyleTokenCatalog::validate(const StyleTokenOptions &options) const {
    auto valid = validate_options(options);
    if (!valid)
        return valid;
    for (const auto &token : tokens_) {
        std::vector<std::string_view> path;
        auto value = resolve_impl(token, options, path);
        if (!value)
            return std::unexpected(value.error());
    }
    return {};
}
StyleTokenResult<double> StyleTokenCatalog::number(std::string_view name, const StyleTokenOptions &o) const {
    return typed<double>(resolve(name, o));
}
StyleTokenResult<StyleColor> StyleTokenCatalog::color(std::string_view name,
                                                      const StyleTokenOptions &o) const {
    return typed<StyleColor>(resolve(name, o));
}
StyleTokenResult<std::string> StyleTokenCatalog::string(std::string_view name,
                                                        const StyleTokenOptions &o) const {
    return typed<std::string>(resolve(name, o));
}
StyleTokenResult<bool> StyleTokenCatalog::boolean(std::string_view name, const StyleTokenOptions &o) const {
    return typed<bool>(resolve(name, o));
}
StyleColor composite_srgb(StyleColor foreground, StyleColor background) {
    const auto alpha = foreground.alpha + background.alpha * (1 - foreground.alpha);
    if (alpha == 0)
        return {0, 0, 0, 0};
    const auto channel = [&](double a, double b) {
        return (a * foreground.alpha + b * background.alpha * (1 - foreground.alpha)) / alpha;
    };
    return {channel(foreground.red, background.red), channel(foreground.green, background.green),
            channel(foreground.blue, background.blue), alpha};
}
StyleTokenResult<StyleColor> parse_style_color(std::string_view input) {
    input = trim(input);
    if (input == "transparent")
        return StyleColor{0, 0, 0, 0};
    if (input == "white")
        return StyleColor{1, 1, 1, 1};
    if (input == "black")
        return StyleColor{0, 0, 0, 1};
    if (input.starts_with('#')) {
        input.remove_prefix(1);
        const bool short_form = input.size() == 3 || input.size() == 4;
        const auto width = short_form ? 1 : 2;
        if (!short_form && input.size() != 6 && input.size() != 8)
            return failure(StyleTokenErrorCode::invalid_value, "Invalid hexadecimal color");
        double channels[]{0, 0, 0, 1};
        for (std::size_t i = 0; i < input.size() / width; ++i) {
            unsigned value = 0;
            auto part = input.substr(i * width, width);
            auto read = std::from_chars(part.data(), part.data() + part.size(), value, 16);
            if (read.ec != std::errc{} || read.ptr != part.data() + part.size())
                return failure(StyleTokenErrorCode::invalid_value, "Invalid hexadecimal color");
            channels[i] = value / (short_form ? 15. : 255.);
        }
        return StyleColor{channels[0], channels[1], channels[2], channels[3]};
    }
    if (input.starts_with("rgba(") && input.ends_with(')')) {
        input = input.substr(5, input.size() - 6);
        std::array<double, 4> channels{};
        for (std::size_t i = 0; i < 4; ++i) {
            auto comma = input.find(',');
            auto part = trim(input.substr(0, comma));
            auto n = ii::core::parse_decimal(part);
            if (!n || (i < 3 && comma == input.npos) || (i == 3 && comma != input.npos))
                return failure(StyleTokenErrorCode::invalid_value, "Invalid RGBA color");
            channels[i] = *n / (i < 3 ? 255. : 1.);
            if (comma != input.npos)
                input.remove_prefix(comma + 1);
        }
        StyleColor color{channels[0], channels[1], channels[2], channels[3]};
        if (valid_style_token_value(color))
            return color;
    }
    return failure(StyleTokenErrorCode::invalid_value, "Invalid style color");
}
StyleTokenResult<std::optional<StyleTokenValue>>
StyleTokenCatalog::css_impl(std::string_view text, const StyleTokenOptions &o, unsigned depth) const {
    text = trim(text);
    if (text.starts_with("Color.") || text.starts_with("Metric.")) {
        auto value = select(text, o);
        if (!value)
            return std::unexpected(value.error());
        return std::optional<StyleTokenValue>{std::move(*value)};
    }
    if (!text.starts_with("var("))
        return std::optional<StyleTokenValue>{};
    if (depth >= 32 || !text.ends_with(')') || text.size() > 8192)
        return failure(StyleTokenErrorCode::invalid_syntax, "Invalid or nested style var() expression");
    auto body = text.substr(4, text.size() - 5);
    unsigned nesting = 0;
    std::size_t comma = body.npos;
    char quote = 0;
    bool escaped = false;
    for (std::size_t i = 0; i < body.size(); ++i) {
        const auto c = body[i];
        if (quote) {
            if (escaped)
                escaped = false;
            else if (c == '\\')
                escaped = true;
            else if (c == quote)
                quote = 0;
            continue;
        }
        if (c == '\'' || c == '"') {
            quote = c;
            continue;
        }
        if (c == '(')
            ++nesting;
        else if (c == ')') {
            if (!nesting)
                return failure(StyleTokenErrorCode::invalid_syntax, "Invalid style var() nesting");
            --nesting;
        } else if (c == ',' && !nesting && comma == body.npos)
            comma = i;
    }
    if (nesting || quote)
        return failure(StyleTokenErrorCode::invalid_syntax, "Unclosed style var() fallback");
    const auto name = trim(body.substr(0, comma));
    if (!name.starts_with("--") || name.size() == 2 || !std::ranges::all_of(name.substr(2), [](char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
                   c == '_';
        }))
        return failure(StyleTokenErrorCode::invalid_syntax,
                       "Style var() requires an ASCII custom property name");
    auto value = resolve(name, o);
    if (value)
        return std::optional<StyleTokenValue>{std::move(*value)};
    if (value.error().code != StyleTokenErrorCode::unknown_token || comma == body.npos)
        return std::unexpected(value.error());
    auto fallback = trim(body.substr(comma + 1));
    if (fallback.empty())
        return failure(StyleTokenErrorCode::invalid_syntax, "Empty style var() fallback");
    if (fallback.starts_with("var(") || fallback.starts_with("Color.") || fallback.starts_with("Metric."))
        return css_impl(fallback, o, depth + 1);
    if (auto color = parse_style_color(fallback); color)
        return std::optional<StyleTokenValue>{*color};
    if (fallback.ends_with("px"))
        fallback.remove_suffix(2);
    if (auto n = ii::core::parse_decimal(fallback))
        return std::optional<StyleTokenValue>{*n};
    if (fallback == "true" || fallback == "false")
        return std::optional<StyleTokenValue>{fallback == "true"};
    if ((fallback.starts_with('"') && fallback.ends_with('"')) ||
        (fallback.starts_with('\'') && fallback.ends_with('\'')))
        fallback = fallback.substr(1, fallback.size() - 2);
    return std::optional<StyleTokenValue>{std::string(fallback)};
}
StyleTokenResult<std::optional<StyleTokenValue>>
StyleTokenCatalog::css_value(std::string_view text, const StyleTokenOptions &options) const {
    auto valid = validate_options(options);
    if (!valid)
        return std::unexpected(valid.error());
    return css_impl(text, options, 0);
}
StyleTokenResult<std::optional<StyleTokenValue>>
StyleTokenCatalog::style_value(std::string_view text, StyleTokenType context,
                               const StyleTokenOptions &options) const {
    text = trim(text);
    const auto identifier_start = [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    };
    if (text.size() > 1 && text.front() == '.' && identifier_start(text[1]) &&
        (context == StyleTokenType::color || context == StyleTokenType::number))
        return css_value(
            std::string(context == StyleTokenType::color ? "Color" : "Metric") + std::string(text), options);
    return css_value(text, options);
}
const StyleTokenCatalog &default_style_tokens() {
#include "StyleTokens.generated.inc"
    return catalog;
}
} // namespace lvui
