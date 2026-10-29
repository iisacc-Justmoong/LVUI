#pragma once
#include "Image.h"
#include <iiGUI/NativeWidgets.h>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <thread>
#include <utility>

namespace lvui::native {
// This adapter is compiled by an application that supplies iiGUI. The Image
// model itself is independent of iiGUI and owns every visual decision.
struct ImageInput final {
    std::shared_ptr<const ii::gui::RasterImage> image;
    ImageConfiguration configuration;
};
inline std::optional<StyleColor> image_color(std::optional<ii::gui::WidgetColor> color) {
    if (!color) return std::nullopt;
    return StyleColor{color->red, color->green, color->blue, color->alpha};
}
inline ImageFit image_fit(ImageFit value) noexcept { return value; }
inline std::optional<ImageFit> image_fit(std::optional<ImageFit> value) noexcept { return value; }
inline ImageFit image_fit(ii::gui::ImageFit value) noexcept {
    switch (value) {
        case ii::gui::ImageFit::stretch: return ImageFit::stretch;
        case ii::gui::ImageFit::contain: return ImageFit::contain;
        case ii::gui::ImageFit::cover: return ImageFit::cover;
    }
    return static_cast<ImageFit>(-1); // Rejected by the LVUI model.
}
inline std::optional<ImageFit> image_fit(std::optional<ii::gui::ImageFit> value) noexcept {
    return value ? std::optional{image_fit(*value)} : std::nullopt;
}

class NativeImage final {
    struct Prepared final {
        Image view;
        std::shared_ptr<const ii::gui::RasterImage> pixels;
    };
    using Snapshot = std::shared_ptr<const Prepared>;
    struct State final {
        Snapshot current;
        const std::thread::id owner=std::this_thread::get_id();
        std::uint64_t revision=0;
    };
    ii::gui::Widget root_;
    std::shared_ptr<State> state_;

    NativeImage(ii::gui::Widget root, std::shared_ptr<State> state)
        : root_(std::move(root)), state_(std::move(state)) {}

    [[nodiscard]] static ii::core::Result<Snapshot> prepare(const ImageInput& input) {
        auto next = std::make_shared<Prepared>();
        std::optional<ImageSize> size;
        if (input.image) size = ImageSize{double(input.image->width()), double(input.image->height())};
        if (!next->view.set_configuration(input.configuration, size))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid LVUI Image configuration");
        next->pixels = input.image;
        return Snapshot(std::move(next));
    }
    [[nodiscard]] static ii::gui::AccessibilitySemantics semantics(const Snapshot& snapshot) {
        ii::gui::AccessibilitySemantics value{};
        value.role = ii::gui::AccessibilityRole::image;
        value.name = snapshot->view.configuration().accessibility_name;
        return value;
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>
    paint(const Snapshot& snapshot, const ii::gui::WidgetPaintState& state) {
        const auto geometry = snapshot->view.geometry(state.size.width, state.size.height);
        if (!geometry) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid LVUI Image extent");
        const auto rectangle = [](ImageRect value) {
            return ii::gui::Rect{float(value.x), float(value.y), float(value.width), float(value.height)};
        };
        const auto linear = [](StyleColor value) {
            const auto channel = [](double c) { return c <= .04045 ? c / 12.92 : std::pow((c + .055) / 1.055, 2.4); };
            return ii::gui::Color{float(channel(value.red)), float(channel(value.green)),
                                  float(channel(value.blue)), float(value.alpha)};
        };
        auto drawing = std::make_shared<ii::gui::WidgetDrawing>();
        const auto& style = snapshot->view.configuration();
        if (geometry->outer.width > 0 && geometry->outer.height > 0) {
            if (style.background) {
                const auto color = linear(*style.background);
                drawing->panels.push_back({rectangle(geometry->outer), color, color, float(geometry->radius)});
            }
            if (style.border && geometry->border_width > 0) {
                const auto color = linear(*style.border);
                drawing->panels.push_back({rectangle(geometry->outer), color, color, float(geometry->radius),
                                           float(geometry->border_width)});
            }
        }
        if (geometry->draw_image && snapshot->pixels) {
            ii::gui::ImageRun run{};
            run.bounds = rectangle(geometry->image); run.image = snapshot->pixels;
            run.source = rectangle(geometry->source); run.radius = float(geometry->image_radius);
            if (geometry->border_on_image) {
                run.border = linear(*style.border);
                run.border_width = float(geometry->border_width);
            }
            drawing->images.push_back(std::move(run));
        }
        return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
    }
public:
    [[nodiscard]] static ii::core::Result<NativeImage> create(ii::gui::WidgetBuilder& builder,
            ii::gui::Widget parent, std::string_view id, const ImageInput& input) {
        try {
            auto prepared = prepare(input);
            if (!prepared) return std::unexpected(prepared.error());
            auto state = std::make_shared<State>();
            state->current = std::move(*prepared);
            const auto& style = state->current->view.configuration();
            auto root = builder.absolute_stack(parent, id, {style.width, style.height});
            if (!root) return std::unexpected(root.error());
            if (auto accessible = root->set_accessibility_semantics(semantics(state->current)); !accessible)
                return std::unexpected(accessible.error());
            if (auto installed = root->set_paint_handler([state](const ii::gui::WidgetPaintState& paint_state) {
                    return paint(state->current, paint_state);
                }); !installed) return std::unexpected(installed.error());
            return NativeImage(*root, std::move(state));
        } catch (...) {
            return ii::core::fail(ii::core::ErrorCode::failed, "LVUI Image construction failed", std::current_exception());
        }
    }

    [[nodiscard]] ii::gui::Widget root() const { return root_; }
    [[nodiscard]] ii::core::Result<void> set_options(const ImageInput& input) const {
        try {
            if (state_->owner != std::this_thread::get_id())
                return ii::core::fail(ii::core::ErrorCode::wrong_executor, "LVUI Image requires its owner thread");
            auto prepared = prepare(input);
            if (!prepared) return std::unexpected(prepared.error());
            if (state_->revision == std::numeric_limits<std::uint64_t>::max())
                return ii::core::fail(ii::core::ErrorCode::out_of_range, "LVUI Image revision exhausted");
            const auto revision = ++state_->revision;
            if (auto current = root_.snapshot(); !current) return std::unexpected(current.error());
            if (state_->revision != revision) return {};
            const auto& style = (*prepared)->view.configuration();
            if (auto resized = root_.set_extent({style.width, style.height}); !resized) return resized;
            if (state_->revision != revision) return {};
            if (auto accessible = root_.set_accessibility_semantics(semantics(*prepared)); !accessible) return accessible;
            if (state_->revision != revision) return {};
            state_->current = std::move(*prepared);
            return root_.request_repaint();
        } catch (...) {
            return ii::core::fail(ii::core::ErrorCode::failed, "LVUI Image update failed", std::current_exception());
        }
    }
};
} // namespace lvui::native
