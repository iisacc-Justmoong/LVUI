#include "NativeOwnedModal.generated.h"
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T>T take(ii::core::Result<T> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
    return std::move(*value);
}
void good(ii::core::Result<void> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
}
std::string text(ii::gui::Widget field) {
    const auto snapshot = take(field.snapshot());
    return take(field.text_content({0, snapshot.text_length})).text;
}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree, std::string_view suffix) {
    const auto nodes = take(tree->snapshots());
    const auto item = std::ranges::find_if(nodes, [&](const auto& value) { return value.id.ends_with(suffix); });
    if (item == nodes.end()) throw std::runtime_error("Generated Modal did not mount");
    return take(tree->find(item->id));
}
}

int main() {
    using View = lvui_generated::NativeOwnedModal;
    using namespace ii::gui;
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {800, 600}));
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder) { return View::build(builder, {}, [](auto&) {}, &scope); }));
    auto modal = find(tree, "/dialog");
    (void)take(tree->drawing_snapshot({800, 600}));
    good(modal.open_at(0, 0));
    auto editor = take(modal.popup_content("editor"));
    const auto first = take(editor.snapshot()).id;
    if (text(editor) != "Body" || take(modal.snapshot()).bounds.width != 600 || scope->backend_0.opens != 1) {
        throw std::runtime_error("Generated Modal body=" + text(editor) +
            " width=" + std::to_string(take(modal.snapshot()).bounds.width) +
            " opens=" + std::to_string(scope->backend_0.opens));
    }
    auto nested = take(modal.popup_content("nested"));
    good(nested.open_at(0, 0));
    auto nested_body = take(nested.popup_content("nestedLabel"));
    require(take(nested_body.snapshot()).text == "Nested body", "Nested Modal body did not activate");
    good(nested.close_popup());
    require(!nested_body.snapshot() && take(modal.snapshot()).opened, "Nested Modal did not retire independently");
    good(take(modal.popup_content("dialog.action-0")).activate());
    require(scope->backend_0.saves == 1 && take(modal.snapshot()).opened, "Generated Modal action did not reach the backend");
    scope->backend_0.title.set("Changed"); scope->backend_0.text.set("Updated body");
    require(take(take(modal.popup_content("dialog.title")).snapshot()).text == "Changed" &&
            text(editor) == "Updated body" && take(editor.snapshot()).id == first,
            "Generated Modal reactive update replaced its activation");
    auto graph = scope->native_lifetime.lock(); require(bool(graph), "Generated Modal bindings are unavailable");
    scope->backend_0.width.set(-1);
    require(!take(graph->failures()).empty() && take(modal.snapshot()).bounds.width == 600,
            "Invalid Modal input changed its popup");
    scope->backend_0.width.set(700); require(take(graph->failures()).empty(), "Modal did not recover after invalid input");
    good(modal.cancel_popup());
    require(scope->backend_0.closes == 1 && scope->backend_0.cancels == 1 && !editor.snapshot(),
            "Generated Modal did not retire and notify its body");
    good(modal.open_at(0, 0));
    auto replacement = take(modal.popup_content("editor"));
    require(take(replacement.snapshot()).id != first && text(replacement) == "Updated body" && scope->backend_0.opens == 2,
            "Generated Modal did not recreate a body against the existing backend");
    good(modal.close_popup());
    std::weak_ptr<View::Scope> weak = scope; good(host.clear_content()); scope.reset(); graph.reset();
    require(weak.expired() && !modal.snapshot(), "Generated Modal retained its activation"); tree->close();
}
