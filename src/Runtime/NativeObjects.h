#pragma once
#include <LVUI/NativeValues.h>
#include <initializer_list>
#include <span>

namespace lvui::native {
template<class T> concept CoreValue = std::same_as<std::remove_cvref_t<T>, ii::core::Value>;
template<class T> concept ObjectOperand = CoreValue<T> || Numeric<T> || String<T> ||
    std::same_as<std::remove_cvref_t<T>, bool>;

inline ii::core::Value object_value(const ii::core::Value& value) { return value; }
template<class T> requires (!CoreValue<T>)
ii::core::Value object_value(T&& value) { return ii::core::Value(std::forward<T>(value)); }
inline ii::core::Value require_object(ii::core::Value value) {
    if (value.kind() != ii::core::ValueKind::object) throw std::invalid_argument("Expected an Object value");
    return value;
}
inline bool object_numeric(const ii::core::Value& value) {
    return value.kind() == ii::core::ValueKind::signed_integer ||
           value.kind() == ii::core::ValueKind::unsigned_integer || value.kind() == ii::core::ValueKind::real;
}

template<ObjectOperand A, ObjectOperand B> requires (CoreValue<A> || CoreValue<B>)
ii::core::Value add(const A& left, const B& right) {
    auto a = object_value(left), b = object_value(right);
    const auto* x = std::get_if<std::string>(&a.storage());
    const auto* y = std::get_if<std::string>(&b.storage());
    if (x && y) return ii::core::Value(add(*x, *y));
    return ii::core::Value(add(number(a), number(b)));
}
#define LVUI_OBJECT_NUMERIC(name) \
template<class A, class B> requires ((CoreValue<A> || CoreValue<B>) && \
    (CoreValue<A> || Numeric<A>) && (CoreValue<B> || Numeric<B>)) \
double name(const A& a, const B& b) { return name(number(a), number(b)); }
LVUI_OBJECT_NUMERIC(subtract)
LVUI_OBJECT_NUMERIC(multiply)
LVUI_OBJECT_NUMERIC(divide)
LVUI_OBJECT_NUMERIC(remainder)
#undef LVUI_OBJECT_NUMERIC
inline double negate(const ii::core::Value& value) { return negate(number(value)); }
template<class Then, class Else>
requires ((CoreValue<std::invoke_result_t<Then>> || CoreValue<std::invoke_result_t<Else>>) &&
          ObjectOperand<std::invoke_result_t<Then>> && ObjectOperand<std::invoke_result_t<Else>>)
ii::core::Value choose(bool condition, Then then_value, Else else_value) {
    return condition ? object_value(then_value()) : object_value(else_value());
}

template<ObjectOperand A, ObjectOperand B> requires (CoreValue<A> || CoreValue<B>)
bool equal(const A& left, const B& right) {
    const auto a = object_value(left), b = object_value(right);
    if (object_numeric(a) && object_numeric(b)) return number(a) == number(b);
    return a == b;
}
template<ObjectOperand A, ObjectOperand B> requires (CoreValue<A> || CoreValue<B>)
bool less(const A& left, const B& right) {
    const auto a = object_value(left), b = object_value(right);
    const auto* x = std::get_if<std::string>(&a.storage());
    const auto* y = std::get_if<std::string>(&b.storage());
    if (x && y) return *x < *y;
    return number(a) < number(b);
}

inline ii::core::Value replace_object_field(const ii::core::Value& object,
                                             std::span<const std::string_view> path,
                                             const ii::core::Value& replacement) {
    if (path.empty()) throw std::invalid_argument("Object assignment requires a field path");
    const auto original = object.get<ii::core::Value::Object>();
    if (!original) throw std::invalid_argument("Object assignment requires an object value");
    auto copy = original->get();
    auto found = copy.find(path.front());
    if (found == copy.end()) throw std::out_of_range("Object assignment field was not found");
    if (path.size() == 1) {
        if (found->second.kind() == replacement.kind()) found->second = replacement;
        else if (object_numeric(found->second) && object_numeric(replacement)) {
            if (found->second.kind() == ii::core::ValueKind::real)
                found->second = ii::core::Value(number(replacement));
            else {
                const auto value = integer(replacement);
                if (found->second.kind() == ii::core::ValueKind::signed_integer)
                    found->second = ii::core::Value(value);
                else {
                    if (value < 0) throw std::out_of_range("Unsigned object field cannot be negative");
                    found->second = ii::core::Value(static_cast<std::uint64_t>(value));
                }
            }
        } else throw std::invalid_argument("Object assignment must preserve the field type");
    } else found->second = replace_object_field(found->second, path.subspan(1), replacement);
    return ii::core::Value(std::move(copy));
}
template<class Source, class Value>
ii::core::Result<void> assign_object_field(Source& source, std::initializer_list<std::string_view> path, Value&& value) {
    auto replacement = object_value(std::forward<Value>(value));
    auto updated = replace_object_field(source.get(), {path.begin(), path.size()}, replacement);
    return assign_value(source, std::move(updated));
}
} // namespace lvui::native
