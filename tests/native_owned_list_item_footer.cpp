#include <LVUI/NativeListItemComposite.h>
#include "list_item_composite_test_binding.h"
#include <iiGUI/WidgetState.h>

#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
bool near(double a,double b){return std::abs(a-b)<.01;}
}
int main(){
    using namespace ii::gui;using namespace lvui;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{500,300}));std::optional<native::NativeListItemComposite> item;
    native::ListItemCompositeInput input;input.row.configuration.variant=ListItemVariant::detail_actions;
    ii::gui::Widget child;unsigned clicks=0;
    const WidgetFactory footer=[&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto result=builder.action_surface({},"custom-footer",{.text="Extra",.width=80,.height=30},[&]{++clicks;});
        if(!result)return std::unexpected(result.error());child=*result;return {};
    };
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto stack=builder.vstack({},"stack",{.spacing=0,.alignment=StackAlignment::leading});if(!stack)return std::unexpected(stack.error());
        auto value=native::NativeListItemComposite::create(builder,*stack,"item",input,{}, {.footer=footer});
        if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    (void)take(tree->drawing_snapshot({500,300}));
    auto root=take(item->root().snapshot()),row=take(item->row().snapshot()),footer_view=take(child.snapshot());
    require(near(root.size.height,162)&&near(row.bounds.height,120)&&near(footer_view.bounds.y,132)&&near(footer_view.bounds.height,30),
            "Footer natural size did not extend the row without overlapping it");
    good(child.activate());require(clicks==1,"Custom footer did not receive input");
    auto disabled=take(item->options());disabled.row.configuration.enabled=false;good(item->set_options(disabled));
    good(child.activate());require(clicks==1,"A disabled row delivered footer input");
    disabled.row.configuration.enabled=true;good(item->set_options(disabled));
    auto child_options=take(child.action_options());child_options.height=50;good(child.set_action_options(child_options));
    (void)take(tree->drawing_snapshot({500,300}));
    require(near(take(item->root().snapshot()).size.height,182)&&near(take(child.snapshot()).bounds.height,50),
            "A changed footer natural size did not update the row");
    auto next=take(item->options());next.row.configuration.visible=false;good(item->set_options(next));
    require(!take(item->root().interaction()).visible,"Hidden row remained visible");
    next.row.configuration.visible=true;good(item->set_options(next));(void)take(tree->drawing_snapshot({500,300}));
    require(bool(child.snapshot())&&near(take(item->root().snapshot()).size.height,182),"Showing the row lost its footer");
    next.row.configuration.variant=ListItemVariant::mini;good(item->set_options(next));(void)take(tree->drawing_snapshot({500,300}));
    require(near(take(item->root().snapshot()).size.height,20)&&!take(child.interaction()).visible,
            "A non-modern row displayed its modern footer");
    child_options.height=70;good(child.set_action_options(child_options));
    next.row.configuration.variant=ListItemVariant::detail_actions;good(item->set_options(next));(void)take(tree->drawing_snapshot({500,300}));
    require(near(take(item->root().snapshot()).size.height,202),"A footer resized while hidden was not remeasured");
    next.row.configuration.variant=ListItemVariant::detail_actions;next.row.configuration.height=110;good(item->set_options(next));
    (void)take(tree->drawing_snapshot({500,300}));
    require(near(take(item->root().snapshot()).size.height,110)&&take(child.snapshot()).bounds.y+take(child.snapshot()).bounds.height<=110.01,
            "Fixed-height row did not constrain its footer");
    auto old=item->root();auto failed=host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=native::NativeListItemComposite::create(builder,{},"bad",input,{}, {.footer=[](WidgetBuilder&)->ii::core::Result<void>{
            return ii::core::fail(ii::core::ErrorCode::failed,"Footer rejected");}});
        if(!value)return std::unexpected(value.error());return {};
    });
    require(!failed&&old.snapshot(),"Failed footer construction replaced the active generation");
    good(host.clear_content());require(!child.snapshot()&&!item->set_options(input),"Retired footer or item remained usable");
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=native::NativeListItemComposite::create(builder,{},"stretched",input,{}, {.footer=footer});
        if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    (void)take(tree->drawing_snapshot({500,300}));
    require(near(take(item->root().snapshot()).bounds.height,300)&&near(take(child.snapshot()).bounds.y,270),
            "Footer did not use the row's allocated height");
    auto stretched_options=take(item->options());stretched_options.status_text="Ready";good(item->set_options(stretched_options));
    (void)take(tree->drawing_snapshot({500,300}));
    require(near(take(item->root().snapshot()).bounds.height,300)&&near(take(child.snapshot()).bounds.y,270),
            "Updating a stretched row discarded the parent's allocated extent");
    child_options=take(child.action_options());child_options.height=50;good(child.set_action_options(child_options));
    (void)take(tree->drawing_snapshot({500,300}));
    require(near(take(item->root().snapshot()).bounds.height,300)&&near(take(child.snapshot()).bounds.y,250),
            "A resized footer lost the parent's allocated extent");
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto stack=builder.vstack({},"outer",{.spacing=0,.alignment=StackAlignment::leading});if(!stack)return std::unexpected(stack.error());
        auto value=native::NativeListItemComposite::create(builder,*stack,"deferred",input,{}, {.footer=[&](WidgetBuilder& nested)->ii::core::Result<void>{
            auto group=nested.vstack({},"group",{.spacing=0,.alignment=StackAlignment::leading});if(!group)return std::unexpected(group.error());
            auto result=nested.action_surface(*group,"later",{.text="Later",.width=80,.height=30},[]{});if(!result)return std::unexpected(result.error());
            child=*result;return child.set_visible(false);
        }});
        if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    (void)take(tree->drawing_snapshot({500,300}));require(near(take(item->root().snapshot()).size.height,120),"Empty footer changed the natural height");
    good(child.set_visible(true));(void)take(tree->drawing_snapshot({500,300}));
    require(near(take(item->root().snapshot()).size.height,162),"An initially empty footer could not grow when its child appeared");
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=native::NativeListItemComposite::create(builder,{},"self-retiring",input,{}, {.footer=[&](WidgetBuilder& nested)->ii::core::Result<void>{
            auto result=nested.action_surface({},"remove",{.text="Remove",.height=30},[&]{good(host.clear_content());});
            if(!result)return std::unexpected(result.error());child=*result;return {};
        }});
        if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    good(child.activate());require(!item->root().snapshot(),"A footer could not retire its parent safely");tree->close();
}
