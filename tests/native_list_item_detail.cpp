#include "NativeListItemDetail.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <stdexcept>
template<class T>T take(ii::core::Result<T> v){if(!v)throw std::runtime_error(v.error().message);return std::move(*v);}
void good(ii::core::Result<void> v){if(!v)throw std::runtime_error(v.error().message);}
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
int main(){using namespace ii::gui;using View=lvui_generated::NativeListItemDetail;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task t){t();});auto host=take(tree->content_host({},"host",{300,160}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& b){return View::build(b,{}, {},&scope);}));auto nodes=take(tree->snapshots());
    auto found=std::ranges::find_if(nodes,[](const auto& n){return n.id.ends_with("/detail.row");});check(found!=nodes.end(),"detail row missing");auto row=take(tree->find(found->id));
    check(take(row.snapshot()).accessibility_description=="No files · Today · Work · Art","details missing from accessibility");
    const auto has=[&](std::string_view text){auto frame=take(tree->drawing_snapshot({300,160}));const auto contains=[&](const auto& runs){return std::ranges::any_of(runs,[&](const auto& t){return t.text==text;});};
        return contains(frame.texts)||std::ranges::any_of(frame.overlays,[&](const auto& layer){return contains(layer.texts);});};
    check(has("Today")&&has("Work")&&has("Art"),"detail metadata was not drawn");good(row.focus());
    scope->backend_0.dateVisible.set(false);scope->backend_0.foldersVisible.set(false);scope->backend_0.tagsVisible.set(false);
    check(!has("Today")&&!has("Work")&&!has("Art")&&take(row.snapshot()).accessibility_description=="No files"&&take(row.snapshot()).focused,
          "hidden metadata remained visible or focus was lost");
    good(host.clear_content());scope.reset();check(!row.snapshot(),"detail survived retirement");tree->close();}
