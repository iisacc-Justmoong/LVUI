#include "NativeOwnedNamedIcons.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& state,std::string_view suffix){
    const auto nodes=take(state->snapshots());
    const auto item=std::ranges::find_if(nodes,[&](const auto& node){return node.id.ends_with(suffix);});
    if(item==nodes.end())throw std::runtime_error("Named icon control did not mount");
    return take(state->find(item->id));
}
std::shared_ptr<const ii::gui::RasterImage> image_in(const ii::gui::CanvasFrame& frame,ii::gui::Rect bounds){
    const auto match=[&](const auto& images)->std::shared_ptr<const ii::gui::RasterImage>{
        for(const auto& run:images)if(run.bounds.x>=bounds.x-.01f&&run.bounds.y>=bounds.y-.01f&&
            run.bounds.x+run.bounds.width<=bounds.x+bounds.width+.01f&&run.bounds.y+run.bounds.height<=bounds.y+bounds.height+.01f)return run.image;
        return {};
    };
    if(auto image=match(frame.images))return image;
    for(const auto& layer:frame.overlays)if(auto image=match(layer.images))return image;
    return {};
}
struct ReentrantSource final:lvui::IconSource {
    mutable std::function<void()> callback;
    lvui::IconResult<std::shared_ptr<const lvui::IconAsset>> resolve(std::string_view)const override{
        auto call=std::move(callback);if(call)call();
        return std::make_shared<const lvui::IconAsset>(lvui::IconAsset{
            "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 10 10'><rect width='10' height='10'/></svg>"});
    }
    std::uint64_t revision()const noexcept override{return 1;}
};
}
int main(){
    using View=lvui_generated::NativeOwnedNamedIcons;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{300,200}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    auto builtin=find(tree,"/builtin"),menu=find(tree,"/builtin_menu"),custom=find(tree,"/custom"),custom_menu=find(tree,"/custom_menu");
    auto frame=take(tree->drawing_snapshot({300,200}));
    auto original=image_in(frame,take(custom.snapshot()).bounds),original_menu=image_in(frame,take(custom_menu.snapshot()).bounds);
    require(image_in(frame,take(builtin.snapshot()).bounds)&&image_in(frame,take(menu.snapshot()).bounds)&&original&&original_menu,
            "Built-in or custom named icons did not draw");
    require(take(builtin.snapshot()).accessibility_name=="Add","Named icon accessibility was lost");
    auto graph=scope->native_lifetime.lock();require(bool(graph),"Named icon binding graph is unavailable");
    scope->backend_0.name.set("missing");
    frame=take(tree->drawing_snapshot({300,200}));
    require(!take(graph->failures()).empty()&&image_in(frame,take(custom.snapshot()).bounds)==original&&
            image_in(frame,take(custom_menu.snapshot()).bounds)==original_menu,"Missing icon replaced the previous image");
    scope->backend_0.name.set("brand");require(take(graph->failures()).empty(),"Named icon did not recover after lookup failure");
    scope->backend_0.owner->replace("<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 10 10'><circle cx='5' cy='5' r='4' fill='#00ff00'/></svg>");
    scope->backend_0.revision.set(2);frame=take(tree->drawing_snapshot({300,200}));
    auto updated=image_in(frame,take(custom.snapshot()).bounds),updated_menu=image_in(frame,take(custom_menu.snapshot()).bounds);
    require(updated&&updated_menu&&updated!=original&&updated_menu!=original_menu&&take(graph->failures()).empty(),
            "Icon source revision did not update both controls");
    scope->backend_0.owner->replace("<svg xmlns='http://www.w3.org/2000/svg'><script>bad</script></svg>");
    scope->backend_0.revision.set(3);frame=take(tree->drawing_snapshot({300,200}));
    require(!take(graph->failures()).empty()&&image_in(frame,take(custom.snapshot()).bounds)==updated&&
            image_in(frame,take(custom_menu.snapshot()).bounds)==updated_menu,"Invalid replacement damaged the live icons");
    scope->backend_0.owner->replace("<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 10 10'><path d='M0 0h10v10H0z'/></svg>");
    scope->backend_0.revision.set(4);require(take(graph->failures()).empty(),"Icons did not recover after invalid SVG");
    scope->backend_0.revision.set(-1);require(!take(graph->failures()).empty(),"Negative icon revision was accepted");
    scope->backend_0.revision.set(4);require(take(graph->failures()).empty(),"Icon revision did not recover");
    std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired()&&!builtin.snapshot()&&!custom.snapshot(),"Named icon controls retained a retired scope");

    std::optional<lvui::native::NativeButton> direct_button;
    std::optional<lvui::native::NativeDropdownButton> direct_dropdown;
    lvui::native::ButtonInput button_input;button_input.configuration.text="Old";
    lvui::native::DropdownInput dropdown_input;dropdown_input.configuration.text="Old";
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto stack=builder.vstack({},"direct",{.spacing=0});if(!stack)return std::unexpected(stack.error());
        auto button=lvui::native::NativeButton::create(builder,*stack,"button",button_input);if(!button)return std::unexpected(button.error());direct_button=*button;
        auto dropdown=lvui::native::NativeDropdownButton::create(builder,*stack,"dropdown",dropdown_input);if(!dropdown)return std::unexpected(dropdown.error());direct_dropdown=*dropdown;
        return {};
    }));
    auto source=std::make_shared<ReentrantSource>();
    source->callback=[&]{auto newer=button_input;newer.configuration.text="New button";good(direct_button->set_options(newer));};
    auto candidate_button=button_input;candidate_button.icon_name="brand";candidate_button.icon_source=source;candidate_button.configuration.icon_mode=true;
    good(direct_button->set_options(candidate_button));
    require(take(direct_button->root().snapshot()).text=="New button","Icon source overwrote a reentrant button update");
    auto second_source=std::make_shared<ReentrantSource>();
    second_source->callback=[&]{auto newer=dropdown_input;newer.configuration.text="New dropdown";good(direct_dropdown->set_options(newer));};
    auto candidate_dropdown=dropdown_input;candidate_dropdown.icon_name="brand";candidate_dropdown.icon_source=second_source;
    good(direct_dropdown->set_options(candidate_dropdown));
    require(take(direct_dropdown->options()).configuration.text=="New dropdown","Icon source overwrote a reentrant dropdown update");
    good(host.clear_content());tree->close();
}
