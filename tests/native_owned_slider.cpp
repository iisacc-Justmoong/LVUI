#include <LVUI/NativeSlider.h>
#include <iiGUI/WidgetState.h>
#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
void owned_slider(){
    using namespace ii::gui;std::optional<lvui::native::NativeSlider> slider;bool reenter=false,throw_callback=false;
    lvui::SliderConfiguration nested;nested.minimum=0;nested.maximum=10;nested.value=3;nested.step=1;nested.width=120;nested.accessibility_name="Newest";
    detail::WidgetFrameHooks hooks;hooks.accessibility_changed=[&]{if(reenter){reenter=false;good(slider->set_options(nested));}};
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();},WidgetAppearance{},hooks);
    auto host=take(tree->content_host({},"host",{640,420}));std::vector<double> moved;
    lvui::SliderConfiguration config;config.minimum=0;config.maximum=10;config.value=5;config.step=1;config.width=100;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto root=builder.vstack({},"root",{.spacing=0,.alignment=StackAlignment::leading});if(!root)return std::unexpected(root.error());
        auto created=lvui::native::NativeSlider::create(builder,*root,"slider",config,[&](double value){
            if(throw_callback)throw std::runtime_error("Slider callback failed");moved.push_back(value);
        });
        if(!created)return std::unexpected(created.error());slider=*created;return {};
    }));
    auto snapshot=take(slider->root().snapshot());auto bounds=snapshot.bounds;
    require(snapshot.kind==WidgetKind::action_surface&&snapshot.accessibility_semantics&&
        snapshot.accessibility_semantics->role==AccessibilityRole::slider&&snapshot.minimum==0&&snapshot.maximum==10&&snapshot.value==5,
        "LVUI Slider did not expose its independent value and accessibility range");
    auto drawing=take(tree->drawing_snapshot({640,420}));bool track=false,fill=false,thumb=false;
    const auto near=[](double a,double b){return std::abs(a-b)<.001;};
    const auto inspect=[&](const auto& panels){for(const auto& panel:panels){const auto& r=panel.bounds;
        track|=near(r.x,bounds.x+9)&&near(r.y,bounds.y+13)&&near(r.width,82)&&near(r.height,6);
        fill|=near(r.x,bounds.x+9)&&near(r.y,bounds.y+13)&&near(r.width,41)&&near(r.height,6);
        thumb|=near(r.x,bounds.x+41)&&near(r.y,bounds.y+7)&&near(r.width,18)&&near(r.height,18);
    }};
    inspect(drawing.panels);for(const auto& layer:drawing.overlays)inspect(layer.panels);
    require(track&&fill&&thumb,"LVUI Slider did not draw its track, fill and thumb");
    auto tiny=config;tiny.step=0;tiny.value=.01;tiny.thumb_diameter=1;good(slider->set_options(tiny));
    good(slider->root().focus());(void)take(tree->drawing_snapshot({640,420}));
    tiny.value=1e-48;good(slider->set_options(tiny));(void)take(tree->drawing_snapshot({640,420}));
    good(slider->set_options(config));
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=bounds.x+9,.y=bounds.y+16}));
    good(tree->input({.kind=WidgetInputKind::pointer_move,.x=bounds.x+91,.y=bounds.y+16}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=bounds.x+91,.y=bounds.y+16}));
    require(moved.size()==2&&moved[0]==0&&moved[1]==10&&take(slider->options()).value==10,
        "LVUI Slider pointer capture or callbacks failed");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::left}));
    auto keyboard=take(slider->root().snapshot());
    if(take(slider->options()).value!=9||moved.back()!=9)
        throw std::runtime_error("LVUI Slider keyboard adjustment: value="+std::to_string(take(slider->options()).value)+
            " focused="+std::to_string(keyboard.focused)+" callback="+std::to_string(moved.back()));
    good(tree->input({.kind=WidgetInputKind::increment,.target=snapshot.id}));
    require(take(slider->options()).value==10&&take(slider->root().snapshot()).value==10,"Accessible increment did not update LVUI state");
    good(tree->input({.kind=WidgetInputKind::decrement,.target=snapshot.id}));
    require(take(slider->options()).value==9,"Accessible decrement did not update LVUI state");
    config.width=120;good(slider->set_options(config));
    require(take(slider->options()).value==9,"An unchanged input value replaced the user's Slider adjustment");
    config.value=4;good(slider->set_options(config));require(take(slider->options()).value==4,"A changed Slider input was ignored");
    auto invalid=config;invalid.maximum=invalid.minimum;require(!slider->set_options(invalid)&&take(slider->options()).value==4,
        "Invalid Slider options replaced valid state");
    auto older=config;older.accessibility_name="Older";older.width=90;reenter=true;good(slider->set_options(older));
    auto latest=take(slider->options());require(!reenter&&latest.accessibility_name=="Newest"&&latest.width==120&&latest.value==3,
        "An older Slider update replaced a reentrant update");
    throw_callback=true;auto callback_error=tree->input({.kind=WidgetInputKind::decrement,.target=snapshot.id});
    require(!callback_error&&bool(callback_error.error().cause)&&take(slider->options()).value==2,
        "Slider callback exception lost its cause or committed value");throw_callback=false;
    nested.enabled=false;good(slider->set_options(nested));
    good(tree->input({.kind=WidgetInputKind::increment,.target=snapshot.id}));
    require(take(slider->options()).value==2,"Disabled Slider accepted an accessibility adjustment");
    bool rejected=false;std::thread worker([&]{auto result=slider->set_options(config);rejected=!result&&result.error().code==ii::core::ErrorCode::wrong_executor;});worker.join();
    require(rejected,"A foreign thread changed Slider state");good(host.clear_content());
    auto retired=slider->set_options(config);require(!retired&&retired.error().code==ii::core::ErrorCode::closed,"Retired Slider accepted an update");
    slider.reset();tree->close();
}
void nested_coordinates(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{640,420}));std::optional<lvui::native::NativeSlider> slider;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto root=builder.absolute_stack({},"outer",{640,420});if(!root)return std::unexpected(root.error());
        auto child=builder.content_host(*root,"nested",{200,100});if(!child)return std::unexpected(child.error());
        if(auto framed=child->set_frame({73,45,200,100});!framed)return framed;
        return child->set_content([&](auto& nested)->ii::core::Result<void>{
            auto layout=nested.vstack({},"layout",{.spacing=0,.alignment=StackAlignment::leading});if(!layout)return std::unexpected(layout.error());
            lvui::SliderConfiguration config;config.minimum=0;config.maximum=10;config.value=5;config.step=1;config.width=100;
            auto created=lvui::native::NativeSlider::create(nested,*layout,"slider",config);if(!created)return std::unexpected(created.error());
            slider=*created;return {};
        });
    }));
    auto bounds=take(slider->root().snapshot()).bounds;require(bounds.x>=73&&bounds.y>=45,"Nested Slider did not move with its host");
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=bounds.x+91,.y=bounds.y+16}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=bounds.x+91,.y=bounds.y+16}));
    require(take(slider->options()).value==10,"Nested host offset changed Slider pointer mapping");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::left}));
    require(take(slider->options()).value==9,"Nested Slider pointer focus did not reach keyboard routing");
    tree->close();
}
}
int main(){owned_slider();nested_coordinates();}
