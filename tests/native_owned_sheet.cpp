#include <LVUI/NativeSheet.h>
#include <iiGUI/WidgetState.h>

#include <memory>
#include <optional>
#include <stdexcept>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T>T take(ii::core::Result<T> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
    return std::move(*value);
}
void good(ii::core::Result<void> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
}
}

int main() {
    using namespace ii::gui;
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {800, 600}));
    std::optional<lvui::native::NativeSheet> sheet;
    lvui::native::SheetInput input;
    input.mobile_platform = false;
    input.configuration.title = "Export";
    input.configuration.description = "Save your file";
    input.configuration.motion_enabled = false;
    int made = 0, opened = 0, closed = 0;
    good(host.set_content([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        auto root = builder.vstack({}, "root"); if (!root) return std::unexpected(root.error());
        auto created = lvui::native::NativeSheet::create(builder, *root, "sheet", input,
            {.opened = [&] { ++opened; }, .closed = [&] { ++closed; }},
            [&](WidgetBuilder& child, Widget parent) -> ii::core::Result<void> {
                ++made;
                for (int index = 0; index < 30; ++index) {
                    auto item = child.label(parent, "row" + std::to_string(index), {.text = "Retained row"});
                    if (!item) return std::unexpected(item.error());
                }
                return {};
            });
        if (!created) return std::unexpected(created.error()); sheet = *created; return {};
    }));
    auto root = sheet->root(), viewport = sheet->viewport();
    (void)take(tree->drawing_snapshot({800, 600})); good(root.open_at(0, 0));
    const auto desktop = take(root.snapshot());
    require(desktop.modal && desktop.bounds.width == 560 && desktop.bounds.height == 420 && made == 1 && opened == 1,
            "Independent desktop Sheet did not open at its preferred geometry");
    auto row = take(root.popup_content("row0")); const auto row_id = take(row.snapshot()).id;
    require(take(viewport.snapshot()).content_height > take(viewport.snapshot()).bounds.height,
            "Sheet did not expose its scrolling content");
    good(viewport.set_scroll_offset(80)); require(take(viewport.snapshot()).scroll_offset > 0, "Sheet did not scroll");
    good(root.close_popup()); good(root.open_at(0, 0));
    require(made == 1 && take(take(root.popup_content("row0")).snapshot()).id == row_id &&
            take(viewport.snapshot()).scroll_offset == 0, "Sheet did not retain its body or reset scrolling");
    auto mobile = input; mobile.configuration.presentation = lvui::SheetPresentation::mobile;
    mobile.configuration.detent = lvui::SheetDetent::medium; mobile.configuration.bottom_safe_inset = 34;
    good(sheet->set_options(mobile));
    auto mobile_frame = take(root.snapshot()).bounds;
    require(mobile_frame.width == 800 && mobile_frame.height == 342 && mobile_frame.y == 258 && made == 1,
            "Sheet did not adopt mobile bottom-aligned geometry while retaining its body");
    auto grabber = take(root.popup_content("sheet.grabber"));
    auto grab = take(grabber.snapshot()).bounds;
    require(take(grabber.snapshot()).visible && grab.height > 0, "Mobile Sheet did not expose its grabber");
    auto x = grab.x + grab.width / 2, y = grab.y + grab.height / 2;
    good(tree->input({.kind = WidgetInputKind::pointer_down, .x = x, .y = y}));
    good(tree->input({.kind = WidgetInputKind::pointer_move, .x = x, .y = y + 20}));
    require(take(root.snapshot()).bounds.y == mobile_frame.y + 20, "Sheet did not follow its captured drag");
    good(tree->input({.kind = WidgetInputKind::pointer_up, .x = x, .y = y + 20}));
    require(take(root.snapshot()).opened && take(root.snapshot()).bounds.y == mobile_frame.y,
            "Short Sheet drag did not restore the surface");
    grab = take(grabber.snapshot()).bounds; x = grab.x + grab.width / 2; y = grab.y + grab.height / 2;
    good(tree->input({.kind = WidgetInputKind::pointer_down, .x = x, .y = y}));
    good(tree->input({.kind = WidgetInputKind::pointer_move, .x = x, .y = y + 100}));
    good(tree->input({.kind = WidgetInputKind::pointer_up, .x = x, .y = y + 100}));
    require(!take(root.snapshot()).opened && closed == 2, "Long Sheet drag did not dismiss exactly once");
    auto invalid = mobile; invalid.configuration.preferred_width = -1;
    require(!sheet->set_options(invalid) && take(sheet->options()).configuration == mobile.configuration,
            "Invalid Sheet options replaced its retained state");
    good(host.clear_content()); require(!row.snapshot() && !sheet->options(), "Sheet retirement retained its body");
    tree->close();
}
