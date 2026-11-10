#include <LVUI/NativeListItemComposite.h>
#include "list_item_composite_test_binding.h"
#include <iiGUI/WidgetState.h>

#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
bool near(double a,double b){return std::abs(a-b)<.01;}
bool contains(const ii::gui::CanvasFrame& frame,std::string_view text){
    for(const auto& run:frame.texts)if(run.text==text)return true;
    for(const auto& layer:frame.overlays)for(const auto& run:layer.texts)if(run.text==text)return true;return false;
}
}
int main(){
    using namespace ii::gui;using namespace lvui;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{500,300}));std::optional<native::NativeListItemComposite> item;
    native::ListItemCompositeInput input;input.row.configuration.variant=ListItemVariant::navigation;
    input.row.configuration.label="Library";input.row.configuration.show_chevron=true;input.trailing_width=64;input.trailing_height=20;
    ii::gui::Widget child;unsigned clicks=0,changes=0,selector_events=0,menu_events=0;
    const WidgetFactory trailing=[&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto result=builder.action_surface({},"custom-trailing",{.text="Custom",.width=64,.height=20},[&]{++clicks;});
        if(!result)return std::unexpected(result.error());child=*result;return {};
    };
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto stack=builder.vstack({},"stack",{.spacing=0,.alignment=StackAlignment::leading});if(!stack)return std::unexpected(stack.error());
        auto value=native::NativeListItemComposite::create(builder,*stack,"item",input,
            {.checked_changed=[&](bool){++changes;},.selector_index_changed=[&](int){++selector_events;},
             .menu_item_triggered=[&](int,const ListItemOption&){++menu_events;}}, {.trailing=trailing});
        if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    auto frame=take(tree->drawing_snapshot({500,300}));auto view=take(child.snapshot());
    require(near(view.bounds.x,212)&&near(view.bounds.width,64)&&near(view.bounds.height,20)&&frame.images.empty(),
            "Custom trailing view did not replace the navigation chevron");
    good(child.activate());require(clicks==1,"Custom trailing view did not receive input");
    auto next=take(item->options());next.row.configuration.variant=ListItemVariant::toggle;good(item->set_options(next));
    (void)take(tree->drawing_snapshot({500,300}));require(!take(item->toggle().interaction()).visible,"A custom trailing view did not hide the default toggle");
    good(item->toggle().activate());require(changes==0,"A hidden default toggle still changed the row");
    next.row.configuration.variant=ListItemVariant::stepper;good(item->set_options(next));frame=take(tree->drawing_snapshot({500,300}));
    require(!take(item->stepper().interaction()).visible&&!contains(frame,"1"),"A custom trailing view did not hide the quantity controls");
    next.row.configuration.variant=ListItemVariant::select;good(item->set_options(next));(void)take(tree->drawing_snapshot({500,300}));
    require(!take(take(item->selector()).root().interaction()).visible,"A custom trailing view did not hide the selector");
    next.row.configuration.variant=ListItemVariant::inline_edit;good(item->set_options(next));(void)take(tree->drawing_snapshot({500,300}));
    require(!take(take(item->input_field()).root().interaction()).visible&&!take(take(item->action(0)).interaction()).visible,
            "A custom trailing view did not hide inline editing controls");
    next.row.configuration.variant=ListItemVariant::action_group;next.primary_action.icon_name="missing-hidden-action-icon";
    good(item->set_options(next));(void)take(tree->drawing_snapshot({500,300}));
    require(take(child.interaction()).visible,"A hidden default action prevented the custom trailing view from rendering");
    next.primary_action.icon_name.clear();
    next.row.configuration.variant=ListItemVariant::checkable;good(item->set_options(next));(void)take(tree->drawing_snapshot({500,300}));
    auto selection=take(item->checkbox().snapshot());view=take(child.snapshot());
    require(take(item->checkbox().interaction()).visible&&selection.bounds.x+selection.bounds.width<=view.bounds.x,
            "The selection control overlapped a custom trailing view");
    good(item->checkbox().activate());require(changes==1,"A custom trailing view disabled the leading checkbox");
    auto invalid=take(item->options());invalid.trailing_width=std::numeric_limits<double>::quiet_NaN();
    const auto valid_options=take(item->options());
    require(!item->set_options(invalid)&&take(item->options())==valid_options,"Invalid trailing dimensions replaced the current row");
    next.row.configuration.variant=ListItemVariant::detail_settings;next.selector.options={{"First",true},{"Second",true}};
    good(item->set_options(next));(void)take(tree->drawing_snapshot({500,300}));
    auto selector=take(item->selector());auto selector_menu=take(selector.menu());
    good(take(selector.control()).activate());require(take(selector_menu.snapshot()).opened,"Compound selector did not open");
    auto stale_option=take(selector.option(1));next.row.configuration.variant=ListItemVariant::select;good(item->set_options(next));
    require(!take(selector_menu.snapshot()).opened,"Switching to a custom trailing view left the selector menu open");
    (void)stale_option.activate();require(selector_events==0,"A stale selector option fired after the trailing view replaced it");
    next.row.configuration.variant=ListItemVariant::detail_actions;good(item->set_options(next));(void)take(tree->drawing_snapshot({500,300}));
    require(!take(child.interaction()).visible&&take(take(item->action(0)).interaction()).visible,
            "A compound row lost its built-in controls to the simple-row trailing view");
    next.row.configuration.variant=ListItemVariant::navigation;next.trailing_width=90;next.trailing_height=30;
    good(item->set_options(next));(void)take(tree->drawing_snapshot({500,300}));
    require(near(take(child.snapshot()).bounds.width,90)&&near(take(child.snapshot()).bounds.height,30),
            "Changing trailing dimensions did not resize the view");
    next.row.configuration.width=20;good(item->set_options(next));frame=take(tree->drawing_snapshot({500,300}));
    require(near(take(child.snapshot()).bounds.width,16)&&!contains(frame,"Library"),
            "A narrow row overlapped its trailing view with text");
    next.row.configuration.width=0;next.trailing_width=0;good(item->set_options(next));frame=take(tree->drawing_snapshot({500,300}));
    require(!take(child.interaction()).visible&&frame.images.empty(),"A zero-sized custom view revealed the default trailing icon");
    next.trailing_width=90;next.row.configuration.variant=ListItemVariant::resource;next.more_items={{"Duplicate",true}};
    good(item->set_options(next));(void)take(tree->drawing_snapshot({500,300}));
    auto menu=take(item->action_menu());good(take(item->action(2)).activate());require(take(menu.snapshot()).opened,"Compound menu did not open");
    auto stale=take(item->menu_item(0));next.row.configuration.variant=ListItemVariant::action_group;good(item->set_options(next));
    require(!take(menu.snapshot()).opened,"Switching to a custom trailing view left the old menu open");
    (void)stale.activate();require(menu_events==0,"A stale menu item fired after the trailing view replaced its action");
    next.row.configuration.variant=ListItemVariant::navigation;good(item->set_options(next));
    next.row.configuration.enabled=false;good(item->set_options(next));good(child.activate());require(clicks==1,"Disabled row delivered trailing-view input");
    auto old=item->root();auto failed=host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=native::NativeListItemComposite::create(builder,{},"bad",input,{}, {.trailing=[](WidgetBuilder&)->ii::core::Result<void>{
            return ii::core::fail(ii::core::ErrorCode::failed,"Trailing view rejected");}});
        if(!value)return std::unexpected(value.error());return {};
    });
    require(!failed&&old.snapshot(),"Failed trailing-view construction replaced the active generation");
    good(host.clear_content());require(!child.snapshot()&&!item->set_options(input),"Retired trailing view or item remained usable");
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto stack=builder.vstack({},"outer",{.spacing=0,.alignment=StackAlignment::leading});if(!stack)return std::unexpected(stack.error());
        auto value=native::NativeListItemComposite::create(builder,*stack,"clipped",input,{}, {.trailing=[&](WidgetBuilder& nested)->ii::core::Result<void>{
            auto container=nested.absolute_stack({},"oversized",{200,100});if(!container)return std::unexpected(container.error());
            auto result=nested.action_surface(*container,"wide",{.text="Wide",.width=200,.height=100},[&]{++clicks;});if(!result)return std::unexpected(result.error());
            child=*result;return child.set_frame({0,0,200,100});
        }});
        if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    (void)take(tree->drawing_snapshot({500,300}));const auto before=clicks;
    const auto click=[&](float x,float y){good(tree->input({.kind=WidgetInputKind::pointer_down,.x=x,.y=y}));
        good(tree->input({.kind=WidgetInputKind::pointer_up,.x=x,.y=y}));};
    click(200,20);require(clicks==before,"Trailing-view input escaped its clipped bounds");
    click(220,20);require(clicks==before+1,"Clipped trailing view did not receive input inside its bounds");
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=native::NativeListItemComposite::create(builder,{},"self-retiring",input,{}, {.trailing=[&](WidgetBuilder& nested)->ii::core::Result<void>{
            auto result=nested.action_surface({},"remove",{.text="Remove"},[&]{good(host.clear_content());});
            if(!result)return std::unexpected(result.error());child=*result;return {};
        }});
        if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    good(child.activate());require(!item->root().snapshot(),"A trailing view could not retire its parent safely");
    native::ListItemCompositeInput automatic;automatic.row.configuration.variant=ListItemVariant::navigation;
    automatic.row.configuration.label="Automatic size";
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto stack=builder.vstack({},"automatic-stack",{.spacing=0,.alignment=StackAlignment::leading});if(!stack)return std::unexpected(stack.error());
        auto value=native::NativeListItemComposite::create(builder,*stack,"automatic",automatic,{}, {.trailing=[&](WidgetBuilder& nested)->ii::core::Result<void>{
            auto result=nested.action_surface({},"natural",{.text="Natural",.width=130,.height=60},[]{});
            if(!result)return std::unexpected(result.error());child=*result;return {};
        }});
        if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    (void)take(tree->drawing_snapshot({500,300}));view=take(child.snapshot());
    if(!near(view.bounds.width,130)||!near(view.bounds.height,60)||!near(take(item->root().snapshot()).size.height,60))
        throw std::runtime_error("The trailing view did not use its child's natural size: "+std::to_string(view.bounds.width)+" x "+
            std::to_string(view.bounds.height)+", row height "+std::to_string(take(item->root().snapshot()).size.height)+
            ", root implicit height "+std::to_string(take(item->root().interaction()).implicit_size.height)+
            ", child implicit "+std::to_string(take(child.interaction()).implicit_size.width)+" x "+std::to_string(take(child.interaction()).implicit_size.height)+
            ", failure "+std::to_string(bool(take(item->failure()))));
    auto child_options=take(child.action_options());child_options.width=80;child_options.height=24;good(child.set_action_options(child_options));
    (void)take(item->root().snapshot());(void)take(tree->drawing_snapshot({500,300}));view=take(child.snapshot());
    if(!near(view.bounds.width,80)||!near(view.bounds.height,24)||!near(take(item->root().snapshot()).size.height,44))
        throw std::runtime_error("The trailing view did not follow a change to its child's natural size: "+std::to_string(view.bounds.width)+" x "+
            std::to_string(view.bounds.height)+", row height "+std::to_string(take(item->root().snapshot()).size.height)+
            ", root implicit height "+std::to_string(take(item->root().interaction()).implicit_size.height)+
            ", child implicit "+std::to_string(take(child.interaction()).implicit_size.width)+" x "+std::to_string(take(child.interaction()).implicit_size.height)+
            ", failure "+std::to_string(bool(take(item->failure()))));
    child_options.width=160;child_options.height=80;good(child.set_action_options(child_options));
    (void)take(item->root().snapshot());view=take(child.snapshot());
    require(near(view.bounds.width,160)&&near(view.bounds.height,80)&&near(take(item->root().snapshot()).size.height,80),
            "The natural trailing view did not grow after shrinking");
    auto sizing=take(item->options());sizing.trailing_width=100;good(item->set_options(sizing));
    child_options.width=240;child_options.height=28;good(child.set_action_options(child_options));
    (void)take(item->root().snapshot());view=take(child.snapshot());
    require(near(view.bounds.width,100)&&near(view.bounds.height,28)&&near(take(item->root().snapshot()).size.height,44),
            "Explicit trailing width did not coexist with natural height");
    sizing.trailing_width.reset();good(item->set_options(sizing));(void)take(item->root().snapshot());view=take(child.snapshot());
    require(near(view.bounds.width,240)&&near(view.bounds.height,28),"Removing an explicit width did not restore the latest natural width");
    child_options.width=0;child_options.height=0;good(child.set_action_options(child_options));(void)take(item->root().snapshot());
    require(!take(child.interaction()).visible,"A zero-sized natural trailing view remained interactive");
    child_options.width=70;child_options.height=18;good(child.set_action_options(child_options));(void)take(item->root().snapshot());view=take(child.snapshot());
    require(take(child.interaction()).visible&&near(view.bounds.width,70)&&near(view.bounds.height,18),
            "A natural trailing view did not reappear after becoming empty");
    sizing.row.configuration.height=30;good(item->set_options(sizing));child_options.height=60;good(child.set_action_options(child_options));
    (void)take(item->root().snapshot());view=take(child.snapshot());
    require(near(take(item->root().snapshot()).size.height,30)&&near(view.bounds.height,30),
            "A fixed-height row did not constrain the natural trailing view");
    sizing.row.configuration.height=0;good(item->set_options(sizing));(void)take(item->root().snapshot());
    require(near(take(item->root().snapshot()).size.height,60),"Removing a fixed height did not restore the natural row height");
    tree->close();
}
