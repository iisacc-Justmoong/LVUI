#include <LVUI/NativeListToolbar.h>
#include <iiGUI/NativeListToolbarView.h>
#include <iiGUI/SvgImageCache.h>
#include <iiGUI/WidgetState.h>

#include <memory>
#include <functional>
#include <optional>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace lvui::native {
using ListToolbarModel=ii::gui::BasicListToolbar<ListToolbarModelPolicy>;
using NativeListToolbar=ListToolbarBinding<ii::gui::BasicListToolbar,ii::gui::BasicNativeListToolbar,ii::gui::CachedSvgDecoder>;
}
static_assert(std::is_same_v<lvui::native::NativeListToolbar,
    ii::gui::BasicNativeListToolbar<lvui::native::ListToolbarHostPolicy<lvui::native::ListToolbarModel,ii::gui::CachedSvgDecoder>>>);

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
    return std::move(*value);
}
void good(ii::core::Result<void> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
}
struct Source final:lvui::IconSource {
    std::shared_ptr<const lvui::IconAsset> asset;mutable std::function<void()> on_resolve;
    mutable unsigned calls=0;std::uint64_t version=1;
    lvui::IconResult<std::shared_ptr<const lvui::IconAsset>> resolve(std::string_view)const override {
        ++calls;if(auto callback=std::exchange(on_resolve,{}))callback();return asset;
    }
    std::uint64_t revision()const noexcept override{return version;}
};
void retire_during_resolution(){
    using namespace ii::gui;
    auto source=std::make_shared<Source>();auto asset=lvui::design_icon_catalog()->resolve("brush");
    if(!asset)throw std::runtime_error("Missing test asset");source->asset=*asset;
    lvui::native::ListToolbarInput input;input.icon_source=source;input.configuration.icons={"home","","generalchevronRight"};
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"retire-host",{200,100}));std::optional<lvui::native::NativeListToolbar> toolbar;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=lvui::native::NativeListToolbar::create(builder,{},"toolbar",input);if(!value)return std::unexpected(value.error());toolbar=*value;return {};
    }));
    auto next=input;++source->version;++next.icon_revision;const auto calls=source->calls;
    source->on_resolve=[&]{good(host.clear_content());};good(toolbar->set_options(next));
    require(source->calls==calls+1,"LVUI list toolbar continued icon resolution after retirement");
    std::weak_ptr<Source> weak=source;input.icon_source.reset();next.icon_source.reset();source.reset();
    require(weak.expired()&&!toolbar->options(),"LVUI list toolbar retained its source after retirement");tree->close();
}
}
int main() {
    retire_during_resolution();
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
