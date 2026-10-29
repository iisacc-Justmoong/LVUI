#include <LVUI/NativeListToolbar.h>
#include <iiGUI/WidgetState.h>

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
}
int main() {
    using namespace ii::gui;
    lvui::native::ListToolbarInput input; input.configuration.icons = {"home", "", "generalchevronRight"};
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {200, 100}));
    std::optional<lvui::native::NativeListToolbar> toolbar; std::vector<std::string> events; bool remove = false;
    good(host.set_content([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        auto column = builder.vstack({}, "column", {.spacing=0, .alignment=StackAlignment::leading});
        if (!column) return std::unexpected(column.error());
        auto value = lvui::native::NativeListToolbar::create(builder, *column, "toolbar", input,
            {.clicked = [&](std::size_t index, std::string name) { events.push_back(std::to_string(index) + ":" + name); if (remove) good(host.clear_content()); }});
        if (!value) return std::unexpected(value.error()); toolbar = *value; return {};
    }));
    const auto root = take(toolbar->root().snapshot()); auto first = take(toolbar->slot(0)); auto blank = take(toolbar->slot(1));
    require(root.size.width == 90 && root.size.height == 30 && !take(blank.interaction()).visible &&
            take(first.action_options()).accessibility_name == "home", "List toolbar lost its geometry, labels or empty slot");
    auto drawing = take(tree->drawing_snapshot({200, 100})); std::size_t images = drawing.images.size();
    for (const auto& layer : drawing.overlays) images += layer.images.size();
    require(images == 2, "List toolbar did not draw its available icons");
    good(first.activate()); good(take(toolbar->slot(2)).activate());
    require(events == std::vector<std::string>{"0:home", "2:generalchevronRight"}, "List toolbar sent an incorrect slot action");
    auto next = input; next.configuration.icons[0] = "missing-icon";
    require(!toolbar->set_options(next) && take(toolbar->options()) == input,
            "An invalid toolbar icon replaced the committed configuration");
    next = input; next.configuration.interactive = false; good(toolbar->set_options(next)); good(first.activate());
    require(events.size() == 2, "A noninteractive toolbar still activated its slot");
    next.configuration.interactive = true; next.configuration.width = 30; good(toolbar->set_options(next));
    auto narrowed = take(first.snapshot());
    require(narrowed.size.width <= 22 && narrowed.bounds.x >= 0, "Narrow toolbar slot exceeded its available extent");
    next.configuration.width = 20; good(toolbar->set_options(next));
    require(!take(first.interaction()).visible, "A zero-sized slot remained accessible");
    next.configuration.width = 90; good(toolbar->set_options(next));
    require(take(first.interaction()).visible, "A slot did not reappear when space returned");
    bool rejected = false;
    std::thread worker([&] { auto result = toolbar->set_options(input); rejected = !result && result.error().code == ii::core::ErrorCode::wrong_executor; });
    worker.join(); require(rejected, "List toolbar accepted a foreign-thread update");
    remove = true; good(first.activate()); require(!toolbar->set_options(input), "Toolbar survived removal inside its own callback");
    tree->close();
}
