#pragma once
#include "NativeColors.h"
#include "NativeBindings.h"
#include <iiGUI/NativeWidgets.h>
#include <thread>

namespace lvui::native {
using WidgetPanelColors=PanelColors<ii::gui::WidgetColor>;
inline void validate_panel_colors(const WidgetPanelColors& value){
    if(value.background)(void)color_channels(*value.background);
    if(value.border)(void)color_channels(*value.border);
    if(!std::isfinite(value.width)||!std::isfinite(value.radius)||value.width<0||value.radius<0||value.width>8192||value.radius>8192||(value.width&&!value.border))
        throw std::invalid_argument("Panel Color geometry requires valid bounds and a border color");
}
inline ii::gui::Color linear_widget_color(ii::gui::WidgetColor value){
    (void)color_channels(value);
    return {static_cast<float>(linear_color_channel(value.red)),static_cast<float>(linear_color_channel(value.green)),static_cast<float>(linear_color_channel(value.blue)),static_cast<float>(value.alpha)};
}
inline ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> panel_color_drawing(const ii::gui::WidgetPaintState& state,const WidgetPanelColors& value){
    try{
        validate_panel_colors(value);
        if(!std::isfinite(state.size.width)||!std::isfinite(state.size.height)||state.size.width<0||state.size.height<0||state.size.width>32767||state.size.height>32767)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid panel paint extent");
        auto drawing=std::make_shared<ii::gui::WidgetDrawing>();
        const ii::gui::Rect bounds{0,0,static_cast<float>(state.size.width),static_cast<float>(state.size.height)};
        if(value.background){const auto c=linear_widget_color(*value.background);drawing->panels.push_back({bounds,c,c,static_cast<float>(value.radius)});}
        if(value.border&&value.width){const auto c=linear_widget_color(*value.border);drawing->panels.push_back({bounds,c,c,static_cast<float>(value.radius),static_cast<float>(value.width)});}
        return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
    }catch(const std::invalid_argument&){return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid panel colors",std::current_exception());}
    catch(const std::domain_error&){return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid panel channels",std::current_exception());}
    catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Panel drawing failed",std::current_exception());}
}
class PanelColorPainter final {
public:
    void set_colors(const WidgetPanelColors& value){check();validate_panel_colors(value);if(value_==value)return;value_=value;cached_.reset();}
    ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> operator()(const ii::gui::WidgetPaintState& state){
        if(owner_!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Panel painter requires its UI domain");
        if(cached_&&size_==state.size)return cached_;
        auto next=panel_color_drawing(state,value_);if(next){cached_=*next;size_=state.size;}return next;
    }
private:
    void check()const{if(owner_!=std::this_thread::get_id())throw std::logic_error("Panel color update requires its UI domain");}
    const std::thread::id owner_=std::this_thread::get_id();
    WidgetPanelColors value_;
    ii::gui::Size size_;
    std::shared_ptr<const ii::gui::WidgetDrawing> cached_;
};
// Construction-time binding. Subsequent values replace only the retained paint
// data; the provider and the native widget keep their identity.
template<class Getter> ii::core::Result<void> bind_panel_colors(Bindings& graph,ii::gui::Widget target,Getter getter){
    struct State final {PanelColorPainter painter;bool installed=false;};
    auto state=std::make_shared<State>();
    auto id=graph.bind([getter=std::move(getter)](ReadContext& context)mutable{
        auto value=std::invoke(getter,context);validate_panel_colors(value);return value;
    },[state,target](const WidgetPanelColors& value)->ii::core::Result<void>{
        state->painter.set_colors(value);
        if(!state->installed){
            auto result=target.set_paint_handler([state](const ii::gui::WidgetPaintState& paint){return state->painter(paint);});
            if(!result)return result;state->installed=true;return {};
        }
        return target.request_repaint();
    });
    if(!id)return std::unexpected(id.error());return {};
}
}
