#include "NativeWindowAccentCollections.generated.h"
#include "NativeWindowAccentCollectionsStatic.generated.h"
#include <LVUI/StyleTokens.h>
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
using ii::gui::detail::WidgetState;
void require(bool value, const std::string& message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
    return std::move(*value);
}
void good(ii::core::Result<void> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
}
ii::gui::Widget find(const std::shared_ptr<WidgetState>& state, std::string_view id) {
    auto snapshots = take(state->snapshots());
    const auto found = std::ranges::find_if(snapshots, [&](const auto& item) {
        return item.id == id || item.id.ends_with(std::string("/") + std::string(id));
    });
    require(found != snapshots.end(), "Missing collection widget: " + std::string(id));
    return take(state->find(found->id));
}
ii::gui::Widget find_inside(const std::shared_ptr<WidgetState>& state, std::string_view container, std::string_view suffix) {
    const auto bounds=take(find(state,container).snapshot()).bounds;
    for(const auto& item:take(state->snapshots())) {
        const auto& b=item.bounds;
        if((item.id==suffix||item.id.ends_with(std::string("/")+std::string(suffix)))&&b.width>0&&b.height>0&&
           b.x>=bounds.x-.01&&b.y>=bounds.y-.01&&b.x+b.width<=bounds.x+bounds.width+.01&&b.y+b.height<=bounds.y+bounds.height+.01)
            return take(state->find(item.id));
    }
    throw std::runtime_error("Missing child in "+std::string(container));
}
double linear(double value) { return value <= .04045 ? value / 12.92 : std::pow((value + .055) / 1.055, 2.4); }
bool near(double a, double b) { return std::abs(a - b) < 1e-5; }
bool has_color(const std::shared_ptr<WidgetState>& state, const ii::gui::Widget& widget, lvui::StyleColor expected) {
    const auto frame = take(state->drawing_snapshot({1000, 320}));
    const auto bounds = take(widget.snapshot()).bounds;
    const auto matches = [&](const auto& panels) {
        return std::ranges::any_of(panels, [&](const auto& panel) {
            const auto& b = panel.bounds; const auto& c = panel.top;
            return b.width > 0 && b.height > 0 && b.x >= bounds.x - .01 && b.y >= bounds.y - .01 &&
                b.x + b.width <= bounds.x + bounds.width + .01 && b.y + b.height <= bounds.y + bounds.height + .01 &&
                near(c.red, linear(expected.red)) && near(c.green, linear(expected.green)) &&
                near(c.blue, linear(expected.blue)) && near(c.alpha, expected.alpha);
        });
    };
    return matches(frame.panels) || std::ranges::any_of(frame.overlays, [&](const auto& layer) { return matches(layer.panels); });
}
void expect_color(const std::shared_ptr<WidgetState>& state, std::string_view id, lvui::StyleColor color) {
    auto widget = find(state, id);
    if (!has_color(state, widget, color)) {
        const auto bounds = take(widget.snapshot()).bounds;
        std::cerr << id << " bounds " << bounds.x << ',' << bounds.y << ' ' << bounds.width << 'x' << bounds.height << '\n';
        for (const auto& entry : take(state->snapshots())) if (entry.id.find(std::string(id)) != std::string::npos)
            std::cerr << "  widget " << entry.id << ' ' << entry.bounds.x << ',' << entry.bounds.y << ' '
                      << entry.bounds.width << 'x' << entry.bounds.height << '\n';
        const auto frame = take(state->drawing_snapshot({1000, 320}));
        const auto dump = [&](const auto& panels) { for (const auto& p : panels) {
            const auto& b = p.bounds;
            if (b.x >= bounds.x - .01 && b.y >= bounds.y - .01 && b.x + b.width <= bounds.x + bounds.width + .01 && b.y + b.height <= bounds.y + bounds.height + .01)
                std::cerr << "  panel " << b.x << ',' << b.y << ' ' << b.width << 'x' << b.height << " color "
                          << p.top.red << ',' << p.top.green << ',' << p.top.blue << ',' << p.top.alpha << '\n';
        }};
        dump(frame.panels); for (const auto& overlay : frame.overlays) dump(overlay.panels);
        throw std::runtime_error("Unexpected accent color for " + std::string(id));
    }
}
void expect_collection_colors(const std::shared_ptr<WidgetState>& state, lvui::StyleColor color, bool virtualized) {
    using lvui::figma_defaults::with_alpha;
    expect_color(state, "item", with_alpha(color, .24));
    for (auto id : {"listItem", "nestedItem", "composite"}) expect_color(state, id, with_alpha(color, .30));
    expect_color(state, "composite.toggle", color);
    expect_color(state, "check.checkbox", color);
    expect_color(state, "quantity.stepper", color);
    expect_color(state, "cell", with_alpha(color, 31. / 255.)); expect_color(state, "cell", color);
    if (virtualized) {
        for (auto id : {"tree", "treePanel"}) expect_color(state, id, with_alpha(color, .24));
        expect_color(state, "list", with_alpha(color, .30));
        expect_color(state, "table", with_alpha(color, 31. / 255.)); expect_color(state, "table", color);
        expect_color(state, "tableRow", with_alpha(color, 31. / 255.)); expect_color(state, "tableRow", color);
        const auto selected = color == lvui::figma_defaults::accent_purple_default
            ? lvui::figma_defaults::accent_purple_selected
            : color == lvui::figma_defaults::accent_green_default
                ? lvui::figma_defaults::accent_green_selected : lvui::figma_defaults::accent_blue_selected;
        expect_color(state, "settings.segments", selected);
    }
    constexpr lvui::StyleColor red{1, 0, 0, 1}, green{0, 1, 0, 1};
    for (auto id : {"fixedItem", "fixedListItem", "fixedCell"}) expect_color(state, id, red);
    expect_color(state, "fixedCell", green);
}
}

int main() {
    using View = lvui_generated::NativeWindowAccentCollections;
    using Static = lvui_generated::NativeWindowAccentCollectionsStatic;
    using namespace ii::gui;
    WindowGeometryValue geometry{"Collections", {1000, 320}, {320, 220}};
    bool reject = false;
    detail::WidgetFrameHooks hooks;
    hooks.window_geometry = [&](const WindowGeometry& next) -> ii::core::Result<void> {
        if (reject) return ii::core::fail(ii::core::ErrorCode::failed, "Native geometry rejected");
        auto updated = resolve_window_geometry(geometry, next);
        if (!updated) return std::unexpected(updated.error());
        geometry = *updated; return {};
    };
    auto state = std::make_shared<WidgetState>([](ii::runtime::Task task) { task(); }, View::appearance(), std::move(hooks));
    std::shared_ptr<View::Scope> scope;
    good(state->construct([&](WidgetBuilder& builder) { return View::build(builder, {}, {}, &scope); }));
    good(state->commit_window_properties()); state->commit_publication();
    auto graph = scope->native_lifetime.lock(); require(bool(graph), "Missing binding graph");
    (void)take(state->drawing_snapshot({1000, 320}));
    good(state->input({.kind=WidgetInputKind::pointer_move, .x=999, .y=319}));
    expect_collection_colors(state, lvui::figma_defaults::accent_blue_default, false);
    auto list = find(state, "list"), tree = find(state, "tree"), table = find(state, "table");
    const auto list_id = take(list.snapshot()).id, tree_id = take(tree.snapshot()).id, table_id = take(table.snapshot()).id;
    scope->backend_0.accent.set("purple");
    require(take(graph->failures()).empty(), "Collection accent update failed");
    expect_collection_colors(state, lvui::figma_defaults::accent_purple_default, false);
    require(take(list.snapshot()).id == list_id && take(tree.snapshot()).id == tree_id && take(table.snapshot()).id == table_id,
            "Accent update recreated a collection");
    require(scope->backend_0.list.get()->selection().value() == "one" &&
            scope->backend_0.hierarchy.get()->selected_key().value() == "root" &&
            scope->backend_0.table.get()->selection().value()->current.row == "r" &&
            scope->backend_0.list_source->snapshot().value()->revision == 0 &&
            scope->backend_0.hierarchy_source->snapshot().value()->revision == 0 &&
            scope->backend_0.table_source->snapshot().value()->revision == 0,
            "Accent update changed selection or document data");
    reject = true; good(graph->begin_batch()); scope->backend_0.width.set(900); scope->backend_0.accent.set("green");
    require(!graph->end_batch(), "Rejected window change was accepted");
    expect_collection_colors(state, lvui::figma_defaults::accent_purple_default, false);
    reject = false; scope->backend_0.width.set(920);
    require(take(graph->failures()).empty(), "Window did not recover");
    expect_collection_colors(state, lvui::figma_defaults::accent_green_default, false);

    // A content host publishes a complete in-memory generation without opening
    // a native window. Drive the committed accent directly to exercise virtual rows.
    auto embedded = std::make_shared<WidgetState>([](ii::runtime::Task task) { task(); }, View::appearance());
    auto host = take(embedded->content_host({}, "host", {1000, 320}));
    std::shared_ptr<View::Scope> embedded_scope;
    good(host.set_content([&](WidgetBuilder& builder) { return View::build(builder, {}, {}, &embedded_scope); }));
    expect_collection_colors(embedded, lvui::figma_defaults::accent_blue_default, true);
    embedded_scope->window_accent.set(lvui::WindowAccent::purple);
    auto embedded_graph = embedded_scope->native_lifetime.lock();
    require(embedded_graph && take(embedded_graph->failures()).empty(), "Embedded accent update failed");
    expect_collection_colors(embedded, lvui::figma_defaults::accent_purple_default, true);
    require(embedded_scope->backend_0.list.get()->selection().value() == "one" &&
            embedded_scope->backend_0.hierarchy.get()->selected_key().value() == "root" &&
            embedded_scope->backend_0.table.get()->selection().value()->current.row == "r",
            "Embedded accent update changed selection");
    auto toolbar_button = find(embedded, "listToolbar.slot0"); good(toolbar_button.focus());
    expect_color(embedded, "listToolbar", lvui::figma_defaults::accent_purple_default);
    embedded_scope->window_accent.set(lvui::WindowAccent::green);
    require(take(toolbar_button.snapshot()).focused && take(embedded_graph->failures()).empty(),
            "Changing accent disturbed a focused list toolbar button");
    expect_color(embedded, "listToolbar", lvui::figma_defaults::accent_green_default);
    auto footer_button = find(embedded, "footer.slot0"); good(footer_button.focus());
    expect_color(embedded, "footer", lvui::figma_defaults::accent_green_default);
    embedded_scope->window_accent.set(lvui::WindowAccent::purple);
    require(take(footer_button.snapshot()).focused && take(embedded_graph->failures()).empty(),
            "Changing accent disturbed a focused list footer button");
    expect_color(embedded, "footer", lvui::figma_defaults::accent_purple_default);
    auto header = find(embedded, "tableHeader"); good(header.focus());
    good(embedded->input({.kind=WidgetInputKind::key_down, .key=WidgetKey::home}));
    expect_color(embedded, "tableHeader", lvui::figma_defaults::accent_purple_default);
    embedded_scope->window_accent.set(lvui::WindowAccent::green);
    require(take(header.snapshot()).focused && take(embedded_graph->failures()).empty(),
            "Changing accent disturbed the focused table header");
    expect_color(embedded, "tableHeader", lvui::figma_defaults::accent_green_default);
    auto hierarchy_button=find_inside(embedded,"hierarchyTools","button"); good(hierarchy_button.focus());
    expect_color(embedded,"hierarchyTools",lvui::figma_defaults::accent_green_default);
    embedded_scope->window_accent.set(lvui::WindowAccent::purple);
    require(take(hierarchy_button.snapshot()).focused&&take(embedded_graph->failures()).empty(),
            "Changing accent disturbed a focused hierarchy toolbar button");
    expect_color(embedded,"hierarchyTools",lvui::figma_defaults::accent_purple_default);

    auto static_state = std::make_shared<WidgetState>([](ii::runtime::Task task) { task(); }, Static::appearance());
    auto static_host = take(static_state->content_host({}, "host", {1000, 320}));
    good(static_host.set_content([](WidgetBuilder& builder) { return Static::build(builder); }));
    using lvui::figma_defaults::with_alpha;
    expect_color(static_state, "staticItem", with_alpha(lvui::figma_defaults::accent_purple_default, .24));
    expect_color(static_state, "staticListItem", with_alpha(lvui::figma_defaults::accent_purple_default, .30));
    expect_color(static_state, "staticCell", with_alpha(lvui::figma_defaults::accent_purple_default, 31. / 255.));
    expect_color(static_state, "staticCell", lvui::figma_defaults::accent_purple_default);
    expect_color(static_state, "staticRow", with_alpha(lvui::figma_defaults::accent_purple_default, 31. / 255.));
    expect_color(static_state, "staticRow", lvui::figma_defaults::accent_purple_default);
    expect_color(static_state, "staticToggle.toggle", lvui::figma_defaults::accent_purple_default);
    static_state->close(); embedded->close(); state->close();
}
