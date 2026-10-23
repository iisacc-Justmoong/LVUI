#include <LVUI/Language.h>
#include <iiSDKCore/Core/Text/Utf8.h>
#include <iiSDKCore/Core/Text/Number.h>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <unordered_set>

namespace lvui {
namespace {
constexpr Keyword words[]{
#define LVUI_KEYWORD(name, kind, file, line, internal) {name, KeywordKind::kind, file, line, internal},
#include "Keywords.def"
#undef LVUI_KEYWORD
};
static_assert([] {
    for (std::size_t i = 1; i < std::size(words); ++i)
        if (words[i - 1].spelling >= words[i].spelling) return false;
    return true;
}(), "LVUI keywords must be unique and sorted");
bool alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
bool digit(char c) { return c >= '0' && c <= '9'; }
struct Lexer final {
    std::string_view source;
    std::size_t position = 0, line = 1, column = 1;
    std::vector<Token> tokens;
    [[noreturn]] void error(std::string message) const { throw Diagnostic{position, line, column, std::move(message)}; }
    bool starts(std::string_view value) const { return source.substr(position).starts_with(value); }
    char peek() const { return position < source.size() ? source[position] : '\0'; }
    char take() {
        const auto c = source[position++];
        if (c == '\n' || (c == '\r' && peek() != '\n')) { ++line; column = 1; }
        else if (c != '\r' && (static_cast<unsigned char>(c) & 0xc0) != 0x80) ++column;
        return c;
    }
    void push(TokenKind kind, std::string text, std::size_t offset, std::size_t row, std::size_t col) {
        if (tokens.size() == 65536) error("Maximum token count is 65536");
        const auto* keyword = kind == TokenKind::identifier ? find_keyword(text) : nullptr;
        tokens.push_back({keyword ? TokenKind::keyword : kind, std::move(text), offset, row, col, keyword});
    }
    void newline() {
        const auto offset = position, row = line, col = column;
        if (take() == '\r' && peek() == '\n') take();
        push(TokenKind::newline, "\n", offset, row, col);
    }
    void comment() {
        if (starts("//")) {
            while (position < source.size() && peek() != '\n' && peek() != '\r') take();
            return;
        }
        take(); take();
        while (!starts("*/")) {
            if (position == source.size()) error("Unclosed block comment");
            if (peek() == '\n' || peek() == '\r') newline(); else take();
        }
        take(); take();
    }
    std::string string() {
        const auto quote = take();
        std::string value;
        while (peek() != quote) {
            if (position == source.size()) error("Unclosed string");
            if (peek() == '\n' || peek() == '\r') error("Use an escape for a newline inside a string");
            auto c = take();
            if (c == '\\') {
                if (position == source.size()) error("Unclosed string escape");
                c = take();
                if (c == 'n') c = '\n'; else if (c == 'r') c = '\r'; else if (c == 't') c = '\t';
                else if (c != '\\' && c != '\'' && c != '"') error("Unsupported string escape");
            }
            value += c;
        }
        take(); return value;
    }
    void run() {
        if (source.size() > 1024 * 1024) error("LVUI input exceeds 1 MiB");
        if (!ii::core::valid_utf8(source)) error("LVUI requires valid UTF-8");
        const auto nul = source.find('\0');
        if (nul != source.npos) {
            while (position < nul) take();
            error("NUL is not allowed in LVUI");
        }
        while (position < source.size()) {
            if (peek() == ' ' || peek() == '\t') { take(); continue; }
            if (peek() == '\r' || peek() == '\n') { newline(); continue; }
            if (starts("//") || starts("/*")) { comment(); continue; }
            const auto offset = position, row = line, col = column;
            const auto c = peek();
            if (alpha(c)) {
                while (alpha(peek()) || digit(peek())) take();
                push(TokenKind::identifier, std::string(source.substr(offset, position - offset)), offset, row, col);
            } else if (c == '"' || c == '\'') {
                push(TokenKind::string, string(), offset, row, col);
            } else if (digit(c)) {
                while (digit(peek())) take();
                if (peek() == '.') { take(); while (digit(peek())) take(); }
                if (peek() == 'e' || peek() == 'E') {
                    take(); if (peek() == '+' || peek() == '-') take();
                    if (!digit(peek())) error("Invalid number exponent");
                    while (digit(peek())) take();
                }
                const auto value = source.substr(offset, position - offset);
                if (!ii::core::parse_decimal(value)) error("Invalid number literal");
                push(TokenKind::number, std::string(value), offset, row, col);
            } else {
                TokenKind kind;
                switch (c) {
                case '(': kind = TokenKind::left_paren; break; case ')': kind = TokenKind::right_paren; break;
                case '{': kind = TokenKind::left_brace; break; case '}': kind = TokenKind::right_brace; break;
                case '[': kind = TokenKind::left_bracket; break; case ']': kind = TokenKind::right_bracket; break;
                case ':': kind = TokenKind::colon; break; case ',': kind = TokenKind::comma; break;
                case '.': kind = TokenKind::dot; break; case '=': kind = TokenKind::equal; break;
                case '-': kind = TokenKind::minus; break;
                default: error("Unsupported character in LVUI component source");
                }
                take(); push(kind, std::string(1, c), offset, row, col);
            }
        }
        // The sentinel is not part of the source-token budget.
        tokens.push_back({TokenKind::end, {}, position, line, column});
    }
};
struct Parser final {
    const std::vector<Token>& tokens;
    std::size_t cursor = 0, views = 0;
    const Token& current() const { return tokens[cursor]; }
    const Token& next() const { return tokens[std::min(cursor + 1, tokens.size() - 1)]; }
    [[noreturn]] void error(std::string message) const {
        const auto& token = current();
        throw Diagnostic{token.offset, token.line, token.column, std::move(message)};
    }
    bool at(TokenKind kind) const { return current().kind == kind; }
    SourcePosition position() const { return {current().offset, current().line, current().column}; }
    bool word(std::string_view value) const { return at(TokenKind::keyword) && current().text == value; }
    bool accept(TokenKind kind) { if (!at(kind)) return false; ++cursor; return true; }
    void expect(TokenKind kind, std::string message) { if (!accept(kind)) error(std::move(message)); }
    void newlines() { while (accept(TokenKind::newline)) {} }
    void separator() {
        if (!at(TokenKind::newline) && !at(TokenKind::right_brace) && !at(TokenKind::end)) error("Expected a newline between statements");
        newlines();
    }
    std::string identifier() {
        if (at(TokenKind::keyword)) error("A reserved identifier cannot be used as a name");
        if (!at(TokenKind::identifier)) error("Expected an identifier");
        return tokens[cursor++].text;
    }
    std::string path() {
        auto result = identifier();
        while (accept(TokenKind::dot)) result += "." + identifier();
        return result;
    }
    std::vector<Value> values(TokenKind end, std::size_t depth) {
        std::vector<Value> result;
        newlines();
        if (accept(end)) return result;
        for (;;) {
            result.push_back(value(depth + 1)); newlines();
            if (accept(end)) return result;
            expect(TokenKind::comma, "Expected ',' between values"); newlines();
            if (accept(end)) return result;
        }
    }
    Value value(std::size_t depth = 1) {
        if (depth > 64) error("Maximum expression depth is 64");
        if (at(TokenKind::string)) return {ValueKind::string, tokens[cursor++].text, {}};
        const bool negative = accept(TokenKind::minus);
        if (at(TokenKind::number)) return {ValueKind::number, (negative ? "-" : "") + tokens[cursor++].text, {}};
        if (negative) error("Expected a number after '-'");
        if (word("true") || word("false")) return {ValueKind::boolean, tokens[cursor++].text, {}};
        if (accept(TokenKind::left_bracket)) return {ValueKind::list, {}, values(TokenKind::right_bracket, depth)};
        if (accept(TokenKind::dot)) return {ValueKind::enumeration, "." + path(), {}};
        if (at(TokenKind::identifier)) {
            auto reference = path();
            if (accept(TokenKind::left_paren)) return {ValueKind::call, std::move(reference), values(TokenKind::right_paren, depth)};
            return {ValueKind::reference, std::move(reference), {}};
        }
        error("Expected a scalar, list or reference value");
    }
    static void bind(std::unordered_set<std::string>& names, const std::string& name, const Parser& parser) {
        if (!names.insert(name).second) parser.error("Duplicate binding name");
    }
    BackendImport import(std::unordered_set<std::string>& names) {
        const auto start = position();
        ++cursor;
        auto name = identifier(); bind(names, name, *this);
        auto type = path(); separator();
        return {std::move(name), std::move(type), start};
    }
    Binding binding(std::unordered_set<std::string>& names) {
        const auto start = position();
        const bool mutableValue = word("var"); ++cursor;
        auto name = identifier(); bind(names, name, *this);
        std::string type;
        if (accept(TokenKind::colon)) {
            if (word("View")) { type = "View"; ++cursor; }
            else type = path();
        }
        expect(TokenKind::equal, "Expected '=' in value declaration");
        if (type == "View") error("View-valued declarations require a later typed initializer implementation");
        auto initializer = value(); separator();
        return {std::move(name), std::move(type), std::move(initializer), mutableValue, start};
    }
    void input(View& view, std::unordered_set<std::string>& names, bool named) {
        const auto start = position();
        std::string name;
        if (named) {
            name = identifier();
            if (!names.insert(name).second) error("Duplicate component input");
            expect(TokenKind::colon, "Expected ':' after an input name");
        }
        view.inputs.push_back({std::move(name), value(), start});
    }
    View view(std::size_t depth) {
        if (depth > 64) error("Maximum View depth is 64");
        if (++views > 4096) error("Maximum View count is 4096");
        if (at(TokenKind::identifier)) error("Unknown View component '" + current().text + "'");
        if (!at(TokenKind::keyword) || current().reserved->kind != KeywordKind::view) error("Expected a concrete View component");
        View result; result.position = position(); result.component = current().reserved; ++cursor;
        std::unordered_set<std::string> inputs, bindings;
        bool namedSeen = false, constructed = false;
        if (accept(TokenKind::left_paren)) {
            constructed = true; newlines();
            if (!accept(TokenKind::right_paren)) for (;;) {
                const bool named = at(TokenKind::identifier) && next().kind == TokenKind::colon;
                if (!named && namedSeen) error("Positional input cannot follow a named input");
                input(result, inputs, named); namedSeen |= named; newlines();
                if (accept(TokenKind::right_paren)) break;
                expect(TokenKind::comma, "Expected ',' between component inputs"); newlines();
                if (accept(TokenKind::right_paren)) break;
            }
        }
        // A body may begin on the next line, without swallowing the next declaration.
        auto body = cursor;
        while (tokens[body].kind == TokenKind::newline) ++body;
        if (tokens[body].kind == TokenKind::left_brace) cursor = body;
        if (accept(TokenKind::left_brace)) {
            constructed = true; newlines();
            while (!accept(TokenKind::right_brace)) {
                if (at(TokenKind::end)) error("Expected '}' to close the View body");
                if (word("let") || word("var")) result.bindings.push_back(binding(bindings));
                else if (word("import")) result.imports.push_back(import(bindings));
                else {
                    if (at(TokenKind::identifier) && next().kind == TokenKind::colon) input(result, inputs, true);
                    else result.children.push_back(view(depth + 1));
                    separator();
                }
            }
        }
        if (!constructed) error("Expected '()' or '{}' after a View component");
        return result;
    }
};
} // namespace

std::span<const Keyword> keywords() noexcept { return words; }
const Keyword* find_keyword(std::string_view name) noexcept {
    const auto found = std::lower_bound(std::begin(words), std::end(words), name,
                                      [](const Keyword& word, std::string_view value) { return word.spelling < value; });
    return found != std::end(words) && found->spelling == name ? found : nullptr;
}
bool is_reserved_identifier(std::string_view name) noexcept { return find_keyword(name) != nullptr; }
TokenizeResult tokenize(std::string_view source) {
    Lexer lexer{source, 0, 1, 1, {}};
    try { lexer.run(); return std::move(lexer.tokens); }
    catch (Diagnostic& diagnostic) { return std::unexpected(std::move(diagnostic)); }
}
ViewParseResult parse_view(std::string_view source) {
    auto tokens = tokenize(source);
    if (!tokens) return std::unexpected(std::move(tokens.error()));
    Parser parser{*tokens};
    try {
        parser.newlines();
        std::unordered_set<std::string> names;
        std::vector<BackendImport> imports;
        std::vector<Binding> bindings;
        while (parser.word("import") || parser.word("let") || parser.word("var")) {
            if (parser.word("import")) imports.push_back(parser.import(names));
            else bindings.push_back(parser.binding(names));
        }
        auto root = parser.view(1); parser.newlines();
        if (!parser.at(TokenKind::end)) parser.error("Expected exactly one root View");
        return ViewDocument(std::move(root), std::move(imports), std::move(bindings));
    } catch (Diagnostic& diagnostic) { return std::unexpected(std::move(diagnostic)); }
}
} // namespace lvui
