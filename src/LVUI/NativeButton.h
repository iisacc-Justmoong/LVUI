#pragma once
#include "Button.h"
#include "NativeIconCache.h"
#include "NativeStylePort.h"
#include <iiGUI/NativeWidgets.h>
#include <iiGUI/Text.h>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>
#include <thread>
#include <utility>

namespace lvui::native {
// Application-side bridge: iiGUI provides input, focus, accessibility and text
// measurement; LVUI owns button configuration, state policy and drawing.
struct ButtonInput final {
    std::shared_ptr<const ii::gui::RasterImage> icon;
    ButtonConfiguration configuration;
    std::string icon_name;
    std::shared_ptr<const IconSource> icon_source;
    std::int64_t icon_revision=0;
    friend bool operator==(const ButtonInput&,const ButtonInput&)=default;
};
inline ButtonTone button_tone(ButtonTone value) noexcept { return value; }
inline StyleColor button_color(ii::gui::WidgetColor value) noexcept { return {value.red,value.green,value.blue,value.alpha}; }
inline ButtonTone button_tone(ii::gui::ButtonTone value) noexcept {
    switch(value){
    case ii::gui::ButtonTone::primary:return ButtonTone::primary;
    case ii::gui::ButtonTone::normal:return ButtonTone::normal;
    case ii::gui::ButtonTone::borderless:return ButtonTone::borderless;
    case ii::gui::ButtonTone::destructive:return ButtonTone::destructive;
    case ii::gui::ButtonTone::disabled:return ButtonTone::disabled;
    }
    return static_cast<ButtonTone>(-1);
}

class NativeButton final {
    struct Prepared final { Button view; std::shared_ptr<const ii::gui::RasterImage> icon; };
    using Snapshot=std::shared_ptr<const Prepared>;
    struct State final {
        ButtonInput authored;
        NativeIconCache icons;
        Snapshot prepared;
        std::optional<ButtonTone> forced_tone;
        ii::gui::Widget root;
        const std::thread::id owner=std::this_thread::get_id();
        std::uint64_t revision=0;
    };
    std::shared_ptr<State> state_;
    explicit NativeButton(std::shared_ptr<State> state):state_(std::move(state)){}

    [[nodiscard]] static ii::core::Result<Snapshot> prepare(const ButtonInput& input,NativeIconCache& icons){
        if((input.icon&&!input.icon_name.empty())||(input.icon_source&&input.icon_name.empty())||input.icon_revision<0)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Conflicting LVUI Button icon inputs");
        auto next=std::make_shared<Prepared>();
        auto value=input.configuration;value.has_icon=bool(input.icon)||!input.icon_name.empty();
        if(!next->view.set_configuration(std::move(value)))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI Button configuration");
        next->icon=input.icon;
        if(!input.icon_name.empty()){
            auto icon=icons.resolve(input.icon_name,input.icon_source,input.configuration.icon_size);
            if(!icon)return std::unexpected(icon.error());next->icon=std::move(*icon);
        }
        return Snapshot(std::move(next));
    }
    [[nodiscard]] static ii::gui::ActionOptions action_options(const Snapshot& snapshot){
        const auto& value=snapshot->view.configuration();
        const auto icon_space=value.icon_mode?value.icon_size+snapshot->view.icon_gap():0;
        return {.text=value.text,.accessibility_name=value.accessibility_name,.enabled=value.enabled,
                .focus_ring=false,.role=ii::gui::ActionRole::button,.text_role=ii::gui::TextRole::body,
                .font_size=value.font_size,.line_height=value.line_height,
                .padding={value.horizontal_padding+icon_space,0,value.horizontal_padding,0},
                .width=value.width,.height=value.height,.interaction_enabled=value.tone!=ButtonTone::disabled};
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>
    paint(const Snapshot& snapshot,const ii::gui::WidgetPaintState& state){
        const auto& value=snapshot->view.configuration();
        const auto visual=snapshot->view.visual({state.enabled,state.hovered,state.pressed,state.focused});
        double text_width=0,text_height=0;
        if(!value.text.empty()){
            auto measured=ii::gui::measure_text_layout({.text=value.text,.font_size=float(value.font_size),
                .font_weight=value.font_weight,.font_family=state.font_family,.line_height=float(value.line_height),.elide_right=true});
            if(!measured)return std::unexpected(measured.error());
            text_width=measured->width;text_height=measured->height;
        }
        const auto geometry=snapshot->view.geometry(state.size.width,state.size.height,text_width,text_height);
        if(!geometry)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI Button extent");
        const auto rect=[](ButtonRect v){return ii::gui::Rect{float(v.x),float(v.y),float(v.width),float(v.height)};};
        const auto linear=[](StyleColor v){const auto channel=[](double c){return c<=.04045?c/12.92:std::pow((c+.055)/1.055,2.4);};
            return ii::gui::Color{float(channel(v.red)),float(channel(v.green)),float(channel(v.blue)),float(v.alpha)};};
        auto drawing=std::make_shared<ii::gui::WidgetDrawing>();
        if(geometry->surface.width>0&&geometry->surface.height>0){
            if(visual.background.alpha>0){const auto color=linear(visual.background);drawing->panels.push_back({rect(geometry->surface),color,color,float(geometry->radius)});}
            if(value.border_color&&value.border_width>0){const auto color=linear(*value.border_color);
                const auto stroke=std::min({value.border_width,geometry->surface.width/2,geometry->surface.height/2});
                if(color.alpha>0&&stroke>0)drawing->panels.push_back({rect(geometry->surface),color,color,float(geometry->radius),float(stroke)});}
            if(visual.focused){const auto color=linear(visual.focus);const auto stroke=std::min({1.5,geometry->surface.width/2,geometry->surface.height/2});
                if(stroke>0)drawing->panels.push_back({rect(geometry->surface),color,color,float(geometry->radius),float(stroke)});}
        }
        if(geometry->draw_icon&&snapshot->icon)
            drawing->images.push_back({.bounds=rect(geometry->icon),.image=snapshot->icon,.opacity=visual.enabled?1.f:.3f});
        if(geometry->draw_text)
            drawing->texts.push_back({.bounds=rect(geometry->text),.text=value.text,.color=linear(visual.text),
                .font_size=float(value.font_size),.font_weight=value.font_weight,.alignment=ii::gui::TextAlignment::center,
                .font_family=state.font_family,.line_height=float(value.line_height),.elide_right=true,
                .clip=ii::gui::Rect{0,0,float(state.size.width),float(state.size.height)}});
        return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
    }
    [[nodiscard]] static ii::core::Result<void> apply(const std::shared_ptr<State>& state,ButtonInput authored,
            std::optional<ButtonTone> forced){
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Button requires its owner thread");
        try{
            if(auto current=state->root.action_options();!current)return std::unexpected(current.error());
            if(state->revision==std::numeric_limits<std::uint64_t>::max())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Button revision exhausted");
            const auto observed_revision=state->revision;
            auto valid=prepare(authored,state->icons);if(!valid)return std::unexpected(valid.error());
            auto effective=authored;
            if(forced&&effective.configuration.tone!=ButtonTone::disabled)effective.configuration.tone=*forced;
            auto prepared=prepare(effective,state->icons);if(!prepared)return std::unexpected(prepared.error());
            if(state->revision!=observed_revision)return {};
            auto previous=state->authored;auto previous_snapshot=state->prepared;auto previous_forced=state->forced_tone;
            const auto revision=++state->revision;state->authored=std::move(authored);state->prepared=std::move(*prepared);state->forced_tone=forced;
            if(auto changed=state->root.set_action_options(action_options(state->prepared));!changed){
                if(state->revision==revision){state->authored=std::move(previous);state->prepared=std::move(previous_snapshot);state->forced_tone=previous_forced;}
                return changed;
            }
            if(state->revision!=revision)return {};
            return state->root.request_repaint();
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Button update failed",std::current_exception());}
    }
public:
    [[nodiscard]] static ii::core::Result<NativeButton> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,const ButtonInput& input,ii::runtime::Task action={}){
        auto state=std::make_shared<State>();
        auto prepared=prepare(input,state->icons);if(!prepared)return std::unexpected(prepared.error());
        state->authored=input;state->prepared=std::move(*prepared);
        auto root=builder.action_surface(parent,id,action_options(state->prepared),std::move(action));
        if(!root)return std::unexpected(root.error());
        state->root=*root;
        if(auto installed=root->set_paint_handler([state](const ii::gui::WidgetPaintState& paint_state){return paint(state->prepared,paint_state);});!installed)
            return std::unexpected(installed.error());
        return NativeButton(std::move(state));
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::core::Result<void> set_options(const ButtonInput& input)const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Button requires its owner thread");
        return apply(state_,input,state_->forced_tone);
    }
    // The weak port lets a parent temporarily select its child tone while the
    // child's authored options and lifetime remain independent.
    [[nodiscard]] BorderlessTonePort weak_borderless_tone_port()const{
        std::weak_ptr<State> weak=state_;
        return {
            .apply=[weak](bool force)->ii::core::Result<void>{
                auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Button has retired");
                if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Button requires its owner thread");
                return apply(state,state->authored,force?std::optional<ButtonTone>{ButtonTone::borderless}:std::nullopt);
            },
            .retired=[weak]{auto state=weak.lock();if(!state)return true;
                if(state->owner!=std::this_thread::get_id())return false;
                auto current=state->root.snapshot();return !current&&current.error().code==ii::core::ErrorCode::closed;}
        };
    }
};
} // namespace lvui::native
