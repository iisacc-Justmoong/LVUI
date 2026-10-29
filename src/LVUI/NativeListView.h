#pragma once

#include "ListViewport.h"
#include "NativeListItemDrawing.h"
#include "NativeIconCache.h"
#include "StyleTokens.h"

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
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace lvui::native {

[[nodiscard]] inline StyleColor list_view_color(ii::gui::WidgetColor value)noexcept{return {value.red,value.green,value.blue,value.alpha};}

using ListViewPalette = ListItemPalette;
struct ListViewInput final {
    std::shared_ptr<ListController> controller;
    std::shared_ptr<const IconSource> icon_source;
    std::int64_t source_revision=0,icon_revision=0;
    double width=200,height=140,row_height=20,left_padding=4,right_padding=4,icon_size=14,spacing=6,corner_radius=4;
    std::size_t overscan=1;
    bool enabled=true,interactive=true,keyboard_navigation=true,show_indicator=true;
    ListViewPalette palette;
    friend bool operator==(const ListViewInput&,const ListViewInput&)=default;
};
struct ListViewEvent final { ListEntry entry; std::size_t index=0; };
struct NativeListViewCallbacks final { std::function<void(const ListViewEvent&)> activated; };

class NativeListView final {
    struct RowInput final { ListItemDrawing drawing; };
    struct Row final {
        std::string key;
        NativeIconCache icons;
        std::shared_ptr<const RowInput> input;
        ii::gui::Widget widget;
    };
    struct State final {
        ListViewInput authored;
        std::shared_ptr<const ListViewportSource> source;
        std::unordered_map<std::string,std::shared_ptr<Row>> rows;
        ii::gui::Widget root,content;
        NativeListViewCallbacks callbacks;
        ii::gui::ScrollViewport viewport;
        std::function<void(const ii::gui::ScrollViewport&)> viewport_handler;
        std::optional<ii::gui::ScrollViewport> pending;
        ListViewportAnchor anchor;
        std::string selected;
        std::optional<ii::core::Error> failure;
        const std::thread::id owner=std::this_thread::get_id();
        std::uint64_t movement=0,revision=0;
        double committed_offset=0;
        bool live=false,viewport_seen=false,rendering=false;
    };
    explicit NativeListView(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static ii::core::Error convert(const ListError& error){
        const auto code=error.code==ListErrorCode::wrong_thread?ii::core::ErrorCode::wrong_executor:
            error.code==ListErrorCode::busy?ii::core::ErrorCode::busy:error.code==ListErrorCode::not_found?ii::core::ErrorCode::not_found:
            error.code==ListErrorCode::exhausted?ii::core::ErrorCode::out_of_range:ii::core::ErrorCode::invalid_argument;
        return {code,error.message};
    }
    [[nodiscard]] static bool valid(const ListViewInput& input)noexcept{
        const auto range=[](double n,double low,double high){return std::isfinite(n)&&n>=low&&n<=high;};
        const auto color=[&](StyleColor c){return range(c.red,0,1)&&range(c.green,0,1)&&range(c.blue,0,1)&&range(c.alpha,0,1);};
        const auto& p=input.palette;
        return input.controller&&input.source_revision>=0&&input.icon_revision>=0&&range(input.width,1,8192)&&range(input.height,1,8192)&&
            range(input.row_height,18,256)&&range(input.left_padding,0,8192)&&range(input.right_padding,0,8192)&&range(input.icon_size,0,256)&&
            range(input.spacing,0,8192)&&range(input.corner_radius,0,8192)&&input.overscan<=128&&color(p.text)&&color(p.description)&&
            color(p.disabled_text)&&color(p.idle)&&color(p.hover)&&color(p.pressed)&&color(p.selected)&&color(p.focus);
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ListViewportSource>> projection(const ListViewInput& input){
        if(!valid(input))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid flat list input");
        auto refreshed=input.controller->refresh();if(!refreshed)return std::unexpected(convert(refreshed.error()));
        auto snapshot=input.controller->snapshot();if(!snapshot)return std::unexpected(convert(snapshot.error()));
        auto source=ListViewportSource::create(std::move(*snapshot));if(!source)return std::unexpected(convert(source.error()));
        return std::make_shared<const ListViewportSource>(std::move(*source));
    }
    [[nodiscard]] static ii::gui::ScrollOptions scroll_options(const State& state){
        return {.width=state.authored.width,.height=state.authored.height,.enabled=true,.show_indicator=state.authored.show_indicator,
            .virtual_content_height=double(state.source->snapshot()->entries.size())*state.authored.row_height};
    }
    [[nodiscard]] static ii::gui::ActionOptions action_options(const RowInput& input){
        return list_item_action(input.drawing);
    }
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> paint(
            const std::shared_ptr<Row>& row,const ii::gui::WidgetPaintState& view){
        if(!row->input)return std::shared_ptr<const ii::gui::WidgetDrawing>(std::make_shared<ii::gui::WidgetDrawing>());
        return list_item_paint(row->input->drawing,view);
    }
    [[nodiscard]] static bool current(const std::shared_ptr<State>& state,const std::shared_ptr<Row>& row){
        const auto found=state->rows.find(row->key);return state->live&&found!=state->rows.end()&&found->second==row;
    }
    static void activate(const std::shared_ptr<State>& state,const std::shared_ptr<Row>& row)noexcept;
    [[nodiscard]] static ii::core::Result<std::shared_ptr<const RowInput>> prepare(const std::shared_ptr<State>& state,
            const std::shared_ptr<Row>& row,const ListEntry& item,double width){
        auto result=std::make_shared<RowInput>();ListItemConfiguration c;
        c.label=native_display_text(item.label);c.description=native_display_text(item.description);
        c.width=width;c.height=state->authored.row_height;c.left_padding=state->authored.left_padding;
        c.right_padding=state->authored.right_padding;c.icon_size=state->authored.icon_size;c.spacing=state->authored.spacing;
        c.corner_radius=state->authored.corner_radius;c.enabled=state->authored.enabled&&item.enabled;
        c.selectable=item.selectable;c.interactive=state->authored.interactive;c.selected=state->selected==item.key;c.palette=state->authored.palette;
        if(!item.icon_name.empty()&&c.icon_size>0){auto icon=row->icons.resolve(item.icon_name,state->authored.icon_source,c.icon_size);
            if(!icon)return std::unexpected(icon.error());result->drawing.icon=std::move(*icon);c.has_icon=bool(result->drawing.icon);}
        if(!result->drawing.model.set_configuration(std::move(c)))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid flat list row");
        return std::shared_ptr<const RowInput>(std::move(result));
    }
    [[nodiscard]] static ii::core::Result<void> frame(const std::shared_ptr<State>& state,const ii::gui::ScrollViewport& viewport){
        auto planned=state->source->frame({.width=double(viewport.bounds.width),.height=double(viewport.bounds.height),.offset=viewport.offset,
            .row_height=state->authored.row_height,.overscan=state->authored.overscan,.movement=state->movement,.anchor=state->anchor});
        if(!planned)return std::unexpected(convert(planned.error()));
        struct Change final {std::shared_ptr<Row> row;std::shared_ptr<const RowInput> input;};
        std::unordered_map<std::string,std::shared_ptr<Row>> next;std::vector<Change> changes;
        std::vector<ii::gui::KeyedContentItem> items;const auto snapshot=state->source->snapshot();
        next.reserve(planned->rows.size());changes.reserve(planned->rows.size());items.reserve(planned->rows.size());
        std::size_t text_bytes=0;std::weak_ptr<State> weak=state;
        for(const auto& cell:planned->rows){
            const auto& item=snapshot->entries[cell.index];auto found=state->rows.find(item.key);
            auto row=found==state->rows.end()?std::make_shared<Row>():found->second;if(row->key.empty())row->key=item.key;
            auto value=prepare(state,row,item,viewport.bounds.width);if(!value)return std::unexpected(value.error());
            const auto& configuration=(*value)->drawing.model.configuration();
            const auto bytes=configuration.label.size()+configuration.description.size();
            if(bytes>4*1024*1024-text_bytes)return ii::core::fail(ii::core::ErrorCode::out_of_range,"Visible list text exceeds its frame budget");
            text_bytes+=bytes;next.emplace(row->key,row);changes.push_back({row,*value});std::weak_ptr<Row> weak_row=row;
            items.push_back({row->key,[weak,weak_row,value=*value](ii::gui::WidgetBuilder& builder)->ii::core::Result<void>{
                auto state=weak.lock();auto row=weak_row.lock();if(!state||!row||!state->live)return ii::core::fail(ii::core::ErrorCode::closed,"Flat list row retired");
                row->input=value;
                auto widget=builder.action_surface({},"row",action_options(*value),[weak,weak_row]{if(auto state=weak.lock())if(auto row=weak_row.lock())activate(state,row);});
                if(!widget)return std::unexpected(widget.error());row->widget=*widget;
                if(auto r=widget->set_paint_handler([weak_row](const auto& paint_state)->ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>>{
                    auto row=weak_row.lock();if(!row)return ii::core::fail(ii::core::ErrorCode::closed,"Flat list row retired");return paint(row,paint_state);
                });!r)return r;
                return {};
            },ii::gui::Rect{0,float(cell.y),float(viewport.bounds.width),float(cell.height)}});
        }
        if(auto r=state->content.set_keyed_content(std::move(items),{.viewport=ii::gui::Size{double(viewport.bounds.width),double(viewport.bounds.height)}});!r)return r;
        for(auto& change:changes){
            if(!change.row->widget.snapshot())return ii::core::fail(ii::core::ErrorCode::closed,"Flat list row did not publish");
            change.row->input=std::move(change.input);
            if(auto r=change.row->widget.set_action_options(action_options(*change.row->input));!r)return r;
            if(auto r=change.row->widget.request_repaint();!r)return r;
        }
        state->rows=std::move(next);state->anchor=planned->anchor;state->committed_offset=planned->offset;
        return state->root.set_virtual_scroll_content({double(viewport.bounds.width),planned->content_height},0,planned->offset);
    }
    [[nodiscard]] static ii::core::Result<void> viewport(const std::shared_ptr<State>& state,ii::gui::ScrollViewport value){
        if(state->rendering){state->pending=value;return {};}
        for(unsigned pass=0;pass<8;++pass){
            if(state->viewport_seen&&std::abs(value.offset-state->committed_offset)>1e-6){
                if(state->movement==std::numeric_limits<std::uint64_t>::max())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Flat list movement exhausted");++state->movement;
            }
            state->viewport=value;state->viewport_seen=true;state->pending.reset();state->rendering=true;
            ii::core::Result<void> result;try{result=frame(state,value);}catch(...){result=ii::core::fail(ii::core::ErrorCode::failed,"Flat list viewport failed",std::current_exception());}
            state->rendering=false;if(!result)return result;if(!state->pending)return {};value=*state->pending;state->pending.reset();
        }
        return ii::core::fail(ii::core::ErrorCode::busy,"Flat list viewport did not settle");
    }
    [[nodiscard]] static ii::core::Result<void> refresh(const std::shared_ptr<State>& state){
        auto source=projection(state->authored);if(!source)return std::unexpected(source.error());
        auto selected=state->authored.controller->selection();if(!selected)return std::unexpected(convert(selected.error()));
        auto old=state->source;auto previous=state->selected;state->source=std::move(*source);state->selected=std::move(*selected);
        if(state->viewport_seen){auto r=viewport(state,state->viewport);if(!r){state->source=std::move(old);state->selected=std::move(previous);(void)viewport(state,state->viewport);return r;}}
        return {};
    }
    [[nodiscard]] static ii::core::Result<void> select(const std::shared_ptr<State>& state,std::string key){
        auto selected=state->authored.controller->select(key);if(!selected)return std::unexpected(convert(selected.error()));
        if(!*selected)return {};const auto previous=std::exchange(state->selected,std::move(key));
        for(const auto& name:{previous,state->selected}){auto found=state->rows.find(name);if(found==state->rows.end()||!found->second->input)continue;
            auto row=found->second;auto input=std::make_shared<RowInput>(*row->input);auto configuration=input->drawing.model.configuration();
            configuration.selected=name==state->selected;(void)input->drawing.model.set_configuration(std::move(configuration));row->input=std::move(input);
            if(auto r=row->widget.request_repaint();!r)return r;}
        return {};
    }
    [[nodiscard]] static ii::core::Result<bool> navigate(const std::shared_ptr<State>& state,const ii::gui::WidgetInput& event){
        using Key=ii::gui::WidgetKey;
        if(event.kind!=ii::gui::WidgetInputKind::key_down||!state->authored.enabled||!state->authored.interactive||!state->authored.keyboard_navigation)return false;
        if(event.key!=Key::up&&event.key!=Key::down&&event.key!=Key::home&&event.key!=Key::end&&event.key!=Key::page_up&&event.key!=Key::page_down)return false;
        auto refreshed=state->authored.controller->refresh();if(!refreshed)return std::unexpected(convert(refreshed.error()));
        if(*refreshed){if(auto r=refresh(state);!r)return std::unexpected(r.error());return true;}
        const auto snapshot=state->source->snapshot();if(snapshot->entries.empty())return true;
        auto index=std::ranges::find_if(snapshot->entries,[&](const auto& item){return item.key==state->selected;});
        const bool has_selection=index!=snapshot->entries.end();
        const bool backwards=event.key==Key::up||event.key==Key::page_up||event.key==Key::end;
        auto position=has_selection?std::size_t(index-snapshot->entries.begin()):(backwards?snapshot->entries.size()-1:0);
        const auto page=std::max<std::size_t>(1,std::size_t(state->viewport.bounds.height/state->authored.row_height));
        if(event.key==Key::home)position=0;else if(event.key==Key::end)position=snapshot->entries.size()-1;
        else if(has_selection&&(event.key==Key::up||event.key==Key::page_up))position=position>=(event.key==Key::up?1:page)?position-(event.key==Key::up?1:page):0;
        else if(has_selection)position=std::min(snapshot->entries.size()-1,position+(event.key==Key::down?1:page));
        while(!snapshot->entries[position].enabled||!snapshot->entries[position].selectable){
            if(backwards){if(!position)return true;--position;}else{if(position+1==snapshot->entries.size())return true;++position;}
        }
        const auto& item=snapshot->entries[position];if(auto r=select(state,item.key);!r)return std::unexpected(r.error());
        const auto top=double(position)*state->authored.row_height,bottom=top+state->authored.row_height;
        auto offset=state->viewport.offset;if(top<offset)offset=top;else if(bottom>offset+state->viewport.bounds.height)offset=bottom-state->viewport.bounds.height;
        if(auto r=state->root.set_scroll_offset(offset);!r)return std::unexpected(r.error());
        auto row=state->rows.find(item.key);if(row!=state->rows.end())(void)row->second->widget.focus();
        if(state->live&&state->callbacks.activated){auto callback=state->callbacks.activated;callback(ListViewEvent{item,position});}
        return true;
    }
    [[nodiscard]] static ii::core::Result<void> update(const std::shared_ptr<State>& state,ListViewInput input){
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Flat list requires its owner thread");
        try{
            if(auto live=state->root.snapshot();!live)return std::unexpected(live.error());
            if(state->revision==std::numeric_limits<std::uint64_t>::max())return ii::core::fail(ii::core::ErrorCode::out_of_range,"Flat list revision exhausted");
            auto source=projection(input);if(!source)return std::unexpected(source.error());
            auto selected=input.controller->selection();if(!selected)return std::unexpected(convert(selected.error()));
            auto old_input=state->authored;auto old_source=state->source;auto old_selected=state->selected;
            const auto revision=++state->revision;state->authored=std::move(input);state->source=std::move(*source);state->selected=std::move(*selected);
            const auto rollback=[&]{if(state->revision==revision){state->authored=old_input;state->source=old_source;state->selected=old_selected;
                if(state->viewport_seen)(void)viewport(state,state->viewport);(void)state->root.set_scroll_options(scroll_options(*state));(void)state->root.set_enabled(state->authored.enabled);}};
            if(state->viewport_seen)if(auto r=viewport(state,state->viewport);!r){rollback();return r;}
            if(state->revision!=revision)return {};
            if(auto r=state->root.set_scroll_options(scroll_options(*state));!r){rollback();return r;}
            if(state->revision!=revision)return {};
            if(auto r=state->root.set_enabled(state->authored.enabled);!r){rollback();return r;}
            state->failure.reset();return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Flat list update failed",std::current_exception());}
    }
public:
    [[nodiscard]] static ii::core::Result<NativeListView> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,
            std::string_view id,ListViewInput input,NativeListViewCallbacks callbacks={}){
        try{
            auto source=projection(input);if(!source)return std::unexpected(source.error());
            auto selected=input.controller->selection();if(!selected)return std::unexpected(convert(selected.error()));
            auto state=std::make_shared<State>();state->authored=std::move(input);state->source=std::move(*source);state->selected=std::move(*selected);state->callbacks=std::move(callbacks);
            std::weak_ptr<State> weak=state;auto root=builder.scroll_view(parent,id,scroll_options(*state));if(!root)return std::unexpected(root.error());state->root=*root;
            auto content=builder.content_host(*root,std::string(id)+".rows",{state->authored.width,state->authored.height});if(!content)return std::unexpected(content.error());state->content=*content;
            state->viewport_handler=[weak](const ii::gui::ScrollViewport& value){if(auto state=weak.lock();state&&state->live){
                auto result=viewport(state,value);if(!result){state->failure=result.error();
                    if(result.error().code==ii::core::ErrorCode::busy)(void)state->root.set_scroll_handler(state->viewport_handler);
                    throw std::runtime_error(result.error().message);}state->failure.reset();}};
            if(auto r=root->set_scroll_handler(state->viewport_handler);!r)return std::unexpected(r.error());
            if(auto r=root->set_input_handler([weak](const auto& event)->ii::core::Result<bool>{auto state=weak.lock();if(!state)return false;
                try{auto result=navigate(state,event);if(!result)state->failure=result.error();return result;}
                catch(...){state->failure=ii::core::Error{ii::core::ErrorCode::failed,"Flat list input failed",std::current_exception()};return std::unexpected(*state->failure);}});!r)return std::unexpected(r.error());
            if(auto r=root->set_enabled(state->authored.enabled);!r)return std::unexpected(r.error());
            if(auto r=builder.retain(state);!r)return std::unexpected(r.error());
            if(auto r=builder.on_publish([weak]{if(auto state=weak.lock()){state->live=true;if(auto r=state->root.set_scroll_handler(state->viewport_handler);!r)state->failure=r.error();}});!r)return std::unexpected(r.error());
            if(auto r=builder.on_retire([weak]{if(auto state=weak.lock()){state->live=false;state->rows.clear();}});!r)return std::unexpected(r.error());
            return NativeListView(std::move(state));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Flat list construction failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::core::Result<ii::gui::Widget> row(std::string_view key)const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Flat list requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());auto found=state_->rows.find(std::string(key));
        if(found==state_->rows.end())return ii::core::fail(ii::core::ErrorCode::not_found,"Flat list row is not mounted");return found->second->widget;
    }
    [[nodiscard]] ii::core::Result<std::vector<std::string>> mounted_keys()const{return state_->content.keyed_content_keys();}
    [[nodiscard]] ii::core::Result<void> set_options(ListViewInput input)const{return update(state_,std::move(input));}
    [[nodiscard]] ii::core::Result<std::optional<ii::core::Error>> failure()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"Flat list requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());return state_->failure;
    }
private:
    std::shared_ptr<State> state_;
};

inline void NativeListView::activate(const std::shared_ptr<State>& state,const std::shared_ptr<Row>& row)noexcept{
    try{
        if(!current(state,row)||!state->authored.enabled||!state->authored.interactive)return;
        auto refreshed=state->authored.controller->refresh();if(!refreshed){state->failure=convert(refreshed.error());return;}
        if(*refreshed){if(auto r=refresh(state);!r)state->failure=r.error();return;}
        const auto snapshot=state->source->snapshot();auto found=std::ranges::find_if(snapshot->entries,[&](const auto& item){return item.key==row->key;});
        if(found==snapshot->entries.end()||!found->enabled||!found->selectable)return;
        if(auto r=select(state,found->key);!r){state->failure=r.error();return;}
        if(current(state,row)&&state->callbacks.activated){auto callback=state->callbacks.activated;callback(ListViewEvent{*found,std::size_t(found-snapshot->entries.begin())});}
    }catch(...){state->failure=ii::core::Error{ii::core::ErrorCode::failed,"Flat list activation failed",std::current_exception()};}
}

} // namespace lvui::native
