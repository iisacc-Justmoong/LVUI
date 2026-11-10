#include "NativeOwnedAvatar.generated.h"
#include <Text/NativeText.h>
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
bool near(double a,double b){return std::abs(a-b)<1e-4;}
std::vector<ii::gui::ImageRun> runs(const ii::gui::CanvasFrame& frame){
    auto result=frame.images;for(const auto& layer:frame.overlays)result.insert(result.end(),layer.images.begin(),layer.images.end());return result;
}
void generated(){
    using View=lvui_generated::NativeOwnedAvatar;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto outer=take(tree->content_host({},"outer",{640,480}));std::shared_ptr<View::Scope> scope;
    good(outer.set_content([&](auto& builder){return View::build(builder,{}, {},&scope);}));
    auto nodes=take(tree->snapshots());auto found=std::ranges::find_if(nodes,[](const auto& item){return item.id.ends_with("/avatar");});
    require(found!=nodes.end(),"Generated Avatar missing");auto widget=take(tree->find(found->id));auto initial=take(widget.snapshot());
    require(near(initial.size.width,40)&&near(initial.size.height,40)&&initial.accessibility_name=="Profile"&&
        initial.accessibility_semantics&&initial.accessibility_semantics->role==ii::gui::AccessibilityRole::image,
        "Avatar natural size or accessibility is incorrect");
    auto placeholder=runs(take(tree->drawing_snapshot({640,480})));
    const auto expected_placeholder=take((*View::avatar_placeholder_source())({40,40},1));
    const auto medium_bounds=take(widget.snapshot()).bounds;
    require(placeholder.size()==1&&placeholder[0].image==expected_placeholder&&near(placeholder[0].bounds.width,20)&&
        near(placeholder[0].bounds.height,20)&&near(placeholder[0].bounds.x,medium_bounds.x+10)&&
        near(placeholder[0].bounds.y,medium_bounds.y+10)&&placeholder[0].tint&&
        near(placeholder[0].tint->red,1)&&near(placeholder[0].tint->green,1)&&
        near(placeholder[0].tint->blue,1)&&near(placeholder[0].tint->alpha,.8),
        "Medium Avatar did not center and color the LVUI profile placeholder");
    good(widget.set_extent({80,80}));
    placeholder=runs(take(tree->drawing_snapshot({640,480})));
    const auto resized=take(widget.snapshot()).bounds;
    require(placeholder.size()==1&&near(placeholder[0].bounds.width,40)&&near(placeholder[0].bounds.height,40)&&
        near(placeholder[0].bounds.x,resized.x+20)&&near(placeholder[0].bounds.y,resized.y+20),
        "Avatar placeholder did not follow its host-assigned extent");
    require(placeholder[0].image->width()==80&&placeholder[0].image->height()==80,
        "Avatar placeholder raster did not follow its resized extent");
    const auto resized_placeholder=placeholder[0].image;
    placeholder=runs(take(tree->drawing_snapshot({640,480})));
    require(placeholder.size()==1&&placeholder[0].image==resized_placeholder,
        "An unchanged Avatar extent did not reuse its placeholder raster");
    auto graph=scope->native_lifetime.lock();require(bool(graph),"Avatar binding graph missing");
    scope->backend_0.size.set(ii::gui::AvatarSize::small);
    require(near(take(widget.snapshot()).size.width,24),"Avatar did not adopt small size");
    placeholder=runs(take(tree->drawing_snapshot({640,480})));
    require(placeholder.size()==1&&near(placeholder[0].bounds.width,12),"Small Avatar placeholder is not half its extent");
    scope->backend_0.size.set(ii::gui::AvatarSize::large);
    require(near(take(widget.snapshot()).size.width,64),"Avatar did not adopt large size");
    placeholder=runs(take(tree->drawing_snapshot({640,480})));
    require(placeholder.size()==1&&near(placeholder[0].bounds.width,32),"Large Avatar placeholder is not half its extent");
    scope->backend_0.size.set(static_cast<ii::gui::AvatarSize>(99));
    require(!take(graph->failures()).empty()&&near(take(widget.snapshot()).size.width,64),"Invalid Avatar size replaced the last valid size");
    scope->backend_0.size.set(ii::gui::AvatarSize::medium);
    require(take(graph->failures()).empty()&&near(take(widget.snapshot()).size.width,40),"Avatar did not recover its natural size");
    auto image=take(ii::gui::rgba_image(200,100,std::vector<std::uint8_t>(200*100*4,255)));
    scope->backend_0.image.set(image);auto drawing=runs(take(tree->drawing_snapshot({640,480})));
    require(drawing.size()==1&&drawing[0].image==image&&!drawing[0].tint&&near(drawing[0].radius,20)&&
        near(drawing[0].source.x,.25)&&near(drawing[0].source.width,.5),"Avatar did not submit the circular center crop");
    scope->backend_0.width.set(100);scope->backend_0.height.set(50);scope->backend_0.name.set("Changed profile");
    auto changed=take(widget.snapshot());drawing=runs(take(tree->drawing_snapshot({640,480})));
    require(changed.id==initial.id&&near(changed.size.width,100)&&near(changed.size.height,50)&&
        changed.accessibility_name=="Changed profile"&&drawing.size()==1&&near(drawing[0].radius,25)&&
        near(drawing[0].bounds.width,50),"Avatar update lost its widget, geometry or accessible name");
    scope->backend_0.enabled.set(false);require(!take(widget.snapshot()).enabled,"Disabled Avatar remained enabled");
    scope->backend_0.width.set(-1);require(!take(graph->failures()).empty()&&near(take(widget.snapshot()).size.width,100),
        "Invalid Avatar update replaced the last valid extent");
    scope->backend_0.width.set(80);require(take(graph->failures()).empty()&&near(take(widget.snapshot()).size.width,80),
        "Avatar did not recover after an invalid update");
    scope->backend_0.image.set({});placeholder=runs(take(tree->drawing_snapshot({640,480})));
    require(placeholder.size()==1&&placeholder[0].image!=image&&near(placeholder[0].bounds.width,25)&&
        near(placeholder[0].bounds.height,25),"Avatar did not restore its centered placeholder after clearing the photo");
    std::weak_ptr<View::Scope> weak=scope;good(outer.clear_content());scope.reset();graph.reset();
    require(weak.expired()&&!widget.snapshot(),"Retired Avatar retained its scope");tree->close();
}
void placeholder_color(){
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto outer=take(tree->content_host({},"color-host",{100,100}));
    std::optional<ii::gui::NativeAvatar> avatar;
    ii::gui::AvatarInput input;input.placeholder_source=lvui_generated::NativeOwnedAvatar::avatar_placeholder_source();input.placeholder_color={.25,.5,.75,1};
    good(outer.set_content([&](auto& builder)->ii::core::Result<void>{
        auto created=ii::gui::NativeAvatar::create(builder,{},"avatar",input);
        if(!created)return std::unexpected(created.error());avatar=*created;return {};
    }));
    auto drawing=runs(take(tree->drawing_snapshot({100,100})));
    require(drawing.size()==1&&drawing[0].tint&&near(drawing[0].tint->red,.0508761)&&
        near(drawing[0].tint->green,.21404114)&&near(drawing[0].tint->blue,.52252155)&&
        near(drawing[0].tint->alpha,1),"Avatar did not submit the common encoded color as linear ink");
    const auto rasterize=[](const std::vector<ii::gui::ImageRun>& images){
        ii::gui::CanvasFrame frame{};frame.viewport={0,0,100,100};frame.images=images;
        return ii::gui::detail::rasterize_foreground(frame.viewport,ii::gui::detail::canvas_layers(frame)[0],1);
    };
    const auto painted=rasterize(drawing);bool opaque=false;
    for(std::size_t i=0;i<painted.rgba.size();i+=4)if(painted.rgba[i+3]==255){
        opaque=true;
        require(std::abs(int(painted.rgba[i])-64)<=1&&std::abs(int(painted.rgba[i+1])-128)<=1&&
            std::abs(int(painted.rgba[i+2])-191)<=1,"Avatar ink was not rasterized in the selected sRGB color");
    }
    require(opaque,"Opaque Avatar ink did not produce an opaque pixel");
    const auto image=drawing[0].image;
    input.placeholder_color={1,0,0,0};good(avatar->set_options(input));
    drawing=runs(take(tree->drawing_snapshot({100,100})));
    require(drawing.size()==1&&drawing[0].image==image&&drawing[0].tint&&
        near(drawing[0].tint->red,1)&&near(drawing[0].tint->alpha,0),
        "Avatar color update lost its mask or transparent alpha");
    const auto transparent=rasterize(drawing);
    for(std::size_t i=3;i<transparent.rgba.size();i+=4)
        require(transparent.rgba[i]==0,"Transparent Avatar ink produced a visible pixel");
    auto invalid=input;invalid.placeholder_color.alpha=std::numeric_limits<double>::quiet_NaN();
    require(!avatar->set_options(invalid)&&runs(take(tree->drawing_snapshot({100,100})))==drawing,
        "Invalid Avatar color replaced the last valid drawing");
    good(outer.clear_content());tree->close();
}
void reentrant_accessibility(){
    std::optional<ii::gui::NativeAvatar> avatar;bool reenter=false,throw_notification=false;
    ii::gui::AvatarInput latest;latest.configuration.width=80;latest.configuration.height=50;
    latest.configuration.accessibility_name="Latest profile";latest.configuration.enabled=true;
    latest.configuration.background={0,1,0,1};
    ii::gui::detail::WidgetFrameHooks hooks;hooks.accessibility_changed=[&]{
        if(throw_notification){throw_notification=false;throw std::runtime_error("Accessibility observer failed");}
        if(reenter){reenter=false;good(avatar->set_options(latest));}
    };
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();},ii::gui::WidgetAppearance{},hooks);
    auto outer=take(tree->content_host({},"outer",{640,480}));
    good(outer.set_content([&](auto& builder)->ii::core::Result<void>{
        auto stack=builder.vstack({},"layout",{.spacing=0,.alignment=ii::gui::StackAlignment::leading});if(!stack)return std::unexpected(stack.error());
        auto created=ii::gui::NativeAvatar::create(builder,*stack,"avatar",{});if(!created)return std::unexpected(created.error());avatar=*created;return {};
    }));
    ii::gui::AvatarInput older;older.configuration.width=100;older.configuration.height=60;
    older.configuration.accessibility_name="Older profile";older.configuration.enabled=false;older.configuration.background={1,0,0,1};
    reenter=true;good(avatar->set_options(older));auto snapshot=take(avatar->root().snapshot());
    require(!reenter&&near(snapshot.size.width,80)&&near(snapshot.size.height,50)&&snapshot.enabled&&
        snapshot.accessibility_name=="Latest profile","A stale Avatar update replaced a reentrant accessibility update");
    auto frame=take(tree->drawing_snapshot({640,480}));bool green=false,red=false;
    const auto inspect=[&](const auto& panels){for(const auto& panel:panels){green|=near(panel.top.green,1)&&near(panel.top.red,0);red|=near(panel.top.red,1)&&near(panel.top.green,0);}};
    inspect(frame.panels);for(const auto& layer:frame.overlays)inspect(layer.panels);
    require(green&&!red,"A stale Image drawing replaced the reentrant Avatar image");
    auto failing=latest;failing.configuration.accessibility_name="Failed notification";throw_notification=true;
    auto error=avatar->set_options(failing);require(!error&&error.error().cause,"An accessibility exception lost its cause");
    good(avatar->set_options(latest));require(take(avatar->root().snapshot()).accessibility_name=="Latest profile","Avatar failed to recover after an observer exception");
    bool rejected=false;std::thread other([&]{auto result=avatar->set_options(latest);rejected=!result&&result.error().code==ii::core::ErrorCode::wrong_executor;});other.join();
    require(rejected,"A foreign thread changed Avatar state");tree->close();
}
}
int main(){generated();placeholder_color();reentrant_accessibility();}
