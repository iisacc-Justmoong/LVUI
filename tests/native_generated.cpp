#include "NativeReactive.generated.h"
#include <iostream>
#include <stdexcept>
int main() {
    try {
        using View = lvui_generated::NativeReactive;
        static_assert(std::same_as<View::NativeBlueprint, NativeBlueprint>);
        auto scope = std::make_shared<View::Scope>();
        auto graph = std::make_shared<lvui::native::Bindings>();
        auto initialized = View::initialize_values(scope, graph);
        if (!initialized) throw std::runtime_error(initialized.error().message);
        auto check = [](bool value) { if (!value) throw std::runtime_error("Generated native state contract failed"); };
        check(scope->active());
        check(NativeBlueprint::constructions == 0);
        check(scope->derived_doubled->get() == 4 && scope->fixed_original == 2);
        check(scope->state_local->get() == 5 && scope->derived_localDoubled->get() == 10);
        check(scope->derived_message->get() == "Native!" && scope->backend_0.calls == 1);
        check(!View::initialize_values(scope, graph) && scope->backend_0.calls == 1);
        scope->backend_0.count.set(5);
        check(scope->derived_doubled->get() == 10 && scope->fixed_original == 2);
        check(scope->state_local->get() == 5);
        scope->state_local->set(8);
        check(scope->derived_localDoubled->get() == 16);
        scope->backend_0.ready.set(false);
        scope->backend_0.title.set("Ignored");
        check(scope->derived_message->get() == "Waiting");
        scope->backend_0.index.set(1);
        check(scope->derived_selected->get() == 7);
        scope->backend_0.index.set(100);
        check(scope->derived_selected->get() == 7 && !graph->failures()->empty());
        scope->backend_0.index.set(0);
        check(scope->derived_selected->get() == 3 && graph->failures()->empty());
        scope->backend_0.suffix = "published";
        scope->revision_0.set(1);
        check(scope->derived_caption->get() == "published" && scope->backend_0.calls == 2);
        check(scope->derived_nested->get() == "Detail");
        auto previous_document = scope->backend_0.document;
        previous_document->title.set("Nested");
        check(scope->derived_nested->get() == "Nested");
        scope->backend_0.document = std::make_shared<NativeFixture::Details>();
        scope->revision_0.set(2);
        previous_document->title.set("Detached");
        check(scope->derived_nested->get() == "Detail");
        scope->backend_0.document.reset(); scope->revision_0.set(3);
        check(scope->derived_nested->get() == "Detail" && !graph->failures()->empty());
        scope->backend_0.document = std::make_shared<NativeFixture::Details>(); scope->revision_0.set(4);
        check(graph->failures()->empty());
        auto closed = graph->close();
        check(bool(closed));
        check(!scope->active());
        scope->backend_0.count.set(8);
        check(scope->fixed_original == 2);
        auto separate = std::make_shared<View::Scope>();
        auto separate_graph = std::make_shared<lvui::native::Bindings>();
        check(bool(View::initialize_values(separate, separate_graph)));
        check(separate->derived_doubled->get() == 4 && separate->fixed_original == 2);
        std::weak_ptr<View::Scope> retained = separate;
        separate.reset();
        check(!retained.expired());
        check(bool(separate_graph->close()) && retained.expired());
        auto failing = std::make_shared<View::Scope>();
        failing->backend_0.index.set(100);
        auto failing_graph = std::make_shared<lvui::native::Bindings>();
        check(!View::initialize_values(failing, failing_graph) && !failing_graph->stats());
        bool retired = false;
        try { (void)failing->derived_doubled->get(); } catch (const std::logic_error&) { retired = true; }
        check(retired && !View::initialize_values(failing, failing_graph));
        std::cout << "Generated C++ native values passed without a UI host\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
