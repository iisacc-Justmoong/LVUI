#include "NativeWindowAccentExtras.generated.h"
#include "NativeWindowAccentExtrasStatic.generated.h"
#include <LVUI/StyleTokens.h>
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
using ii::gui::detail::WidgetState;
void require(bool condition, const std::string& message) { if (!condition) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> result) {
    if (!result) {
        if (result.error().cause) std::rethrow_exception(result.error().cause);
        throw std::runtime_error(result.error().message);
    }
    return std::move(*result);
}
void good(ii::core::Result<void> result) {
    if (!result) {
        if (result.error().cause) std::rethrow_exception(result.error().cause);
        throw std::runtime_error(result.error().message);
    }
}
ii::gui::Widget find(const std::shared_ptr<WidgetState>& state, std::string_view suffix) {
    auto snapshots = take(state->snapshots());
    auto item = std::ranges::find_if(snapshots, [&](const auto& value) {
        return value.id == suffix || value.id.ends_with(std::string("/") + std::string(suffix));
    });
    require(item != snapshots.end(), "Missing control: " + std::string(suffix));
    return take(state->find(item->id));
}
double linear(double value) { return value <= .04045 ? value / 12.92 : std::pow((value + .055) / 1.055, 2.4); }
bool near(double a, double b) { return std::abs(a - b) < 1e-5; }
bool has_color(const std::shared_ptr<WidgetState>& state, const ii::gui::Widget& widget, lvui::StyleColor expected) {
    const auto frame = take(state->drawing_snapshot({640, 420}));
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
    if (matches(frame.panels)) return true;
    return std::ranges::any_of(frame.overlays, [&](const auto& layer) { return matches(layer.panels); });
}
void expect_color(const std::shared_ptr<WidgetState>& state, std::string_view id, lvui::StyleColor color) {
    require(has_color(state, find(state, id), color), "Unexpected color for " + std::string(id));
}
void expect_focus(const std::shared_ptr<WidgetState>& state, std::string_view id, lvui::StyleColor color) {
    good(find(state, id).focus()); expect_color(state, id, color);
}
void expect_palette(const std::shared_ptr<WidgetState>& state, lvui::StyleColor accent) {
    constexpr lvui::StyleColor red{1, 0, 0, 1}, swatch{0, 1, 0, 1};
    expect_color(state, "progress", accent);
    expect_color(state, "nestedProgress", accent);
    expect_color(state, "alert", accent);
    expect_color(state, "fixedProgress", red);
    expect_color(state, "fixedAlert", red);
    expect_color(state, "dangerAlert", lvui::figma_defaults::accent_red_default);
    expect_focus(state, "color", accent);
    expect_color(state, "color", swatch);
    expect_focus(state, "fixedColor", red);
    expect_color(state, "fixedColor", swatch);
    expect_color(state, "tool", accent);
    expect_color(state, "fixedTool", red);
    expect_color(state, "dangerTool", lvui::figma_defaults::accent_red_default);
    expect_focus(state, "bareTool", accent);
    expect_focus(state, "fixedTool", red);
    expect_focus(state, "fixedBareTool", red);
}
}

int main() {
    using View = lvui_generated::NativeWindowAccentExtras;
    using Static = lvui_generated::NativeWindowAccentExtrasStatic;
    using namespace ii::gui;
    WindowGeometryValue geometry{"Extras", {640, 420}, {320, 220}};
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
    good(state->commit_window_properties());
    state->commit_publication();
    auto graph = scope->native_lifetime.lock(); require(bool(graph), "Missing binding graph");
    expect_palette(state, lvui::figma_defaults::accent_blue_default);
    auto progress = find(state, "progress"), alert = find(state, "alert"), color = find(state, "color"), tool = find(state, "tool");
    const auto progress_id = take(progress.snapshot()).id;
    good(alert.activate()); good(color.activate()); good(tool.activate());
    require(scope->backend_0.alerts == 1 && scope->backend_0.colors == 1 && scope->backend_0.tools == 1,
            "Initial callbacks failed: " + std::to_string(scope->backend_0.alerts) + ", " + std::to_string(scope->backend_0.colors));
    scope->backend_0.progress.set(75);
    expect_focus(state, "sourceColor", lvui::figma_defaults::accent_blue_default);
    constexpr lvui::StyleColor yellow{1, 1, 0, 1}, cyan{0, 1, 1, 1};
    expect_color(state, "sourceColor", yellow);
    scope->backend_0.color_owner->set(cyan); scope->backend_0.color_revision.set(2);
    scope->backend_0.accent.set("purple");
    require(take(graph->failures()).empty(), "Accent update failed");
    expect_palette(state, lvui::figma_defaults::accent_purple_default);
    expect_focus(state, "sourceColor", lvui::figma_defaults::accent_purple_default);
    expect_color(state, "sourceColor", cyan);
    require(take(progress.snapshot()).id == progress_id && near(take(progress.snapshot()).value, .75),
            "Accent update changed progress or replaced the widget");
    require(scope->backend_0.alerts == 1 && scope->backend_0.colors == 1 && scope->backend_0.tools == 1,
            "Accent update emitted an interaction");
    const auto alert_bounds = take(alert.snapshot()).bounds;
    const auto x = alert_bounds.x + alert_bounds.width / 2, y = alert_bounds.y + alert_bounds.height / 2;
    good(state->input({.kind = WidgetInputKind::pointer_move, .x = x, .y = y}));
    expect_color(state, "alert", lvui::figma_defaults::accent_purple_hover);
    good(state->input({.kind = WidgetInputKind::pointer_down, .x = x, .y = y}));
    expect_color(state, "alert", lvui::figma_defaults::accent_purple_pressed);
    good(state->input({.kind = WidgetInputKind::pointer_up, .x = x, .y = y}));
    good(state->input({.kind = WidgetInputKind::pointer_move, .x = 639, .y = 419}));
    require(scope->backend_0.alerts == 2, "Alert interaction was not retained");
    const auto bare_bounds = take(find(state, "bareTool").snapshot()).bounds;
    const auto bx = bare_bounds.x + bare_bounds.width / 2, by = bare_bounds.y + bare_bounds.height / 2;
    good(state->input({.kind = WidgetInputKind::pointer_move, .x = bx, .y = by}));
    good(state->input({.kind = WidgetInputKind::pointer_down, .x = bx, .y = by}));
    expect_color(state, "bareTool", lvui::figma_defaults::accent_purple_subtle);
    good(state->input({.kind = WidgetInputKind::pointer_up, .x = bx, .y = by}));
    const auto fixed_bare_bounds = take(find(state, "fixedBareTool").snapshot()).bounds;
    const auto fx = fixed_bare_bounds.x + fixed_bare_bounds.width / 2, fy = fixed_bare_bounds.y + fixed_bare_bounds.height / 2;
    good(state->input({.kind = WidgetInputKind::pointer_move, .x = fx, .y = fy}));
    good(state->input({.kind = WidgetInputKind::pointer_down, .x = fx, .y = fy}));
    constexpr lvui::StyleColor pressed_green{0, 1, 0, 1};
    expect_color(state, "fixedBareTool", pressed_green);
    good(state->input({.kind = WidgetInputKind::pointer_up, .x = fx, .y = fy}));
    good(state->input({.kind = WidgetInputKind::pointer_move, .x = 639, .y = 419}));

    auto static_state = std::make_shared<WidgetState>([](ii::runtime::Task task) { task(); }, Static::appearance());
    auto static_host = take(static_state->content_host({}, "host", {640, 420}));
    good(static_host.set_content([](WidgetBuilder& builder) { return Static::build(builder); }));
    expect_palette(static_state, lvui::figma_defaults::accent_purple_default);
    auto static_dialog = find(static_state, "dialog"); good(static_dialog.open_at(0, 0));
    auto static_primary = take(static_dialog.popup_content("dialog.action-0"));
    require(has_color(static_state, static_primary, lvui::figma_defaults::accent_purple_default), "Static Alert did not inherit accent");
    good(static_dialog.close_popup());
    auto static_fixed_dialog = find(static_state, "fixedDialog"); good(static_fixed_dialog.open_at(0, 0));
    constexpr lvui::StyleColor red{1, 0, 0, 1};
    require(has_color(static_state, take(static_fixed_dialog.popup_content("fixedDialog.action-0")), red), "Static Alert ignored primaryColor");
    good(static_fixed_dialog.close_popup());

    scope->backend_0.accent.set("unknown");
    require(!take(graph->failures()).empty(), "Invalid window accent was accepted");
    expect_palette(state, lvui::figma_defaults::accent_purple_default);
    scope->backend_0.accent.set("green");
    require(take(graph->failures()).empty(), "Valid accent did not recover");
    expect_palette(state, lvui::figma_defaults::accent_green_default);
    reject = true; good(graph->begin_batch());
    scope->backend_0.width.set(700); scope->backend_0.accent.set("purple");
    require(!graph->end_batch(), "Rejected window change was accepted");
    expect_palette(state, lvui::figma_defaults::accent_green_default);
    reject = false; scope->backend_0.width.set(720);
    require(take(graph->failures()).empty(), "Window did not recover");
    expect_palette(state, lvui::figma_defaults::accent_purple_default);
    good(alert.activate()); good(color.activate()); good(tool.activate());
    require(scope->backend_0.alerts == 3 && scope->backend_0.colors == 2 && scope->backend_0.tools == 2, "Retained callbacks failed");
    // A headless content host can open a popup without attaching an OS window.
    // Its scoped committed accent is changed directly, as with the menu test.
    auto popup_state = std::make_shared<WidgetState>([](ii::runtime::Task task) { task(); });
    auto popup_host = take(popup_state->content_host({}, "host", {640, 420}));
    std::shared_ptr<View::Scope> popup_scope;
    good(popup_host.set_content([&](WidgetBuilder& builder) { return View::build(builder, {}, {}, &popup_scope); }));
    auto popup_graph = popup_scope->native_lifetime.lock(); require(bool(popup_graph), "Missing popup binding graph");
    popup_scope->window_accent.set(lvui::WindowAccent::green);
    auto dialog = find(popup_state, "dialog"); good(dialog.open_at(0, 0));
    auto primary = take(dialog.popup_content("dialog.action-0")); good(primary.focus());
    const auto primary_id = take(primary.snapshot()).id;
    require(has_color(popup_state, primary, lvui::figma_defaults::accent_green_default), "Open Alert did not inherit green");
    popup_scope->window_accent.set(lvui::WindowAccent::purple);
    require(take(popup_graph->failures()).empty() && take(dialog.snapshot()).opened && take(primary.snapshot()).focused &&
            take(primary.snapshot()).id == primary_id && popup_scope->backend_0.dialog_opens == 1 && popup_scope->backend_0.dialog_closes == 0,
            "Accent update disturbed an open Alert");
    require(has_color(popup_state, primary, lvui::figma_defaults::accent_purple_default), "Open Alert did not update its accent");
    good(primary.activate());
    require(take(dialog.snapshot()).opened && popup_scope->backend_0.dialog_actions == 1 && popup_scope->backend_0.dialog_closes == 0,
            "Alert callbacks were not retained after the accent update");
    good(dialog.close_popup());
    require(popup_scope->backend_0.dialog_closes == 1, "Alert close callback was not retained");
    auto fixed_dialog = find(popup_state, "fixedDialog"); good(fixed_dialog.open_at(0, 0));
    require(has_color(popup_state, take(fixed_dialog.popup_content("fixedDialog.action-0")), red), "Alert ignored primaryColor");
    good(fixed_dialog.close_popup());
    popup_state->close(); static_state->close(); state->close();
}
