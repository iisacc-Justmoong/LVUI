#include <LVUI/ConstantExpression.h>
#include <iiSDKCore/Core/Text/Number.h>
#include <cmath>
#include <map>

namespace lvui {
bool is_expression(ValueKind kind) noexcept {
    return kind == ValueKind::unary || kind == ValueKind::binary ||
           kind == ValueKind::conditional || kind == ValueKind::index ||
           kind == ValueKind::member || kind == ValueKind::invocation;
}
namespace {
bool numeric(ValueKind kind) { return kind == ValueKind::number || kind == ValueKind::metric; }
class Folder final {
    const ConstantResolver& resolver_;
    SourcePosition fallback_;
    std::map<const Value*, Value> leaves_;
    std::map<const Value*, ValueKind> types_;
    std::size_t visited_ = 0;
    [[noreturn]] void fail(const Value& value, std::string message) const {
        const auto at = value.position.offset ? value.position : fallback_;
        throw Diagnostic{at.offset, at.line, at.column, std::move(message)};
    }
    const Value& leaf(const Value& value) {
        if (value.kind == ValueKind::string || value.kind == ValueKind::number ||
            value.kind == ValueKind::boolean || value.kind == ValueKind::metric ||
            value.kind == ValueKind::color || value.kind == ValueKind::list) {
            if (value.kind == ValueKind::string && value.text.size() > 1024 * 1024)
                fail(value, "Constant String exceeds 1 MiB");
            return value;
        }
        if (const auto found = leaves_.find(&value); found != leaves_.end()) return found->second;
        auto resolved = resolver_ ? resolver_(value) : ConstantExpressionResult(value);
        if (!resolved) throw std::move(resolved.error());
        switch (resolved->kind) {
        case ValueKind::string: case ValueKind::number: case ValueKind::boolean:
        case ValueKind::metric: case ValueKind::color: case ValueKind::list: break;
        default: fail(value, "Expression requires a compile-time value or a native reactive binding");
        }
        if (resolved->kind == ValueKind::string && resolved->text.size() > 1024 * 1024)
            fail(value, "Constant String exceeds 1 MiB");
        return leaves_.emplace(&value, std::move(*resolved)).first->second;
    }
    double number(const Value& value) const {
        const auto parsed = ii::core::parse_decimal(value.text);
        if (!numeric(value.kind) || !parsed) fail(value, "Expected a finite numeric expression");
        return *parsed;
    }
    Value number(double value, ValueKind kind, const Value& at) const {
        if (!std::isfinite(value)) fail(at, "Numeric expression produced a non-finite value");
        return {kind, ii::core::format_decimal(value), {}, at.position};
    }
    static Value boolean(bool value) { return {ValueKind::boolean, value ? "true" : "false", {}}; }
    ValueKind common(const Value& at, ValueKind a, ValueKind b) const {
        if (a == b) return a;
        if (numeric(a) && numeric(b)) return ValueKind::metric;
        fail(at, "Conditional branches must have compatible types");
    }
    const Value& indexed(const Value& value) {
        if (auto found = leaves_.find(&value); found != leaves_.end()) return found->second;
        if (value.elements.size() != 2) fail(value, "Malformed index expression");
        const auto base = evaluate(value.elements[0]);
        if (base.kind != ValueKind::list) fail(value, "Constant indexing requires a List");
        const auto index = number(evaluate(value.elements[1]));
        if (index < 0 || std::trunc(index) != index || index >= static_cast<double>(base.elements.size()))
            fail(value, "Constant List index is outside its bounds");
        return leaves_.emplace(&value, base.elements[static_cast<std::size_t>(index)]).first->second;
    }
    ValueKind type(const Value& value, std::size_t depth = 1) {
        if (depth > 64) fail(value, "Maximum expression depth is 64");
        if (auto found = types_.find(&value); found != types_.end()) return found->second;
        if (++visited_ > 65536) fail(value, "Maximum expression node count is 65536");
        ValueKind result;
        if (value.kind == ValueKind::unary) {
            if (value.elements.size() != 1) fail(value, "Malformed unary expression");
            result = type(value.elements[0], depth + 1);
            if (value.text == "!") {
                if (result != ValueKind::boolean) fail(value, "Logical negation requires Bool");
            } else if (value.text != "+" && value.text != "-") fail(value, "Unknown unary operator");
            else if (!numeric(result)) fail(value, "Unary arithmetic requires Number or Metric");
        } else if (value.kind == ValueKind::binary) {
            if (value.elements.size() != 2) fail(value, "Malformed binary expression");
            const auto a = type(value.elements[0], depth + 1), b = type(value.elements[1], depth + 1);
            const auto& op = value.text;
            if (op == "&&" || op == "||") {
                if (a != ValueKind::boolean || b != ValueKind::boolean)
                    fail(value, "Logical operators require Bool operands");
                result = ValueKind::boolean;
            } else if (op == "==" || op == "!=") {
                if (a != b && !(numeric(a) && numeric(b))) fail(value, "Equality requires compatible operand types");
                result = ValueKind::boolean;
            } else if (op == "<" || op == "<=" || op == ">" || op == ">=") {
                if (!(numeric(a) && numeric(b)) && !(a == ValueKind::string && b == ValueKind::string))
                    fail(value, "Ordering requires numeric operands or Strings");
                result = ValueKind::boolean;
            } else if (op == "+" && a == ValueKind::string && b == ValueKind::string) result = ValueKind::string;
            else {
                if (!numeric(a) || !numeric(b)) fail(value, "Arithmetic requires numeric operands");
                if (op != "+" && op != "-" && op != "*" && op != "/" && op != "%")
                    fail(value, "Unknown binary operator");
                if ((op == "*" && a == ValueKind::metric && b == ValueKind::metric) ||
                    (op == "/" && a == ValueKind::number && b == ValueKind::metric))
                    fail(value, "Arithmetic produces an unsupported Metric dimension");
                result = (op == "/" && a == ValueKind::metric && b == ValueKind::metric)
                             ? ValueKind::number : (a == ValueKind::metric || b == ValueKind::metric)
                                                        ? ValueKind::metric : ValueKind::number;
            }
        } else if (value.kind == ValueKind::conditional) {
            if (value.elements.size() != 3) fail(value, "Malformed conditional expression");
            if (type(value.elements[0], depth + 1) != ValueKind::boolean)
                fail(value, "Conditional test requires Bool");
            result = common(value, type(value.elements[1], depth + 1), type(value.elements[2], depth + 1));
        } else if (value.kind == ValueKind::index) {
            const auto& selected = indexed(value);
            result = selected.kind;
        } else if (value.kind == ValueKind::member || value.kind == ValueKind::invocation) {
            fail(value, "Postfix access requires a native typed binding");
        } else {
            const auto& resolved = leaf(value);
            result = resolved.kind;
            if (numeric(result)) (void)number(resolved);
            if (result == ValueKind::list)
                for (const auto& item : resolved.elements) (void)type(item, depth + 1);
        }
        types_[&value] = result;
        return result;
    }
    bool equal(const Value& a, const Value& b) {
        if (numeric(a.kind) && numeric(b.kind)) return number(a) == number(b);
        if (a.kind != b.kind) return false;
        if (a.text != b.text || a.elements.size() != b.elements.size()) return false;
        for (std::size_t i = 0; i < a.elements.size(); ++i)
            if (!equal(a.elements[i], b.elements[i])) return false;
        return true;
    }
    Value evaluate(const Value& value) {
        if (!is_expression(value.kind)) {
            const auto& resolved = leaf(value);
            if (resolved.kind != ValueKind::list) return resolved;
            Value list{ValueKind::list, {}, {}, value.position};
            list.elements.reserve(resolved.elements.size());
            for (const auto& item : resolved.elements) {
                (void)type(item);
                list.elements.push_back(evaluate(item));
            }
            return list;
        }
        (void)type(value);
        if (value.kind == ValueKind::unary) {
            auto operand = evaluate(value.elements[0]);
            if (value.text == "!") return boolean(operand.text != "true");
            return number((value.text == "-" ? -1 : 1) * number(operand), operand.kind, value);
        }
        if (value.kind == ValueKind::conditional) {
            auto selected = evaluate(value.elements[evaluate(value.elements[0]).text == "true" ? 1 : 2]);
            if (numeric(selected.kind)) selected.kind = types_.at(&value);
            return selected;
        }
        if (value.kind == ValueKind::index) {
            return indexed(value);
        }
        auto a = evaluate(value.elements[0]);
        const auto& op = value.text;
        if (op == "&&" && a.text == "false") return boolean(false);
        if (op == "||" && a.text == "true") return boolean(true);
        auto b = evaluate(value.elements[1]);
        if (op == "&&" || op == "||") return boolean(b.text == "true");
        if (op == "==" || op == "!=") return boolean(equal(a, b) == (op == "=="));
        if (op == "+" && a.kind == ValueKind::string) {
            if (a.text.size() > 1024 * 1024 - b.text.size()) fail(value, "Constant String exceeds 1 MiB");
            a.text += b.text;
            return a;
        }
        if (op == "<" || op == "<=" || op == ">" || op == ">=") {
            const auto less = a.kind == ValueKind::string ? a.text < b.text : number(a) < number(b);
            const auto same = equal(a, b);
            return boolean(op == "<" ? less : op == "<=" ? less || same : op == ">" ? !less && !same : !less);
        }
        const auto left = number(a), right = number(b);
        if ((op == "/" || op == "%") && right == 0) fail(value, "Division or remainder by zero");
        const auto result = op == "+" ? left + right : op == "-" ? left - right : op == "*" ? left * right :
                            op == "/" ? left / right : std::fmod(left, right);
        return number(result, types_.at(&value), value);
    }
public:
    Folder(const ConstantResolver& resolver, SourcePosition fallback) : resolver_(resolver), fallback_(fallback) {}
    Value run(const Value& value) { (void)type(value); return evaluate(value); }
};
} // namespace
ConstantExpressionResult fold_constant_expression(const Value& value, const ConstantResolver& resolver,
                                                  SourcePosition fallback) {
    try { return Folder(resolver, fallback).run(value); }
    catch (Diagnostic& diagnostic) { return std::unexpected(std::move(diagnostic)); }
}
} // namespace lvui
