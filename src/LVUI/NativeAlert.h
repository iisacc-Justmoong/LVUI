#pragma once

#include "Alert.h"
#include "NativeAlertButton.h"
#include "NativeIconCache.h"
#include "NativeImage.h"

#include <iiGUI/NativeWidgets.h>
#include <iiGUI/Text.h>

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

struct AlertInput final {
    AlertConfiguration configuration;
    std::shared_ptr<const ii::gui::RasterImage> icon;
    std::string icon_name;
    std::shared_ptr<const IconSource> icon_source;
    std::int64_t icon_revision = 0;
    bool use_default_icon = true;
    friend bool operator==(const AlertInput&, const AlertInput&) = default;
};

struct NativeAlertCallbacks final {
    std::array<std::function<void(const AlertButtonEvent&)>, 3> methods;
    std::array<std::function<void()>, 3> clicked;
    std::function<void()> opened, closed, canceled;
};

class NativeAlert final {
    struct Prepared final {
        Alert model;
        ImageInput icon;
        std::array<AlertButtonConfiguration, 3> actions;
    };
    using Snapshot = std::shared_ptr<const Prepared>;
    struct State final {
        AlertInput authored;
        Snapshot prepared;
        NativeIconCache icons;
        ii::gui::Widget root, content, title, description;
        std::optional<NativeImage> icon;
        std::array<std::optional<NativeAlertButton>, 3> actions;
        NativeAlertCallbacks callbacks;
        std::string font_family;
        std::optional<ii::core::Error> layout_failure;
        std::optional<double> allocated_width;
        const std::thread::id owner = std::this_thread::get_id();
        std::uint64_t revision = 0, opening = 0;
        bool live = false;
    };

    explicit NativeAlert(std::shared_ptr<State> state) : state_(std::move(state)) {}

    [[nodiscard]] static ii::gui::WidgetColor host_color(StyleColor color) noexcept {
        return {color.red, color.green, color.blue, color.alpha};
    }

    [[nodiscard]] static ii::gui::PopoverOptions host_options(const AlertConfiguration& c) {
        ii::gui::PopoverOptions value;
        value.width = std::min(c.maximum_width, std::max(c.minimum_width, c.preferred_width));
        value.padding = 0;
        value.margins = c.side_padding;
        value.corner_radius = c.corner_radius;
        value.corner_limit = .4;
        value.surface_opacity = 1;
        value.surface_color = host_color(c.palette.frame);
        value.modal = true; value.dim = true; value.focus_on_open = true;
        value.dismiss_on_escape = c.dismiss_on_escape;
        value.dismiss_on_outside_press = c.dismiss_on_background;
        value.accessibility_name = c.accessibility_name;
        value.placement = ii::gui::PopoverPlacement::centered;
        value.minimum_width = c.minimum_width; value.maximum_width = c.maximum_width;
        value.fit_to_viewport = true; value.opaque_base = false;
        value.cylinder = c.shape == AlertShape::cylinder;
        value.backdrop_color = host_color(c.palette.backdrop);
        value.border_width = c.border_width; value.border_color = host_color(c.palette.border);
        if (c.glass_enabled) value.blur_radius = c.blur_radius;
        if (c.shadow_enabled) value.shadow = ii::gui::PopoverShadow{};
        ii::gui::PopoverTransition transition;
        transition.enter_duration = std::chrono::duration_cast<ii::runtime::Clock::duration>(
            std::chrono::duration<double, std::milli>(c.enter_duration_ms));
        transition.exit_duration = std::chrono::duration_cast<ii::runtime::Clock::duration>(
            std::chrono::duration<double, std::milli>(c.exit_duration_ms));
        transition.enter_easing = ii::animation::Easing::out_back;
        transition.exit_easing = ii::animation::Easing::in_cubic;
        transition.overshoot = c.overshoot; transition.collapsed_scale = c.initial_scale;
        transition.enabled = c.motion_enabled; value.transition = std::move(transition);
        return value;
    }

    [[nodiscard]] static ii::core::Result<Snapshot> prepare(const AlertInput& input, NativeIconCache& icons) {
        if (!Alert::validate(input.configuration) || !valid_accessibility_text(input.icon_name) ||
            input.icon_revision < 0 || (input.icon && !input.icon_name.empty()) ||
            (input.icon_source && input.icon_name.empty())) {
            return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid LVUI Alert configuration");
        }
        try {
            auto prepared = std::make_shared<Prepared>();
            if (!prepared->model.set_configuration(input.configuration)) {
                return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid LVUI Alert configuration");
            }
            const auto& c = prepared->model.configuration();
            const auto count = prepared->model.action_count();
            prepared->icon.image = input.icon;
            if (c.show_icon && !prepared->icon.image && (!input.icon_name.empty() || input.use_default_icon)) {
                const auto name = input.icon_name.empty() ? (count == 3 ? "alertFileText" : "alertAdjustments") : input.icon_name;
                auto image = icons.resolve(name, input.icon_source, c.icon_extent);
                if (!image) return std::unexpected(image.error());
                prepared->icon.image = std::move(*image);
            }
            auto& image = prepared->icon.configuration;
            image.width = image.height = c.icon_extent;
            image.padding = c.icon_extent * (count == 3 ? 15. / 86 : 11. / 86);
            image.corner_radius = c.icon_extent * 28. / 86;
            image.border_width = 1;
            image.background = StyleColor{.1411764771, .1411764771, .1411764771, 1};
            image.border = StyleColor{208. / 255, 215. / 255, 226. / 255, .18};
            image.fit = ImageFit::contain; image.allow_empty = true;
            for (unsigned index = 0; index < 3; ++index) {
                const auto action = prepared->model.action(index);
                auto& button = prepared->actions[index];
                button.text = std::string(action.text); button.enabled = action.enabled;
                button.tone = action.tone; button.dialog_style = true;
                button.primary_color = c.primary_color;
                button.height = index == 2 && count == 3 ? 44 : 56;
                if (action.destructive) button.text_color = button.palette.danger;
                if (!AlertButton::resolve(button)) {
                    return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid LVUI Alert action");
                }
            }
            return Snapshot(std::move(prepared));
        } catch (...) {
            return ii::core::fail(ii::core::ErrorCode::failed, "Alert preparation failed", std::current_exception());
        }
    }

    [[nodiscard]] static ii::gui::LabelOptions title_options(const AlertConfiguration& c) {
        return {.text = c.title, .font_size = 26, .selectable = false, .role = ii::gui::TextRole::title,
                .color = host_color(c.palette.title), .alignment = ii::gui::TextAlignment::center,
                .line_height = 26, .font_weight = 700};
    }
    [[nodiscard]] static ii::gui::LabelOptions description_options(const Alert& model) {
        return {.text = std::string(model.description()), .font_size = 13, .selectable = false,
                .role = ii::gui::TextRole::body, .color = host_color(model.configuration().palette.description),
                .alignment = ii::gui::TextAlignment::center, .line_height = 13, .font_weight = 500};
    }

    [[nodiscard]] static ii::core::Result<double> measure(std::string_view text, double width, double size,
                                                           unsigned weight, std::string_view family) {
        if (text.empty()) return 0.;
        auto metrics = ii::gui::measure_wrapped_text_layout({.bounds = {0, 0, float(std::max(1., width)), 8192},
            .text = std::string(text), .font_size = float(size), .font_weight = weight,
            .alignment = ii::gui::TextAlignment::center, .font_family = std::string(family), .line_height = float(size)});
        if (!metrics) return std::unexpected(metrics.error());
        return metrics->height;
    }

    [[nodiscard]] static ii::core::Result<void> layout(const std::shared_ptr<State>& state, double width) {
        const auto& prepared = *state->prepared;
        const auto& c = prepared.model.configuration();
        const auto text_width = std::max(0., width - 2 * c.text_padding);
        auto title = measure(c.title, text_width, 26, 700, state->font_family);
        if (!title) return std::unexpected(title.error());
        auto description = measure(prepared.model.description(), text_width, 13, 500, state->font_family);
        if (!description) return std::unexpected(description.error());
        auto result = prepared.model.layout_for_width(width, *title, *description);
        if (!result) return ii::core::fail(ii::core::ErrorCode::out_of_range, "Alert content exceeds its layout limits");
        const auto rect = [](AlertRect value) {
            return ii::gui::Rect{float(value.x), float(value.y), float(value.width), float(value.height)};
        };
        const auto desired = std::min(c.maximum_width, std::max(c.minimum_width, c.preferred_width));
        if (auto r = state->content.set_extent({desired, result->frame.height}); !r) return r;
        if (auto r = state->icon->root().set_frame(rect(result->icon)); !r) return r;
        if (auto r = state->title.set_frame(rect(result->title)); !r) return r;
        if (auto r = state->description.set_frame(rect(result->description)); !r) return r;
        for (unsigned index = 0; index < 3; ++index) {
            if (auto r = state->actions[index]->root().set_frame(rect(result->actions[index])); !r) return r;
        }
        state->allocated_width = width;
        state->layout_failure.reset();
        return {};
    }

    [[nodiscard]] static ii::core::Result<bool> apply_host(const std::shared_ptr<State>& state,
                                                            Snapshot prepared, std::uint64_t revision) {
        const auto& model = prepared->model;
        const auto& c = model.configuration();
        const auto step = [&](auto&& operation) -> ii::core::Result<bool> {
            if (state->revision != revision) return false;
            auto result = operation();
            if (!result) return std::unexpected(result.error());
            return state->revision == revision;
        };
        auto result = step([&] { return state->root.set_popover_options(host_options(c)); });
        if (!result || !*result) return result;
        result = step([&] { return state->title.set_label_options(title_options(c)); });
        if (!result || !*result) return result;
        result = step([&] { return state->description.set_label_options(description_options(model)); });
        if (!result || !*result) return result;
        result = step([&] { return state->title.set_visible(!c.title.empty()); });
        if (!result || !*result) return result;
        result = step([&] { return state->description.set_visible(!model.description().empty()); });
        if (!result || !*result) return result;
        result = step([&] { return state->icon->set_options(prepared->icon); });
        if (!result || !*result) return result;
        result = step([&] { return state->icon->root().set_visible(c.show_icon); });
        if (!result || !*result) return result;
        for (unsigned index = 0; index < 3; ++index) {
            result = step([&] { return state->actions[index]->set_options(prepared->actions[index]); });
            if (!result || !*result) return result;
            result = step([&] { return state->actions[index]->root().set_visible(model.action(index).visible); });
            if (!result || !*result) return result;
        }
        const auto width = state->allocated_width.value_or(std::min(c.maximum_width, std::max(c.minimum_width, c.preferred_width)));
        return step([&] { return layout(state, width); });
    }

public:
    [[nodiscard]] static ii::core::Result<NativeAlert> create(
            ii::gui::WidgetBuilder& builder, ii::gui::Widget parent, std::string_view id,
            AlertInput input = {}, NativeAlertCallbacks callbacks = {}) {
        try {
            auto state = std::make_shared<State>();
            auto prepared = prepare(input, state->icons);
            if (!prepared) return std::unexpected(prepared.error());
            auto appearance = builder.appearance();
            if (!appearance) return std::unexpected(appearance.error());
            state->font_family = appearance->styled ? appearance->font_family : std::string{};
            state->authored = std::move(input); state->prepared = std::move(*prepared);
            state->callbacks = std::move(callbacks);
            std::weak_ptr<State> weak = state;
            const auto event = [weak](auto member, bool opened) {
                return [weak, member, opened] {
                    if (auto state = weak.lock(); state && state->live) {
                        auto current = state->root.snapshot();
                        if (!current || current->opened != opened) return;
                        if (member != &NativeAlertCallbacks::canceled) ++state->opening;
                        auto callback = state->callbacks.*member;
                        if (callback) callback();
                    }
                };
            };
            auto root = builder.popover(parent, id, host_options(state->prepared->model.configuration()),
                {.opened = event(&NativeAlertCallbacks::opened, true), .closed = event(&NativeAlertCallbacks::closed, false),
                 .canceled = event(&NativeAlertCallbacks::canceled, false)});
            if (!root) return std::unexpected(root.error()); state->root = *root;
            const auto name = [&](std::string_view suffix) { return std::string(id) + "." + std::string(suffix); };
            auto content = builder.absolute_stack(*root, name("content"), {500, 300});
            if (!content) return std::unexpected(content.error()); state->content = *content;
            auto icon = NativeImage::create(builder, *content, name("icon"), state->prepared->icon);
            if (!icon) return std::unexpected(icon.error()); state->icon = *icon;
            auto title = builder.label(*content, name("title"), title_options(state->prepared->model.configuration()));
            if (!title) return std::unexpected(title.error()); state->title = *title;
            auto description = builder.label(*content, name("description"), description_options(state->prepared->model));
            if (!description) return std::unexpected(description.error()); state->description = *description;
            for (unsigned index = 0; index < 3; ++index) {
                NativeAlertButtonCallbacks button_callbacks;
                button_callbacks.method = [weak, index](const AlertButtonEvent& event) {
                    auto state = weak.lock();
                    if (!state || !state->live) return;
                    auto current = state->root.snapshot();
                    if (!current || !current->opened || !state->prepared->model.action(index).enabled) return;
                    const auto opening = state->opening;
                    auto method = state->callbacks.methods[index];
                    if (method) method(event);
                    if (!state->live || state->opening != opening) return;
                    current = state->root.snapshot();
                    if (!current || !current->opened || !state->prepared->model.action(index).enabled) return;
                    auto clicked = state->callbacks.clicked[index];
                    if (clicked) clicked();
                };
                auto button = NativeAlertButton::create(builder, *content, name("action-" + std::to_string(index)),
                                                         state->prepared->actions[index], std::move(button_callbacks));
                if (!button) return std::unexpected(button.error()); state->actions[index] = *button;
            }
            if (auto applied = apply_host(state, state->prepared, state->revision); !applied) return std::unexpected(applied.error());
            if (auto installed = content->set_layout_handler([weak](ii::gui::Rect bounds) {
                if (auto state = weak.lock(); state && state->live && bounds.width > 0) {
                    auto result = layout(state, bounds.width);
                    if (!result) state->layout_failure = result.error();
                }
            }); !installed) return std::unexpected(installed.error());
            if (auto retained = builder.retain(state); !retained) return std::unexpected(retained.error());
            if (auto published = builder.on_publish([weak] { if (auto state = weak.lock()) state->live = true; }); !published) return std::unexpected(published.error());
            if (auto retired = builder.on_retire([weak] { if (auto state = weak.lock()) state->live = false; }); !retired) return std::unexpected(retired.error());
            return NativeAlert(std::move(state));
        } catch (...) {
            return ii::core::fail(ii::core::ErrorCode::failed, "Alert construction failed", std::current_exception());
        }
    }

    [[nodiscard]] ii::gui::Widget root() const { return state_->root; }

    [[nodiscard]] ii::core::Result<AlertInput> options() const {
        if (state_->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Alert requires its owner thread");
        if (auto valid = state_->root.snapshot(); !valid) return std::unexpected(valid.error());
        try { return state_->authored; }
        catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Alert options failed", std::current_exception()); }
    }

    [[nodiscard]] ii::core::Result<void> set_options(AlertInput input) const {
        if (state_->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Alert requires its owner thread");
        try {
            if (auto valid = state_->root.snapshot(); !valid) return std::unexpected(valid.error());
            if (state_->revision == std::numeric_limits<std::uint64_t>::max()) return ii::core::fail(ii::core::ErrorCode::out_of_range, "Alert revision exhausted");
            const auto observed = state_->revision;
            auto next = prepare(input, state_->icons);
            if (!next) return std::unexpected(next.error());
            if (state_->revision != observed) return {};
            auto old_input = state_->authored; auto old = state_->prepared;
            const auto revision = ++state_->revision;
            state_->authored = std::move(input); state_->prepared = std::move(*next);
            auto applied = apply_host(state_, state_->prepared, revision);
            if (!applied) {
                if (state_->revision == revision) {
                    state_->authored = std::move(old_input); state_->prepared = std::move(old);
                    (void)apply_host(state_, state_->prepared, revision);
                }
                return std::unexpected(applied.error());
            }
            return {};
        } catch (...) {
            return ii::core::fail(ii::core::ErrorCode::failed, "Alert update failed", std::current_exception());
        }
    }

    [[nodiscard]] ii::core::Result<std::optional<ii::core::Error>> layout_failure() const {
        if (state_->owner != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Alert requires its owner thread");
        if (auto valid = state_->root.snapshot(); !valid) return std::unexpected(valid.error());
        return state_->layout_failure;
    }

private:
    std::shared_ptr<State> state_;
};

} // namespace lvui::native
