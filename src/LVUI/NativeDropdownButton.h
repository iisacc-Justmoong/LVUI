#pragma once
#include "DropdownButton.h"
#include "NativeIconCache.h"
#include "NativeStylePort.h"
#include <iiGUI/NativeWidgets.h>
#include <iiGUI/Text.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
namespace lvui::native {
[[nodiscard]] inline ButtonTone dropdown_tone(ButtonTone value)noexcept{return value;}
[[nodiscard]] inline ButtonTone dropdown_tone(ii::gui::ButtonTone value)noexcept{
    switch(value){case ii::gui::ButtonTone::primary:return ButtonTone::primary;case ii::gui::ButtonTone::normal:return ButtonTone::normal;
        case ii::gui::ButtonTone::borderless:return ButtonTone::borderless;case ii::gui::ButtonTone::destructive:return ButtonTone::destructive;
        case ii::gui::ButtonTone::disabled:return ButtonTone::disabled;}
    return static_cast<ButtonTone>(-1);
}
[[nodiscard]] inline StyleColor dropdown_color(ii::gui::WidgetColor value)noexcept{return {value.red,value.green,value.blue,value.alpha};}
struct DropdownInput final {
    DropdownConfiguration configuration;
    std::shared_ptr<const ii::gui::RasterImage> icon;
    std::string icon_name;
    std::shared_ptr<const IconSource> icon_source;
    std::int64_t icon_revision=0;
    friend bool operator==(const DropdownInput&,const DropdownInput&)=default;
};
struct NativeDropdownCallbacks final {std::function<void()> clicked,pressed,released,canceled;};
class NativeDropdownButton final {
    struct Prepared final {DropdownButton view;std::shared_ptr<const ii::gui::RasterImage> icon;};
    using Snapshot=std::shared_ptr<const Prepared>;
    struct State final {
        DropdownInput authored;
        NativeIconCache icons;
        Snapshot prepared;
        std::optional<ButtonTone> forced_tone;
        ii::gui::Widget root;
        NativeDropdownCallbacks callbacks;
        const std::thread::id owner=std::this_thread::get_id();
        std::uint64_t revision=0;
        bool live=false;
    };
    std::shared_ptr<State> state_;
    explicit NativeDropdownButton(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static ii::core::Result<Snapshot> prepare(const DropdownInput& input,std::optional<ButtonTone> force,NativeIconCache& icons){
        if((input.icon&&!input.icon_name.empty())||(input.icon_source&&input.icon_name.empty())||input.icon_revision<0)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Conflicting LVUI Dropdown icon inputs");
        if(!DropdownButton::validate(input.configuration))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI Dropdown configuration");
        auto next=std::make_shared<Prepared>();auto effective=input.configuration;
        if(force&&effective.tone!=ButtonTone::disabled)effective.tone=*force;
        if(!next->view.set_configuration(std::move(effective)))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI Dropdown tone");
        next->icon=input.icon;
        if(!input.icon_name.empty()){
            auto icon=icons.resolve(input.icon_name,input.icon_source,input.configuration.icon_size);
            if(!icon)return std::unexpected(icon.error());next->icon=std::move(*icon);
        }
        return Snapshot(std::move(next));
    }
    [[nodiscard]] static ii::gui::ActionOptions action(const Snapshot& value){
        const auto& c=value->view.configuration();ii::gui::ActionOptions result;
        result.text=c.icon_mode?std::string{}:c.text;
        result.accessibility_name=c.accessibility_name.empty()?(c.text.empty()?"Menu":c.text):c.accessibility_name;
        result.enabled=c.enabled;result.focus_ring=false;result.font_size=c.font_size;result.line_height=c.line_height;
        result.padding={c.left_padding+(c.icon_mode?c.icon_size:0),0,c.right_padding+c.indicator_size+c.spacing,0};
        result.width=c.width;result.height=c.height;result.interaction_enabled=c.tone!=ButtonTone::disabled;return result;
    }
    [[nodiscard]] static ii::gui::Color linear(StyleColor c)noexcept{
        const auto channel=[](double v){return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);};
        return {float(channel(c.red)),float(channel(c.green)),float(channel(c.blue)),float(c.alpha)};
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>
    paint(const Snapshot& snapshot,const ii::gui::WidgetPaintState& input){
        try{
            const auto& c=snapshot->view.configuration();const auto visual=snapshot->view.visual({input.enabled,input.hovered,input.pressed,input.focused});
            double text_width=0,text_height=0;
            if(!c.icon_mode&&!c.text.empty()){
                auto measured=ii::gui::measure_text_layout({.text=c.text,.font_size=float(c.font_size),.font_weight=c.font_weight,
                    .font_family=input.font_family,.line_height=float(c.line_height),.elide_right=true});
                if(!measured)return std::unexpected(measured.error());text_width=std::min(8192.,measured->width);text_height=std::min(8192.,measured->height);
            }
            const auto geometry=snapshot->view.geometry(input.size.width,input.size.height,text_width,text_height);
            if(!geometry)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI Dropdown extent");
            auto result=std::make_shared<ii::gui::WidgetDrawing>();
            const auto rect=[](ButtonRect r){return ii::gui::Rect{float(r.x),float(r.y),float(r.width),float(r.height)};};
            if(geometry->surface.width>0&&geometry->surface.height>0){
                if(visual.background.alpha>0){const auto ink=linear(visual.background);result->panels.push_back({rect(geometry->surface),ink,ink,float(geometry->radius)});}
                if(visual.focused){const auto ink=linear(visual.focus);const auto stroke=float(std::min({1.5,input.size.width/2,input.size.height/2}));
                    if(stroke>0)result->panels.push_back({rect(geometry->surface),ink,ink,float(geometry->radius),stroke});}
            }
            const ii::gui::Rect clip{0,0,float(input.size.width),float(input.size.height)};
            if(geometry->label.width>0&&geometry->label.height>0&&!c.text.empty())
                result->texts.push_back({.bounds=rect(geometry->label),.text=c.text,.color=linear(visual.text),.font_size=float(c.font_size),
                    .font_weight=c.font_weight,.alignment=ii::gui::TextAlignment::left,.font_family=input.font_family,
                    .line_height=float(c.line_height),.elide_right=true,.clip=clip});
            if(geometry->icon.width>0&&geometry->icon.height>0){
                if(snapshot->icon)result->images.push_back({.bounds=rect(geometry->icon),.image=snapshot->icon,.opacity=visual.enabled?1.f:.3f});
                else{
                    // LVUI fallback: an outlined folder with three accent tiles.
                    const auto& r=geometry->icon;const auto scale=float(r.width/18);const auto x=float(r.x),y=float(r.y);
                    auto ink=linear(visual.text),accent=linear(c.palette.primary);if(!visual.enabled)accent.alpha*=.3f;
                    if(scale>0){result->panels.push_back({{x+scale,y+4*scale,14*scale,10*scale},ink,ink,scale,scale});
                        result->panels.push_back({{x+2*scale,y+2*scale,5*scale,2*scale},ink,ink,0});
                        for(auto [dx,dy]:{std::pair{10,9},std::pair{13,9},std::pair{13,12}})
                            result->panels.push_back({{x+dx*scale,y+dy*scale,2*scale,2*scale},accent,accent,.4f*scale});}
                }
            }
            if(geometry->indicator.width>0&&geometry->indicator.height>0){
                ii::gui::TextRun glyph{rect(geometry->indicator),"⌄",linear(visual.text),float(std::min(13.,geometry->indicator.height)),
                    400,ii::gui::TextAlignment::center,input.font_family};
                auto metrics=ii::gui::measure_text_layout(glyph);if(!metrics)return std::unexpected(metrics.error());
                glyph.bounds.y+=float(std::max(0.,(geometry->indicator.height-metrics->height)/2));
                glyph.bounds.height=float(std::min(geometry->indicator.height,metrics->height));glyph.clip=clip;
                if(glyph.bounds.height>0)result->texts.push_back(std::move(glyph));
            }
            return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(result));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"LVUI Dropdown paint failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<void> apply(const std::shared_ptr<State>& state,DropdownInput input,std::optional<ButtonTone> force){
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Dropdown requires its owner thread");
        try{
            if(auto active=state->root.action_options();!active)return std::unexpected(active.error());
            if(state->revision==std::numeric_limits<std::uint64_t>::max())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Dropdown revision exhausted");
            const auto observed_revision=state->revision;
            auto prepared=prepare(input,force,state->icons);if(!prepared)return std::unexpected(prepared.error());
            if(state->revision!=observed_revision)return {};
            auto previous=state->authored;auto old_snapshot=state->prepared;auto old_force=state->forced_tone;
            const auto revision=++state->revision;state->authored=std::move(input);state->prepared=std::move(*prepared);state->forced_tone=force;
            if(auto r=state->root.set_action_options(action(state->prepared));!r){
                if(state->revision==revision){state->authored=std::move(previous);state->prepared=std::move(old_snapshot);state->forced_tone=old_force;}return r;
            }
            if(state->revision!=revision)return {};
            return state->root.request_repaint();
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Dropdown update failed",std::current_exception());}
    }
public:
    [[nodiscard]] static ii::core::Result<NativeDropdownButton> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,DropdownInput input={},NativeDropdownCallbacks callbacks={}){
        try{
            auto state=std::make_shared<State>();
            auto prepared=prepare(input,std::nullopt,state->icons);if(!prepared)return std::unexpected(prepared.error());
            state->authored=std::move(input);state->prepared=std::move(*prepared);state->callbacks=std::move(callbacks);
            std::weak_ptr<State> weak=state;
            const auto event=[weak](auto member){return [weak,member]{if(auto state=weak.lock();state&&state->live){auto callback=state->callbacks.*member;if(callback)callback();}};};
            auto root=builder.action_surface(parent,id,action(state->prepared),event(&NativeDropdownCallbacks::clicked));if(!root)return std::unexpected(root.error());state->root=*root;
            if(auto r=root->set_control_events({.pressed=event(&NativeDropdownCallbacks::pressed),.released=event(&NativeDropdownCallbacks::released),.canceled=event(&NativeDropdownCallbacks::canceled)});!r)return std::unexpected(r.error());
            if(auto r=root->set_paint_handler([state](const auto& value){return paint(state->prepared,value);});!r)return std::unexpected(r.error());
            if(auto r=builder.on_publish([weak]{if(auto state=weak.lock())state->live=true;});!r)return std::unexpected(r.error());
            if(auto r=builder.on_retire([weak]{if(auto state=weak.lock())state->live=false;});!r)return std::unexpected(r.error());
            return NativeDropdownButton(std::move(state));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"LVUI Dropdown creation failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::core::Result<DropdownInput> options()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Dropdown requires its owner thread");
        if(auto active=state_->root.action_options();!active)return std::unexpected(active.error());
        try{return state_->authored;}catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Dropdown options failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<void> set_options(DropdownInput value)const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Dropdown requires its owner thread");
        return apply(state_,std::move(value),state_->forced_tone);
    }
    [[nodiscard]] BorderlessTonePort weak_borderless_tone_port()const{
        std::weak_ptr<State> weak=state_;
        return {.apply=[weak](bool force)->ii::core::Result<void>{auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Dropdown has retired");
                    if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Dropdown requires its owner thread");
                    return apply(state,state->authored,force?std::optional<ButtonTone>{ButtonTone::borderless}:std::nullopt);},
                .retired=[weak]{auto state=weak.lock();if(!state)return true;if(state->owner!=std::this_thread::get_id())return false;
                    auto current=state->root.snapshot();return !current&&current.error().code==ii::core::ErrorCode::closed;}};
    }
};
} // namespace lvui::native
