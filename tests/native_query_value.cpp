#include "NativeQueryValue.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view suffix){
    const auto nodes=take(tree->snapshots());
    const auto found=std::ranges::find_if(nodes,[&](const auto& node){return node.id.ends_with(suffix);});
    if(found==nodes.end())throw std::runtime_error("Query value widget missing");return take(tree->find(found->id));
}
void type(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,ii::gui::Widget widget,std::string value){
    good(widget.focus());good(tree->input({.kind=ii::gui::WidgetInputKind::text,.text=std::move(value)}));
}
}
int main(){
    try{
        using View=lvui_generated::NativeQueryValue;
        auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
        auto host=take(tree->content_host({},"host",{400,400}));std::shared_ptr<View::Scope> scope;
        good(host.set_content([&](auto& builder){return View::build(builder,{},{},&scope);}));
        check(take(find(tree,"/live").snapshot()).text=="Draft"&&scope->backend_0.reads>0,
              "Initial view did not read the model query");
        type(tree,find(tree,"/editor"),"!");
        check(scope->backend_0.stored=="Draft!"&&scope->backend_0.writes==1&&
              take(find(tree,"/live").snapshot()).text=="Draft!",
              "User editing did not update the source through its write query");
        auto update=scope->backend_0.write.send({"Remote"});
        check(update&&*update,"An external query update was rejected");
        check(take(find(tree,"/live").snapshot()).text=="Remote",
              "An external query change did not update the label");
        check(take(find(tree,"/method").snapshot()).text=="Remote",
              "A query read inside a C++ method was not tracked");
        check(take(find(tree,"/editor").text_content({0,6})).text=="Remote",
              "An external query change did not update the input");
        check(take(find(tree,"/initial").snapshot()).text=="Draft","A static snapshot followed a query update");
        good(find(tree,"/editor").focus());
        auto rejected=tree->input({.kind=ii::gui::WidgetInputKind::text,.text="123456789"});
        check(!rejected&&scope->backend_0.stored=="Remote"&&scope->backend_0.writes==3&&
              take(find(tree,"/live").snapshot()).text=="Remote",
              "A rejected query edit changed the source or was silently accepted");
        const auto writes=scope->backend_0.writes;
        type(tree,find(tree,"/readonly"),"x");
        check(scope->backend_0.stored=="Remote"&&scope->backend_0.writes==writes,
              "A const query value accepted an edit");
        type(tree,find(tree,"/local"),"!");
        check(take(find(tree,"/localLabel").snapshot()).text=="Local!"&&scope->backend_0.writes==writes,
              "Temporary local state escaped into the model query");
        const auto reads=scope->backend_0.reads;
        (void)take(tree->drawing_snapshot({400,400}));(void)take(tree->drawing_snapshot({400,400}));
        check(scope->backend_0.reads==reads,"An unchanged drawing polled the query");
        std::weak_ptr<View::Scope> retired=scope;good(host.clear_content());scope.reset();
        check(retired.expired(),"The query value retained a retired view");tree->close();
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
