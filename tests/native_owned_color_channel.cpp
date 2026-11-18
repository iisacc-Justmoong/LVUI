#include <LVUI/ColorSelectionSurface.h>
#include <LVUI/NativeColorChannel.h>
#include <iiGUI/NativeColorChannelView.h>
#include <iiGUI/ColorChannelSurface.h>
#include <iiGUI/NativeColorSurfaceView.h>
using ColorChannelTestField=lvui::native::InputFieldBinding<ii::gui::BasicNativeInputField,ii::gui::BasicTextInput,ii::gui::BasicTextFieldMaterial,ii::gui::NativeTextAffinity>;
using ColorChannelTestRamp=lvui::ColorChannelSurfaceBinding<ii::gui::BasicColorChannelSurface>;
using ColorChannelTestSurface=lvui::native::ColorSurfaceBinding<ii::gui::BasicColorSurface,ii::gui::BasicNativeColorSurface>;
using ColorChannelTestView=lvui::native::ColorChannelBinding<ii::gui::BasicNativeColorChannel,ColorChannelTestRamp,ColorChannelTestSurface,ColorChannelTestField>;
#include <iiGUI/WidgetState.h>
#include <iiGUI/SrgbColor.h>

#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
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
bool near(double a, double b) { return std::abs(a - b) < 1e-7; }
std::string text(ii::gui::Widget field) {
    const auto snapshot = take(field.snapshot()); return take(field.text_content({0, snapshot.text_length})).text;
}
const ii::gui::TextRun& run(const ii::gui::CanvasFrame& frame,std::string_view value) {
    for(const auto& item:frame.texts)if(item.text==value)return item;
    for(const auto& layer:frame.overlays)for(const auto& item:layer.texts)if(item.text==value)return item;
    throw std::runtime_error("Color channel label was not drawn");
}
}

int main() {
    using namespace ii::gui;
    auto appearance=default_widget_appearance();
    appearance.text_styles=std::array<TextRoleStyle,8>{};appearance.text_colors=std::array<WidgetColor,8>{};
    (*appearance.text_styles)[static_cast<unsigned>(TextRole::body)]={19,20,700};
    (*appearance.text_colors)[static_cast<unsigned>(TextRole::description)]={.2,.3,.4,.6};
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); },appearance);
    auto host = take(tree->content_host({}, "host", {400, 200}));
    auto selection = std::make_shared<lvui::ColorSelection>(lvui::StyleColor{1, 0, 0, 1});
    auto source = std::make_shared<lvui::ColorSelectionSurface>(selection);
    lvui::native::ColorChannelInput input; input.source = source; input.channel = lvui::ColorChannel::red;
    std::optional<ColorChannelTestView> channel;
    Widget other; std::vector<lvui::StyleColor> edited;
    good(host.set_content([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        auto root = builder.vstack({}, "root", {.spacing = 12, .alignment = StackAlignment::leading});
        if (!root) return std::unexpected(root.error());
        auto value = ColorChannelTestView::create(builder, *root, "red", input,
            [&](lvui::StyleColor color) { edited.push_back(color); });
        if (!value) return std::unexpected(value.error()); channel = *value;
        auto button = builder.action_surface(*root, "other", {.text = "Other", .width = 60, .height = 22});
        if (!button) return std::unexpected(button.error()); other = *button; return {};
    }));
    const auto ramp = channel->surface(), field = channel->field();
    const auto ramp_snapshot = take(ramp.snapshot());
    require(ramp_snapshot.accessibility_semantics && ramp_snapshot.accessibility_semantics->role == AccessibilityRole::slider &&
            text(field) == "255", "Color channel did not expose its initial value");
    const auto themed=take(tree->drawing_snapshot({400, 200}));const auto& label=run(themed,"R");
    require(label.font_size==19 && label.line_height==20 && label.font_weight==700 &&
            label.color==srgb_to_linear(WidgetColor{.2,.3,.4,.6}),"Color channel ignored its Figma body typography or description color");
    const auto bounds = take(ramp.snapshot()).bounds;
    good(tree->input({.kind = WidgetInputKind::pointer_down, .x = bounds.x + bounds.width / 2, .y = bounds.y + bounds.height / 2}));
    good(tree->input({.kind = WidgetInputKind::pointer_up, .x = bounds.x + bounds.width / 2, .y = bounds.y + bounds.height / 2}));
    require(near(selection->snapshot().color.red, .5) && edited.size() == 1 && text(field) == "128",
            "Color channel ramp did not edit and refresh its numeric value");
    const auto replace = [&](std::string value) {
        good(field.focus());
        good(tree->input({.kind = WidgetInputKind::key_down, .key = WidgetKey::select_all}));
        good(tree->input({.kind = WidgetInputKind::text, .text = std::move(value)}));
    };
    replace("64"); good(tree->input({.kind = WidgetInputKind::key_down, .key = WidgetKey::enter}));
    require(near(selection->snapshot().color.red, 64. / 255) && edited.size() == 2,
            "Color channel numeric submission did not commit");
    replace("invalid"); good(tree->input({.kind = WidgetInputKind::key_down, .key = WidgetKey::enter}));
    require(text(field) == "64" && edited.size() == 2, "Invalid numeric input changed the color or remained displayed");
    replace("200"); good(other.focus()); (void)take(tree->drawing_snapshot({400, 200}));
    require(near(selection->snapshot().color.red, 200. / 255) && edited.size() == 3,
            "Color channel did not commit a changed numeric value on focus loss");
    replace("20"); require(bool(selection->set_color({0, 1, 0, 1})), "External color update failed");
    input.source_revision = std::int64_t(selection->revision()); good(channel->set_options(input));
    good(tree->input({.kind = WidgetInputKind::key_down, .key = WidgetKey::enter}));
    require(selection->snapshot().color == lvui::StyleColor{0, 1, 0, 1} && text(field) == "0" && edited.size() == 3,
            "A pending stale numeric edit overwrote an external color");
    auto green = input; green.channel = lvui::ColorChannel::green; good(channel->set_options(green));
    require(text(field) == "255", "Color channel did not rebind its source and value");
    const auto rebound=take(tree->drawing_snapshot({400,200}));
    require(run(rebound,"G").font_size==19 && run(rebound,"G").color==srgb_to_linear(WidgetColor{.2,.3,.4,.6}),
            "Color channel lost its themed label on update");
    bool rejected = false;
    std::thread worker([&] { auto result = channel->set_options(input); rejected = !result && result.error().code == ii::core::ErrorCode::wrong_executor; });
    worker.join(); require(rejected, "Color channel accepted a foreign-thread update");
    good(host.clear_content()); require(!field.snapshot() && !channel->set_options(input), "Retired channel remained writable");
    tree->close();
}
