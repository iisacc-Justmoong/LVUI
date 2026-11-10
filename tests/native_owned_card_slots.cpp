#include "native_card_composition.h"
#include <iiGUI/WidgetState.h>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace {
using namespace ii::gui;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
bool text(const CanvasFrame& frame,std::string_view expected){
    for(const auto& item:frame.texts)if(item.text==expected)return true;
    for(const auto& layer:frame.overlays)for(const auto& item:layer.texts)if(item.text==expected)return true;
    return false;
}
Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& state,std::string_view suffix){
    for(const auto& value:take(state->snapshots()))if(value.id.ends_with(suffix))return take(state->find(value.id));
    throw std::runtime_error("Card content widget not found");
}
}
int main(){
    using namespace lvui;using lvui::CardType;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{400,400}));
    native::CardInput input;input.configuration.type=CardType::model;input.configuration.title="Default title";
    input.configuration.selectable=false;input.configuration.show_menu=false;input.configuration.show_action=false;
    input.configuration.width=240;input.configuration.height=200;
    unsigned clicked=0,child_clicked=0;std::optional<native::NativeCard> card;
    native::NativeCardViews views;
    views.content=[&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto root=builder.vstack({},"custom",{.spacing=0});if(!root)return std::unexpected(root.error());
        auto label=builder.label(*root,"custom-label",{.text="Custom content"});if(!label)return std::unexpected(label.error());
        auto button=builder.button(*root,"custom-button",{.text="Child"},[&]{++child_clicked;});
        if(!button)return std::unexpected(button.error());return {};
    };
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto column=builder.vstack({},"column",{.spacing=0,.alignment=StackAlignment::leading});if(!column)return std::unexpected(column.error());
        auto value=native::NativeCard::create(builder,*column,"card",input,{.clicked=[&]{++clicked;}},views);
        if(!value)return std::unexpected(value.error());card=*value;return {};
    }));
    auto frame=take(tree->drawing_snapshot({400,400}));
    require(text(frame,"Custom content")&&!text(frame,"Default title"),"Custom card content did not replace the default body");
    auto button=find(tree,"/custom-button");good(button.activate());good(card->surface().activate());
    require(child_clicked==1&&clicked==1,"Custom content and card activation were not independent");
    auto changed=take(card->options());changed.configuration.width=300;good(card->set_options(changed));
    require(bool(button.snapshot()),"Card resize replaced its custom View");
    require(take(card->root().snapshot()).size.width==300,"Card resize did not update its requested width");
    changed.configuration.enabled=false;good(card->set_options(changed));good(button.activate());
    require(child_clicked==1,"Disabled card delivered a custom child action");
    good(host.clear_content());require(!button.snapshot(),"Card retirement left its content active");

    native::CardInput file;file.configuration.type=CardType::file;file.configuration.title="File";
    file.preview=take(rgba_image(1,1,{255,0,0,255}));
    native::NativeCardViews preview_views;preview_views.preview=[](WidgetBuilder& builder)->ii::core::Result<void>{
        auto label=builder.label({},"preview-label",{.text="Custom preview"});return label?ii::core::Result<void>{}:std::unexpected(label.error());
    };
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=native::NativeCard::create(builder,{},"file",file,{},preview_views);if(!value)return std::unexpected(value.error());card=*value;return {};
    }));
    auto preview=find(tree,"/preview-label");frame=take(tree->drawing_snapshot({400,400}));
    require(text(frame,"Custom preview")&&frame.images.empty(),"Custom preview did not replace the default image");
    auto rejected=preview_views;rejected.content=preview_views.preview;
    require(!host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=native::NativeCard::create(builder,{},"invalid",file,{},rejected);return value?ii::core::Result<void>{}:std::unexpected(value.error());
    })&&preview.snapshot(),"Invalid card Views displaced the current card");
    native::NativeCardViews failing;failing.icon=[](WidgetBuilder&)->ii::core::Result<void>{return ii::core::fail(ii::core::ErrorCode::failed,"candidate rejected");};
    require(!host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=native::NativeCard::create(builder,{},"failure",input,{},failing);return value?ii::core::Result<void>{}:std::unexpected(value.error());
    })&&preview.snapshot(),"A failed card View displaced the current card");
    native::CardInput icon;icon.configuration.type=CardType::model;icon.configuration.icon_name="not-an-icon";
    native::NativeCardViews icon_views;icon_views.icon=[](WidgetBuilder& builder)->ii::core::Result<void>{
        auto label=builder.label({},"icon-label",{.text="Custom icon"});return label?ii::core::Result<void>{}:std::unexpected(label.error());
    };
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=native::NativeCard::create(builder,{},"icon",icon,{},icon_views);if(!value)return std::unexpected(value.error());card=*value;return {};
    }));
    require(!preview.snapshot()&&text(take(tree->drawing_snapshot({400,400})),"Custom icon"),"Custom icon did not replace icon resolution");
    good(host.clear_content());
    tree->close();
}
