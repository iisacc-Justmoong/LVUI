#pragma once
#include <LVUI/Document.h>
#include <span>

namespace lvui {
enum class KeywordKind { view, modifier, type, declaration, literal };
struct Keyword final {
    std::string_view spelling;
    KeywordKind kind;
    std::string_view source_file;
    std::size_t source_line;
    bool internal;
};
// Sorted, process-lifetime descriptors generated from the audited LVRS snapshot.
[[nodiscard]] std::span<const Keyword> keywords() noexcept;
[[nodiscard]] const Keyword* find_keyword(std::string_view) noexcept;
[[nodiscard]] bool is_reserved_identifier(std::string_view) noexcept;

enum class TokenKind {
    identifier, keyword, string, number, left_paren, right_paren,
    left_brace, right_brace, left_bracket, right_bracket, colon, comma,
    dot, equal, minus, newline, end
};
struct Token final {
    TokenKind kind;
    std::string text;
    std::size_t offset, line, column;
    const Keyword* reserved = nullptr;
};
using TokenizeResult = std::expected<std::vector<Token>, Diagnostic>;
[[nodiscard]] TokenizeResult tokenize(std::string_view source);

enum class ValueKind {
    string,
    number,
    boolean,
    reference,
    enumeration,
    call,
    list,
    color,
    metric,
    style_member
};
struct Value final {
    ValueKind kind;
    std::string text;
    std::vector<Value> elements; // Positional call inputs or list elements.
};
[[nodiscard]] Value style_token_literal(const StyleTokenValue&);
struct SourcePosition final { std::size_t offset = 0, line = 1, column = 1; };
struct Input final { std::string name; Value value; SourcePosition position; }; // Empty name means positional.
struct Binding final {
    std::string name, type;
    Value initializer;
    bool mutable_value = false;
    SourcePosition position;
};
struct BackendImport final { std::string name, type; SourcePosition position; };
struct View final {
    const Keyword* component = nullptr; // Always KeywordKind::view; static lifetime.
    std::vector<Input> inputs;
    std::vector<View> children; // Arguments passed through {}.
    std::vector<Binding> bindings;
    std::vector<BackendImport> imports;
    SourcePosition position;
};
class ViewDocument;
using ViewParseResult = std::expected<ViewDocument, Diagnostic>;
[[nodiscard]] ViewParseResult parse_view(std::string_view source);
// HTML-shaped LVUI with inline styles and an optional declarative script block.
// This front end produces the same typed AOT tree; it embeds no script engine.
[[nodiscard]] ViewParseResult parse_native_markup(std::string_view source);
[[nodiscard]] ViewParseResult parse_native_markup(std::string_view source, const StyleTokenOptions&);
// A compile-time front-end value. Parsing does not execute imports, calls or UI.
class ViewDocument final {
public:
    [[nodiscard]] const View& root() const noexcept { return root_; }
    [[nodiscard]] const std::vector<BackendImport>& imports() const noexcept { return imports_; }
    [[nodiscard]] const std::vector<Binding>& bindings() const noexcept { return bindings_; }
private:
    ViewDocument(View root, std::vector<BackendImport> imports, std::vector<Binding> bindings)
        : root_(std::move(root)), imports_(std::move(imports)), bindings_(std::move(bindings)) {}
    friend ViewParseResult parse_view(std::string_view);
    friend ViewParseResult parse_native_markup(std::string_view);
    friend ViewParseResult parse_native_markup(std::string_view, const StyleTokenOptions&);
    View root_;
    std::vector<BackendImport> imports_;
    std::vector<Binding> bindings_;
};
// UTF-8 <=1 MiB; <=65,536 tokens, <=4,096 Views; view/expression depth <=64.
} // namespace lvui
