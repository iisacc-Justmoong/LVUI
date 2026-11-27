#include "EditorAutoPanel.generated.h"

#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> value) {
    if (!value) throw std::runtime_error(value.error().message);
    return std::move(*value);
}
void good(ii::core::Result<void> value) { if (!value) throw std::runtime_error(value.error().message); }
}

int main() {
    using View = lvui_generated::EditorAutoPanel;
    using namespace ii::gui;
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {640, 480}));
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder) { return View::build(builder, {}, {}, &scope); }));
    auto snapshots = take(tree->snapshots());
    auto input = std::ranges::find_if(snapshots, [](const auto& node) {
        return node.text_input && node.accessibility_name == "Text field";
    });
    require(input != snapshots.end(), "The schema did not create an accessible text field");
    require(std::ranges::any_of(snapshots, [](const auto& node) { return node.accessibility_name == "Boolean field"; }) &&
            std::ranges::any_of(snapshots, [](const auto& node) { return node.accessibility_name == "Amount field"; }) &&
            std::ranges::any_of(snapshots, [](const auto& node) { return node.accessibility_name == "Choice field"; }) &&
            std::ranges::any_of(snapshots, [](const auto& node) { return node.accessibility_name == "Pending field"; }) &&
            std::ranges::any_of(snapshots, [](const auto& node) { return node.text_input && node.accessibility_name == "Wide field"; }),
            "The schema did not create its other typed controls");
    auto field = take(tree->find(input->id));
    good(field.focus());
    good(tree->input({.kind = WidgetInputKind::text, .text = "x"}));
    auto& app = scope->backend_0;
    require(!app.edits.empty() && app.edits.back().field_id == "text" &&
            std::get<std::string>(*app.edits.back().value).find('x') != std::string::npos,
            "A generated input did not deliver its typed edit to the app");

    app.schema[0].value = std::string{"Revised"};
    app.schema[1].visible = false;
    app.schema.push_back({"added", "Added field", lvui::EditorFieldKind::text, std::string{"New"}});
    bool replacement_sink_called = false;
    auto rows = lvui::EditorAutoFormBinding{}.execute({app.schema,
        [&app, &replacement_sink_called](lvui::EditorFieldEdit edit) {
            replacement_sink_called = true;
            app.edits.push_back(std::move(edit));
        }});
    require(rows.has_value(), "The updated schema could not be projected");
    app.autoModel->fields.set(std::move(*rows));
    const auto refreshed = take(field.snapshot());
    require(take(field.text_content({0, refreshed.text_length})).text == "Revised",
            "The existing keyed field did not receive the updated value");
    good(field.focus());
    good(tree->input({.kind = WidgetInputKind::text, .text = "z"}));
    require(replacement_sink_called && app.edits.back().field_id == "text",
            "The keyed field kept a stale event destination after schema replacement");
    snapshots = take(tree->snapshots());
    require(!std::ranges::any_of(snapshots, [](const auto& node) { return node.accessibility_name == "Boolean field"; }) &&
            std::ranges::any_of(snapshots, [](const auto& node) { return node.accessibility_name == "Added field"; }),
            "Visibility or dynamic field insertion did not update the view");
    good(host.clear_content());
    scope.reset();
    require(!field.snapshot(), "A retired automatic form retained its input");
    tree->close();
}
