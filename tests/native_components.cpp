#include "NativeComponents.generated.h"
#include "NativeComponentRetirement.generated.h"
#include <iostream>
#include <stdexcept>
int main() {
    try {
        using View = lvui_generated::NativeComponents;
        using Outer = View::LocalView_0<View::Scope>;
        using Inner = View::LocalView_1<Outer::Scope>;
        auto check = [](bool value) { if (!value) throw std::runtime_error("Native component value contract failed"); };
        auto parent = std::make_shared<View::Scope>();
        auto outer = std::make_shared<Outer::Scope>(); outer->parent = parent;
        auto inner = std::make_shared<Inner::Scope>(); inner->parent = outer;
        auto parent_bindings = std::make_shared<lvui::native::Bindings>();
        auto child_bindings = std::make_shared<lvui::native::Bindings>();
        parent->native_lifetime = parent_bindings;
        outer->native_lifetime = parent_bindings;
        check(bool(Inner::initialize_values(inner, child_bindings)));
        check(inner->derived_doubled->get() == 4 && inner->fixed_frozen == 2);
        parent->backend_1.count.set(7);
        check(inner->derived_doubled->get() == 14 && inner->fixed_frozen == 2);
        parent->backend_0.count.set(20);
        outer->backend_0.count.set(30);
        inner->backend_0.count.set(40);
        check(inner->derived_doubled->get() == 14);
        check(bool(child_bindings->close()));
        parent->backend_1.count.set(8);
        bool refused = false;
        try { (void)inner->derived_doubled->get(); } catch (const std::logic_error&) { refused = true; }
        check(refused);
        check(bool(parent_bindings->close()));
        using Conditional = lvui_generated::NativeComponentRetirement;
        using Branch = Conditional::LocalView_0<Conditional::Scope>;
        auto owner = std::make_shared<Conditional::Scope>();
        auto branch = std::make_shared<Branch::Scope>(); branch->parent = owner;
        auto bindings = std::make_shared<lvui::native::Bindings>();
        owner->backend_0.count.set(9);
        check(bool(Branch::initialize_values(branch, bindings)) && branch->fixed_initial == 9);
        check(bool(bindings->close()));
        std::cout << "Typed component C++ scopes, forwarding and reactive values passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
