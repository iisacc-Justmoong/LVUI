#include <LVUI/ColorSelectionSurface.h>
#include <LVUI/NativeColorSurface.h>
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T>T take(ii::core::Result<T> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
    return std::move(*value);
}
void good(ii::core::Result<void> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
}
struct Source final : lvui::ColorSurfaceSource {
    std::shared_ptr<lvui::ColorSurfaceSource> delegate;
    mutable std::function<void()> on_frame;
    bool malformed = false;
    [[nodiscard]] lvui::ColorSourceResult color() const override { return delegate->color(); }
    [[nodiscard]] std::uint64_t revision() const noexcept override { return delegate->revision(); }
    [[nodiscard]] lvui::ColorSurfaceResult<lvui::ColorSurfaceFrame> frame(
            lvui::ColorSurfaceKind kind, std::uint32_t width, std::uint32_t height) const override {
        auto callback = std::exchange(on_frame, {}); if (callback) callback();
        auto value = delegate->frame(kind, width, height);
        if (value && malformed) value->rgba.pop_back();
        return value;
    }
    [[nodiscard]] lvui::ColorSurfaceResult<std::uint64_t> edit(
            lvui::ColorSurfaceKind kind, lvui::ColorSurfacePoint point, std::uint64_t revision) override {
        return delegate->edit(kind, point, revision);
    }
};
}

int main() {
    using namespace ii::gui;
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {300, 200}));
    auto selection = std::make_shared<lvui::ColorSelection>(lvui::StyleColor{1, 0, 0, .5});
    auto source = std::make_shared<lvui::ColorSelectionSurface>(selection);
    auto proxy = std::make_shared<Source>(); proxy->delegate = source;
    lvui::native::ColorSurfaceInput input;
    input.source = proxy; input.configuration.kind = lvui::ColorSurfaceKind::saturation_brightness;
    input.configuration.width = input.configuration.height = 100;
    input.configuration.accessibility_name = "Color plane";
    std::optional<lvui::native::NativeColorSurface> surface;
    std::vector<WidgetColor> edits;
    bool disable_on_callback = false;
    good(host.set_content([&](auto& builder) -> ii::core::Result<void> {
        auto stack = builder.vstack({}, "stack", {.spacing = 0, .alignment = StackAlignment::leading});
        if (!stack) return std::unexpected(stack.error());
        auto value = lvui::native::NativeColorSurface::create(builder, *stack, "surface", input, [&](WidgetColor color) {
            edits.push_back(color);
            if (disable_on_callback) { auto next = input; next.configuration.enabled = false; good(surface->set_options(next)); }
        });
        if (!value) return std::unexpected(value.error());
        surface = *value;
        return {};
    }));
    auto snapshot = take(surface->root().snapshot());
    require(snapshot.kind == WidgetKind::action_surface && snapshot.accessibility_name == "Color plane" &&
            snapshot.accessibility_description == "#FF000080", "Color surface did not expose its value");
    const auto frame = take(tree->drawing_snapshot({300, 200}));
    bool plane = false, marker = false;
    const auto check = [&](const auto& images) {
        for (const auto& image : images) if (image.image) {
            plane |= image.image->width() == 100 && image.image->height() == 100;
            marker |= image.image->width() == 32 && image.image->height() == 32;
        }
    };
    check(frame.images); for (const auto& layer : frame.overlays) check(layer.images);
    require(plane && marker, "Color surface did not draw its raster and marker");
    const auto bounds = snapshot.bounds;
    good(tree->input({.kind = WidgetInputKind::pointer_down, .x = bounds.x + 50, .y = bounds.y + 50}));
    good(tree->input({.kind = WidgetInputKind::pointer_up, .x = bounds.x + 50, .y = bounds.y + 50}));
    require(edits.size() == 1 && std::abs(edits.back().red - .5) < 1e-8 &&
            std::abs(edits.back().green - .25) < 1e-8 && edits.back().alpha == .5,
            "Color plane did not map its pointer edit");
    good(tree->input({.kind = WidgetInputKind::key_down, .key = WidgetKey::right}));
    require(edits.size() == 2 && edits.back().green < .25, "Color surface keyboard adjustment failed");

    require(bool(selection->set_color({0, 1, 0, 1})), "External color update failed");
    const auto callbacks = edits.size();
    good(tree->input({.kind = WidgetInputKind::pointer_down, .x = bounds.x + 20, .y = bounds.y + 20}));
    good(tree->input({.kind = WidgetInputKind::pointer_up, .x = bounds.x + 20, .y = bounds.y + 20}));
    require(edits.size() == callbacks && take(surface->root().snapshot()).accessibility_description == "#00FF00FF" &&
            selection->snapshot().color == lvui::StyleColor{0, 1, 0, 1},
            "An old gesture overwrote an external color or delivered a callback");
    proxy->malformed = true;
    require(!surface->set_options(input) && take(surface->root().snapshot()).accessibility_description == "#00FF00FF",
            "A malformed source frame replaced the visible surface");
    proxy->malformed = false;

    auto newer = input; newer.source = source; newer.configuration.accessibility_name = "Newest";
    proxy->on_frame = [&] { good(surface->set_options(newer)); };
    auto older = input; older.configuration.accessibility_name = "Older";
    good(surface->set_options(older));
    require(take(surface->options()).configuration.accessibility_name == "Newest",
            "An older source read replaced a reentrant update");

    auto rail = newer; rail.configuration.kind = lvui::ColorSurfaceKind::hue_rail;
    rail.configuration.width = 20;
    good(surface->set_options(rail));
    const auto rail_snapshot = take(surface->root().snapshot());
    require(rail_snapshot.accessibility_semantics && rail_snapshot.accessibility_semantics->role == AccessibilityRole::slider,
            "The color rail did not expose an adjustable accessibility role");
    const auto before_increment = edits.size();
    good(tree->input({.kind = WidgetInputKind::increment, .target = rail_snapshot.id}));
    require(edits.size() == before_increment + 1, "An accessibility increment did not edit the color rail");
    good(surface->set_options(newer));

    good(tree->input({.kind = WidgetInputKind::pointer_down, .x = bounds.x + 30, .y = bounds.y + 40}));
    const auto before_switch = selection->snapshot().color;
    auto switched = newer; switched.configuration.kind = lvui::ColorSurfaceKind::wheel;
    good(surface->set_options(switched));
    good(tree->input({.kind = WidgetInputKind::pointer_move, .x = bounds.x + 80, .y = bounds.y + 70}));
    good(tree->input({.kind = WidgetInputKind::pointer_up, .x = bounds.x + 80, .y = bounds.y + 70}));
    require(selection->snapshot().color == before_switch, "A gesture continued after its surface kind changed");
    good(surface->set_options(newer));
    good(tree->input({.kind = WidgetInputKind::pointer_down, .x = bounds.x + 30, .y = bounds.y + 40}));
    auto other_selection = std::make_shared<lvui::ColorSelection>(lvui::StyleColor{0, 0, 1, 1});
    auto other_source = std::make_shared<lvui::ColorSelectionSurface>(other_selection);
    switched = newer; switched.source = other_source; good(surface->set_options(switched));
    good(tree->input({.kind = WidgetInputKind::pointer_move, .x = bounds.x + 80, .y = bounds.y + 70}));
    good(tree->input({.kind = WidgetInputKind::pointer_up, .x = bounds.x + 80, .y = bounds.y + 70}));
    require(other_selection->snapshot().color == lvui::StyleColor{0, 0, 1, 1},
            "A gesture edited a replacement color source");
    good(surface->set_options(newer));

    disable_on_callback = true;
    good(tree->input({.kind = WidgetInputKind::pointer_down, .x = bounds.x + 80, .y = bounds.y + 30}));
    if (take(surface->root().snapshot()).enabled) {
        const auto color = selection->snapshot().color;
        throw std::runtime_error("A reentrant callback update was lost: edits=" + std::to_string(edits.size()) +
            " color=" + std::to_string(color.red) + "," + std::to_string(color.green) + "," +
            std::to_string(color.blue) + " revision=" + std::to_string(selection->revision()) +
            " options=" + std::to_string(take(surface->options()).configuration.enabled));
    }
    const auto disabled_callbacks = edits.size();
    good(tree->input({.kind = WidgetInputKind::pointer_up, .x = bounds.x + 90, .y = bounds.y + 30}));
    require(edits.size() == disabled_callbacks, "Disabled surface continued a pointer gesture");
    bool rejected = false;
    std::thread worker([&] { auto result = surface->set_options(input); rejected = !result && result.error().code == ii::core::ErrorCode::wrong_executor; });
    worker.join(); require(rejected, "Color surface accepted a foreign-thread update");
    good(host.clear_content()); require(!surface->options(), "Retired color surface accepted access");
    tree->close();
}
