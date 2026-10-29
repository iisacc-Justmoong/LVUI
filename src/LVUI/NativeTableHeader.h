#pragma once

#include "NativeVisual.h"
#include "TableHeader.h"

#include <iiGUI/NativeWidgets.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace lvui::native {

[[nodiscard]] inline StyleColor table_header_color(ii::gui::WidgetColor value)noexcept{return {value.red,value.green,value.blue,value.alpha};}
[[nodiscard]] inline std::vector<TableColumn> table_header_columns(std::vector<TableColumn> value){return value;}
[[nodiscard]] inline std::vector<TableColumn> table_header_columns(const std::vector<std::string>& values){
    std::vector<TableColumn> columns;columns.reserve(values.size());
    for(std::size_t i=0;i<values.size();++i)columns.push_back({std::to_string(i),values[i]});return columns;
}
template<std::ranges::input_range Range>
void table_header_widths(std::vector<TableColumn>& columns,const Range& values){
    std::size_t index=0;
    for(const auto value:values){
        if(index>=columns.size())throw std::invalid_argument("More column widths than header columns");
        const auto width=double(value);
        if(!std::isfinite(width)||width<0||width>8192||(width>0&&width<32))throw std::invalid_argument("Invalid header column width");
        columns[index++].width=width;
    }
}
struct TableHeaderInput final {
    TableHeaderConfiguration configuration;
    double offset=0;
    friend bool operator==(const TableHeaderInput&,const TableHeaderInput&)=default;
};
struct NativeTableHeaderCallbacks final {std::function<void(std::size_t,const TableColumn&)> column_clicked;};

class NativeTableHeader final {
    struct Prepared final {TableHeaderInput authored;TableHeader model;TableViewportFrame frame;};
    using Snapshot=std::shared_ptr<const Prepared>;
    struct State final {
        Snapshot prepared;
        ii::gui::Widget root;
        NativeTableHeaderCallbacks callbacks;
        std::optional<std::string> focused,pressed;
        std::optional<ii::core::Error> failure;
        const std::thread::id owner=std::this_thread::get_id();
        std::uint64_t revision=0;
        bool live=false;
    };
    explicit NativeTableHeader(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static ii::core::Result<Snapshot> prepare(const TableHeaderInput& input,double width,std::optional<double> offset={}) {
        try {
            auto result=std::make_shared<Prepared>();result->authored=input;
            if(!result->model.set_configuration(input.configuration))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid table header configuration");
            auto frame=result->model.layout(width,offset.value_or(input.offset));if(!frame)return ii::core::fail(ii::core::ErrorCode::invalid_argument,frame.error().message);
            result->frame=std::move(*frame);return Snapshot(std::move(result));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Table header preparation failed",std::current_exception());}
    }
    [[nodiscard]] static ii::gui::ActionOptions action(const Snapshot& value){
        const auto& c=value->authored.configuration;
        return {.accessibility_name="Table header",.enabled=c.enabled,.focus_ring=false,.width=c.width,
                .height=c.row_height+c.separator_height,.interaction_enabled=c.interactive};
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> paint(
            const std::shared_ptr<State>& state,const ii::gui::WidgetPaintState& view){
        try {
            auto drawing=std::make_shared<ii::gui::WidgetDrawing>();const auto& prepared=*state->prepared;
            const auto& c=prepared.authored.configuration;const auto& frame=prepared.frame;
            const auto background=native_encoded_color(c.palette.background),divider=native_encoded_color(c.palette.divider),ink=native_encoded_color(c.palette.header_text);
            if(view.size.width>0&&view.size.height>0)drawing->panels.push_back({{0,0,float(view.size.width),float(view.size.height)},background,background});
            const auto budget=std::min<std::size_t>(8192,std::max<std::size_t>(4,128*1024/std::max<std::size_t>(1,frame.columns.size())));
            for(const auto& span:frame.columns){
                const auto& column=c.columns[span.index];const auto left=std::max(0.,span.offset),right=std::min(view.size.width,span.offset+span.extent);
                if(right<=left)continue;const auto pad=std::min(c.horizontal_padding,span.extent/2),available=std::max(0.,span.extent-2*pad);
                if(!column.title.empty()&&available>0)drawing->texts.push_back({.bounds={float(span.offset+pad),0,float(available),float(std::min(c.row_height,view.size.height))},
                    .text=native_display_text(column.title,budget),.color=ink,.font_size=13,.font_weight=500,.alignment=ii::gui::TextAlignment::left,
                    .font_family=view.font_family,.line_height=16,.elide_right=true,
                    .clip=ii::gui::Rect{float(left),0,float(right-left),float(std::min(c.row_height,view.size.height))}});
                const auto edge=span.offset+span.extent-1;
                if(edge>=0&&edge<view.size.width&&view.size.height>0)drawing->panels.push_back({{float(edge),0,1,float(view.size.height)},divider,divider});
                if(view.focused&&c.interactive&&c.enabled&&state->focused==column.key&&right-left>=2&&c.row_height>=2){
                    const auto focus=native_encoded_color(c.palette.current);drawing->panels.push_back({{float(left),0,float(right-left),float(std::min(c.row_height,view.size.height))},focus,focus,0,1});
                }
            }
            const auto separator=std::min(c.separator_height,view.size.height);
            if(view.size.width>0&&separator>0)drawing->panels.push_back({{0,float(view.size.height-separator),float(view.size.width),float(separator)},divider,divider});
            return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Table header paint failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<void> update(const std::shared_ptr<State>& state,TableHeaderInput input){
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Table header requires its owner thread");
        try{
            if(auto live=state->root.snapshot();!live)return std::unexpected(live.error());if(input==state->prepared->authored)return {};
            if(state->revision==std::numeric_limits<std::uint64_t>::max())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Table header revision exhausted");
            const auto observed=state->revision;auto next=prepare(input,input.configuration.width,
                input.offset==state->prepared->authored.offset?std::optional(state->prepared->frame.x):std::nullopt);if(!next)return std::unexpected(next.error());
            if(!state->live||state->revision!=observed)return {};
            auto previous=state->prepared;const auto previous_focused=state->focused;const auto revision=++state->revision;
            state->prepared=std::move(*next);state->pressed.reset();
            if(state->focused&&std::ranges::none_of(input.configuration.columns,[&](const auto& column){return column.enabled&&column.key==*state->focused;}))state->focused.reset();
            const auto rollback=[&]{if(state->revision==revision){state->prepared=previous;state->focused=previous_focused;
                (void)state->root.set_action_options(action(previous));(void)state->root.set_visible(previous->authored.configuration.visible);(void)state->root.request_repaint();}};
            if(auto result=state->root.set_action_options(action(state->prepared));!result){rollback();return result;}
            if(state->revision!=revision)return {};
            if(auto result=state->root.set_visible(input.configuration.visible);!result){rollback();return result;}
            if(state->revision!=revision)return {};
            if(auto result=state->root.request_repaint();!result){rollback();return result;}
            state->failure.reset();return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Table header update failed",std::current_exception());}
    }
    [[nodiscard]] static ii::core::Result<bool> input(const std::shared_ptr<State>& state,const ii::gui::WidgetInput& event){
        using ii::gui::WidgetInputKind;using ii::gui::WidgetKey;
        if(!state->live)return false;const auto current=state->prepared;const auto& c=current->authored.configuration;
        if(event.kind==WidgetInputKind::focus_lost||event.kind==WidgetInputKind::pointer_leave){state->pressed.reset();return false;}
        if(!c.enabled||!c.interactive||!c.visible)return false;
        const auto send=[&](std::size_t index){if(state->live&&state->prepared==current&&state->callbacks.column_clicked){
            auto callback=state->callbacks.column_clicked;auto column=c.columns[index];callback(index,column);}};
        if(event.kind==WidgetInputKind::key_down&&!event.alt&&!event.control&&!event.meta){
            std::optional<std::size_t> index;for(std::size_t i=0;i<c.columns.size();++i)if(c.columns[i].enabled&&state->focused==c.columns[i].key){index=i;break;}
            if(!index)for(std::size_t i=0;i<c.columns.size();++i)if(c.columns[i].enabled){index=i;break;}
            if(!index)return false;
            if(event.key==WidgetKey::left||event.key==WidgetKey::right||event.key==WidgetKey::home||event.key==WidgetKey::end){
                const auto direction=event.key==WidgetKey::left||event.key==WidgetKey::end?-1:1;
                auto position=event.key==WidgetKey::home?std::ptrdiff_t(-1):event.key==WidgetKey::end?std::ptrdiff_t(c.columns.size()):std::ptrdiff_t(*index);
                for(position+=direction;position>=0&&position<std::ptrdiff_t(c.columns.size());position+=direction)
                    if(c.columns[std::size_t(position)].enabled){index=std::size_t(position);break;}
                state->focused=c.columns[*index].key;
                auto revealed=current->frame.source->frame({.height=c.row_height,.header_height=0,.x=current->frame.x,
                    .overscan_rows=0,.overscan_columns=0,.reveal_column=*state->focused});
                if(!revealed)return ii::core::fail(ii::core::ErrorCode::invalid_argument,revealed.error().message);
                if(revealed->x!=current->frame.x){auto next=std::make_shared<Prepared>(*current);next->frame=std::move(*revealed);state->prepared=std::move(next);}
                if(auto result=state->root.request_repaint();!result)return std::unexpected(result.error());return true;
            }
            if(event.key==WidgetKey::enter||event.key==WidgetKey::space){state->focused=c.columns[*index].key;send(*index);return true;}
            return false;
        }
        if(event.kind!=WidgetInputKind::pointer_down&&event.kind!=WidgetInputKind::pointer_up)return false;
        std::optional<std::size_t> hit;
        if(event.local_position&&event.local_position->y>=0&&event.local_position->y<c.row_height)
            hit=current->model.hit(current->frame,event.local_position->x);
        if(event.kind==WidgetInputKind::pointer_down){
            state->pressed=hit?std::optional(c.columns[*hit].key):std::nullopt;
            if(hit){state->focused=c.columns[*hit].key;if(auto focused=state->root.focus();!focused)return std::unexpected(focused.error());}
            return true;
        }
        const auto pressed=std::exchange(state->pressed,std::nullopt);
        if(hit&&pressed&&*pressed==c.columns[*hit].key)send(*hit);return true;
    }
public:
    [[nodiscard]] static ii::core::Result<NativeTableHeader> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,std::string_view id,
            TableHeaderInput input={},NativeTableHeaderCallbacks callbacks={}){
        try{
            auto state=std::make_shared<State>();auto prepared=prepare(input,input.configuration.width);if(!prepared)return std::unexpected(prepared.error());
            state->prepared=std::move(*prepared);state->callbacks=std::move(callbacks);std::weak_ptr<State> weak=state;
            auto root=builder.action_surface(parent,id,action(state->prepared),{});if(!root)return std::unexpected(root.error());state->root=*root;
            if(auto handler=root->set_paint_handler([weak](const auto& view)->ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>{
                auto state=weak.lock();if(!state)return ii::core::fail(ii::core::ErrorCode::closed,"Table header retired");return paint(state,view);
            });!handler)return std::unexpected(handler.error());
            if(auto handler=root->set_input_handler([weak](const ii::gui::WidgetInput& event)->ii::core::Result<bool>{
                auto state=weak.lock();if(!state)return false;try{auto result=NativeTableHeader::input(state,event);if(!result)state->failure=result.error();return result;}
                catch(...){state->failure=ii::core::Error{ii::core::ErrorCode::failed,"Table header input failed",std::current_exception()};return std::unexpected(*state->failure);}
            });!handler)return std::unexpected(handler.error());
            if(auto handler=root->set_layout_handler([weak](ii::gui::Rect bounds){if(auto state=weak.lock()){
                if(bounds.width<1||bounds.width==state->prepared->frame.width)return;auto result=prepare(state->prepared->authored,bounds.width,state->prepared->frame.x);
                if(result){state->prepared=std::move(*result);(void)state->root.request_repaint();}else state->failure=result.error();
            }});!handler)return std::unexpected(handler.error());
            if(auto visible=root->set_visible(input.configuration.visible);!visible)return std::unexpected(visible.error());
            if(auto retained=builder.retain(state);!retained)return std::unexpected(retained.error());
            if(auto published=builder.on_publish([weak]{if(auto state=weak.lock())state->live=true;});!published)return std::unexpected(published.error());
            if(auto retired=builder.on_retire([weak]{if(auto state=weak.lock()){state->live=false;state->pressed.reset();}});!retired)return std::unexpected(retired.error());
            return NativeTableHeader(std::move(state));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Table header construction failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::core::Result<TableHeaderInput> options()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Table header requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());return state_->prepared->authored;
    }
    [[nodiscard]] ii::core::Result<TableViewportFrame> viewport()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Table header requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());return state_->prepared->frame;
    }
    [[nodiscard]] ii::core::Result<void> set_options(TableHeaderInput value)const{return update(state_,std::move(value));}
    [[nodiscard]] ii::core::Result<std::optional<ii::core::Error>> failure()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Table header requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());return state_->failure;
    }
private:
    std::shared_ptr<State> state_;
};

} // namespace lvui::native
