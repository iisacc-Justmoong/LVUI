#pragma once
#include <concepts>
#include <functional>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace lvui::native {
// AOT supplies a unique field selector. Parent traversal is resolved by the
// C++ type system, including forwarding views without their own widget node.
// This borrows storage; it never retains another activation or evaluates it.
template<class ScopePointer, class Selector>
decltype(auto) component_storage(const ScopePointer& scope, const Selector& select) {
    using Scope = std::remove_reference_t<decltype(*scope)>;
    if (!scope) throw std::logic_error("Component input owner is unavailable");
    if constexpr (std::invocable<const Selector&, Scope&>) {
        static_assert(std::is_lvalue_reference_v<std::invoke_result_t<const Selector&, Scope&>>,
                      "A component storage selector must borrow its field");
        const auto& value = std::invoke(select, *scope);
        if (!value) throw std::logic_error("Component input is not initialized");
        return (value);
    } else if constexpr (requires { scope->parent; }) {
        return component_storage(scope->parent, select);
    } else {
        static_assert(std::invocable<const Selector&, Scope&>, "Component input escaped its compiled activation");
    }
}
template<class ScopePointer, class Selector>
decltype(auto) component_input(const ScopePointer& scope, const Selector& select) {
    return *component_storage(scope, select);
}
}
