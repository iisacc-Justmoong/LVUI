#include "NativeTableStructureControls.generated.h"
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
    return std::move(*value);
}
template<class T> T take(lvui::TableResult<T> value) {
    if (!value) throw std::runtime_error(value.error().message); return std::move(*value);
}
void good(ii::core::Result<void> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
}
}
int main() {
    using namespace ii::gui; using View = lvui_generated::NativeTableStructureControls;
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {400, 240})); std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder) { return View::build(builder, {}, [](auto&) {}, &scope); }));
    const auto find = [&](std::string_view suffix) {
        const auto nodes = take(tree->snapshots()); const auto node = std::ranges::find_if(nodes, [&](const auto& value) { return value.id.ends_with(suffix); });
        if (node == nodes.end()) throw std::runtime_error("Generated table control was not mounted"); return take(tree->find(node->id));
    };
    auto rows = find(".row-controls"); auto bounds = take(rows.snapshot()).bounds;
    good(tree->input({.kind=WidgetInputKind::pointer_down, .x=bounds.x+10, .y=bounds.y+12, .buttons=1}));
    good(tree->input({.kind=WidgetInputKind::pointer_up, .x=bounds.x+10, .y=bounds.y+12, .buttons=1}));
    require(take(scope->backend_0.source->snapshot())->rows.size() == 4 && scope->backend_0.structures == 1,
            "Generated structure controls did not commit through the injected editor");
    auto graph = scope->native_lifetime.lock(); require(graph && take(graph->failures()).empty(), "Generated control reported a reactive error");
    auto body = find(".body"), menu = find(".structure-menu"), delete_row = find(".delete-row");
    auto text=lvui::TableText{};text.cell_name="{row}행 {column}";text.delete_row="행 삭제";
    const auto before_locale=take(scope->backend_0.source->snapshot())->revision;
    scope->backend_0.tableText.set(text);
    const auto access=take(tree->accessibility_snapshots());
    require(std::ranges::any_of(access,[](const auto& node){return node.accessibility_name=="1행 Alpha";})&&
            take(delete_row.snapshot()).accessibility_name=="행 삭제"&&take(scope->backend_0.source->snapshot())->revision==before_locale&&
            take(graph->failures()).empty(),"Reactive declarative table text did not update without a document edit");
    const auto body_bounds = take(body.snapshot()).bounds;
    good(tree->input({.kind=WidgetInputKind::context_request, .x=body_bounds.x+10, .y=body_bounds.y+12, .target=take(body.snapshot()).id}));
    require(take(menu.snapshot()).opened, "Generated table did not open its delete menu");
    good(delete_row.activate());
    require(take(scope->backend_0.source->snapshot())->rows.size() == 3 && scope->backend_0.structures == 2 &&
            !take(menu.snapshot()).opened && take(graph->failures()).empty(),
            "Reactive document publication during menu deletion did not complete safely");
    scope->backend_0.showStructure.set(false);
    require(!take(rows.snapshot()).visible && take(graph->failures()).empty(), "Reactive structure visibility did not hide the controls");
    scope->backend_0.showStructure.set(true);
    const auto revision=take(scope->backend_0.source->snapshot())->revision;
    (void)take(scope->backend_0.source->replace({}, {}, revision));
    scope->backend_0.controller.set(std::make_shared<lvui::TableController>(scope->backend_0.source));
    auto first_row=find(".first-row"),first_column=find(".first-column");
    require(take(first_row.snapshot()).visible&&take(first_column.snapshot()).visible,
            "Generated table did not expose initial controls when the model became empty");
    good(first_row.activate());good(first_column.activate());
    const auto initialized=take(scope->backend_0.source->snapshot());
    require(initialized->rows.size()==1&&initialized->columns.size()==1&&initialized->rows[0].cells.size()==1&&
            scope->backend_0.structures==4&&!take(first_row.snapshot()).visible&&!take(first_column.snapshot()).visible&&take(graph->failures()).empty(),
            "Generated initial controls lost their structure callbacks or reactive state");
    good(host.clear_content()); scope.reset(); graph.reset(); tree->close();
}
