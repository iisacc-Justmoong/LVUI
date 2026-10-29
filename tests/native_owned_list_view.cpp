#include <LVUI/NativeListView.h>
#include <iiGUI/WidgetState.h>

#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
template<class T>T take(lvui::ListResult<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
bool text_present(const ii::gui::CanvasFrame& frame,std::string_view value){
    for(const auto& text:frame.texts)if(text.text==value)return true;
    for(const auto& layer:frame.overlays)for(const auto& text:layer.texts)if(text.text==value)return true;return false;
}
}
int main(){
    using namespace lvui;using namespace ii::gui;
    std::vector<ListEntry> entries;for(int i=0;i<1000;++i)entries.push_back({.key=std::to_string(i),.label="Item "+std::to_string(i),.description="Detail "+std::to_string(i)});
    auto source=take(ListMemory::create(entries));auto controller=std::make_shared<ListController>(take(ListController::create(source)));
    native::ListViewInput input;input.controller=controller;input.width=240;input.height=100;input.row_height=36;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{300,160}));std::optional<native::NativeListView> view;
    std::vector<std::string> activated;bool retire=false;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto column=builder.vstack({},"column",{.spacing=0,.alignment=StackAlignment::leading});if(!column)return std::unexpected(column.error());
        auto result=native::NativeListView::create(builder,*column,"list",input,{.activated=[&](const auto& item){activated.push_back(item.entry.key);if(retire)good(host.clear_content());}});
        if(!result)return std::unexpected(result.error());view=*result;return {};
    }));
    (void)take(tree->drawing_snapshot({300,160}));(void)take(view->root().snapshot());
    auto drawing=take(tree->drawing_snapshot({300,160}));auto mounted=take(view->mounted_keys());
    if(mounted.empty()||mounted.size()>5||mounted.front()!="0"||!text_present(drawing,"Item 0")||!text_present(drawing,"Detail 0")){
        auto failure=take(view->failure());
        throw std::runtime_error("Flat list initial display: rows="+std::to_string(mounted.size())+", label="+
            std::to_string(text_present(drawing,"Item 0"))+", detail="+std::to_string(text_present(drawing,"Detail 0"))+
            ", failure="+(failure?failure->message:std::string("none")));
    }
    auto first=take(view->row("0"));good(first.activate());
    require(activated==std::vector<std::string>{"0"}&&take(controller->selection())=="0"&&take(source->snapshot())->revision==0,
            "Flat list activation lost its selection or changed the document");
    good(view->root().set_scroll_offset(18000));mounted=take(view->mounted_keys());
    require(mounted.size()<=6&&!first.snapshot()&&take(source->snapshot())->revision==0,"Flat list scroll retained offscreen rows or edited data");
    const auto key=mounted[mounted.size()/2];auto retained=take(view->row(key));const auto identity=take(retained.placement()).scope.lock();
    auto position=std::size_t(std::stoul(key));auto replacement=entries[position];replacement.label="Updated";
    input.source_revision=std::int64_t(take(source->apply({.revision=0,.kind=ListEditKind::replace,.key=key,.item=replacement})));
    good(view->set_options(input));
    require(take(take(view->row(key)).action_options()).accessibility_name=="Updated"&&
            take(take(view->row(key)).placement()).scope.lock()==identity,"Flat list refresh rebuilt a visible key or lost its changed label");
    replacement.icon_name="missing-icon";input.source_revision=std::int64_t(take(source->apply({.revision=1,.kind=ListEditKind::replace,.key=key,.item=replacement})));
    require(!view->set_options(input)&&take(take(view->row(key)).action_options()).accessibility_name=="Updated",
            "An invalid flat-list icon replaced the last working row");
    replacement.icon_name.clear();input.source_revision=std::int64_t(take(source->apply({.revision=2,.kind=ListEditKind::replace,.key=key,.item=replacement})));
    good(view->set_options(input));
    const auto previous=activated.size();replacement.label="External";(void)take(source->apply({.revision=3,.kind=ListEditKind::replace,.key=key,.item=replacement}));
    good(take(view->row(key)).activate());require(activated.size()==previous&&take(take(view->row(key)).action_options()).accessibility_name=="External",
            "A stale flat-list action was delivered or failed to refresh");
    good(take(view->row(key)).activate());require(activated.size()==previous+1&&activated.back()==key,"Refreshed list action was not delivered");
    auto selected=take(view->row(key));good(selected.focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::down}));
    require(take(controller->selection())!=key&&take(source->snapshot())->revision==4,"Flat-list keyboard navigation edited the source or failed");
    bool wrong=false;std::thread worker([&]{auto result=view->set_options(input);wrong=!result&&result.error().code==ii::core::ErrorCode::wrong_executor;});
    worker.join();require(wrong,"Flat list accepted a foreign-thread update");
    retire=true;auto current=take(controller->selection());good(take(view->row(current)).activate());
    require(!view->set_options(input),"Flat list survived removal during its callback");tree->close();

    std::string long_text;for(int i=0;i<6000;++i)long_text+="한";
    std::vector<ListEntry> long_entries;for(int i=0;i<40;++i)long_entries.push_back({.key=std::to_string(i),.label=long_text,.description=long_text});
    auto long_source=take(ListMemory::create(std::move(long_entries)));
    native::ListViewInput long_input;long_input.controller=std::make_shared<ListController>(take(ListController::create(long_source)));
    long_input.width=200;long_input.height=720;long_input.row_height=36;
    auto next=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});auto next_host=take(next->content_host({},"host",{240,760}));
    std::optional<native::NativeListView> long_view;
    good(next_host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto column=builder.vstack({},"column",{.spacing=0,.alignment=StackAlignment::leading});if(!column)return std::unexpected(column.error());
        auto result=native::NativeListView::create(builder,*column,"long",long_input);if(!result)return std::unexpected(result.error());long_view=*result;return {};
    }));
    (void)take(long_view->root().snapshot());auto bounded=take(next->drawing_snapshot({240,760}));std::size_t runs=0;
    const auto check=[&](const auto& values){for(const auto& text:values){require(text.text.size()<=8192&&valid_accessibility_text(text.text),"List drawing exceeded its UTF-8 text budget");++runs;}};
    check(bounded.texts);for(const auto& layer:bounded.overlays)check(layer.texts);
    require(runs>=20&&take(long_source->snapshot())->entries.front().label==long_text&&take(take(long_view->row("0")).action_options()).accessibility_name.size()<=8192,
            "Bounded list drawing changed the source or lost visible rows");
    good(next_host.clear_content());next->close();

    auto keys_source=take(ListMemory::create({{.key="a",.label="A"},{.key="b",.label="B",.enabled=false},{.key="c",.label="C"}}));
    auto keys_controller=std::make_shared<ListController>(take(ListController::create(keys_source)));native::ListViewInput keys_input;keys_input.controller=keys_controller;
    auto keyboard_tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto keyboard_host=take(keyboard_tree->content_host({},"host",{240,160}));std::optional<native::NativeListView> keyboard_view;
    good(keyboard_host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto column=builder.vstack({},"column",{.spacing=0,.alignment=StackAlignment::leading});if(!column)return std::unexpected(column.error());
        auto result=native::NativeListView::create(builder,*column,"keys",keys_input);if(!result)return std::unexpected(result.error());keyboard_view=*result;return {};
    }));
    (void)take(keyboard_view->root().snapshot());good(take(keyboard_view->row("a")).focus());
    good(keyboard_tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::down}));
    require(take(keys_controller->selection())=="a","Keyboard navigation skipped the first row with no selection");
    good(keyboard_tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::down}));
    require(take(keys_controller->selection())=="c"&&take(keys_source->snapshot())->revision==0,"Keyboard navigation did not skip a disabled row");
    good(keyboard_host.clear_content());keyboard_tree->close();
}
