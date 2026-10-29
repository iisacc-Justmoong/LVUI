#pragma once

#include "Modal.h"
#include "AlertButton.h"
#include "NativeButton.h"
#include "NativeIconCache.h"
#include "NativeImage.h"

#include <iiGUI/NativeWidgets.h>

#include <array>
#include <chrono>
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

[[nodiscard]] inline StyleColor modal_color(ii::gui::WidgetColor value) noexcept {
    return {value.red, value.green, value.blue, value.alpha};
}

struct ModalInput final {
    ModalConfiguration configuration;
    std::shared_ptr<const ii::gui::RasterImage> icon;
    std::shared_ptr<const IconSource> icon_source;
    std::int64_t icon_revision = 0;
    friend bool operator==(const ModalInput&, const ModalInput&) = default;
};

struct NativeModalCallbacks final {
    std::array<std::function<void()>, 3> actions;
    std::function<void()> opened, closed, canceled;
};

using ModalContentFactory = std::function<ii::core::Result<void>(ii::gui::WidgetBuilder&, ii::gui::Widget)>;

class NativeModal final {
    struct Prepared final {
        Modal model;
        ImageInput icon;
        std::array<ButtonInput, 3> actions;
    };
    using Snapshot = std::shared_ptr<const Prepared>;
    struct State final {
        ModalInput authored;
        NativeIconCache icons;
        Snapshot prepared;
        ii::gui::Widget root, chrome, header, text_column, title, description, anchor, action_row;
        std::optional<NativeImage> icon;
        std::array<std::optional<NativeButton>, 3> actions;
        NativeModalCallbacks callbacks;
        const std::thread::id owner = std::this_thread::get_id();
        std::uint64_t revision = 0;
        bool live = false;
    };

    explicit NativeModal(std::shared_ptr<State> state) : state_(std::move(state)) {}

    [[nodiscard]] static ii::gui::WidgetColor host_color(StyleColor value) noexcept {
        return {value.red, value.green, value.blue, value.alpha};
    }

    [[nodiscard]] static ii::gui::PopoverOptions host_options(const ModalConfiguration& config) {
        ii::gui::PopoverOptions result;
        result.width = config.preferred_width;
        result.padding = config.content_padding;
        result.margins = config.side_padding;
        result.corner_radius = config.corner_radius;
        result.surface_opacity = 1;
        result.surface_color = host_color(config.palette.frame);
        result.modal = true;
        result.dim = true;
        result.focus_on_open = true;
        result.dismiss_on_escape = config.dismiss_on_escape;
        result.dismiss_on_outside_press = config.dismiss_on_background;
        result.accessibility_name = config.accessibility_name;
        result.placement = ii::gui::PopoverPlacement::centered;
        result.minimum_width = config.minimum_width;
        result.maximum_width = config.maximum_width;
        result.minimum_height = config.minimum_height;
        result.vertical_offset = config.vertical_offset;
        result.cylinder = config.shape == ModalShape::cylinder;
        result.backdrop_color = host_color(config.palette.backdrop);
        result.opaque_base = false;
        ii::gui::PopoverTransition transition;
        transition.enter_duration = std::chrono::duration_cast<ii::runtime::Clock::duration>(
            std::chrono::duration<double, std::milli>(config.enter_duration_ms));
        transition.exit_duration = std::chrono::duration_cast<ii::runtime::Clock::duration>(
            std::chrono::duration<double, std::milli>(config.exit_duration_ms));
        transition.enter_easing = ii::animation::Easing::out_back;
        transition.exit_easing = ii::animation::Easing::in_cubic;
        transition.overshoot = config.overshoot;
        transition.collapsed_scale = config.initial_scale;
        transition.enabled = config.motion_enabled;
        result.transition = std::move(transition);
        return result;
    }

    [[nodiscard]] static ii::core::Result<Snapshot> prepare(const ModalInput& input, NativeIconCache& icons) {
        if (!Modal::validate(input.configuration) || input.icon_revision < 0 ||
            (input.icon && !input.configuration.icon_name.empty()) ||
            (input.icon_source && input.configuration.icon_name.empty())) {
            return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid LVUI Modal configuration");
        }
        try {
            auto prepared = std::make_shared<Prepared>();
            if (!prepared->model.set_configuration(input.configuration)) {
                return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid LVUI Modal configuration");
            }
            const auto& config = prepared->model.configuration();
            prepared->icon.image = input.icon;
            if (config.show_icon && !config.icon_name.empty()) {
                auto resolved = icons.resolve(config.icon_name, input.icon_source, config.icon_size);
                if (!resolved) return std::unexpected(resolved.error());
                prepared->icon.image = std::move(*resolved);
            }
            auto& icon = prepared->icon.configuration;
            icon.width = icon.height = config.icon_size;
            icon.padding = std::min(8., config.icon_size / 2);
            icon.corner_radius = config.icon_corner_radius;
            icon.border_width = 1;
            icon.background = config.palette.icon_background;
            icon.border = config.palette.icon_border;
            icon.fit = ImageFit::contain;
            icon.allow_empty = true;
            for (unsigned index = 0; index < 3; ++index) {
                const auto action = prepared->model.action(index);
                AlertButtonConfiguration button;
                button.text = std::string(action.text);
                button.enabled = action.enabled;
                button.tone = index == 0 ? ButtonTone::primary : ButtonTone::normal;
                button.height = 20;
                auto appearance = AlertButton::resolve(button);
                if (!appearance) {
                    return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid LVUI Modal action");
                }
                prepared->actions[index].configuration = std::move(appearance->button);
            }
            return Snapshot(std::move(prepared));
        } catch (...) {
            return ii::core::fail(ii::core::ErrorCode::failed, "Modal preparation failed", std::current_exception());
        }
    }

    [[nodiscard]] static ii::gui::LabelOptions title_options(const ModalConfiguration& config) {
        return {.text = config.title, .font_size = 22, .selectable = false,
                .role = ii::gui::TextRole::title2, .color = ii::gui::WidgetColor{1, 1, 1, .9},
                .line_height = 22, .font_weight = 700};
    }

    [[nodiscard]] static ii::gui::LabelOptions description_options(const Modal& modal) {
        return {.text = std::string(modal.description()), .font_size = 13, .selectable = false,
                .role = ii::gui::TextRole::body, .color = ii::gui::WidgetColor{1, 1, 1, .8},
                .line_height = 18, .font_weight = 500};
    }

    [[nodiscard]] static ii::core::Result<bool> apply_host(
            const std::shared_ptr<State>& state, Snapshot prepared, std::uint64_t revision) {
        const auto& config = prepared->model.configuration();
        const bool text_visible = !config.title.empty() || !prepared->model.description().empty();
        const auto step = [&](auto&& operation) -> ii::core::Result<bool> {
            if (state->revision != revision) return false;
            auto result = operation();
            if (!result) return std::unexpected(result.error());
            return state->revision == revision;
        };
        const auto proceed = [&](auto&& operation) -> ii::core::Result<bool> { return step(operation); };
        auto result = proceed([&] { return state->root.set_popover_options(host_options(config)); });
        if (!result || !*result) return result;
        result = proceed([&] { return state->chrome.set_stack_options({.spacing = config.section_spacing, .alignment = ii::gui::StackAlignment::stretch}); });
        if (!result || !*result) return result;
        result = proceed([&] { return state->header.set_stack_options({.spacing = config.section_spacing, .alignment = ii::gui::StackAlignment::leading}); });
        if (!result || !*result) return result;
        result = proceed([&] { return state->action_row.set_stack_options({.spacing = config.action_spacing, .alignment = ii::gui::StackAlignment::center}); });
        if (!result || !*result) return result;
        result = proceed([&] { return state->title.set_label_options(title_options(config)); });
        if (!result || !*result) return result;
        result = proceed([&] { return state->description.set_label_options(description_options(prepared->model)); });
        if (!result || !*result) return result;
        result = proceed([&] { return state->icon->set_options(prepared->icon); });
        if (!result || !*result) return result;
        result = proceed([&] { return state->icon->root().set_visible(config.show_icon); });
        if (!result || !*result) return result;
        result = proceed([&] { return state->title.set_visible(!config.title.empty()); });
        if (!result || !*result) return result;
        result = proceed([&] { return state->description.set_visible(!prepared->model.description().empty()); });
        if (!result || !*result) return result;
        result = proceed([&] { return state->text_column.set_visible(text_visible); });
        if (!result || !*result) return result;
        result = proceed([&] { return state->header.set_visible(config.show_icon || text_visible); });
        if (!result || !*result) return result;
        for (unsigned index = 0; index < 3; ++index) {
            result = proceed([&] { return state->actions[index]->set_options(prepared->actions[index]); });
            if (!result || !*result) return result;
            result = proceed([&] { return state->actions[index]->root().set_visible(prepared->model.action(index).visible); });
            if (!result || !*result) return result;
        }
        return proceed([&] { return state->action_row.set_visible(prepared->model.action_count() > 0); });
    }

    [[nodiscard]] static ii::gui::WidgetFactory wrap_content(const std::weak_ptr<State>& weak,
                                                              ModalContentFactory content) {
        if (!content) return {};
        return [weak, content = std::move(content)](ii::gui::WidgetBuilder& builder) -> ii::core::Result<void> {
            auto state = weak.lock();
            if (!state || !state->live) return ii::core::fail(ii::core::ErrorCode::closed, "Modal owner retired");
            auto root = builder.vstack({}, "@lvui-modal-content", {.spacing = 0, .alignment = ii::gui::StackAlignment::stretch});
            if (!root) return std::unexpected(root.error());
            return content(builder, *root);
        };
    }

public:
    [[nodiscard]] static ii::core::Result<NativeModal> create(
            ii::gui::WidgetBuilder& builder, ii::gui::Widget parent, std::string_view id,
            ModalInput input = {}, NativeModalCallbacks callbacks = {}, ModalContentFactory content = {}) {
        try {
            auto state = std::make_shared<State>();
            auto prepared = prepare(input, state->icons);
            if (!prepared) return std::unexpected(prepared.error());
            state->authored = std::move(input);
            state->prepared = std::move(*prepared);
            state->callbacks = std::move(callbacks);
            std::weak_ptr<State> weak = state;
            const auto event = [weak](auto member, bool open) {
                return [weak, member, open] {
                    if (auto state = weak.lock(); state && state->live) {
                        auto current = state->root.snapshot();
                        if (!current || current->opened != open) return;
                        auto callback = state->callbacks.*member;
                        if (callback) callback();
                    }
                };
            };
            auto root = builder.popover(parent, id, host_options(state->prepared->model.configuration()),
                {.opened = event(&NativeModalCallbacks::opened, true),
                 .closed = event(&NativeModalCallbacks::closed, false),
                 .canceled = event(&NativeModalCallbacks::canceled, false)});
            if (!root) return std::unexpected(root.error());
            state->root = *root;
            const auto name = [&](std::string_view suffix) { return std::string(id) + "." + std::string(suffix); };
            auto chrome = builder.vstack(*root, name("chrome"), {.spacing = 20, .alignment = ii::gui::StackAlignment::stretch});
            if (!chrome) return std::unexpected(chrome.error()); state->chrome = *chrome;
            auto header = builder.hstack(*chrome, name("header"), {.spacing = 20, .alignment = ii::gui::StackAlignment::leading});
            if (!header) return std::unexpected(header.error()); state->header = *header;
            auto icon = NativeImage::create(builder, *header, name("icon"), state->prepared->icon);
            if (!icon) return std::unexpected(icon.error()); state->icon = *icon;
            auto text = builder.vstack(*header, name("text"), {.spacing = 8, .alignment = ii::gui::StackAlignment::stretch});
            if (!text) return std::unexpected(text.error()); state->text_column = *text;
            if (auto flex = text->set_flex(1); !flex) return std::unexpected(flex.error());
            auto title = builder.label(*text, name("title"), title_options(state->prepared->model.configuration()));
            if (!title) return std::unexpected(title.error()); state->title = *title;
            auto description = builder.label(*text, name("description"), description_options(state->prepared->model));
            if (!description) return std::unexpected(description.error()); state->description = *description;
            auto anchor = builder.vstack(*chrome, name("body"), {.spacing = 0, .alignment = ii::gui::StackAlignment::stretch});
            if (!anchor) return std::unexpected(anchor.error()); state->anchor = *anchor;
            auto actions = builder.hstack(*chrome, name("actions"), {.spacing = 12, .alignment = ii::gui::StackAlignment::center});
            if (!actions) return std::unexpected(actions.error()); state->action_row = *actions;
            auto spacer = builder.spacer(*actions, name("action-spacer"));
            if (!spacer) return std::unexpected(spacer.error());
            if (auto flex = spacer->set_flex(1); !flex) return std::unexpected(flex.error());
            for (int index = 2; index >= 0; --index) {
                auto clicked = [weak, index] {
                    if (auto state = weak.lock(); state && state->live) {
                        auto snapshot = state->root.snapshot();
                        const auto action = state->prepared->model.action(unsigned(index));
                        if (!snapshot || !snapshot->opened || !action.visible || !action.enabled) return;
                        auto callback = state->callbacks.actions[unsigned(index)];
                        if (callback) callback();
                    }
                };
                auto button = NativeButton::create(builder, *actions, name("action-" + std::to_string(index)),
                                                   state->prepared->actions[unsigned(index)], std::move(clicked));
                if (!button) return std::unexpected(button.error());
                state->actions[unsigned(index)] = *button;
            }
            if (auto configured = apply_host(state, state->prepared, state->revision); !configured) {
                return std::unexpected(configured.error());
            }
            if (auto installed = root->set_popup_content_at(*anchor, wrap_content(weak, std::move(content))); !installed) {
                return std::unexpected(installed.error());
            }
            if (auto retained = builder.retain(state); !retained) return std::unexpected(retained.error());
            if (auto published = builder.on_publish([weak] { if (auto state = weak.lock()) state->live = true; }); !published) {
                return std::unexpected(published.error());
            }
            if (auto retired = builder.on_retire([weak] { if (auto state = weak.lock()) state->live = false; }); !retired) {
                return std::unexpected(retired.error());
            }
            return NativeModal(std::move(state));
        } catch (...) {
            return ii::core::fail(ii::core::ErrorCode::failed, "Modal construction failed", std::current_exception());
        }
    }

    [[nodiscard]] ii::gui::Widget root() const { return state_->root; }

    [[nodiscard]] ii::core::Result<ModalInput> options() const {
        if (state_->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Modal requires its owner thread");
        if (auto valid = state_->root.snapshot(); !valid) return std::unexpected(valid.error());
        try { return state_->authored; }
        catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Modal options failed", std::current_exception()); }
    }

    [[nodiscard]] ii::core::Result<void> set_options(ModalInput input) const {
        if (state_->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Modal requires its owner thread");
        try {
            if (auto valid = state_->root.snapshot(); !valid) return std::unexpected(valid.error());
            if (state_->revision == std::numeric_limits<std::uint64_t>::max()) return ii::core::fail(ii::core::ErrorCode::out_of_range, "Modal revision exhausted");
            const auto observed = state_->revision;
            auto next = prepare(input, state_->icons);
            if (!next) return std::unexpected(next.error());
            if (state_->revision != observed) return {};
            auto previous_input = state_->authored;
            auto previous = state_->prepared;
            const auto revision = ++state_->revision;
            state_->authored = std::move(input);
            state_->prepared = std::move(*next);
            auto applied = apply_host(state_, state_->prepared, revision);
            if (!applied) {
                if (state_->revision == revision) {
                    state_->authored = std::move(previous_input);
                    state_->prepared = std::move(previous);
                    (void)apply_host(state_, state_->prepared, revision);
                }
                return std::unexpected(applied.error());
            }
            return {};
        } catch (...) {
            return ii::core::fail(ii::core::ErrorCode::failed, "Modal update failed", std::current_exception());
        }
    }

    [[nodiscard]] ii::core::Result<void> set_content(ModalContentFactory content) const {
        if (state_->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Modal requires its owner thread");
        try { return state_->root.set_popup_content_at(state_->anchor, wrap_content(state_, std::move(content))); }
        catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Modal content update failed", std::current_exception()); }
    }

private:
    std::shared_ptr<State> state_;
};

} // namespace lvui::native
