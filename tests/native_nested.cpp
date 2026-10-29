#include "NativeNested.generated.h"
#include "NativeRootScope.generated.h"
#include "NativeLocalOnly.generated.h"
#include <iostream>
#include <stdexcept>
namespace {
void check(bool value) { if (!value) throw std::runtime_error("Nested native activation contract failed"); }
}
int main() {
    try {
        using View = lvui_generated::NativeNested;
        using Child = View::LocalView_0<View::Scope>;
        using Grandchild = View::LocalView_1<Child::Scope>;
        using Sibling = View::LocalView_2<View::Scope>;
        auto parent = std::make_shared<View::Scope>();
        auto child = std::make_shared<Child::Scope>(); child->parent = parent;
        auto grandchild = std::make_shared<Grandchild::Scope>(); grandchild->parent = child;
        auto sibling = std::make_shared<Sibling::Scope>(); sibling->parent = parent;
        auto a = std::make_shared<lvui::native::Bindings>();
        auto b = std::make_shared<lvui::native::Bindings>();
        auto c = std::make_shared<lvui::native::Bindings>();
        check(bool(Child::initialize_values(child, a)));
        check(bool(Grandchild::initialize_values(grandchild, b)));
        check(bool(Sibling::initialize_values(sibling, c)));
        check(child->derived_inherited->get() == 6 && child->state_own->get() == 3);
        check(grandchild->derived_deep->get() == 8 && sibling->derived_isolated->get() == 10);
        parent->backend_0.count.set(4);
        check(child->derived_inherited->get() == 12 && grandchild->derived_deep->get() == 14);
        check(sibling->derived_isolated->get() == 10);
        child->backend_0.count.set(5);
        check(parent->backend_0.count.get() == 4 && child->state_own->get() == 3);
        check(grandchild->derived_deep->get() == 17);
        sibling->backend_0.count.set(3);
        check(sibling->derived_isolated->get() == 15 && child->backend_0.count.get() == 5);
        check(bool(b->close()) && bool(a->close()) && bool(c->close()));
        parent->backend_0.count.set(9);
        check(parent->backend_0.count.get() == 9);
        using Root = lvui_generated::NativeRootScope;
        auto root = std::make_shared<Root::Scope>();
        auto root_bindings = std::make_shared<lvui::native::Bindings>();
        check(bool(Root::initialize_values(root, root_bindings)));
        check(root->derived_moduleValue->get() == 6 && root->derived_windowValue->get() == 12);
        root->value_count.set(5);
        check(root->derived_moduleValue->get() == 10 && root->derived_windowValue->get() == 12);
        root->state_count_1->set(7);
        check(root->derived_windowValue->get() == 21 && root->derived_moduleValue->get() == 10);
        root->backend_0.count.set(12);
        check(root->backend_1.count.get() == 2);
        check(bool(root_bindings->close()));
        using Local = lvui_generated::NativeLocalOnly;
        auto lexical_parent = std::make_shared<Local::Scope>();
        auto local_only = std::make_shared<Local::LocalView_0<Local::Scope>::Scope>();
        local_only->parent = lexical_parent;
        check(local_only->value_count.get() == 3);
        std::cout << "Nested native C++ imports, shadowing and borrowed values passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
