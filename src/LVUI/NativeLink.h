#pragma once
#include "Link.h"
#include <iiGUI/NativeWidgets.h>
#include <iiGUI/Text.h>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace lvui::native {
inline StyleColor link_color(ii::gui::WidgetColor value)noexcept{return {value.red,value.green,value.blue,value.alpha};}
inline ButtonTone link_tone(ButtonTone value)noexcept{return value;}
inline ButtonTone link_tone(ii::gui::ButtonTone value)noexcept {
    switch(value){case ii::gui::ButtonTone::primary:return ButtonTone::primary;case ii::gui::ButtonTone::normal:return ButtonTone::normal;
    case ii::gui::ButtonTone::borderless:return ButtonTone::borderless;case ii::gui::ButtonTone::destructive:return ButtonTone::destructive;
    case ii::gui::ButtonTone::disabled:return ButtonTone::disabled;}return static_cast<ButtonTone>(-1);
}
template<class T> requires (!std::is_const_v<T> && std::derived_from<T,NavigationTarget>)
inline std::weak_ptr<NavigationTarget> navigation_target(const std::shared_ptr<T>& value)noexcept{return value;}
template<class T> requires (!std::is_const_v<T> && std::derived_from<T,NavigationTarget>)
inline std::weak_ptr<NavigationTarget> navigation_target(const std::weak_ptr<T>& value)noexcept{return value;}
inline std::weak_ptr<NavigationTarget> navigation_target(std::nullptr_t)noexcept{return {};}
template<class T> requires std::derived_from<std::remove_const_t<T>,NavigationPage>
inline std::shared_ptr<const NavigationPage> navigation_page(const std::shared_ptr<T>& value)noexcept{return value;}
inline std::shared_ptr<const NavigationPage> navigation_page(std::nullptr_t)noexcept{return {};}

struct LinkCallbacks final {
    LinkMethod method;
    std::vector<LinkMethod> methods;
    std::function<void()> clicked,pressed,released,canceled;
    std::function<void(bool)> navigated;
    std::function<void(const ii::core::Error&)> navigation_failed;
};
class NativeLink final {
    struct State final {
        Link view;
        ii::gui::Widget root;
        std::shared_ptr<NavigationContext> context;
        LinkCallbacks callbacks;
        bool live=false,activating=false,dispatching=false;
        std::uint64_t revision=0;
    };
    std::shared_ptr<State> state_;
    explicit NativeLink(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static ii::gui::ActionOptions action_options(const Link& view){
        const auto& c=view.configuration();const auto& p=c.palette;
        const auto convert=[](StyleColor color){return ii::gui::WidgetColor{color.red,color.green,color.blue,color.alpha};};
        const auto pressed=p.pressed.value_or(StyleColor{p.link.red/1.2,p.link.green/1.2,p.link.blue/1.2,p.link.alpha});
        return {.text=c.text,.accessibility_name=c.accessibility_name,.enabled=c.enabled,
            .underline=c.underline,.focus_ring=false,.role=ii::gui::ActionRole::link,.text_role=ii::gui::TextRole::description,
            .font_size=c.font_size,.line_height=c.line_height,.padding={c.horizontal_padding,c.vertical_padding,c.horizontal_padding,c.vertical_padding},
            .minimum_height=c.minimum_height,.corner_radius=c.corner_radius,.width=c.width,.height=c.height,
            .text_color=convert(p.link),.hover_text_color=convert(p.hover),.pressed_text_color=convert(pressed),.disabled_text_color=convert(p.disabled),
            .interaction_enabled=c.tone!=ButtonTone::disabled};
    }
    [[nodiscard]] static ii::gui::Color linear(StyleColor value)noexcept {
        const auto channel=[](double c){return c<=.04045?c/12.92:std::pow((c+.055)/1.055,2.4);};
        return {float(channel(value.red)),float(channel(value.green)),float(channel(value.blue)),float(value.alpha)};
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>
    paint(const std::shared_ptr<State>& state,const ii::gui::WidgetPaintState& input){
        const auto& c=state->view.configuration();const auto visual=state->view.visual({input.enabled,input.hovered,input.pressed,input.focused});
        double width=0,height=0;
        if(!c.text.empty()&&input.content_children==0){
            auto metrics=ii::gui::measure_text_layout({.text=c.text,.font_size=float(c.font_size),.font_weight=c.font_weight,
                .font_family=input.font_family,.line_height=float(c.line_height),.elide_right=true});
            if(!metrics)return std::unexpected(metrics.error());width=metrics->width;height=metrics->height;
        }
        const auto geometry=state->view.geometry(input.size.width,input.size.height,width,height,input.content_children>0);
        if(!geometry)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI Link extent");
        auto drawing=std::make_shared<ii::gui::WidgetDrawing>();
        if(visual.focused){const auto stroke=std::min({1.5,input.size.width/2,input.size.height/2});if(stroke>0){const auto ink=linear(visual.focus);
            drawing->panels.push_back({{0,0,float(input.size.width),float(input.size.height)},ink,ink,float(geometry->radius),float(stroke)});}}
        if(geometry->draw_text){const auto& r=geometry->text;drawing->texts.push_back({.bounds={float(r.x),float(r.y),float(r.width),float(r.height)},
            .text=c.text,.color=linear(visual.text),.font_size=float(c.font_size),.font_weight=c.font_weight,
            .alignment=ii::gui::TextAlignment::left,.font_family=input.font_family,.line_height=float(c.line_height),.elide_right=true,
            .clip=ii::gui::Rect{0,0,float(input.size.width),float(input.size.height)},.underline=visual.underline});}
        return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
    }
    struct Guard final {bool& value;explicit Guard(bool& v):value(v){value=true;}~Guard(){value=false;}};
    [[nodiscard]] static ii::core::Result<std::optional<bool>> dispatch(const std::shared_ptr<State>& state,bool* handled=nullptr){
        if(handled)*handled=false;
        if(!state->live)return ii::core::fail(ii::core::ErrorCode::closed,"Link has retired");
        if(state->dispatching)return ii::core::fail(ii::core::ErrorCode::busy,"Link navigation is already active");
        Guard guard(state->dispatching);auto result=state->view.navigate(state->context?state->context->target():nullptr);
        if(!state->live)return result;
        try {
            if(!result){auto callback=state->callbacks.navigation_failed;if(callback){callback(result.error());if(handled)*handled=true;}}
            else if(*result){auto callback=state->callbacks.navigated;if(callback)callback(**result);}
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Link navigation callback threw",std::current_exception());}
        return result;
    }
    [[nodiscard]] static ii::core::Result<void> activate(const std::shared_ptr<State>& state){
        if(!state->live)return {};
        if(state->activating)return ii::core::fail(ii::core::ErrorCode::busy,"Link activation is already active");
        Guard guard(state->activating);const auto& c=state->view.configuration();
        const LinkEvent event{{c.href,c.params,c.target,c.replace}};
        bool handled=false;auto result=dispatch(state,&handled);if(!state->live)return {};
        if(!result&&!handled)return std::unexpected(result.error());
        auto method=state->callbacks.method;auto methods=state->callbacks.methods;auto clicked=state->callbacks.clicked;
        if(method){method(event);if(!state->live)return {};}
        for(auto& item:methods){if(item)item(event);if(!state->live)return {};}
        if(clicked)clicked();return {};
    }
public:
    [[nodiscard]] static ii::core::Result<NativeLink> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,std::shared_ptr<NavigationContext> context,const LinkConfiguration& value,LinkCallbacks callbacks={}){
        if(!context||callbacks.methods.size()>256)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI Link context or methods");
        auto state=std::make_shared<State>();state->context=std::move(context);state->callbacks=std::move(callbacks);
        if(!state->view.set_configuration(value))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI Link configuration");
        std::weak_ptr<State> weak=state;
        auto root=builder.action_surface(parent,id,action_options(state->view),[weak]{if(auto state=weak.lock()){
            auto result=activate(state);if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}
        }});if(!root)return std::unexpected(root.error());state->root=*root;
        const auto event=[weak](auto member){return [weak,member]{if(auto state=weak.lock();state&&state->live){auto callback=state->callbacks.*member;if(callback)callback();}};};
        if(auto installed=root->set_control_events({.pressed=event(&LinkCallbacks::pressed),.released=event(&LinkCallbacks::released),
                .canceled=event(&LinkCallbacks::canceled)});!installed)return std::unexpected(installed.error());
        if(auto installed=root->set_paint_handler([state](const auto& input){return paint(state,input);});!installed)return std::unexpected(installed.error());
        if(auto registered=builder.on_publish([weak]{if(auto state=weak.lock())state->live=true;});!registered)return std::unexpected(registered.error());
        if(auto registered=builder.on_retire([weak]{if(auto state=weak.lock())state->live=false;});!registered)return std::unexpected(registered.error());
        return NativeLink(std::move(state));
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::core::Result<void> set_options(LinkConfiguration value)const {
        if(!Link::validate(value))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI Link configuration");
        if(auto current=state_->root.action_options();!current)return std::unexpected(current.error());
        auto next=state_->view;if(!next.set_configuration(std::move(value)))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI Link configuration");
        auto previous=state_->view;const auto revision=++state_->revision;state_->view=std::move(next);
        auto changed=state_->root.set_action_options(action_options(state_->view));
        if(!changed){if(state_->revision==revision){state_->view=std::move(previous);++state_->revision;}return changed;}
        if(state_->revision!=revision||!state_->live)return {};
        return state_->root.request_repaint();
    }
    [[nodiscard]] ii::core::Result<void> set_methods(std::vector<LinkMethod> methods)const {
        if(methods.size()>256)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Too many LVUI Link methods");
        if(auto current=state_->root.action_options();!current)return std::unexpected(current.error());
        state_->callbacks.methods=std::move(methods);return {};
    }
    [[nodiscard]] ii::core::Result<std::optional<bool>> trigger_navigation()const {
        if(auto current=state_->root.action_options();!current)return std::unexpected(current.error());return dispatch(state_);
    }
};
} // namespace lvui::native
