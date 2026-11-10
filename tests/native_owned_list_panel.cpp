#include "list_panel_test_binding.h"
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
}
int main(){
    using namespace lvui;using namespace ii::gui;
    std::vector<ListEntry> entries;for(int i=0;i<500;++i)entries.push_back({.key=std::to_string(i),.label="Item "+std::to_string(i)});
    auto source=take(ListMemory::create(entries));auto controller=std::make_shared<ListController>(take(ListController::create(source)));
    native::ListPanelInput input;input.appearance.width=200;input.appearance.height=140;input.appearance.toolbar_visible=true;
    input.list.controller=controller;input.toolbar.configuration.icons[0]="home";
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{240,160}));std::optional<ListPanelTestView> panel;
    int selected=0,toolbar=0,footer=0;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto column=builder.vstack({},"column",{.spacing=0,.alignment=StackAlignment::leading});if(!column)return std::unexpected(column.error());
        auto value=ListPanelTestView::create(builder,*column,"list",input,
            {.item_activated=[&](const auto&){++selected;},.toolbar_clicked=[&](std::size_t,std::string){++toolbar;},
             .footer_clicked=[&](std::size_t,const auto&){++footer;}});
        if(!value)return std::unexpected(value.error());panel=*value;return {};
    }));
    (void)take(tree->drawing_snapshot({240,160}));auto geometry=take(panel->geometry());
    auto list=panel->list();(void)take(list.root().snapshot());
    require(geometry.toolbar.height==30 && geometry.list.height==84 && geometry.footer.height==26 &&
            !take(list.mounted_keys()).empty() && take(list.mounted_keys()).size()<=6,
            "Flat list panel did not compose its regions or virtualize rows");
    auto row=take(list.row("0"));good(row.activate());good(take(panel->toolbar().slot(0)).activate());good(take(panel->footer().slot(0)).activate());
    require(selected==1&&toolbar==1&&footer==1&&take(source->snapshot())->revision==0,
            "Flat list panel lost child actions or modified the source");
    auto bounds=take(list.root().snapshot()).bounds;
    const auto wheel=[&]{good(tree->input({.kind=WidgetInputKind::scroll,.x=bounds.x+10,.y=bounds.y+10,.scroll_y=20}));};
    wheel();{const auto result=take(list.root().snapshot());if(result.scroll_offset!=20)
        throw std::runtime_error("List did not accept user scrolling: offset="+std::to_string(result.scroll_offset)+
            " content="+std::to_string(result.content_height)+" x="+std::to_string(bounds.x)+" y="+std::to_string(bounds.y)+
            " width="+std::to_string(bounds.width)+" height="+std::to_string(bounds.height));}
    auto frozen=input;frozen.list.scrollable=false;good(panel->set_options(frozen));wheel();
    require(take(list.root().snapshot()).scroll_offset==20,"Non-scrollable list accepted user scrolling");
    good(list.root().set_scroll_offset(30));
    require(take(list.root().snapshot()).scroll_offset==30,"Non-scrollable list rejected programmatic scrolling");
    good(list.root().set_scroll_offset(0));good(panel->set_options(input));
    auto next=input;next.appearance.width=180;next.appearance.height=100;next.appearance.footer_visible=false;good(panel->set_options(next));
    require(take(panel->geometry()).list.height==70&&!take(panel->footer().root().interaction()).visible&&row.snapshot(),
            "Flat list panel resizing replaced a retained row or kept the footer visible");
    auto invalid=next;invalid.toolbar.configuration.icons[0]="missing-icon";
    require(!panel->set_options(invalid)&&take(panel->options())==next&&row.snapshot(),"Invalid flat list panel input replaced the working view");
    next.appearance.interactive=false;good(panel->set_options(next));const auto count=selected;good(row.activate());
    require(selected==count,"Noninteractive flat list panel still activated a row");
    bounds=take(list.root().snapshot()).bounds;wheel();
    require(take(list.root().snapshot()).scroll_offset==0,"Noninteractive list still accepted user scrolling");
    bool wrong=false;std::thread worker([&]{auto result=panel->set_options(next);wrong=!result&&result.error().code==ii::core::ErrorCode::wrong_executor;});
    worker.join();require(wrong,"Flat list panel accepted a foreign-thread update");
    good(host.clear_content());require(!panel->set_options(next)&&!row.snapshot(),"Retired flat list panel accepted updates");tree->close();
}
