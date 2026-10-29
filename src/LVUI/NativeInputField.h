#pragma once
#include "InputField.h"
#include "NativeTextBoundary.h"
#include "NativeTextLayout.h"
#include <iiGUI/Desktop.h>
#include <iiGUI/Image.h>
#include <iiGUI/NativeWidgets.h>
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
[[nodiscard]] inline StyleColor input_field_color(ii::gui::WidgetColor value)noexcept{
    return {value.red,value.green,value.blue,value.alpha};
}
struct NativeInputFieldCallbacks final {
    std::function<void(std::string)> edited;
    std::function<void()> submitted;
    std::function<void(bool)> focus_changed;
};
struct NativeInputFieldClipboard final {
    std::function<ii::core::Result<void>(std::string)> write;
};
struct NativeInputFieldSnapshot final {
    std::string text;
    TextInputRange selection;
    std::optional<TextInputRange> marked;
    std::size_t anchor=0,caret=0;
    double horizontal_scroll=0,vertical_scroll=0;
};

class NativeInputField final {
    struct State final {
        InputFieldConfiguration options;
        TextInput input;
        NativeTextLayout layout;
        NativeInputFieldCallbacks callbacks;
        NativeInputFieldClipboard clipboard;
        ii::gui::Widget root;
        ii::gui::Size extent;
        std::string last_input_text,inherited_font;
        const std::thread::id owner=std::this_thread::get_id();
        std::uint64_t revision=0;
        double scroll_x=0,scroll_y=0;
        std::optional<double> preferred_x;
        bool dragging=false,hovered=false;
        State(InputFieldConfiguration value,TextInput text,NativeTextLayout shaped,
              NativeInputFieldCallbacks events,NativeInputFieldClipboard port,std::string font)
            :options(std::move(value)),input(std::move(text)),layout(std::move(shaped)),
             callbacks(std::move(events)),clipboard(std::move(port)),extent{options.width,options.height},last_input_text(options.text),inherited_font(std::move(font)){}
    };
    struct Viewport final {double x=0,y=0,width=0,height=0;};
    struct Scroll final {double x=0,y=0;};
    std::shared_ptr<State> state_;
    explicit NativeInputField(std::shared_ptr<State> state):state_(std::move(state)){}

    [[nodiscard]] static ii::core::Error input_error(const TextInputError& error){
        const auto code=error.code==TextInputErrorCode::too_large?ii::core::ErrorCode::out_of_range:
                        error.code==TextInputErrorCode::unavailable?ii::core::ErrorCode::failed:ii::core::ErrorCode::invalid_argument;
        return {code,error.message};
    }
    [[nodiscard]] static ii::gui::TextLineStyle line_style(const InputFieldConfiguration& value,std::string_view inherited){
        return {.font_family=value.font_family.empty()?std::string(inherited):value.font_family,.font_size=value.font_size,.line_height=value.line_height};
    }
    [[nodiscard]] static Viewport viewport(const InputFieldConfiguration& value,ii::gui::Size size,const NativeTextLayout& layout){
        const auto x=std::min(value.horizontal_padding,std::max(0.,size.width/2));
        const auto y=value.multiline?std::min(value.vertical_padding,std::max(0.,size.height/2)):
            std::max(0.,(size.height-layout.lines().front().height)/2);
        return {x,y,std::max(0.,size.width-x*2),std::max(0.,size.height-y*2)};
    }
    [[nodiscard]] static ii::core::Result<Scroll> visible_scroll(const InputFieldConfiguration& options,ii::gui::Size size,
            const TextInput& input,const NativeTextLayout& layout,Scroll previous){
        const auto area=viewport(options,size,layout);auto caret=layout.caret(input.caret());if(!caret)return std::unexpected(caret.error());
        auto x=previous.x,y=previous.y;
        if(area.width>0){if(caret->x<x)x=caret->x;else if(caret->x>x+area.width-1)x=caret->x-area.width+1;}
        if(area.height>0){if(caret->y<y)y=caret->y;else if(caret->y+caret->height>y+area.height)y=caret->y+caret->height-area.height;}
        return Scroll{std::clamp(x,0.,std::max(0.,layout.width()-area.width+1)),
                      std::clamp(y,0.,std::max(0.,layout.height()-area.height))};
    }
    [[nodiscard]] static ii::gui::Color color(StyleColor value,bool enabled=true){
        const auto linear=[](double c){return c<=.04045?c/12.92:std::pow((c+.055)/1.055,2.4);};
        return {float(linear(value.red)),float(linear(value.green)),float(linear(value.blue)),float(value.alpha*(enabled?1:.5))};
    }
    [[nodiscard]] static std::optional<ii::gui::Rect> clip(ii::gui::Rect r,Viewport bounds){
        const auto left=std::max(double(r.x),bounds.x),top=std::max(double(r.y),bounds.y);
        const auto right=std::min(double(r.x+r.width),bounds.x+bounds.width),bottom=std::min(double(r.y+r.height),bounds.y+bounds.height);
        if(right<=left||bottom<=top)return std::nullopt;
        return ii::gui::Rect{float(left),float(top),float(right-left),float(bottom-top)};
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> paint(
            const std::shared_ptr<State>& state,const ii::gui::WidgetPaintState& view){
        try {
            auto drawing=std::make_shared<ii::gui::WidgetDrawing>();const auto& options=state->options;
            const auto width=float(view.size.width),height=float(view.size.height);
            if(width<=0||height<=0)return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
            const auto fill=color(state->hovered?options.palette.hover:options.palette.background,view.enabled);
            drawing->panels.push_back({{0,0,width,height},fill,fill,float(std::min({options.corner_radius,double(width)/2,double(height)/2}))});
            if(view.focused){const auto focus=color(options.palette.focus,view.enabled);drawing->panels.push_back({{0,0,width,height},focus,focus,
                float(std::min({options.corner_radius,double(width)/2,double(height)/2})),float(std::min({1.5,double(width)/2,double(height)/2}))});}
            const auto area=viewport(options,view.size,state->layout);
            if(area.width<=0||area.height<=0)return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
            if(view.focused&&state->input.selection().length){
                auto spans=state->layout.selection(state->input.selection());if(!spans)return std::unexpected(spans.error());
                const auto selected=color(options.palette.selection,view.enabled);
                for(const auto& span:*spans){ii::gui::Rect rectangle{float(area.x+span.x-state->scroll_x),float(area.y+span.y-state->scroll_y),float(span.width),float(span.height)};
                    if(auto bounded=clip(rectangle,area))drawing->panels.push_back({*bounded,selected,selected,0});}
            }
            if(view.focused&&state->input.marked()){
                auto spans=state->layout.selection(*state->input.marked());if(!spans)return std::unexpected(spans.error());
                const auto marked=color(options.palette.focus,view.enabled);
                for(const auto& span:*spans){ii::gui::Rect rectangle{float(area.x+span.x-state->scroll_x),float(area.y+span.y+span.height-1-state->scroll_y),float(span.width),1};
                    if(auto bounded=clip(rectangle,area))drawing->panels.push_back({*bounded,marked,marked,0});}
            }
            if(view.focused&&!options.read_only){auto caret=state->layout.caret(state->input.caret());if(!caret)return std::unexpected(caret.error());
                ii::gui::Rect rectangle{float(area.x+caret->x-state->scroll_x),float(area.y+caret->y-state->scroll_y),1,float(caret->height)};
                if(auto bounded=clip(rectangle,area)){const auto ink=color(options.palette.caret,view.enabled);drawing->panels.push_back({*bounded,ink,ink,0});}}
            if(state->input.text().empty()&&!options.placeholder.empty()){
                const auto line_height=state->layout.lines().front().height;
                ii::gui::TextRun run{{float(area.x),float(area.y),float(area.width),float(std::min(area.height,line_height))},options.placeholder,
                    color(options.palette.placeholder,view.enabled),float(options.font_size),400,ii::gui::TextAlignment::left,options.font_family.empty()?state->inherited_font:options.font_family};
                run.line_height=float(options.line_height);run.clip=ii::gui::Rect{float(area.x),float(area.y),float(area.width),float(area.height)};
                drawing->texts.push_back(std::move(run));
            }else{
                const auto ink=color(options.palette.text,view.enabled);
                for(std::size_t i=0;i<state->layout.lines().size();++i){
                    const auto& line=state->layout.lines()[i];const auto y=area.y+line.y-state->scroll_y;
                    const auto top=std::max(y,area.y),bottom=std::min(y+line.height,area.y+area.height);
                    if(bottom<=top||state->layout.display_text(i).empty())continue;
                    auto bitmap=state->layout.render_line(i,state->scroll_x,area.width,ink,view.backing_scale);if(!bitmap)return std::unexpected(bitmap.error());
                    auto image=ii::gui::rgba_image(bitmap->width,bitmap->height,std::move(bitmap->rgba));if(!image)return std::unexpected(image.error());
                    ii::gui::ImageRun run;run.bounds={float(area.x),float(top),float(area.width),float(bottom-top)};run.image=std::move(*image);
                    run.source={0,float((top-y)/line.height),1,float((bottom-top)/line.height)};drawing->images.push_back(std::move(run));
                }
            }
            return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"LVUI input field paint failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<std::size_t> position(const State& state,double x,double y){
        const auto area=viewport(state.options,state.extent,state.layout);
        return state.layout.hit_test(x-area.x+state.scroll_x,y-area.y+state.scroll_y);
    }
    [[nodiscard]] static ii::core::Result<ii::gui::TextInputState> text_state(const State& state){
        const auto area=viewport(state.options,state.extent,state.layout);auto caret=state.layout.caret(state.input.caret());
        if(!caret)return std::unexpected(caret.error());
        const auto selected=state.input.selection();const auto marked=state.input.marked();
        ii::gui::TextInputState value;value.length=state.input.length();value.selection={selected.location,selected.length};
        if(marked)value.marked=ii::gui::TextRange{marked->location,marked->length};
        const ii::gui::Rect rectangle{float(area.x+caret->x-state.scroll_x),float(area.y+caret->y-state.scroll_y),1,float(caret->height)};
        value.caret=clip(rectangle,area).value_or(ii::gui::Rect{float(area.x),float(area.y),0,0});
        value.read_only=state.options.read_only;value.multiline=state.options.multiline;value.password=state.options.password;return value;
    }
    template<class F> [[nodiscard]] static ii::core::Result<void> mutate(const std::shared_ptr<State>& state,F&& operation,bool emit_edit){
        try {
            auto next=state->input;auto result=operation(next);if(!result)return std::unexpected(input_error(result.error()));
            const bool text_changed=next.text()!=state->input.text();
            const bool committed_changed=emit_edit&&next.committed_text()!=state->input.committed_text();
            const bool changed=text_changed||next.anchor()!=state->input.anchor()||next.caret()!=state->input.caret()||next.marked()!=state->input.marked();
            if(!changed&&!committed_changed)return {};
            auto shaped=state->layout;
            if(text_changed){auto layout=NativeTextLayout::create(next.text(),line_style(state->options,state->inherited_font),state->options.password);
                if(!layout)return std::unexpected(layout.error());shaped=std::move(*layout);}
            auto scroll=visible_scroll(state->options,state->extent,next,shaped,{state->scroll_x,state->scroll_y});if(!scroll)return std::unexpected(scroll.error());
            if(state->revision==std::numeric_limits<std::uint64_t>::max())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Input field revision exhausted");
            const auto revision=++state->revision;state->input=std::move(next);state->layout=std::move(shaped);
            state->scroll_x=scroll->x;state->scroll_y=scroll->y;
            if(auto published=state->root.notify_text_input_changed(text_changed||committed_changed);!published)return published;
            if(state->revision==revision&&committed_changed&&state->callbacks.edited){auto callback=state->callbacks.edited;callback(state->input.text());}
            return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"LVUI input field update failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<bool> input(const std::shared_ptr<State>& state,const ii::gui::WidgetInput& event){
        using Kind=ii::gui::WidgetInputKind;using Key=ii::gui::WidgetKey;
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Input field requires its owner thread");
        try {
            if(event.kind==Kind::focus_lost){state->dragging=false;state->hovered=false;return false;}
            if(!state->options.enabled)return false;
            if(event.kind==Kind::pointer_leave){state->hovered=false;if(auto r=state->root.request_repaint();!r)return std::unexpected(r.error());return false;}
            if(event.kind==Kind::pointer_move&&!state->dragging){state->hovered=true;if(auto r=state->root.request_repaint();!r)return std::unexpected(r.error());return false;}
            if(event.kind==Kind::pointer_down||(state->dragging&&(event.kind==Kind::pointer_move||event.kind==Kind::pointer_up))){
                const auto revision=state->revision;
                if(event.kind==Kind::pointer_down){if(auto focused=state->root.focus();!focused)return std::unexpected(focused.error());}
                if(state->revision!=revision)return true;
                if(!event.local_position)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Input field needs local pointer coordinates");
                auto where=position(*state,event.local_position->x,event.local_position->y);if(!where)return std::unexpected(where.error());
                const auto anchor=event.kind==Kind::pointer_down&&!event.shift?*where:state->input.anchor();
                state->dragging=event.kind!=Kind::pointer_up;state->hovered=true;state->preferred_x.reset();
                if(auto r=mutate(state,[&](TextInput& next){return next.set_selection(anchor,*where);},false);!r)return std::unexpected(r.error());
                return true;
            }
            if(event.kind==Kind::composition){
                if(state->options.read_only)return true;
                const auto range=event.replacement_range?std::optional<TextInputRange>{{event.replacement_range->location,event.replacement_range->length}}:std::nullopt;
                const auto selection=event.composition_selection?std::optional<TextInputRange>{{event.composition_selection->location,event.composition_selection->length}}:std::nullopt;
                auto r=mutate(state,[&](TextInput& next){return event.text.empty()?next.cancel_composition():next.set_composition(event.text,range,selection);},false);
                if(!r)return std::unexpected(r.error());return true;
            }
            if(event.kind==Kind::commit_composition){
                auto r=mutate(state,[](TextInput& next){return next.commit_composition();},true);
                if(!r)return std::unexpected(r.error());return true;
            }
            if(event.kind==Kind::text){
                if(state->options.read_only)return true;
                const auto range=event.replacement_range?std::optional<TextInputRange>{{event.replacement_range->location,event.replacement_range->length}}:std::nullopt;
                if(event.content&&event.content->text.empty())return true;
                const auto& text=event.content?event.content->text:event.text;
                auto r=mutate(state,[&](TextInput& next){return next.insert(text,range);},true);
                if(!r)return std::unexpected(r.error());state->preferred_x.reset();return true;
            }
            if(event.kind!=Kind::key_down)return false;
            if(event.key==Key::copy||event.key==Key::cut){
                if(state->options.password||!state->input.selection().length)return true;
                auto selected=state->input.read(state->input.selection());if(!selected)return std::unexpected(input_error(selected.error()));
                const auto revision=state->revision;
                auto copied=state->clipboard.write?state->clipboard.write(std::move(*selected)):
                    ii::gui::write_clipboard({.text=std::move(*selected)});
                if(!copied)return std::unexpected(copied.error());
                if(state->revision!=revision||event.key==Key::copy||state->options.read_only)return true;
                auto r=mutate(state,[](TextInput& next){return next.insert({});},true);if(!r)return std::unexpected(r.error());return true;
            }
            if(event.key==Key::enter&&(!state->options.multiline||event.control||event.meta)){
                if(state->callbacks.submitted){auto callback=state->callbacks.submitted;callback();}return true;
            }
            if(event.key==Key::select_all){state->preferred_x.reset();auto r=mutate(state,[](TextInput& next){return next.set_selection(0,next.length());},false);if(!r)return std::unexpected(r.error());return true;}
            if(event.key==Key::left||event.key==Key::right){state->preferred_x.reset();auto r=mutate(state,[&](TextInput& next){return next.move(event.key==Key::left?-1:1,event.shift,event.alt||event.control);},false);if(!r)return std::unexpected(r.error());return true;}
            if(event.key==Key::home||event.key==Key::end){
                state->preferred_x.reset();const auto caret=state->input.caret();const auto& lines=state->layout.lines();
                auto it=std::upper_bound(lines.begin(),lines.end(),caret,[](auto value,const auto& line){return value<line.first;});
                const auto& line=it==lines.begin()?lines.front():*std::prev(it);
                const auto target=(event.control||event.meta)?(event.key==Key::home?std::size_t(0):state->input.length()):(event.key==Key::home?line.first:line.last);
                auto r=mutate(state,[&](TextInput& next){return next.set_selection(event.shift?next.anchor():target,target);},false);if(!r)return std::unexpected(r.error());return true;
            }
            if(event.key==Key::up||event.key==Key::down){
                auto caret=state->layout.caret(state->input.caret());if(!caret)return std::unexpected(caret.error());
                if(!state->preferred_x)state->preferred_x=caret->x;
                auto target=state->layout.hit_test(*state->preferred_x,event.key==Key::up?caret->y-1:caret->y+caret->height+1);
                if(!target)return std::unexpected(target.error());
                auto r=mutate(state,[&](TextInput& next){return next.set_selection(event.shift?next.anchor():*target,*target);},false);if(!r)return std::unexpected(r.error());return true;
            }
            if(event.key==Key::backspace||event.key==Key::delete_forward||event.key==Key::enter){
                if(state->options.read_only)return true;state->preferred_x.reset();
                auto r=mutate(state,[&](TextInput& next)->TextInputResult<bool>{
                    bool committed=false;if(next.marked()){auto done=next.commit_composition();if(!done)return std::unexpected(done.error());committed=*done;}
                    auto changed=(event.key==Key::backspace||event.key==Key::delete_forward)?next.erase(event.key==Key::backspace?-1:1,event.alt||event.control):next.insert("\n");
                    if(!changed)return std::unexpected(changed.error());return committed||*changed;
                },true);if(!r)return std::unexpected(r.error());return true;
            }
            return false;
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"LVUI input field event failed",std::current_exception());}
    }
public:
    [[nodiscard]] static ii::core::Result<NativeInputField> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,InputFieldConfiguration options={},NativeInputFieldCallbacks callbacks={},NativeInputFieldClipboard clipboard={}){
        if(!valid_input_field_configuration(options))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI input field configuration");
        try {
            auto appearance=builder.appearance();if(!appearance)return std::unexpected(appearance.error());
            const auto inherited_font=appearance->font_family;
            auto input=TextInput::create(native_text_boundary,options.text);if(!input)return std::unexpected(input_error(input.error()));
            auto layout=NativeTextLayout::create(input->text(),line_style(options,inherited_font),options.password);if(!layout)return std::unexpected(layout.error());
            auto state=std::make_shared<State>(std::move(options),std::move(*input),std::move(*layout),std::move(callbacks),std::move(clipboard),inherited_font);
            auto root=builder.absolute_stack(parent,id,{state->options.width,state->options.height});if(!root)return std::unexpected(root.error());state->root=*root;
            if(auto r=root->set_input_handler([state](const auto& event){return NativeInputField::input(state,event);});!r)return std::unexpected(r.error());
            auto provider=std::make_shared<ii::gui::TextInputProvider>();std::weak_ptr<State> weak=state;
            provider->state=[weak]()->ii::core::Result<ii::gui::TextInputState>{auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Input field has retired");return text_state(*state);};
            provider->read=[weak](ii::gui::TextRange range)->ii::core::Result<ii::gui::TextContent>{auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Input field has retired");
                auto text=state->input.read({range.location,range.length});if(!text)return std::unexpected(input_error(text.error()));return ii::gui::TextContent{range,std::move(*text)};};
            provider->position=[weak](double x,double y)->ii::core::Result<std::size_t>{auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Input field has retired");return position(*state,x,y);};
            provider->focus_changed=[weak](bool focused)->ii::core::Result<void>{
                auto state=weak.lock();if(!state)return {};state->dragging=false;
                if(!focused&&state->input.marked()){
                    if(auto result=mutate(state,[](TextInput& next){return next.commit_composition();},true);!result)return result;
                    if(!state->root.snapshot())return {};
                }
                if(state->callbacks.focus_changed){auto callback=state->callbacks.focus_changed;callback(focused);}
                return {};
            };
            provider->set_text=[weak](std::string_view text)->ii::core::Result<void>{auto state=weak.lock();
                if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Input field has retired");
                return NativeInputField(std::move(state)).set_text(std::string(text));};
            if(auto r=root->set_text_input_provider(std::move(provider));!r)return std::unexpected(r.error());
            if(auto r=root->set_accessibility_name(state->options.accessibility_name);!r)return std::unexpected(r.error());
            if(auto r=root->set_enabled(state->options.enabled);!r)return std::unexpected(r.error());
            if(auto r=root->set_layout_handler([weak](ii::gui::Rect rect){if(auto state=weak.lock()){
                state->extent={rect.width,rect.height};auto scroll=visible_scroll(state->options,state->extent,state->input,state->layout,{state->scroll_x,state->scroll_y});
                if(scroll){state->scroll_x=scroll->x;state->scroll_y=scroll->y;}}});!r)return std::unexpected(r.error());
            if(auto r=root->set_paint_handler([state](const auto& value){return paint(state,value);});!r)return std::unexpected(r.error());
            auto scroll=visible_scroll(state->options,state->extent,state->input,state->layout,{});if(!scroll)return std::unexpected(scroll.error());
            state->scroll_x=scroll->x;state->scroll_y=scroll->y;return NativeInputField(std::move(state));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"LVUI input field construction failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::core::Result<NativeInputFieldSnapshot> snapshot()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Input field requires its owner thread");
        if(auto active=state_->root.snapshot();!active)return std::unexpected(active.error());
        try{return NativeInputFieldSnapshot{state_->input.text(),state_->input.selection(),state_->input.marked(),state_->input.anchor(),state_->input.caret(),state_->scroll_x,state_->scroll_y};}
        catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Input field snapshot failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<InputFieldConfiguration> options()const{
        auto current=snapshot();if(!current)return std::unexpected(current.error());
        try{auto value=state_->options;value.text=std::move(current->text);return value;}
        catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Input field options failed",std::current_exception());}
    }
    [[nodiscard]] ii::core::Result<void> set_selection(std::size_t anchor,std::size_t caret)const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Input field requires its owner thread");
        if(auto active=state_->root.snapshot();!active)return std::unexpected(active.error());
        return mutate(state_,[&](TextInput& next){return next.set_selection(anchor,caret);},false);
    }
private:
    [[nodiscard]] ii::core::Result<void> apply_options(InputFieldConfiguration options,bool force_text)const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Input field requires its owner thread");
        if(!valid_input_field_configuration(options))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid LVUI input field configuration");
        try {
            if(auto active=state_->root.snapshot();!active)return std::unexpected(active.error());
            auto next=state_->input;const auto incoming=options.text;
            if(force_text||incoming!=state_->last_input_text){auto changed=next.set_text(incoming);if(!changed)return std::unexpected(input_error(changed.error()));}
            auto shaped=NativeTextLayout::create(next.text(),line_style(options,state_->inherited_font),options.password);if(!shaped)return std::unexpected(shaped.error());
            const ii::gui::Size extent{options.width,options.height};auto scroll=visible_scroll(options,extent,next,*shaped,{state_->scroll_x,state_->scroll_y});
            if(!scroll)return std::unexpected(scroll.error());
            if(state_->revision==std::numeric_limits<std::uint64_t>::max())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Input field revision exhausted");
            const auto previous_options=state_->options;const auto previous_input=state_->input;const auto previous_layout=state_->layout;
            const auto previous_external=state_->last_input_text;const auto previous_extent=state_->extent;const auto previous_x=state_->scroll_x,previous_y=state_->scroll_y;
            const bool changed_text=next.text()!=state_->input.text();const auto revision=++state_->revision;
            state_->options=std::move(options);state_->input=std::move(next);state_->layout=std::move(*shaped);state_->last_input_text=incoming;
            state_->extent=extent;state_->scroll_x=scroll->x;state_->scroll_y=scroll->y;
            const auto rollback=[&]{if(state_->revision==revision){state_->options=previous_options;state_->input=previous_input;state_->layout=previous_layout;
                state_->last_input_text=previous_external;state_->extent=previous_extent;state_->scroll_x=previous_x;state_->scroll_y=previous_y;}};
            if(auto r=state_->root.set_extent(extent);!r){rollback();return r;}if(state_->revision!=revision)return {};
            if(auto r=state_->root.set_enabled(state_->options.enabled);!r){const bool current=state_->revision==revision;rollback();if(current)(void)state_->root.set_extent(previous_extent);return r;}if(state_->revision!=revision)return {};
            if(auto r=state_->root.set_accessibility_name(state_->options.accessibility_name);!r){const bool current=state_->revision==revision;rollback();if(current){(void)state_->root.set_extent(previous_extent);(void)state_->root.set_enabled(previous_options.enabled);}return r;}
            if(state_->revision!=revision)return {};
            return state_->root.notify_text_input_changed(changed_text);
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"LVUI input field options failed",std::current_exception());}
    }
public:
    [[nodiscard]] ii::core::Result<void> set_options(InputFieldConfiguration options)const{return apply_options(std::move(options),false);}
    [[nodiscard]] ii::core::Result<void> set_text(std::string text)const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Input field requires its owner thread");
        try{auto options=state_->options;options.text=std::move(text);return apply_options(std::move(options),true);}
        catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"LVUI input field text update failed",std::current_exception());}
    }
};
} // namespace lvui::native
