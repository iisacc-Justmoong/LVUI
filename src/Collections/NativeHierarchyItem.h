#pragma once

#include <LVUI/HierarchyItem.h>
#include <LVUI/NativeIconCache.h>

#include <iiGUI/NativeWidgets.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace lvui::native {

[[nodiscard]] inline StyleColor hierarchy_item_color(ii::gui::WidgetColor value) noexcept {
    return {value.red, value.green, value.blue, value.alpha};
}
struct HierarchyItemInput final {
    HierarchyItemConfiguration configuration;
    std::shared_ptr<const ii::gui::RasterImage> icon;
    std::shared_ptr<const IconSource> icon_source;
    std::int64_t icon_revision = 0;
    friend bool operator==(const HierarchyItemInput&, const HierarchyItemInput&) = default;
};
struct NativeHierarchyItemCallbacks final {
    std::function<void()> activated;
    std::function<void(bool)> expansion_changed;
};

class NativeHierarchyItem final {
    struct Prepared final {
        HierarchyItem model;
        std::shared_ptr<const ii::gui::RasterImage> icon, chevron;
    };
    using Snapshot = std::shared_ptr<const Prepared>;
    struct State final {
        HierarchyItemInput authored;
        Snapshot prepared;
        NativeIconCache icons;
        ii::gui::Widget root, row, chevron;
        std::optional<ii::gui::Widget> count_view;
        std::optional<ii::gui::Widget> count_content;
        NativeHierarchyItemCallbacks callbacks;
        ii::gui::Size extent;
        std::optional<ii::core::Error> failure;
        const std::thread::id owner = std::this_thread::get_id();
        std::uint64_t revision = 0;
        bool live = false;
    };
    explicit NativeHierarchyItem(std::shared_ptr<State> state) : state_(std::move(state)) {}

    [[nodiscard]] static ii::core::Result<Snapshot> prepare(const HierarchyItemInput& input, NativeIconCache& icons) {
        if (!HierarchyItem::validate(input.configuration) || input.icon_revision < 0 ||
            ((input.icon || input.icon_source) && input.configuration.icon_name.empty()) || (input.icon && input.icon_source))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid hierarchy item configuration");
        try {
            auto result = std::make_shared<Prepared>(); (void)result->model.set_configuration(input.configuration);
            const auto& c = result->model.configuration();
            if (!c.icon_name.empty() && c.icon_size > 0) {
                if (input.icon) result->icon = input.icon;
                else {
                    auto icon = icons.resolve(c.icon_name, input.icon_source, c.icon_size);
                    if (!icon) return std::unexpected(icon.error()); result->icon = std::move(*icon);
                }
            }
            if (c.has_children && c.show_chevron && c.chevron_size > 0) {
                auto chevron = icons.resolve(c.expanded ? "generalchevronDown" : "generalchevronRight", {}, c.chevron_size);
                if (!chevron) return std::unexpected(chevron.error()); result->chevron = std::move(*chevron);
            }
            return Snapshot(std::move(result));
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Hierarchy item preparation failed", std::current_exception()); }
    }
    [[nodiscard]] static ii::gui::Rect rect(HierarchyItemRect value) noexcept {
        return {float(value.x), float(value.y), float(value.width), float(value.height)};
    }
    [[nodiscard]] static ii::gui::Color linear(StyleColor value) noexcept {
        const auto convert = [](double c) { return c <= .04045 ? c / 12.92 : std::pow((c + .055) / 1.055, 2.4); };
        return {float(convert(value.red)), float(convert(value.green)), float(convert(value.blue)), float(value.alpha)};
    }
    [[nodiscard]] static ii::gui::ActionOptions row_options(const Snapshot& prepared) {
        const auto& c = prepared->model.configuration();
        return {.accessibility_name = c.accessibility_name.empty() ? c.label : c.accessibility_name,
                .enabled = c.enabled, .focus_ring = false, .width = c.width, .height = c.height,
                .interaction_enabled = c.activatable,
                .accessibility_description = c.count >= 0 ? "Count " + std::to_string(c.count) : std::string{}};
    }
    [[nodiscard]] static ii::gui::ActionOptions chevron_options(const Snapshot& prepared) {
        const auto& c = prepared->model.configuration();
        return {.accessibility_name = std::string(c.expanded ? "Collapse " : "Expand ") + c.label,
                .enabled = c.enabled && c.has_children && c.show_chevron, .focus_ring = false,
                .width = c.chevron_size, .height = c.chevron_size};
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> paint_row(
            const Snapshot& prepared, const ii::gui::WidgetPaintState& state, bool custom_count) {
        try {
            const auto geometry = prepared->model.geometry(state.size.width, state.size.height);
            if (!geometry) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid hierarchy item extent");
            const auto& c = prepared->model.configuration();
            const auto visual = prepared->model.visual({state.hovered, state.pressed, state.focused});
            auto result = std::make_shared<ii::gui::WidgetDrawing>();
            const auto background = linear(visual.background), focus = linear(visual.focus), ink = linear(visual.text);
            const auto radius = float(std::min({c.corner_radius, state.size.width / 2, state.size.height / 2}));
            if (background.alpha > 0) result->panels.push_back({rect(geometry->surface), background, background, radius});
            if (visual.focused) result->panels.push_back({rect(geometry->surface), focus, focus, radius, 1.5f});
            if (geometry->show_icon && prepared->icon && geometry->icon.width > 0)
                result->images.push_back({.bounds = rect(geometry->icon), .image = prepared->icon, .opacity = c.enabled ? 1.f : .35f});
            if (!c.label.empty() && geometry->label.width > 0) result->texts.push_back({.bounds = rect(geometry->label), .text = c.label,
                .color = ink, .font_size = 13, .font_weight = 500, .alignment = ii::gui::TextAlignment::left,
                .font_family = state.font_family, .line_height = 16, .elide_right = true, .clip = rect(geometry->label)});
            if (!custom_count && geometry->show_count && geometry->count.width > 0) result->texts.push_back({.bounds = rect(geometry->count),
                .text = std::to_string(c.count), .color = ink, .font_size = 11, .font_weight = 400,
                .alignment = ii::gui::TextAlignment::right, .font_family = state.font_family,
                .line_height = 13, .elide_right = true, .clip = rect(geometry->count)});
            return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(result));
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Hierarchy item paint failed", std::current_exception()); }
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> paint_chevron(
            const Snapshot& prepared, const ii::gui::WidgetPaintState& state) {
        try {
            auto result = std::make_shared<ii::gui::WidgetDrawing>();
            if (state.enabled && state.focused) {
                const auto focus = linear(prepared->model.configuration().palette.focus);
                result->panels.push_back({{0, 0, float(state.size.width), float(state.size.height)}, focus, focus, 4, 1.5f});
            }
            if (prepared->chevron) result->images.push_back({.bounds = {0, 0, float(state.size.width), float(state.size.height)},
                .image = prepared->chevron, .opacity = state.enabled ? 1.f : .35f});
            return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(result));
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Hierarchy chevron paint failed", std::current_exception()); }
    }
    [[nodiscard]] static ii::core::Result<void> frames(const std::shared_ptr<State>& state) {
        const auto geometry = state->prepared->model.geometry(state->extent.width, state->extent.height);
        if (!geometry) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid hierarchy item extent");
        if (auto r = state->row.set_frame(rect(geometry->surface)); !r) return r;
        if (auto r = state->chevron.set_visible(geometry->show_chevron); !r) return r;
        if (geometry->show_chevron) if (auto r = state->chevron.set_frame(rect(geometry->chevron)); !r) return r;
        if (state->count_view) {
            if (auto r=state->count_view->set_enabled(state->prepared->model.configuration().enabled); !r) return r;
            if (auto r=state->count_view->set_visible(geometry->show_count); !r) return r;
            if (geometry->show_count) {
                if (auto r=state->count_view->set_frame(rect(geometry->count)); !r) return r;
                if (auto r=state->count_content->set_frame({0,0,float(geometry->count.width),float(geometry->count.height)}); !r) return r;
            }
        }
        return {};
    }
    [[nodiscard]] static ii::core::Result<bool> apply(const std::shared_ptr<State>& state, std::uint64_t revision) {
        const auto step = [&](auto&& action) -> ii::core::Result<bool> {
            if (state->revision != revision) return false;
            auto result = action(); if (!result) return std::unexpected(result.error()); return state->revision == revision;
        };
        const auto& c = state->prepared->model.configuration();
        auto result = step([&] { return state->root.set_extent({c.width, c.height}); }); if (!result || !*result) return result;
        result = step([&] { return state->row.set_action_options(row_options(state->prepared)); }); if (!result || !*result) return result;
        result = step([&] { return state->chevron.set_action_options(chevron_options(state->prepared)); }); if (!result || !*result) return result;
        result = step([&] { return frames(state); }); if (!result || !*result) return result;
        result = step([&] { return state->row.request_repaint(); }); if (!result || !*result) return result;
        return step([&] { return state->chevron.request_repaint(); });
    }
    [[nodiscard]] static ii::core::Result<void> update(const std::shared_ptr<State>& state, HierarchyItemInput input) {
        if (state->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Hierarchy item requires its owner thread");
        try {
            if (auto live = state->root.snapshot(); !live) return std::unexpected(live.error());
            if (input == state->authored) return {};
            if (state->revision == std::numeric_limits<std::uint64_t>::max())
                return ii::core::fail(ii::core::ErrorCode::out_of_range, "Hierarchy item revision exhausted");
            const auto observed = state->revision; auto next = prepare(input, state->icons); if (!next) return std::unexpected(next.error());
            if (state->revision != observed) return {};
            auto old_input = state->authored; auto old_prepared = state->prepared; const auto old_extent = state->extent;
            const auto revision = ++state->revision; state->authored = std::move(input); state->prepared = std::move(*next);
            state->extent = {state->authored.configuration.width, state->authored.configuration.height};
            auto applied = apply(state, revision);
            if (!applied) {
                if (state->revision == revision) { state->authored = std::move(old_input); state->prepared = std::move(old_prepared); state->extent = old_extent; (void)apply(state, revision); }
                return std::unexpected(applied.error());
            }
            state->failure.reset(); return {};
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Hierarchy item update failed", std::current_exception()); }
    }
    static void activate(const std::shared_ptr<State>& state) noexcept {
        try { if (state->live && state->authored.configuration.enabled && state->authored.configuration.activatable && state->callbacks.activated) {
            auto callback = state->callbacks.activated; callback();
        }} catch (...) { try { state->failure = ii::core::Error{ii::core::ErrorCode::failed, "Hierarchy item activation failed", std::current_exception()}; } catch (...) {} }
    }
    static void toggle(const std::shared_ptr<State>& state) noexcept {
        try {
            if (!state->live) return;
            auto model = state->prepared->model; const auto expanded = model.toggle_expansion(); if (!expanded) return;
            auto input = state->authored; input.configuration.expanded = *expanded; const auto before = state->revision;
            auto result = update(state, std::move(input)); if (!result) { state->failure = result.error(); return; }
            if (!state->live || state->revision != before + 1) return;
            if (state->callbacks.expansion_changed) { auto callback = state->callbacks.expansion_changed; callback(*expanded); }
            if (state->live && state->revision == before + 1) activate(state);
        } catch (...) { try { state->failure = ii::core::Error{ii::core::ErrorCode::failed, "Hierarchy expansion failed", std::current_exception()}; } catch (...) {} }
    }

public:
    [[nodiscard]] static ii::core::Result<NativeHierarchyItem> create(ii::gui::WidgetBuilder& builder, ii::gui::Widget parent,
            std::string_view id, HierarchyItemInput input, NativeHierarchyItemCallbacks callbacks = {},
            ii::gui::WidgetFactory count_view = {}) {
        try {
            auto state = std::make_shared<State>(); auto prepared = prepare(input, state->icons); if (!prepared) return std::unexpected(prepared.error());
            state->authored = std::move(input); state->prepared = std::move(*prepared); state->callbacks = std::move(callbacks);
            state->extent = {state->authored.configuration.width, state->authored.configuration.height}; std::weak_ptr<State> weak = state;
            const auto name = [&](std::string_view suffix) { return std::string(id) + "." + std::string(suffix); };
            auto root = builder.absolute_stack(parent, id, state->extent); if (!root) return std::unexpected(root.error()); state->root = *root;
            auto row = builder.action_surface(*root, name("row"), row_options(state->prepared), [weak] { if (auto state = weak.lock()) activate(state); });
            if (!row) return std::unexpected(row.error()); state->row = *row;
            if (auto paint = row->set_paint_handler([weak](const auto& view) -> ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> {
                    auto state = weak.lock(); if (!state) return ii::core::fail(ii::core::ErrorCode::closed, "Hierarchy item retired");
                    return paint_row(state->prepared, view, state->count_view.has_value());
                }); !paint) return std::unexpected(paint.error());
            if (auto input_handler = row->set_input_handler([weak](const ii::gui::WidgetInput& event) -> ii::core::Result<bool> {
                    auto state = weak.lock(); if (!state || !state->live) return false;
                    if (event.kind == ii::gui::WidgetInputKind::key_down &&
                        ((event.key == ii::gui::WidgetKey::right && !state->authored.configuration.expanded) ||
                         (event.key == ii::gui::WidgetKey::left && state->authored.configuration.expanded))) {
                        if (state->authored.configuration.enabled && state->authored.configuration.show_chevron &&
                            state->authored.configuration.has_children && state->authored.configuration.chevron_size > 0) {
                            toggle(state); return true;
                        }
                    }
                    return false;
                }); !input_handler) return std::unexpected(input_handler.error());
            auto chevron = builder.action_surface(*root, name("chevron"), chevron_options(state->prepared), [weak] { if (auto state = weak.lock()) toggle(state); });
            if (!chevron) return std::unexpected(chevron.error()); state->chevron = *chevron;
            if (auto paint = chevron->set_paint_handler([weak](const auto& view) -> ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> {
                    auto state = weak.lock(); if (!state) return ii::core::fail(ii::core::ErrorCode::closed, "Hierarchy item retired");
                    return paint_chevron(state->prepared, view);
                }); !paint) return std::unexpected(paint.error());
            if (count_view) {
                auto container=builder.absolute_stack(*root,name("count"),{});
                if(!container)return std::unexpected(container.error());
                state->count_view=*container;
                if(auto clipped=container->set_clip_content(true);!clipped)return std::unexpected(clipped.error());
                auto content=builder.content_host(*container,name("count.content"),{});
                if(!content)return std::unexpected(content.error());
                state->count_content=*content;
                if(auto mounted=content->set_content(std::move(count_view));!mounted)return std::unexpected(mounted.error());
            }
            if (auto layout = root->set_layout_handler([weak](ii::gui::Rect bounds) { if (auto state = weak.lock()) {
                    if (state->extent.width == bounds.width && state->extent.height == bounds.height) return;
                    state->extent = {bounds.width, bounds.height}; auto result = frames(state);
                    if (!result) state->failure = result.error();
                }}); !layout) return std::unexpected(layout.error());
            auto applied = apply(state, state->revision); if (!applied) return std::unexpected(applied.error());
            if (auto retained = builder.retain(state); !retained) return std::unexpected(retained.error());
            if (auto published = builder.on_publish([weak] { if (auto state = weak.lock()) state->live = true; }); !published) return std::unexpected(published.error());
            if (auto retired = builder.on_retire([weak] { if (auto state = weak.lock()) state->live = false; }); !retired) return std::unexpected(retired.error());
            return NativeHierarchyItem(std::move(state));
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Hierarchy item construction failed", std::current_exception()); }
    }
    [[nodiscard]] ii::gui::Widget root() const { return state_->root; }
    [[nodiscard]] ii::gui::Widget row() const { return state_->row; }
    [[nodiscard]] ii::gui::Widget chevron() const { return state_->chevron; }
    [[nodiscard]] ii::core::Result<HierarchyItemInput> options() const {
        if (state_->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Hierarchy item requires its owner thread");
        if (auto live = state_->root.snapshot(); !live) return std::unexpected(live.error());
        try { return state_->authored; }
        catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Hierarchy item options failed", std::current_exception()); }
    }
    [[nodiscard]] ii::core::Result<void> set_options(HierarchyItemInput input) const { return update(state_, std::move(input)); }
    [[nodiscard]] ii::core::Result<std::optional<ii::core::Error>> failure() const {
        if (state_->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Hierarchy item requires its owner thread");
        if (auto live = state_->root.snapshot(); !live) return std::unexpected(live.error()); return state_->failure;
    }
private:
    std::shared_ptr<State> state_;
};

} // namespace lvui::native
