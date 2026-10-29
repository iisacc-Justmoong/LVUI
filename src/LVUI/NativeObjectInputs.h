#pragma once
#include "NativeValues.h"

namespace lvui::native {
namespace detail {
template<class T>struct InputObject final {using Type=std::remove_cv_t<T>;};
template<class T>struct InputObject<T*> final {using Type=typename InputObject<std::remove_cv_t<T>>::Type;};
template<class T>struct InputObject<std::shared_ptr<T>> final {using Type=typename InputObject<std::remove_cv_t<T>>::Type;};
template<class T,class D>struct InputObject<std::unique_ptr<T,D>> final {using Type=typename InputObject<std::remove_cv_t<T>>::Type;};
template<class T>struct InputObject<std::reference_wrapper<T>> final {using Type=typename InputObject<std::remove_cv_t<T>>::Type;};
template<class T>void require_input_object(const T& value) {
    if constexpr(std::is_pointer_v<T>||requires{value.get();*value;bool(value);}){
        if(!value)throw std::invalid_argument("A typed C++ View input requires a live object");
        if constexpr(!std::is_function_v<std::remove_reference_t<decltype(*value)>>)require_input_object(*value);
    }else if constexpr(member_reference<T>)require_input_object(value.get());
}
}
// C++ determines the value/handle type. The declaration names the exact payload
// type; storage keeps the caller's ownership and const qualifications.
template<class Expected,class T>auto object_input(T&& value) {
    using Actual=std::remove_cvref_t<T>;
    using Stored=detail::StoredValue<T>;
    static_assert(std::same_as<Actual,std::remove_cv_t<Expected>>||
        std::same_as<typename detail::InputObject<Actual>::Type,std::remove_cv_t<Expected>>,
        "C++ View input does not match its declared payload type");
    static_assert(std::copy_constructible<Stored>&&std::is_nothrow_move_assignable_v<Stored>,
        "C++ View input requires a copyable native value or shared/borrowed handle");
    detail::require_input_object(value);
    return value_copy(std::forward<T>(value));
}
}
