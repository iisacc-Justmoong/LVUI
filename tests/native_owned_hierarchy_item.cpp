#include <LVUI/NativeHierarchyItem.h>
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
    return std::move(*value);
}
void good(ii::core::Result<void> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
}
bool near(double a, double b) { return std::abs(a - b) < 1e-7; }
}

int main() {
    using namespace ii::gui;
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {400, 100}));
    lvui::native::HierarchyItemInput input; auto& config = input.configuration;
    config.key = "camera"; config.label = "Camera"; config.icon_name = "home";
    config.depth = 1; config.count = 12; config.has_children = true;
    std::optional<lvui::native::NativeHierarchyItem> item; unsigned activations = 0; std::vector<bool> expanded;
    good(host.set_content([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        auto column = builder.vstack({}, "column", {.spacing = 0, .alignment = StackAlignment::leading});
        if (!column) return std::unexpected(column.error());
        auto value = lvui::native::NativeHierarchyItem::create(builder, *column, "item", input,
            {.activated = [&] { ++activations; }, .expansion_changed = [&](bool value) {
                expanded.push_back(value); if (item) good(item->set_options(take(item->options())));
            }});
        if (!value) return std::unexpected(value.error()); item = *value; return {};
    }));
    auto row = item->row(), chevron = item->chevron();
    auto root = take(item->root().snapshot()); auto arrow = take(chevron.snapshot());
    require(near(root.size.width, 200) && near(root.size.height, 20) && near(arrow.bounds.x, 176) &&
            take(row.action_options()).accessibility_name == "Camera" &&
            take(row.action_options()).accessibility_description == "Count 12" &&
            take(chevron.action_options()).accessibility_name == "Expand Camera", "Hierarchy row geometry or accessibility name changed");
    auto drawing = take(tree->drawing_snapshot({400, 100})); bool icon = !drawing.images.empty(), count = false;
    for (const auto& run : drawing.texts) if (run.text == "12") count = true;
    for (const auto& layer : drawing.overlays) {
        icon |= !layer.images.empty(); for (const auto& run : layer.texts) if (run.text == "12") count = true;
    }
    require(icon && count, "Hierarchy row did not draw its icon and count");
    arrow = take(chevron.snapshot());
    good(tree->input({.kind = WidgetInputKind::pointer_down, .x = arrow.bounds.x + arrow.bounds.width / 2,
                      .y = arrow.bounds.y + arrow.bounds.height / 2}));
    good(tree->input({.kind = WidgetInputKind::pointer_up, .x = arrow.bounds.x + arrow.bounds.width / 2,
                      .y = arrow.bounds.y + arrow.bounds.height / 2}));
    require(take(item->options()).configuration.expanded && expanded == std::vector<bool>{true} && activations == 1 &&
            take(chevron.action_options()).accessibility_name == "Collapse Camera", "Chevron did not expand and activate the row");
    good(row.focus()); good(tree->input({.kind = WidgetInputKind::key_down, .key = WidgetKey::left}));
    require(!take(item->options()).configuration.expanded && expanded == std::vector<bool>{true, false} && activations == 2,
            "Keyboard left did not collapse the hierarchy row");
    good(row.activate()); require(activations == 3, "Hierarchy row activation did not reach its owner");
    auto next = take(item->options()); next.configuration.width = 300; next.configuration.selected = true;
    next.configuration.label = "Layer"; next.configuration.has_children = false;
    good(item->set_options(next));
    require(near(take(item->root().snapshot()).size.width, 300) && !take(chevron.interaction()).visible &&
            take(row.action_options()).accessibility_name == "Layer", "Hierarchy item did not update its retained controls");
    next.configuration.icon_name = "provided";
    next.icon = take(ii::gui::rgba_image(1, 1, {255, 0, 0, 255})); good(item->set_options(next));
    auto missing_name = next; missing_name.configuration.icon_name.clear();
    require(!item->set_options(missing_name) && take(item->options()).icon == next.icon,
            "Invalid provided row icon replaced the last valid image");
    auto invalid = next; invalid.configuration.width = 0;
    require(!item->set_options(invalid) && take(item->options()).configuration == next.configuration,
            "Invalid hierarchy row update replaced its committed state");
    next.configuration.enabled = false; good(item->set_options(next)); good(row.activate());
    require(activations == 3, "Disabled hierarchy row activated");
    bool rejected = false;
    std::thread worker([&] { auto value = item->set_options(input); rejected = !value && value.error().code == ii::core::ErrorCode::wrong_executor; });
    worker.join(); require(rejected, "Hierarchy row accepted a foreign-thread update");
    good(host.clear_content()); require(!item->set_options(input), "Retired hierarchy row remained writable");

    input.configuration.enabled = true; input.configuration.has_children = true; unsigned after_retirement = 0;
    good(host.set_content([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        auto value = lvui::native::NativeHierarchyItem::create(builder, {}, "self-removing", input,
            {.activated = [&] { ++after_retirement; }, .expansion_changed = [&](bool) { good(host.clear_content()); }});
        if (!value) return std::unexpected(value.error()); item = *value; return {};
    }));
    good(item->chevron().activate());
    require(after_retirement == 0 && !item->row().snapshot(), "Retired hierarchy row received a stale activation callback");

    good(host.set_content([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        auto value=lvui::native::NativeHierarchyItem::create(builder,{},"stable",input);
        if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    auto stable=item->root();
    auto rejected_count=host.set_content([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        auto value=lvui::native::NativeHierarchyItem::create(builder,{},"candidate",input,{},
            [](WidgetBuilder&) -> ii::core::Result<void> {return ii::core::fail(ii::core::ErrorCode::failed,"Count View rejected");});
        if(!value)return std::unexpected(value.error());return {};
    });
    require(!rejected_count&&stable.snapshot(),"A failed custom count factory replaced the previous hierarchy row");
    tree->close();
}
