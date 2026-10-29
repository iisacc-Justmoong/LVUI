#include <LVUI/NativeTableHeader.h>
#include <iiGUI/WidgetState.h>

#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
bool contains(const ii::gui::CanvasFrame& frame,std::string_view value){
    for(const auto& run:frame.texts)if(run.text==value)return true;
    for(const auto& layer:frame.overlays)for(const auto& run:layer.texts)if(run.text==value)return true;return false;
}
}
int main(){
    using namespace lvui;using namespace ii::gui;
    native::TableHeaderInput input;input.configuration.width=300;input.configuration.interactive=true;
    input.configuration.columns={{"a","First",100},{"b","Second",100},{"c","Third",100}};
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{350,80}));std::optional<native::NativeTableHeader> header;
    std::vector<std::string> clicked;bool retire=false;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto column=builder.vstack({},"column",{.spacing=0,.alignment=StackAlignment::leading});if(!column)return std::unexpected(column.error());
        auto value=native::NativeTableHeader::create(builder,*column,"header",input,{.column_clicked=[&](std::size_t,const TableColumn& column){
            clicked.push_back(column.key);if(retire)good(host.clear_content());}});
        if(!value)return std::unexpected(value.error());header=*value;return {};
    }));
    auto root=take(header->root().snapshot());auto drawing=take(tree->drawing_snapshot({350,80}));
    require(root.size.width==300&&root.size.height==25&&contains(drawing,"First")&&contains(drawing,"Third"),"Header sizing or text failed");
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=root.bounds.x+150,.y=root.bounds.y+10}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=root.bounds.x+150,.y=root.bounds.y+10}));
    require(clicked==std::vector<std::string>{"b"},"Header pointer did not report its column");
    good(header->root().focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::right}));
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));
    require(clicked==std::vector<std::string>({"b","c"}),"Header keyboard navigation failed");
    auto next=input;next.configuration.columns[1].title="Changed";next.configuration.columns[1].enabled=false;
    good(header->set_options(next));drawing=take(tree->drawing_snapshot({350,80}));require(contains(drawing,"Changed"),"Header did not update its content");
    auto invalid=next;invalid.configuration.columns[2].key="a";
    require(!header->set_options(invalid)&&take(header->options())==next,"Invalid columns replaced the good header");
    bool wrong=false;std::thread worker([&]{auto result=header->set_options(input);wrong=!result&&result.error().code==ii::core::ErrorCode::wrong_executor;});
    worker.join();require(wrong,"Header accepted a foreign-thread update");
    next.configuration.enabled=false;good(header->set_options(next));
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=root.bounds.x+20,.y=root.bounds.y+10}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=root.bounds.x+20,.y=root.bounds.y+10}));
    require(clicked.size()==2,"Disabled header delivered an event");
    next.configuration.enabled=true;good(header->set_options(next));retire=true;
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=root.bounds.x+20,.y=root.bounds.y+10}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=root.bounds.x+20,.y=root.bounds.y+10}));
    require(clicked.size()==3&&!header->set_options(input),"Retired header accepted an update");
    input.configuration.width=100;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto column=builder.vstack({},"column",{.spacing=0,.alignment=StackAlignment::leading});if(!column)return std::unexpected(column.error());
        auto value=native::NativeTableHeader::create(builder,*column,"scrolled",input,{.column_clicked=[&](std::size_t,const TableColumn& column){clicked.push_back(column.key);}});
        if(!value)return std::unexpected(value.error());header=*value;return {};
    }));
    good(header->root().focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::home}));
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::right}));
    require(take(header->viewport()).x==100,"Keyboard navigation did not reveal the focused column");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));
    require(clicked.back()=="b","Scrolled keyboard activation selected the wrong column");
    good(host.clear_content());tree->close();
}
