#include "InlineTokens.generated.h"
#include "TokenWindow.generated.h"
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
void check(bool value, const char* why) {
    if (!value)
        throw std::runtime_error(why);
}
template <class T> T take(ii::core::Result<T> value) {
    if (!value)
        throw std::runtime_error(value.error().message);
    return std::move(*value);
}
void good(ii::core::Result<void> value) {
    if (!value)
        throw std::runtime_error(value.error().message);
}
void contracts() {
    const auto a = lvui_generated::TokenWindow::appearance();
    const auto b = lvui_generated::InlineTokens::appearance();
    check(a.font_family == "Pretendard" && b.font_family == a.font_family, "Compiled font token missing");
    check(std::abs(a.window.red - .0784313753247261) < 1e-12 && a.window == b.window,
          "Compiled window token changed");
    check(a.button_height == 22 && a.button_padding == 8 && a.button_radius == 8,
          "Compiled control metrics changed");
    check(a.primary.red == .08120492845773697, "Action alias changed");
    check(a.text_alpha[4] == .800000011920929, "Text alpha precision changed");
}
void native(bool vulkan) {
    auto options = lvui_generated::TokenWindow::options();
    options.renderer = vulkan ? ii::gui::Renderer::vulkan : ii::gui::Renderer::native;
    ii::gui::Host host(options);
    lvui_generated::TokenWindow view;
    std::string failure;
    auto verify = [&] {
        try {
            auto mount = view.widgets();
            const auto panel = take(take(mount.find("panel")).snapshot());
            const auto title = take(take(mount.find("title")).snapshot());
            const auto body = take(take(mount.find("body")).snapshot());
            check(title.font_size == 26 && body.font_size == 13 && title.font_family == "Pretendard",
                  "Mounted typography differs from tokens");
            const auto image = take(host.capture_canvas());
            const auto window = take(host.snapshot());
            const auto color = [&](double x, double y) {
                const auto px = static_cast<unsigned>(x * window.backing_scale),
                           py = static_cast<unsigned>(y * window.backing_scale);
                check(px < image.width && py < image.height, "Pixel sample outside native canvas");
                const auto offset = (py * image.width + px) * 4;
                return image.rgba[offset];
            };
            check(std::abs(int(color(4, 4)) - 20) <= 1, "Default window pixel differs from Figma");
            check(std::abs(int(color(panel.bounds.x + 8, panel.bounds.y + 8)) - 20) <= 1,
                  "Stack background color differs from window");
            check(std::abs(int(color(panel.bounds.x + .25, panel.bounds.y + panel.bounds.height / 2)) - 48) <=
                      2,
                  "sRGB border composite differs from Figma");
            const auto info = take(host.graphics_info());
            constexpr auto native_api =
#if defined(__APPLE__)
                ii::gui::GraphicsApi::metal;
#elif defined(_WIN32)
                ii::gui::GraphicsApi::direct3d11;
#else
                ii::gui::GraphicsApi::vulkan;
#endif
            check(info.api == (vulkan ? ii::gui::GraphicsApi::vulkan : native_api),
                  "Requested native renderer not used");
            check(info.hardware && info.srgb_attachment, "Hardware/sRGB attachment unavailable");
            good(view.detach(host));
            lvui_generated::InlineTokens inline_view;
            good(inline_view.attach(host));
            auto slider = take(inline_view.widgets().find("distance"));
            check(take(slider.snapshot()).value == 8, "Metric initial value did not mount");
            good(inline_view.set_distance(14));
            check(take(slider.snapshot()).value == 14, "Metric Observable did not update its native widget");
            good(inline_view.detach(host));
            std::cout << (vulkan ? "Vulkan" : "Native")
                      << ": mounted generated LVUI view, window=20, border=48, "
                         "Pretendard typography and Metric Observable verified\n";
        } catch (const std::exception& error) {
            failure = error.what();
        }
        (void)host.request_close();
    };
    good(host.run({.opened = [&] {
        good(view.attach(host));
        auto admitted = host.executor().after(std::chrono::milliseconds(250), verify);
        check(bool(admitted), "Native verification callback not admitted");
    }}));
    if (!failure.empty())
        throw std::runtime_error(failure);
}
} // namespace
int main(int argc, char** argv) {
    try {
        contracts();
        if (argc > 1)
            native(std::string_view(argv[1]) == "vulkan");
        else
            std::cout << "Compiled LVUI/inline token appearances verified\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
