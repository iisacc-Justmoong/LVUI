#include "NativeOwnedIcon.generated.h"
#include "NativeOwnedStaticIcon.generated.h"
#include <iiGUI/WidgetState.h>
#include <LVUI/NativeAuthoredIcons.h>
#include <iiGUI/SvgImageCache.h>
#include <algorithm>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> value) {
    if (!value) throw std::runtime_error(value.error().message);
    return std::move(*value);
}
void good(ii::core::Result<void> value) { if (!value) throw std::runtime_error(value.error().message); }
std::vector<ii::gui::ImageRun> images(const ii::gui::CanvasFrame& frame) {
    auto result = frame.images;
    for (const auto& overlay : frame.overlays)
        result.insert(result.end(), overlay.images.begin(), overlay.images.end());
    return result;
}
struct CountingSource final : lvui::IconSource {
    std::shared_ptr<const lvui::IconAsset> asset;mutable unsigned calls=0;std::uint64_t version=1;
    lvui::IconResult<std::shared_ptr<const lvui::IconAsset>> resolve(std::string_view)const override{++calls;return asset;}
    std::uint64_t revision()const noexcept override{return version;}
};
}
int main() {
    using View = lvui_generated::NativeOwnedIcon;
    auto tree = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {100, 100}));
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](ii::gui::WidgetBuilder& builder) { return View::build(builder, {}, {}, &scope); }));
    const auto snapshots = take(tree->snapshots());
    const auto found = std::ranges::find_if(snapshots, [](const auto& value) { return value.id.ends_with("/symbol"); });
    check(found != snapshots.end(), "Declarative icon did not mount");
    auto widget = take(tree->find(found->id));
    auto runs = images(take(tree->drawing_snapshot({100, 100})));
    check(runs.size() == 1 && runs[0].image->width() == 36 && take(widget.snapshot()).accessibility_name == "Brush",
          "Declarative icon did not use its initial source and name");
    scope->backend_0.name.set("previous-frame");
    scope->backend_0.size.set(24);
    scope->backend_0.label.set("Previous frame");
    runs = images(take(tree->drawing_snapshot({100, 100})));
    check(runs.size() == 1 && runs[0].image->width() == 48 &&
          take(widget.snapshot()).accessibility_name == "Previous frame", "Reactive icon update failed");
    auto graph = scope->native_lifetime.lock();
    check(bool(graph), "Icon binding graph is unavailable");
    scope->backend_0.size.set(-1);
    auto rejected = images(take(tree->drawing_snapshot({100, 100})));
    check(!take(graph->failures()).empty() && rejected.size() == 1 && rejected[0].image == runs[0].image,
          "Invalid reactive icon size changed its visible image");
    scope->backend_0.size.set(18);
    runs = images(take(tree->drawing_snapshot({100, 100})));
    check(take(graph->failures()).empty() && runs.size() == 1 && runs[0].image->width() == 36,
          "Icon binding did not recover after invalid input");
    scope->backend_0.name.set("brush");
    scope->backend_0.source.set(lvui::authored_icon_catalog());
    scope->backend_0.revision.set(1);
    runs = images(take(tree->drawing_snapshot({100, 100})));
    check(runs.size() == 1 && runs[0].image == take(lvui::native::authored_icon_image<ii::gui::CachedSvgDecoder>("brush", 36)),
          "Explicit reactive icon source was not used");
    auto counting=std::make_shared<CountingSource>();auto asset=lvui::design_icon_catalog()->resolve("brush");
    check(bool(asset),"Missing counting-source icon");counting->asset=*asset;
    scope->backend_0.source.set(counting);const auto count=counting->calls;
    scope->backend_0.label.set("Counting source");
    check(counting->calls==count&&take(widget.snapshot()).accessibility_name=="Counting source",
          "Changing an accessible name re-resolved the unchanged icon");
    ++counting->version;scope->backend_0.revision.set(2);
    check(counting->calls==count+1,"An explicitly published icon-source revision was not applied");
    std::weak_ptr<CountingSource> weak_source=counting;
    good(host.clear_content());
    check(!widget.snapshot(), "Retired icon remained attached");
    scope.reset(); graph.reset(); counting.reset();
    check(weak_source.expired(),"Retired icon retained its custom source");
    good(host.set_content([&](ii::gui::WidgetBuilder& builder) {
        return lvui_generated::NativeOwnedStaticIcon::build(builder);
    }));
    runs = images(take(tree->drawing_snapshot({100, 100})));
    check(runs.size() == 1 && runs[0].image->width() == 36,
          "Static declarative icon did not use its intrinsic default");
    good(host.clear_content()); tree->close();
}
