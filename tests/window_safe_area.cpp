#include "WindowSafeArea.generated.h"
#include "WindowIgnoreSafeArea.generated.h"
#include "WindowCustomTitleBar.generated.h"
#include "WindowApplicationSafeArea.generated.h"
#include <iiGUI/PaintView.h>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
using ii::core::Result;
using namespace ii::gui;

template<class T> T take(Result<T> result) {
    if (!result) throw std::runtime_error(result.error().message);
    return std::move(*result);
}
void good(Result<void> result) {
    if (!result) throw std::runtime_error(result.error().message);
}
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
struct Device final : PaintDevice {
    int frames = 0;
    PaintCapabilities capabilities() const noexcept override { return {}; }
    Result<void> begin_frame(const PaintFrameInfo&) override { ++frames; return {}; }
    Result<void> begin_scene(const PaintSceneInfo&) override { return {}; }
    Result<void> end_scene() override { return {}; }
    Result<void> begin_layer(const PaintLayerInfo&) override { return {}; }
    Result<void> draw_panels(std::span<const Panel>) override { return {}; }
    Result<void> draw_text(std::span<const TextRun>) override { return {}; }
    Result<void> draw_images(std::span<const ImageRun>) override { return {}; }
    Result<void> end_layer() override { return {}; }
    Result<void> present() override { return {}; }
    void abort_frame() noexcept override {}
};

template<class View> void geometry(double inset, bool embedded = false) {
    auto device = std::make_shared<Device>();
    auto view = take(PaintView::create(device, [embedded](WidgetBuilder& widgets) {
        return embedded ? View::build(widgets) : View::build_window_content(widgets);
    }));
    check(device->frames == 0, "Mount requested an unsolicited frame");
    for (const auto size : {Size{640, 480}, Size{800, 500}}) {
        good(view->paint(size, 2));
        auto bounds = take(take(view->find("content")).snapshot()).bounds;
        if (!(bounds.x == inset && bounds.y == 0 && bounds.width == size.width - 2 * inset &&
              bounds.height > 0 && bounds.height <= size.height))
            throw std::runtime_error("Window content geometry: " + std::to_string(bounds.x) + ", " +
                std::to_string(bounds.y) + ", " + std::to_string(bounds.width) + ", " +
                std::to_string(bounds.height) + " expected inset " + std::to_string(inset));
    }
    good(view->paint({20, 100}, 2));
    const auto narrow = take(take(view->find("content")).snapshot()).bounds;
    check(std::isfinite(narrow.x) && std::isfinite(narrow.width) && narrow.width >= 0,
          "A narrow window produced invalid geometry");
    check(device->frames == 3, "Safe-area layout scheduled an additional frame");
    good(view->close());
}
void custom_chrome() {
    auto device = std::make_shared<Device>();
    auto view = take(PaintView::create(device, [](WidgetBuilder& widgets) {
        return lvui_generated::WindowCustomTitleBar::build_window_content(widgets);
    }));
    good(view->paint({640, 480}, 2));
    const auto chrome = take(take(view->find("chrome")).snapshot()).bounds;
    const auto content = take(take(view->find("content")).snapshot()).bounds;
    check(chrome.x == 0 && chrome.width == 640 && content.x == 16 && content.width == 608 &&
          content.y >= chrome.y + chrome.height,
          "Custom title bar was inset or overlapped the window content");
    good(view->close());
}
void application_scene() {
    auto device = std::make_shared<Device>();
    auto view = take(PaintView::create(device, [](WidgetBuilder& widgets) {
        return lvui_generated::WindowApplicationSafeArea::build(widgets);
    }));
    good(view->paint({640, 480}, 2));
    const auto chrome = take(take(view->find("chrome")).snapshot()).bounds;
    const auto content = take(take(view->find("content")).snapshot()).bounds;
    check(chrome.x == 0 && chrome.width == 640 && content.x == 16 && content.width == 608 &&
          content.y >= chrome.y + chrome.height,
          "Application scene lost the custom title-bar and content boundary");
    good(view->close());
}
}
int main() {
    try {
        geometry<lvui_generated::WindowSafeArea>(16);
        geometry<lvui_generated::WindowIgnoreSafeArea>(0);
        geometry<lvui_generated::WindowSafeArea>(0, true);
        custom_chrome();
        application_scene();
        std::cout << "Window content safe-area geometry passed without an application window\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
