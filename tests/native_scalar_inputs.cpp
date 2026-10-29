#include "NativeScalarInputs.generated.h"
#include <iostream>
#include <stdexcept>
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int main() {
    try {
        using View = lvui_generated::NativeScalarInputs;
        using Child = View::LocalView_0<View::Scope>;
        auto owner = std::make_shared<View::Scope>();
        auto child = std::make_shared<Child::Scope>(); child->parent = owner;
        auto parent_graph = std::make_shared<lvui::native::Bindings>();
        auto graph = std::make_shared<lvui::native::Bindings>();
        owner->native_lifetime = parent_graph;
        check(bool(Child::initialize_values(child, graph)), "Component scalar initialization failed");
        auto& model = owner->backend_0;
        check(model.calls == 1, "A scalar component input was evaluated separately for its consumers");
        check(child->derived_first->get() == "2#1" && child->derived_second->get() == "2#1" &&
              child->derived_fallback->get() == "2#1" && child->fixed_pinned == "2#1",
              "Consumers, dependent defaults or Static did not share one input value");
        model.count.set(5);
        check(model.calls == 2 && child->derived_first->get() == "5#2" &&
              child->derived_second->get() == "5#2" && child->derived_fallback->get() == "5#2" && child->fixed_pinned == "2#1",
              "A dependency update repeated input evaluation or changed a Static snapshot");
        model.fail = true; model.count.set(7);
        check(model.calls == 3 && child->derived_first->get() == "5#2" && !graph->failures()->empty(),
              "A failed component input replaced its last valid value");
        model.fail = false; model.count.set(8);
        check(model.calls == 4 && child->derived_first->get() == "8#4" && child->derived_second->get() == "8#4",
              "A failed input did not recover as one value");
        check(bool(graph->close()), "Component input graph did not close");
        model.count.set(9);
        check(model.calls == 4, "A retired component input continued evaluating");
        bool refused = false;
        try { (void)child->derived_first->get(); } catch (const std::logic_error&) { refused = true; }
        check(refused, "A closed component input allowed a stale read");
        check(bool(parent_graph->close()), "Parent graph did not close");
        child.reset(); owner.reset();
        check(NativeScalarInputModel::live == 0, "A scalar input retained its backend owner");
        std::cout << "Native component scalar inputs: one value, defaults, updates, failure and retirement passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
