#include <LVUI/Language.h>
#include <iiSDKCore/Core/Text/Utf8.h>
#include <iiSDKCore/Core/Text/Number.h>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <unordered_set>
#include <unordered_map>
#include <type_traits>

namespace lvui {
Value style_token_literal(const StyleTokenValue& value) {
    return std::visit(
        [](const auto& item) -> Value {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, double>)
                return {ValueKind::number, ii::core::format_decimal(item), {}};
            else if constexpr (std::is_same_v<T, std::string>)
                return {ValueKind::string, item, {}};
            else if constexpr (std::is_same_v<T, bool>)
                return {ValueKind::boolean, item ? "true" : "false", {}};
            else
                return {ValueKind::color,
                        "",
                        {{ValueKind::number, ii::core::format_decimal(item.red), {}},
                         {ValueKind::number, ii::core::format_decimal(item.green), {}},
                         {ValueKind::number, ii::core::format_decimal(item.blue), {}},
                         {ValueKind::number, ii::core::format_decimal(item.alpha), {}}}};
        },
        value);
}
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
            } else if (digit(c) || (c=='.'&&position+1<source.size()&&digit(source[position+1]))) {
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
                if (starts("===") || starts("!=="))
                    error("LVUI uses typed '==' and '!=' rather than JavaScript equality operators");
                constexpr std::pair<std::string_view, TokenKind> pairs[]{
                    {"==", TokenKind::equal_equal}, {"!=", TokenKind::bang_equal},
                    {"<=", TokenKind::less_equal}, {">=", TokenKind::greater_equal},
                    {"&&", TokenKind::logical_and}, {"||", TokenKind::logical_or}, {"->", TokenKind::arrow}};
                bool paired = false;
                for (const auto& [spelling, token] : pairs) {
                    if (!starts(spelling)) continue;
                    take(); take();
                    push(token, std::string(spelling), offset, row, col);
                    paired = true;
                    break;
                }
                if (paired) continue;
                switch (c) {
                case '(': kind = TokenKind::left_paren; break; case ')': kind = TokenKind::right_paren; break;
                case '{': kind = TokenKind::left_brace; break; case '}': kind = TokenKind::right_brace; break;
                case '[': kind = TokenKind::left_bracket; break; case ']': kind = TokenKind::right_bracket; break;
                case ':': kind = TokenKind::colon; break; case ',': kind = TokenKind::comma; break;
                case '.': kind = TokenKind::dot; break; case '=': kind = TokenKind::equal; break;
                case '-': kind = TokenKind::minus; break;
                case '+': kind = TokenKind::plus; break; case '*': kind = TokenKind::star; break;
                case '/': kind = TokenKind::slash; break; case '%': kind = TokenKind::percent; break;
                case '!': kind = TokenKind::bang; break; case '<': kind = TokenKind::less; break;
                case '>': kind = TokenKind::greater; break; case '?': kind = TokenKind::question; break;
                case '@': kind = TokenKind::annotation; break;
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
    std::size_t active_view_depth = 0;
    std::vector<std::unordered_map<std::string, std::string>> view_names{{}};
    std::string view_type(std::string_view name) const {
        for (auto scope = view_names.rbegin(); scope != view_names.rend(); ++scope)
            if (auto found = scope->find(std::string(name)); found != scope->end()) return found->second;
        return {};
    }
    bool known_view(std::string_view name)const{return !view_type(name).empty();}
    bool contains_view(const Value& value)const{
        if(value.kind==ValueKind::view||(value.kind==ValueKind::reference&&known_view(value.text)))return true;
        return value.kind==ValueKind::list&&std::ranges::any_of(value.elements,[&](const auto& child){return contains_view(child);});
    }
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
    std::string cpp_type_path() {
        const auto part=[&]() -> std::string {
            if(at(TokenKind::keyword)&&current().reserved&&(current().reserved->kind==KeywordKind::view||current().reserved->kind==KeywordKind::type))return tokens[cursor++].text;
            return identifier();
        };
        auto result=part();while(accept(TokenKind::dot))result+="."+part();return result;
    }
    std::string enumeration_path() {
        const auto name=[&]() -> std::string {
            if(at(TokenKind::keyword)&&current().reserved&&current().reserved->kind==KeywordKind::view)return tokens[cursor++].text;
            return identifier();
        };
        auto result=name();while(accept(TokenKind::dot))result+="."+name();return result;
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
    static std::size_t tree_depth(const Value& value) {
        std::size_t result = 1;
        for (const auto& child : value.elements) result = std::max(result, tree_depth(child) + 1);
        return result;
    }
    Value node(ValueKind kind, std::string text, std::vector<Value> children, SourcePosition at) const {
        Value result{kind, std::move(text), std::move(children), at};
        if (tree_depth(result) > 64)
            throw Diagnostic{at.offset, at.line, at.column, "Maximum expression depth is 64"};
        return result;
    }
    Value primary(std::size_t depth) {
        if (depth > 64) error("Maximum expression depth is 64");
        const auto start = position();
        if ((at(TokenKind::identifier)&&(current().text=="if"||current().text=="for"))||(at(TokenKind::keyword) && current().reserved->kind == KeywordKind::view) ||
            (at(TokenKind::identifier) && known_view(current().text) &&
             (next().kind == TokenKind::left_paren || next().kind == TokenKind::left_brace))) {
            Value result{ValueKind::view, {}, {}, start};
            result.view = std::make_shared<View>(view(active_view_depth + 1));
            return result;
        }
        if (at(TokenKind::string)) return {ValueKind::string, tokens[cursor++].text, {}, start};
        if (at(TokenKind::number)) return {ValueKind::number, tokens[cursor++].text, {}, start};
        if (word("true") || word("false")) return {ValueKind::boolean, tokens[cursor++].text, {}, start};
        if (accept(TokenKind::left_paren)) {
            newlines();
            auto result = value(depth + 1);
            newlines();
            expect(TokenKind::right_paren, "Expected ')' after grouped expression");
            return result;
        }
        if (accept(TokenKind::left_bracket))
            return node(ValueKind::list, {}, values(TokenKind::right_bracket, depth), start);
        if (accept(TokenKind::dot)) return {ValueKind::enumeration, "." + enumeration_path(), {}, start};
        if (word("Color") || word("Metric")) {
            auto type = tokens[cursor++].text;
            if (accept(TokenKind::left_paren))
                return node(ValueKind::call, std::move(type), values(TokenKind::right_paren, depth), start);
            expect(TokenKind::dot, "Expected '.' and a member after a style type");
            return {ValueKind::style_member, type + "." + enumeration_path(), {}, start};
        }
        if (at(TokenKind::identifier)) {
            auto reference = path();
            if (accept(TokenKind::left_paren))
                return node(ValueKind::call, std::move(reference), values(TokenKind::right_paren, depth), start);
            return {ValueKind::reference, std::move(reference), {}, start};
        }
        error("Expected a scalar, list or reference value");
    }
    Value postfix(std::size_t depth) {
        auto result = primary(depth);
        for (;;) {
            const auto start = position();
            if (accept(TokenKind::left_bracket)) {
                newlines();
                auto index = value(depth + 1);
                newlines();
                expect(TokenKind::right_bracket, "Expected ']' after index expression");
                result = node(ValueKind::index, {}, {std::move(result), std::move(index)}, start);
            } else if (accept(TokenKind::dot)) {
                auto member = identifier();
                result = node(ValueKind::member, std::move(member), {std::move(result)}, start);
            } else if (accept(TokenKind::left_paren)) {
                auto arguments = values(TokenKind::right_paren, depth);
                arguments.insert(arguments.begin(), std::move(result));
                result = node(ValueKind::invocation, {}, std::move(arguments), start);
            } else return result;
        }
    }
    Value unary(std::size_t depth) {
        if (depth > 64) error("Maximum expression depth is 64");
        if (at(TokenKind::minus) || at(TokenKind::plus) || at(TokenKind::bang)) {
            const auto start = position();
            auto operation = tokens[cursor++].text;
            newlines();
            // Preserve the existing negative-literal AST contract.
            if (operation == "-" && at(TokenKind::number) &&
                tokens[std::min(cursor + 1, tokens.size() - 1)].kind != TokenKind::left_bracket) {
                auto result = postfix(depth + 1);
                if (result.kind == ValueKind::number) {
                    result.text = "-" + result.text;
                    result.position = start;
                    return result;
                }
                return node(ValueKind::unary, std::move(operation), {std::move(result)}, start);
            }
            return node(ValueKind::unary, std::move(operation), {unary(depth + 1)}, start);
        }
        return postfix(depth);
    }
    static unsigned precedence(TokenKind kind) {
        switch (kind) {
        case TokenKind::logical_or: return 1;
        case TokenKind::logical_and: return 2;
        case TokenKind::equal_equal: case TokenKind::bang_equal: return 3;
        case TokenKind::less: case TokenKind::less_equal:
        case TokenKind::greater: case TokenKind::greater_equal: return 4;
        case TokenKind::plus: case TokenKind::minus: return 5;
        case TokenKind::star: case TokenKind::slash: case TokenKind::percent: return 6;
        default: return 0;
        }
    }
    Value binary(unsigned minimum, std::size_t depth) {
        if (depth > 64) error("Maximum expression depth is 64");
        auto left = unary(depth);
        for (;;) {
            const auto level = precedence(current().kind);
            if (level < minimum) return left;
            const auto start = position();
            auto operation = tokens[cursor++].text;
            newlines();
            auto right = binary(level + 1, depth + 1);
            left = node(ValueKind::binary, std::move(operation), {std::move(left), std::move(right)}, start);
        }
    }
    Value value(std::size_t depth = 1) {
        auto condition = binary(1, depth);
        const auto start = position();
        if (!accept(TokenKind::question)) return condition;
        newlines();
        auto accepted = value(depth + 1);
        newlines();
        expect(TokenKind::colon, "Expected ':' in conditional expression");
        newlines();
        auto rejected = value(depth + 1);
        return node(ValueKind::conditional, {},
                    {std::move(condition), std::move(accepted), std::move(rejected)}, start);
    }
    static void bind(std::unordered_set<std::string>& names, const std::string& name, const Parser& parser) {
        if (!names.insert(name).second) parser.error("Duplicate binding name");
    }
    BackendImport import(std::unordered_set<std::string>& names) {
        const auto start = position();
        ++cursor;
        if (at(TokenKind::identifier) && current().text == "class") {
            ++cursor;
            auto type = cpp_type_path();
            auto name = type.substr(type.find_last_of('.') == type.npos ? 0 : type.find_last_of('.') + 1);
            if(at(TokenKind::identifier)){
                if(type.find('.')!=type.npos)error("An explicit C++ type alias must be an unqualified name");
                name=std::move(type);type=cpp_type_path();
            }
            if(find_keyword(name))error("A reserved C++ type name requires an explicit blueprint alias");
            bind(names, name, *this);
            view_names.back()[name] = {};
            separator();
            return {std::move(name), std::move(type), start, true};
        }
        auto name = identifier(); bind(names, name, *this);
        view_names.back()[name] = {};
        auto type = cpp_type_path(); separator();
        return {std::move(name), std::move(type), start};
    }
    std::pair<std::string,std::optional<CallableSignature>> declared_type(){
        const auto type_name=[&]{return (word("View")||word("Color")||word("Metric"))?tokens[cursor++].text:path();};
        if(accept(TokenKind::left_bracket)){
            newlines();if(!word("View"))error("A View sequence type requires '[View]'");++cursor;newlines();
            expect(TokenKind::right_bracket,"Expected ']' after View sequence type");return {"[View]",{}};
        }
        if(!accept(TokenKind::left_paren))return {type_name(),{}};
        CallableSignature signature;newlines();
        if(!accept(TokenKind::right_paren))for(;;){
            if(signature.arguments.size()==16)error("Maximum callback argument count is 16");
            signature.arguments.push_back(type_name());newlines();
            if(accept(TokenKind::right_paren))break;
            expect(TokenKind::comma,"Expected ',' between callback argument types");newlines();
            if(accept(TokenKind::right_paren))break;
        }
        newlines();expect(TokenKind::arrow,"Expected '->' and a callback result type");newlines();
        signature.result=type_name();return {"Callable",std::move(signature)};
    }
    Binding binding(std::unordered_set<std::string>& names) {
        const auto start = position();
        BindingAnnotation annotation = BindingAnnotation::none;
        while (accept(TokenKind::annotation)) {
            const auto name = identifier();
            const auto next = name == "Static" ? BindingAnnotation::static_value :
                              name == "Observable" ? BindingAnnotation::observable : BindingAnnotation::none;
            if (next == BindingAnnotation::none) error("Unknown declaration annotation '" + name + "'");
            if (annotation != BindingAnnotation::none) error("Duplicate or conflicting declaration annotations");
            annotation = next;
            newlines();
        }
        if (!word("let") && !word("var")) error("An annotation must precede a let or var declaration");
        const bool mutableValue = word("var"); ++cursor;
        if (mutableValue && annotation == BindingAnnotation::static_value)
            error("@Static requires an immutable let declaration");
        auto name = identifier(); bind(names, name, *this);
        std::optional<std::vector<ComponentParameter>> parameters;
        if (accept(TokenKind::left_paren)) {
            if (mutableValue || annotation != BindingAnnotation::none)
                error("A reusable View signature requires an unannotated let declaration");
            parameters.emplace();
            view_names.emplace_back();
            std::unordered_set<std::string> parameter_names;
            newlines();
            if (!accept(TokenKind::right_paren)) for (;;) {
                if (parameters->size() == 64) error("Maximum component parameter count is 64");
                const auto at = position();
                auto parameter_name = identifier(); bind(parameter_names, parameter_name, *this);
                expect(TokenKind::colon, "Expected ':' and a type after a component parameter name");
                auto [parameter_type,callable]=declared_type();
                std::optional<Value> fallback;
                if (accept(TokenKind::equal)) { newlines(); fallback = value(); }
                view_names.back()[parameter_name] = parameter_type=="View"||parameter_type=="[View]"?parameter_type:std::string{};
                parameters->push_back({std::move(parameter_name), std::move(parameter_type), std::move(fallback), at, std::move(callable)});
                newlines();
                if (accept(TokenKind::right_paren)) break;
                expect(TokenKind::comma, "Expected ',' between component parameters"); newlines();
                if (accept(TokenKind::right_paren)) break;
            }
        }
        std::string type;
        std::optional<CallableSignature> callable;
        if (accept(TokenKind::colon)) {
            auto declared=declared_type();type=std::move(declared.first);callable=std::move(declared.second);
        }
        expect(TokenKind::equal, "Expected '=' in value declaration");
        const bool sequence_constructor=at(TokenKind::identifier)&&view_type(current().text)=="[View]";
        const bool visual = (at(TokenKind::keyword) && current().reserved->kind == KeywordKind::view) ||
                            (at(TokenKind::identifier) && (current().text == "if" || current().text == "for")) ||
                            (at(TokenKind::identifier) && known_view(current().text) && next().kind == TokenKind::left_paren);
        Value initializer{ValueKind::reference, {}, {}};
        if (visual) {
            const auto visual_type=sequence_constructor?std::string("[View]"):std::string("View");
            if (!type.empty() && type != visual_type) error("A View initializer requires its declared View or [View] type");
            type = visual_type;
            initializer.kind = ValueKind::view;
            initializer.position = position();
            initializer.view = std::make_shared<View>(view(active_view_depth + 1));
        } else initializer = value();
        if (type.empty() && initializer.kind == ValueKind::reference && known_view(initializer.text)) type = view_type(initializer.text);
        if(type.empty()&&initializer.kind==ValueKind::list&&contains_view(initializer))type="[View]";
        if (type == "View"||type=="[View]") {
            if (mutableValue) error("Mutable View declarations require dynamic replacement support");
            if (annotation == BindingAnnotation::static_value) error("@Static View declarations require a deep snapshot contract");
        }
        if (parameters) {
            if ((type != "View" || initializer.kind != ValueKind::view)&&
                (type != "[View]"||(initializer.kind!=ValueKind::list&&initializer.kind!=ValueKind::view)))
                error("A reusable View signature requires a visual View initializer");
            view_names.pop_back();
        }
        view_names.back()[name] = type=="View"||type=="[View]"?type:std::string{};
        separator();
        return {std::move(name), std::move(type), std::move(initializer), mutableValue, start, annotation, std::move(parameters),std::move(callable)};
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
        View result; result.position = position();
        if (at(TokenKind::identifier) && current().text == "for") {
            ++cursor;
            expect(TokenKind::left_paren,"Expected '(' after for"); newlines();
            auto name=identifier();
            if(!at(TokenKind::identifier)||current().text!="in")error("Expected 'in' after the repeated item name");
            ++cursor; newlines();
            auto source=value(); newlines();
            std::optional<Value> key;
            std::string index;
            std::vector<Input> options;
            std::unordered_set<std::string> names;
            while(!accept(TokenKind::right_paren)){
                expect(TokenKind::comma,"Expected ',' before a repetition option");newlines();
                const auto at=position();auto input_name=identifier();
                if(!names.insert(input_name).second)error("Duplicate repetition option");
                expect(TokenKind::colon,"Expected ':' after a repetition option");newlines();
                if(input_name=="key")key=value();
                else if(input_name=="index"){
                    index=identifier();
                    if(index==name)error("Repeated item and index names must differ");
                }else options.push_back({std::move(input_name),value(),at});
                newlines();
            }
            if(!key)error("A repeated View requires an explicit stable key");
            result.repetition=Repetition{name,index,std::move(source),std::move(*key),std::move(options),result.position};
            newlines();expect(TokenKind::left_brace,"Expected '{' to open a repeated View");newlines();
            view_names.emplace_back();
            view_names.back()[name]={};
            if(!index.empty())view_names.back()[index]={};
            result.children.push_back(view(depth+1));newlines();
            expect(TokenKind::right_brace,"A repeated View requires exactly one root View");
            view_names.pop_back();
            return result;
        }
        if (at(TokenKind::identifier) && current().text == "if") {
            ++cursor;
            expect(TokenKind::left_paren, "Expected '(' after if"); newlines();
            result.condition = value(); newlines();
            expect(TokenKind::right_paren, "Expected ')' after a View condition"); newlines();
            const auto branch = [&] {
                expect(TokenKind::left_brace, "Expected '{' to open a conditional View branch"); newlines();
                view_names.emplace_back();
                auto child = view(depth + 1); newlines();
                expect(TokenKind::right_brace, "A conditional View branch requires exactly one root View");
                view_names.pop_back();
                return child;
            };
            result.children.push_back(branch());
            auto following = cursor;
            while (tokens[following].kind == TokenKind::newline) ++following;
            if (tokens[following].kind == TokenKind::identifier && tokens[following].text == "else") {
                cursor = following + 1; newlines();
                if (at(TokenKind::identifier) && current().text == "if") result.children.push_back(view(depth + 1));
                else result.children.push_back(branch());
            }
            return result;
        }
        if (at(TokenKind::identifier)) {
            if (!known_view(current().text)) error("Unknown View component '" + current().text + "'");
            result.custom_component = tokens[cursor++].text;
        } else {
            if (!at(TokenKind::keyword) || current().reserved->kind != KeywordKind::view) error("Expected a concrete View component");
            result.component = current().reserved; ++cursor;
        }
        const auto previous_depth = std::exchange(active_view_depth, depth);
        view_names.emplace_back();
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
                if (word("let") || word("var") || at(TokenKind::annotation)) result.bindings.push_back(binding(bindings));
                else if (word("import")) result.imports.push_back(import(bindings));
                else {
                    if (at(TokenKind::identifier) && next().kind == TokenKind::colon) input(result, inputs, true);
                    else result.children.push_back(view(depth + 1));
                    separator();
                }
            }
        }
        if (!constructed) error("Expected '()' or '{}' after a View component");
        view_names.pop_back();
        active_view_depth = previous_depth;
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
        while (parser.word("import") || parser.word("let") || parser.word("var") || parser.at(TokenKind::annotation)) {
            if (parser.word("import")) imports.push_back(parser.import(names));
            else bindings.push_back(parser.binding(names));
        }
        auto root = parser.view(1); parser.newlines();
        if (!parser.at(TokenKind::end)) parser.error("Expected exactly one root View");
        return ViewDocument(std::move(root), std::move(imports), std::move(bindings));
    } catch (Diagnostic& diagnostic) { return std::unexpected(std::move(diagnostic)); }
}
} // namespace lvui
