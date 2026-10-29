#pragma once
#include "RadioButton.h"
#include "RadioGroup.h"
#include <iiGUI/NativeWidgets.h>
#include <iiGUI/Text.h>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace lvui::native {
inline StyleColor radio_color(ii::gui::WidgetColor value)noexcept{return {value.red,value.green,value.blue,value.alpha};}
inline Motion radio_motion_duration(Motion value,double milliseconds)noexcept{
    if(!std::isfinite(milliseconds)||milliseconds<0||milliseconds>86400000||std::floor(milliseconds)!=milliseconds)
        value.duration=std::chrono::milliseconds{-1};
    else value.duration=std::chrono::milliseconds{static_cast<std::int64_t>(milliseconds)};
    return value;
}

class NativeRadioButton final {
    struct State final:RadioGroupMember {
        RadioButton view;
        ii::gui::Widget root;
        std::shared_ptr<RadioGroups> groups;
        std::function<void(bool)> toggled;
        ii::gui::WidgetFrameHandler frame;
        bool live=false,last_input_checked=false,host_checked=false,dirty_paint=true;
        std::uint64_t revision=0;
        [[nodiscard]] std::string_view group()const noexcept override{return view.configuration().group;}
        [[nodiscard]] bool selected()const noexcept override{return view.checked();}
        [[nodiscard]] bool participating()const noexcept override{return live;}
        void assign(bool value)noexcept override{
            if(view.checked()==value)return;
            if(view.set_checked(value,std::chrono::steady_clock::now())){++revision;dirty_paint=true;}
        }
        void publish()noexcept override{
            if(!live)return;
            const auto next=view.checked(),previous=host_checked;const auto version=revision;
            try{
                if(next!=host_checked){
                    host_checked=next;
                    if(auto changed=root.set_checked(next);!changed){if(version==revision)host_checked=previous;return;}
                }
                if(!live||revision!=version)return;
                if(!dirty_paint)return;
                if(view.animating()){
                    if(auto started=root.request_frames();!started){view.finish_animation();(void)root.set_frame_handler(frame,false);}
                }else (void)root.set_frame_handler(frame,false);
                if(!live||revision!=version)return;
                if(root.request_repaint()&&revision==version)dirty_paint=false;
            }catch(...){}
        }
    };
    std::shared_ptr<State> state_;
    explicit NativeRadioButton(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static ii::gui::ActionOptions action_options(const RadioButton& view){
        const auto& c=view.configuration();const auto height=std::max(c.indicator_size,c.text.empty()?0.0:(c.line_height>0?c.line_height:c.font_size));
        return {.text=c.text,.accessibility_name=c.accessibility_name,.enabled=c.enabled,.focus_ring=false,
                .role=ii::gui::ActionRole::radio_button,.text_role=ii::gui::TextRole::body,
                .font_size=c.font_size,.line_height=c.line_height,
                .padding={c.indicator_size+(c.text.empty()?0:c.label_gap),0,0,0},
                .minimum_width=c.indicator_size,.height=height,.checked=c.checked};
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>
    paint(const std::shared_ptr<State>& state,const ii::gui::WidgetPaintState& input){
        const auto& c=state->view.configuration();const auto visual=state->view.visual({input.enabled,input.focused});
        double text_width=0,text_height=0;
        if(!c.text.empty()){
            auto measured=ii::gui::measure_text_layout({.text=c.text,.font_size=float(c.font_size),.font_weight=c.font_weight,
                .font_family=input.font_family,.line_height=float(c.line_height),.elide_right=true});
            if(!measured)return std::unexpected(measured.error());text_width=measured->width;text_height=measured->height;
        }
        const auto geometry=state->view.geometry(input.size.width,input.size.height,text_width,text_height);
        if(!geometry)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI RadioButton extent");
        const auto rect=[](RadioButtonRect v){return ii::gui::Rect{float(v.x),float(v.y),float(v.width),float(v.height)};};
        const auto linear=[](StyleColor v){const auto channel=[](double x){return x<=.04045?x/12.92:std::pow((x+.055)/1.055,2.4);};
            return ii::gui::Color{float(channel(v.red)),float(channel(v.green)),float(channel(v.blue)),float(v.alpha)};};
        auto drawing=std::make_shared<ii::gui::WidgetDrawing>();
        if(geometry->draw_indicator){const auto color=linear(visual.indicator);drawing->panels.push_back({rect(geometry->indicator),color,color,float(geometry->indicator_radius)});
            if(visual.focused){const auto focus=linear(visual.focus);const auto width=std::min(1.5,geometry->indicator.width/2);
                if(width>0)drawing->panels.push_back({rect(geometry->indicator),focus,focus,float(geometry->indicator_radius),float(width)});}}
        if(visual.draw_dot&&geometry->draw_dot){const auto dot=linear(visual.dot);drawing->panels.push_back({rect(geometry->dot),dot,dot,float(geometry->dot_radius)});}
        if(geometry->draw_text)drawing->texts.push_back({.bounds=rect(geometry->text),.text=c.text,.color=linear(visual.text),
            .font_size=float(c.font_size),.font_weight=c.font_weight,.alignment=ii::gui::TextAlignment::left,
            .font_family=input.font_family,.line_height=float(c.line_height),.elide_right=true,
            .clip=ii::gui::Rect{0,0,float(input.size.width),float(input.size.height)}});
        return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
    }
    [[nodiscard]] static ii::core::Result<void> activate(const std::shared_ptr<State>& state){
        if(!state->live||!state->view.configuration().enabled||state->view.checked())return {};
        if(auto current=state->root.action_options();!current)return std::unexpected(current.error());
        if(!state->groups->select(state,true))return ii::core::fail(ii::core::ErrorCode::busy,"Radio group did not settle");
        if(state->live&&state->view.checked()&&state->toggled){auto callback=state->toggled;callback(true);}
        return {};
    }
public:
    [[nodiscard]] static ii::core::Result<NativeRadioButton> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,std::shared_ptr<RadioGroups> groups,const RadioButtonConfiguration& configuration,
            std::function<void(bool)> toggled={}){
        if(!groups)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"RadioButton requires a group owner");
        auto state=std::make_shared<State>();state->groups=std::move(groups);
        if(!state->view.set_configuration(configuration,std::chrono::steady_clock::now()))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI RadioButton configuration");
        state->last_input_checked=state->host_checked=configuration.checked;state->toggled=std::move(toggled);
        std::weak_ptr<State> weak=state;
        auto root=builder.action_surface(parent,id,action_options(state->view),[weak]{if(auto owner=weak.lock()){
            auto result=activate(owner);if(!result)throw std::runtime_error(result.error().message);
        }});
        if(!root)return std::unexpected(root.error());state->root=*root;
        state->frame=[weak](ii::runtime::Clock::time_point now)->ii::core::Result<bool>{
            auto owner=weak.lock();if(!owner||!owner->live)return false;const bool active=owner->view.advance(now);
            if(auto painted=owner->root.request_repaint();!painted)return std::unexpected(painted.error());return active;
        };
        if(auto installed=root->set_frame_handler(state->frame,false);!installed)return std::unexpected(installed.error());
        if(auto installed=root->set_paint_handler([state](const ii::gui::WidgetPaintState& value){return paint(state,value);});!installed)return std::unexpected(installed.error());
        if(!state->groups->prepare(state))return ii::core::fail(ii::core::ErrorCode::full,"Radio group membership capacity exhausted");
        if(auto registered=builder.on_retire([weak]{if(auto owner=weak.lock()){owner->live=false;owner->groups->retire(owner);}});!registered)return std::unexpected(registered.error());
        if(auto registered=builder.on_publish([weak]{if(auto owner=weak.lock()){owner->live=true;(void)owner->groups->refresh(owner);}});!registered)return std::unexpected(registered.error());
        return NativeRadioButton(std::move(state));
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::core::Result<void> set_options(RadioButtonConfiguration value)const{
        if(!RadioButton::validate(value))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI RadioButton configuration");
        if(auto current=state_->root.action_options();!current)return std::unexpected(current.error());
        const bool incoming=value.checked;if(incoming==state_->last_input_checked)value.checked=state_->view.checked();
        auto next=state_->view;
        if(!next.set_configuration(std::move(value),std::chrono::steady_clock::now()))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI RadioButton configuration");
        state_->view=std::move(next);state_->last_input_checked=incoming;state_->dirty_paint=true;++state_->revision;
        if(state_->live&&!state_->groups->refresh(state_))return ii::core::fail(ii::core::ErrorCode::busy,"Radio group did not settle");
        // A group notification can select this member again or change its
        // options. Publish the latest retained model, not the older candidate.
        const auto revision=state_->revision;
        if(auto changed=state_->root.set_action_options(action_options(state_->view));!changed)return changed;
        if(state_->revision!=revision)return {};
        state_->host_checked=state_->view.checked();state_->publish();return {};
    }
};
} // namespace lvui::native
