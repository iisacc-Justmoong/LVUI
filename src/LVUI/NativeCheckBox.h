#pragma once
#include "CheckBox.h"
#include <iiGUI/Image.h>
#include <iiGUI/NativeWidgets.h>
#include <iiGUI/Text.h>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace lvui::native {
inline StyleColor checkbox_color(ii::gui::WidgetColor value)noexcept{
    return {value.red,value.green,value.blue,value.alpha};
}

class NativeCheckBox final {
    struct State final {
        CheckBox view;
        ii::gui::Widget root;
        std::shared_ptr<const ii::gui::RasterImage> mark;
        std::function<void(bool)> toggled;
        bool last_input_checked=false;
        std::uint64_t revision=0;
    };
    std::shared_ptr<State> state_;
    explicit NativeCheckBox(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::RasterImage>> mask(const CheckBox& view){
        constexpr std::uint32_t size=68;
        std::vector<std::uint8_t> rgba(size*size*4);
        for(std::uint32_t y=0;y<size;++y)for(std::uint32_t x=0;x<size;++x){
            double coverage=0;
            for(unsigned sy=0;sy<4;++sy)for(unsigned sx=0;sx<4;++sx)
                coverage+=view.mark_coverage((x+(sx+.5)/4)/size,(y+(sy+.5)/4)/size);
            const auto offset=4*(y*size+x);
            rgba[offset]=rgba[offset+1]=rgba[offset+2]=255;
            rgba[offset+3]=static_cast<std::uint8_t>(std::lround(coverage*255/16));
        }
        return ii::gui::rgba_image(size,size,std::move(rgba));
    }
    [[nodiscard]] static ii::gui::ActionOptions action_options(const CheckBox& view){
        const auto& c=view.configuration();const auto frame=c.box_size+2*c.frame_padding;
        return {.text=c.text,.accessibility_name=c.accessibility_name,.enabled=c.enabled,.focus_ring=false,
                .role=ii::gui::ActionRole::check_box,.text_role=ii::gui::TextRole::body,
                .font_size=c.font_size,.line_height=c.line_height,
                .padding={c.frame_padding+c.box_size+(c.text.empty()?0:c.label_gap),0,c.frame_padding,0},
                .minimum_width=frame,.height=frame,.checked=c.checked};
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>
    paint(const std::shared_ptr<State>& state,const ii::gui::WidgetPaintState& input){
        const auto& c=state->view.configuration();const auto visual=state->view.visual({input.enabled,input.hovered,input.pressed,input.focused});
        double text_width=0,text_height=0;
        if(!c.text.empty()){
            auto measured=ii::gui::measure_text_layout({.text=c.text,.font_size=float(c.font_size),.font_weight=c.font_weight,
                .font_family=input.font_family,.line_height=float(c.line_height),.elide_right=true});
            if(!measured)return std::unexpected(measured.error());
            text_width=measured->width;text_height=measured->height;
        }
        const auto geometry=state->view.geometry(input.size.width,input.size.height,text_width,text_height);
        if(!geometry)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI CheckBox extent");
        const auto rect=[](CheckBoxRect v){return ii::gui::Rect{float(v.x),float(v.y),float(v.width),float(v.height)};};
        const auto linear=[](StyleColor v){const auto channel=[](double x){return x<=.04045?x/12.92:std::pow((x+.055)/1.055,2.4);};
            return ii::gui::Color{float(channel(v.red)),float(channel(v.green)),float(channel(v.blue)),float(v.alpha)};};
        auto drawing=std::make_shared<ii::gui::WidgetDrawing>();drawing->images_before_text=true;
        if(geometry->draw_box){
            const auto bounds=rect(geometry->box);const auto fill=linear(visual.fill);
            drawing->panels.push_back({bounds,fill,fill,float(geometry->radius)});
            const auto stroke=std::min(visual.border_width,geometry->box.width/2);
            if(stroke>0&&visual.border.alpha>0){const auto border=linear(visual.border);drawing->panels.push_back({bounds,border,border,float(geometry->radius),float(stroke)});}
            if(visual.focused){const auto focus=linear(visual.focus);const auto width=std::min(1.5,geometry->box.width/2);
                if(width>0)drawing->panels.push_back({bounds,focus,focus,float(geometry->radius),float(width)});}
            if(visual.draw_mark&&state->mark)drawing->images.push_back({.bounds=bounds,.image=state->mark,.tint=linear(visual.mark)});
        }
        if(geometry->draw_text)drawing->texts.push_back({.bounds=rect(geometry->text),.text=c.text,.color=linear(visual.text),
            .font_size=float(c.font_size),.font_weight=c.font_weight,.alignment=ii::gui::TextAlignment::left,
            .font_family=input.font_family,.line_height=float(c.line_height),.elide_right=true,
            .clip=ii::gui::Rect{0,0,float(input.size.width),float(input.size.height)}});
        return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
    }
    [[nodiscard]] static ii::core::Result<void> activate(const std::shared_ptr<State>& state){
        auto next=state->view;auto changed=next.activate();if(!changed)return {};
        const auto previous=state->view;const auto revision=++state->revision;state->view=std::move(next);
        if(auto applied=state->root.set_checked(*changed);!applied){
            if(state->revision==revision)state->view=previous;return applied;
        }
        if(state->revision!=revision)return {};
        if(auto repaint=state->root.request_repaint();!repaint)return repaint;
        if(state->revision==revision&&state->toggled){auto callback=state->toggled;callback(*changed);}
        return {};
    }
public:
    [[nodiscard]] static ii::core::Result<NativeCheckBox> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,const CheckBoxConfiguration& configuration,std::function<void(bool)> toggled={}){
        auto state=std::make_shared<State>();
        if(!state->view.set_configuration(configuration))
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI CheckBox configuration");
        auto image=mask(state->view);if(!image)return std::unexpected(image.error());state->mark=std::move(*image);
        state->last_input_checked=configuration.checked;state->toggled=std::move(toggled);
        std::weak_ptr<State> weak=state;
        auto root=builder.action_surface(parent,id,action_options(state->view),[weak]{if(auto owner=weak.lock()){
            auto result=activate(owner);if(!result)throw std::runtime_error(result.error().message);
        }});
        if(!root)return std::unexpected(root.error());state->root=*root;
        if(auto installed=root->set_paint_handler([state](const ii::gui::WidgetPaintState& value){return paint(state,value);});!installed)
            return std::unexpected(installed.error());
        return NativeCheckBox(std::move(state));
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::core::Result<void> set_options(CheckBoxConfiguration value)const{
        if(!CheckBox::validate(value))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI CheckBox configuration");
        if(auto current=state_->root.action_options();!current)return std::unexpected(current.error());
        const bool incoming=value.checked;if(incoming==state_->last_input_checked)value.checked=state_->view.checked();
        auto next=state_->view;
        if(!next.set_configuration(std::move(value)))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI CheckBox configuration");
        auto image=state_->mark;const auto& before=state_->view.configuration();const auto& after=next.configuration();
        if(before.box_size!=after.box_size||before.mark_stroke_width!=after.mark_stroke_width){
            auto generated=mask(next);if(!generated)return std::unexpected(generated.error());image=std::move(*generated);
        }
        const auto previous=state_->view;const auto previous_image=state_->mark;const auto previous_input=state_->last_input_checked;
        const auto revision=++state_->revision;state_->view=std::move(next);state_->mark=std::move(image);state_->last_input_checked=incoming;
        if(auto changed=state_->root.set_action_options(action_options(state_->view));!changed){
            if(state_->revision==revision){state_->view=previous;state_->mark=previous_image;state_->last_input_checked=previous_input;}return changed;
        }
        if(state_->revision!=revision)return {};
        return state_->root.request_repaint();
    }
};
} // namespace lvui::native
