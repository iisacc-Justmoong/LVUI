#include "native_card_composition.h"
#include <iiGUI/WidgetState.h>
#include <stdexcept>
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<class T>T take(ii::core::Result<T> v){if(!v)throw std::runtime_error(v.error().message);return std::move(*v);}
void good(ii::core::Result<void> v){if(!v)throw std::runtime_error(v.error().message);}
int main(){using namespace ii::gui;auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{210,240}));std::optional<lvui::native::NativeCard> card;int clicks=0,opens=0;
    lvui::native::CardInput options;options.configuration.type=lvui::CardType::file;options.configuration.selectable=false;
    good(host.set_content([&](WidgetBuilder& b)->ii::core::Result<void>{auto result=lvui::native::NativeCard::create(b,{},"card",options,
        {.clicked=[&]{++clicks;},.double_clicked=[&]{++opens;}});if(!result)return std::unexpected(result.error());card=*result;return {};}));
    (void)take(tree->drawing_snapshot({210,240}));
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=40,.y=40,.timestamp=1,.buttons=1,.native_click_count=1}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=40,.y=40,.timestamp=1.1}));
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=40,.y=40,.timestamp=1.2,.buttons=1,.native_click_count=2}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=40,.y=40,.timestamp=1.3}));
    check(clicks==2&&opens==1,"native double-click did not follow successful second activation");
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=40,.y=40,.timestamp=2,.buttons=1,.native_click_count=2}));
    good(tree->input({.kind=WidgetInputKind::pointer_cancel,.x=40,.y=40,.timestamp=2.1}));
    good(card->surface().activate());check(opens==1,"canceled pointer caused a later double-click");
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=40,.y=40,.timestamp=3,.buttons=1}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=40,.y=40,.timestamp=3.1}));
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=41,.y=41,.timestamp=3.2,.buttons=1}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=41,.y=41,.timestamp=3.3}));
    check(opens==2,"fallback double-click was not recognized");
    good(host.clear_content());tree->close();}
