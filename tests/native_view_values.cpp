#include "NativeViewValues.generated.h"
#include "NativeConditional.generated.h"
#include <iostream>
#include <stdexcept>
int main() {
    try {
        using View = lvui_generated::NativeViewValues;
        using First = View::LocalView_0<View::Scope>;
        using Second = View::LocalView_1<View::Scope>;
        auto parent = std::make_shared<View::Scope>();
        auto first = std::make_shared<First::Scope>(); first->parent = parent;
        auto second = std::make_shared<Second::Scope>(); second->parent = parent;
        auto a = std::make_shared<lvui::native::Bindings>();
        auto b = std::make_shared<lvui::native::Bindings>();
        auto check = [](bool value) { if (!value) throw std::runtime_error("Reusable View activation contract failed"); };
        check(bool(First::initialize_values(first, a)) && bool(Second::initialize_values(second, b)));
        check(first->derived_value->get() == 4 && second->derived_value->get() == 4);
        first->backend_0.count.set(7);
        check(first->derived_value->get() == 14 && first->fixed_initial == 2 && second->derived_value->get() == 4);
        parent->backend_0.count.set(12);
        check(second->backend_0.count.get() == 2);
        check(bool(a->close()) && bool(b->close()));
        // Compile the actual host/factory bodies, then exercise the branch's
        // independent state without opening a window in this test group.
        using Conditional = lvui_generated::NativeConditional;
        using Branch = Conditional::LocalView_0<Conditional::Scope>;
        auto condition_owner = std::make_shared<Conditional::Scope>();
        auto branch = std::make_shared<Branch::Scope>(); branch->parent = condition_owner;
        auto branch_bindings = std::make_shared<lvui::native::Bindings>();
        check(bool(Branch::initialize_values(branch, branch_bindings)) && branch->derived_value->get() == 4);
        check(bool(branch_bindings->close()));
        std::cout << "Reusable View values compiled into independent C++ activations\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
