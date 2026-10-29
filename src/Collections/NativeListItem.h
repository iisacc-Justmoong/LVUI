#pragma once

#include <LVUI/NativeIconCache.h>
#include <LVUI/NativeListItemDrawing.h>

#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <thread>
#include <utility>

namespace lvui::native {

[[nodiscard]] inline StyleColor list_item_color(ii::gui::WidgetColor value) noexcept {
    return {value.red, value.green, value.blue, value.alpha};
}
[[nodiscard]] inline ListItemVariant list_item_variant(ListItemVariant value) noexcept { return value; }
struct ListItemInput final {
    ListItemConfiguration configuration;
    std::shared_ptr<const ii::gui::RasterImage> icon;
    std::shared_ptr<const IconSource> icon_source;
    std::int64_t icon_revision = 0;
    friend bool operator==(const ListItemInput&, const ListItemInput&) = default;
};
struct NativeListItemCallbacks final { std::function<void()> activated; };

class NativeListItem final {
    struct Prepared final { ListItemInput input; ListItemDrawing drawing; };
    struct State final {
        std::shared_ptr<const Prepared> prepared;
        NativeIconCache icons, chevrons;
        ii::gui::Widget root;
        NativeListItemCallbacks callbacks;
        std::optional<ii::core::Error> failure;
        const std::thread::id owner = std::this_thread::get_id();
        std::uint64_t revision = 0;
        bool live = false;
    };
    explicit NativeListItem(std::shared_ptr<State> state) : state_(std::move(state)) {}
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const Prepared>> prepare(const ListItemInput& input, State& state) {
        if (!ListItem::validate(input.configuration) || input.icon_revision < 0 ||
            (input.icon && !input.configuration.icon_name.empty()) ||
            (!input.configuration.icon_name.empty() && input.configuration.icon_size > 0 && input.configuration.icon_size < 1) ||
            (input.configuration.show_chevron && input.configuration.chevron_size > 0 && input.configuration.chevron_size < 1))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid list item configuration");
        try {
            auto result = std::make_shared<Prepared>(); result->input = input; auto c = input.configuration;
            c.label = native_display_text(c.label); c.description = native_display_text(c.description);
            c.accessibility_name = native_display_text(c.accessibility_name);
            if (c.icon_size > 0) {
                result->drawing.icon = input.icon;
                if (!c.icon_name.empty()) {
                    auto image = state.icons.resolve(c.icon_name, input.icon_source, c.icon_size);
                    if (!image) return std::unexpected(image.error()); result->drawing.icon = std::move(*image);
                }
            }
            c.has_icon = bool(result->drawing.icon);
            if (c.show_chevron && c.chevron_size > 0) {
                auto image = state.chevrons.resolve("generalchevronRight", {}, c.chevron_size);
                if (!image) return std::unexpected(image.error()); result->drawing.chevron = std::move(*image);
            }
            if (!result->drawing.model.set_configuration(std::move(c)))
                return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid list item display configuration");
            return std::shared_ptr<const Prepared>(std::move(result));
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "List item preparation failed", std::current_exception()); }
    }
    [[nodiscard]] static ii::core::Result<void> update(const std::shared_ptr<State>& state, ListItemInput input) {
        if (state->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "List item requires its owner thread");
        try {
            if (auto live = state->root.snapshot(); !live) return std::unexpected(live.error());
            if (state->prepared->input == input) return {};
            if (state->revision == std::numeric_limits<std::uint64_t>::max()) return ii::core::fail(ii::core::ErrorCode::out_of_range, "List item revision exhausted");
            const auto observed = state->revision; auto next = prepare(input, *state); if (!next) return std::unexpected(next.error());
            if (!state->live || state->revision != observed) return {};
            auto previous = state->prepared; const auto revision = ++state->revision; state->prepared = std::move(*next);
            const auto rollback = [&] { if (state->revision == revision) { state->prepared = previous;
                (void)state->root.set_action_options(list_item_action(previous->drawing));
                (void)state->root.set_visible(previous->input.configuration.visible); (void)state->root.request_repaint(); } };
            if (auto changed = state->root.set_action_options(list_item_action(state->prepared->drawing)); !changed) { rollback(); return changed; }
            if (state->revision != revision) return {};
            if (auto shown = state->root.set_visible(state->prepared->input.configuration.visible); !shown) { rollback(); return shown; }
            if (state->revision != revision) return {};
            if (auto painted = state->root.request_repaint(); !painted) { rollback(); return painted; }
            state->failure.reset(); return {};
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "List item update failed", std::current_exception()); }
    }
public:
    [[nodiscard]] static ii::core::Result<NativeListItem> create(ii::gui::WidgetBuilder& builder, ii::gui::Widget parent,
            std::string_view id, ListItemInput input = {}, NativeListItemCallbacks callbacks = {}) {
        try {
            auto state = std::make_shared<State>(); auto prepared = prepare(input, *state); if (!prepared) return std::unexpected(prepared.error());
            state->prepared = std::move(*prepared); state->callbacks = std::move(callbacks); std::weak_ptr<State> weak = state;
            auto root = builder.action_surface(parent, id, list_item_action(state->prepared->drawing), [weak] {
                if (auto state = weak.lock(); state && state->live) try {
                    const auto& c = state->prepared->input.configuration;
                    if (c.enabled && c.visible && c.interactive && c.selectable && state->callbacks.activated) {
                        auto callback = state->callbacks.activated; callback();
                    }
                } catch (...) { state->failure = ii::core::Error{ii::core::ErrorCode::failed, "List item activation failed", std::current_exception()}; }
            });
            if (!root) return std::unexpected(root.error()); state->root = *root;
            if (auto installed = root->set_paint_handler([weak](const auto& view) -> ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> {
                    auto state = weak.lock(); if (!state) return ii::core::fail(ii::core::ErrorCode::closed, "List item retired");
                    return list_item_paint(state->prepared->drawing, view);
                }); !installed) return std::unexpected(installed.error());
            if (auto shown = root->set_visible(state->prepared->input.configuration.visible); !shown) return std::unexpected(shown.error());
            if (auto retained = builder.retain(state); !retained) return std::unexpected(retained.error());
            if (auto published = builder.on_publish([weak] { if (auto state = weak.lock()) state->live = true; }); !published) return std::unexpected(published.error());
            if (auto retired = builder.on_retire([weak] { if (auto state = weak.lock()) state->live = false; }); !retired) return std::unexpected(retired.error());
            return NativeListItem(std::move(state));
        } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "List item construction failed", std::current_exception()); }
    }
    [[nodiscard]] ii::gui::Widget root() const { return state_->root; }
    [[nodiscard]] ii::core::Result<ListItemInput> options() const {
        if (state_->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "List item requires its owner thread");
        if (auto live = state_->root.snapshot(); !live) return std::unexpected(live.error()); return state_->prepared->input;
    }
    [[nodiscard]] ii::core::Result<void> set_options(ListItemInput input) const { return update(state_, std::move(input)); }
    [[nodiscard]] ii::core::Result<std::optional<ii::core::Error>> failure() const {
        if (state_->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "List item requires its owner thread");
        if (auto live = state_->root.snapshot(); !live) return std::unexpected(live.error()); return state_->failure;
    }
private:
    std::shared_ptr<State> state_;
};

} // namespace lvui::native
