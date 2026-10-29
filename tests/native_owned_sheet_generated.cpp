#include "NativeOwnedSheet.generated.h"
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <memory>
#include <stdexcept>

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
}

int main() {
    using View = lvui_generated::NativeOwnedSheet;
    auto tree = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {800, 600}));
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder) { return View::build(builder, {}, [](auto&) {}, &scope); }));
    const auto nodes = take(tree->snapshots());
    const auto found = std::ranges::find_if(nodes, [](const auto& value) { return value.id.ends_with("/sheet"); });
    require(found != nodes.end(), "Generated Sheet did not mount");
    auto sheet = take(tree->find(found->id));
    (void)take(tree->drawing_snapshot({800, 600})); good(sheet.open_at(0, 0));
    auto editor = take(sheet.popup_content("editor")); const auto id = take(editor.snapshot()).id;
    require(text(editor) == "Document" && take(sheet.snapshot()).bounds.width == 560 && scope->backend_0.opens == 1,
            "Generated Sheet did not mount its retained content");
    scope->backend_0.title.set("Changed"); scope->backend_0.text.set("Updated");
    require(take(take(sheet.popup_content("sheet.title")).snapshot()).text == "Changed" && text(editor) == "Updated",
            "Generated Sheet did not update its content");
    auto graph = scope->native_lifetime.lock(); require(bool(graph), "Generated Sheet graph is unavailable");
    scope->backend_0.width.set(-1);
    require(!take(graph->failures()).empty() && take(sheet.snapshot()).bounds.width == 560,
            "Invalid Sheet width changed the mounted surface");
    scope->backend_0.width.set(600); require(take(graph->failures()).empty(), "Sheet did not recover from invalid width");
    good(sheet.close_popup()); good(sheet.open_at(0, 0));
    require(take(take(sheet.popup_content("editor")).snapshot()).id == id && text(editor) == "Updated" &&
            scope->backend_0.opens == 2 && scope->backend_0.closes == 1,
            "Generated Sheet did not preserve content across openings");
    scope->backend_0.presentation.set(lvui::SheetPresentation::mobile);
    scope->backend_0.detent.set(lvui::SheetDetent::medium);
    require(take(sheet.snapshot()).bounds.width == 800 && take(sheet.snapshot()).bounds.height == 342 &&
            take(editor.snapshot()).id == id, "Reactive Sheet presentation replaced content or ignored its detent");
    good(sheet.close_popup()); std::weak_ptr<View::Scope> weak = scope; good(host.clear_content()); scope.reset(); graph.reset();
    require(weak.expired() && !sheet.snapshot() && !editor.snapshot(), "Generated Sheet retained its activation"); tree->close();
}
