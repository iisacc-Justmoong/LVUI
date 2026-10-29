#include "NativeOwnedImage.generated.h"
#include "NativeOwnedImageAdvanced.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> result) {
    if (!result) throw std::runtime_error(result.error().message);
    return std::move(*result);
}
void good(ii::core::Result<void> result) { if (!result) throw std::runtime_error(result.error().message); }
bool near(double a, double b) { return std::abs(a - b) < 1e-4; }
std::vector<std::shared_ptr<const ii::gui::RasterImage>> images(const ii::gui::CanvasFrame& drawing) {
    std::vector<std::shared_ptr<const ii::gui::RasterImage>> result;
    const auto collect = [&](const auto& items) { for (const auto& item : items) result.push_back(item.image); };
    collect(drawing.images);
    for (const auto& layer : drawing.overlays) collect(layer.images);
    return result;
}
std::vector<ii::gui::ImageRun> runs(const ii::gui::CanvasFrame& drawing) {
    auto result = drawing.images;
    for (const auto& layer : drawing.overlays) result.insert(result.end(), layer.images.begin(), layer.images.end());
    return result;
}

void advanced() {
    using View = lvui_generated::NativeOwnedImageAdvanced;
    auto state = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(state->content_host({}, "advanced", {640, 480}));
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](ii::gui::WidgetBuilder& builder) { return View::build(builder, {}, {}, &scope); }));
    const auto snapshots = take(state->snapshots());
    const auto found = std::ranges::find_if(snapshots, [](const auto& value) { return value.id.ends_with("/art"); });
    require(found != snapshots.end(), "Advanced image did not mount");
    auto widget = take(state->find(found->id));
    auto snapshot = take(widget.snapshot());
    require(snapshot.kind == ii::gui::WidgetKind::absolute_stack && snapshot.accessibility_semantics &&
            snapshot.accessibility_semantics->role == ii::gui::AccessibilityRole::image &&
            snapshot.accessibility_name == "Art preview", "Image host or semantics are not LVUI-owned");
    auto pixels = take(ii::gui::rgba_image(400, 200, std::vector<std::uint8_t>(400 * 200 * 4, 255)));
    scope->backend_0.image.set(pixels);
    auto frame = take(state->drawing_snapshot({640, 480}));
    auto paint = runs(frame);
    require(paint.size() == 1 && paint[0].image == pixels &&
            near(paint[0].bounds.x - snapshot.bounds.x, 60) && near(paint[0].bounds.y - snapshot.bounds.y, 10) &&
            near(paint[0].bounds.width, 80) && near(paint[0].bounds.height, 80) &&
            near(paint[0].source.x, .25) && near(paint[0].source.width, .5) && near(paint[0].radius, 40),
            "LVUI circular cover geometry was not submitted to the host");
    const auto has_linear_gray = [](const auto& panels) {
        return std::ranges::any_of(panels, [](const auto& panel) { return near(panel.top.red, .21404114); });
    };
    bool gray = has_linear_gray(frame.panels);
    for (const auto& layer : frame.overlays) gray = gray || has_linear_gray(layer.panels);
    require(gray, "Authored sRGB background did not become linear host ink");
    scope->backend_0.fit.set(ii::gui::ImageFit::contain);
    paint = runs(take(state->drawing_snapshot({640, 480})));
    require(paint.size() == 1 && near(paint[0].bounds.width, 80) && near(paint[0].bounds.height, 40) &&
            near(paint[0].source.width, 1) && near(paint[0].radius, 0), "Reactive contain geometry failed");
    scope->backend_0.name.set("Updated preview");
    require(take(widget.snapshot()).accessibility_name == "Updated preview", "Accessible name did not update");
    auto graph = scope->native_lifetime.lock();
    require(bool(graph), "Advanced image binding graph is unavailable");
    scope->backend_0.padding.set(60);
    require(!take(graph->failures()).empty() && runs(take(state->drawing_snapshot({640, 480}))) == paint,
            "An invalid padding update changed the last valid drawing");
    scope->backend_0.padding.set(0);
    scope->backend_0.fit.set(ii::gui::ImageFit::cover);
    paint = runs(take(state->drawing_snapshot({640, 480})));
    require(take(graph->failures()).empty() && paint.size() == 1 && near(paint[0].radius, 50) &&
            near(paint[0].border_width, 2) && near(paint[0].border.red, 1), "Image border update failed");
    scope->backend_0.allow_empty.set(false);
    scope->backend_0.image.set({});
    require(!take(graph->failures()).empty() && runs(take(state->drawing_snapshot({640, 480}))).size() == 1,
            "A missing required source changed the last valid drawing");
    scope->backend_0.allow_empty.set(true);
    require(take(graph->failures()).empty() && runs(take(state->drawing_snapshot({640, 480}))).empty(),
            "The image did not recover and clear its source");
    good(host.clear_content());
    scope.reset(); graph.reset();
    require(!widget.snapshot(), "Advanced image survived retirement");
    state->close();
}
}

int main() {
    using View = lvui_generated::NativeOwnedImage;
    auto state = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(state->content_host({}, "host", {640, 480}));
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](ii::gui::WidgetBuilder& builder) { return View::build(builder, {}, {}, &scope); }));
    auto snapshots = take(state->snapshots());
    auto found = std::ranges::find_if(snapshots, [](const auto& value) { return value.id.ends_with("/preview"); });
    require(found != snapshots.end(), "Image did not mount");
    auto widget = take(state->find(found->id));
    const auto before = take(widget.snapshot());
    require(before.kind == ii::gui::WidgetKind::absolute_stack && before.accessibility_semantics &&
            before.accessibility_semantics->role == ii::gui::AccessibilityRole::image &&
            before.accessibility_name == "Preview" && images(take(state->drawing_snapshot({640, 480}))).empty(),
            "The empty image or accessible name is incorrect");
    require(scope->backend_0.reads == 1, "The initial image provider was evaluated more than once");
    auto image = take(ii::gui::rgba_image(1, 1, std::vector<std::uint8_t>{255, 0, 0, 255}));
    scope->backend_0.image.set(image);
    require(scope->backend_0.reads == 2, "The image provider was not observed exactly once per update");
    auto rendered = images(take(state->drawing_snapshot({640, 480})));
    require(rendered.size() == 1 && rendered.front() == image, "The native image was not drawn");
    scope->backend_0.width.set(100);
    require(scope->backend_0.reads == 3, "A geometry update evaluated the image provider more than once");
    auto changed = take(widget.snapshot());
    require(changed.id == before.id && near(changed.bounds.width, 100), "The image update replaced its widget");
    auto graph = scope->native_lifetime.lock();
    require(bool(graph), "Image binding graph is unavailable");
    scope->backend_0.width.set(-1);
    require(!take(graph->failures()).empty() && near(take(widget.snapshot()).bounds.width, 100),
            "An invalid image candidate changed the prior state");
    scope->backend_0.width.set(80);
    require(take(graph->failures()).empty() && near(take(widget.snapshot()).bounds.width, 80),
            "The image failed to recover after an invalid update");
    scope->backend_0.image.set({});
    require(images(take(state->drawing_snapshot({640, 480}))).empty(), "An empty image did not clear");
    std::weak_ptr<View::Scope> weak = scope;
    good(host.clear_content());
    scope.reset();
    graph.reset();
    require(weak.expired() && !widget.snapshot(), "Retired image retained its scope or widget");
    state->close();
    advanced();
}
