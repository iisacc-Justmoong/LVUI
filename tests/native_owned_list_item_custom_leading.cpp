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
double text_x(const ii::gui::CanvasFrame& frame,std::string_view text){
    for(const auto& run:frame.texts)if(run.text==text)return run.bounds.x;
    for(const auto& layer:frame.overlays)for(const auto& run:layer.texts)if(run.text==text)return run.bounds.x;return -1;
}
}
int main(){
    using namespace ii::gui;using namespace lvui;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{500,260}));std::optional<native::NativeListItemComposite> item;
    native::ListItemCompositeInput input;input.row.configuration.variant=ListItemVariant::navigation;
    input.row.configuration.label="Library";input.row.configuration.icon_name="missing-custom-leading-icon";input.leading_size=14;
    ii::gui::Widget child;unsigned clicks=0;
    const WidgetFactory leading=[&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto result=builder.action_surface({},"custom-leading",{.text="L",.width=14,.height=14},[&]{++clicks;});
        if(!result)return std::unexpected(result.error());child=*result;return {};
    };
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto stack=builder.vstack({},"stack",{.spacing=0,.alignment=StackAlignment::leading});if(!stack)return std::unexpected(stack.error());
        auto value=native::NativeListItemComposite::create(builder,*stack,"item",input,{}, {.leading=leading});
        if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    auto frame=take(tree->drawing_snapshot({500,260}));auto view=take(child.snapshot());
    require(near(view.bounds.x,4)&&near(view.bounds.width,14)&&near(view.bounds.height,14)&&near(text_x(frame,"Library"),24),
            "Custom leading view did not reserve the normal icon area");
    good(child.activate());require(clicks==1,"Custom leading view did not receive input");
    auto next=take(item->options());next.show_leading_icon=false;good(item->set_options(next));frame=take(tree->drawing_snapshot({500,260}));
    require(!take(child.interaction()).visible&&near(text_x(frame,"Library"),4),"Hiding the leading view did not release its space");
    next.show_leading_icon=true;next.leading_size=18;next.row.configuration.variant=ListItemVariant::detail_actions;good(item->set_options(next));
    (void)take(tree->drawing_snapshot({500,260}));view=take(child.snapshot());
    require(near(view.bounds.x,12)&&near(view.bounds.width,18)&&take(child.interaction()).visible,
            "Custom leading view did not occupy the detail header");
    next.row.configuration.variant=ListItemVariant::detail_settings;good(item->set_options(next));(void)take(tree->drawing_snapshot({500,260}));
    require(take(child.interaction()).visible,"Settings lost its custom leading view");
    next.row.configuration.variant=ListItemVariant::detail_quantity;good(item->set_options(next));(void)take(tree->drawing_snapshot({500,260}));
    require(!take(child.interaction()).visible,"Quantity detail unexpectedly displayed a leading view");
    next.row.configuration.variant=ListItemVariant::checkable;good(item->set_options(next));(void)take(tree->drawing_snapshot({500,260}));
    require(!take(child.interaction()).visible,"A checkable row unexpectedly displayed a leading view");
    next.row.configuration.variant=ListItemVariant::navigation;next.row.configuration.icon_size=24;next.detail_icon_size=24;next.leading_size=24;
    good(item->set_options(next));(void)take(tree->drawing_snapshot({500,260}));
    require(near(take(child.snapshot()).bounds.width,24),"Changing icon size did not resize the leading view");
    next.row.configuration.width=20;good(item->set_options(next));frame=take(tree->drawing_snapshot({500,260}));
    require(near(take(child.snapshot()).bounds.width,12)&&text_x(frame,"Library")<0,
            "A narrow row overlapped its leading view with text");
    next.row.configuration.width=0;good(item->set_options(next));
    next.row.configuration.enabled=false;good(item->set_options(next));good(child.activate());require(clicks==1,"Disabled row delivered leading-view input");
    auto old=item->root();auto failed=host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=native::NativeListItemComposite::create(builder,{},"bad",input,{}, {.leading=[](WidgetBuilder&)->ii::core::Result<void>{
            return ii::core::fail(ii::core::ErrorCode::failed,"Leading view rejected");}});
        if(!value)return std::unexpected(value.error());return {};
    });
    require(!failed&&old.snapshot(),"Failed leading-view construction replaced the active generation");
    good(host.clear_content());require(!child.snapshot()&&!item->set_options(input),"Retired leading view or item remained usable");
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto stack=builder.vstack({},"outer",{.spacing=0,.alignment=StackAlignment::leading});if(!stack)return std::unexpected(stack.error());
        auto value=native::NativeListItemComposite::create(builder,*stack,"clipped",input,{}, {.leading=[&](WidgetBuilder& nested)->ii::core::Result<void>{
            auto container=nested.absolute_stack({},"oversized",{100,100});if(!container)return std::unexpected(container.error());
            auto result=nested.action_surface(*container,"wide",{.text="Wide",.width=100,.height=100},[&]{++clicks;});if(!result)return std::unexpected(result.error());
            child=*result;return child.set_frame({0,0,100,100});
        }});
        if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    (void)take(tree->drawing_snapshot({500,260}));const auto before=clicks;
    const auto click=[&](float x,float y){good(tree->input({.kind=WidgetInputKind::pointer_down,.x=x,.y=y}));
        good(tree->input({.kind=WidgetInputKind::pointer_up,.x=x,.y=y}));};
    click(40,20);require(clicks==before,"Leading-view input escaped its clipped bounds");
    click(10,20);require(clicks==before+1,"Clipped leading view did not receive input inside its bounds");
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=native::NativeListItemComposite::create(builder,{},"self-retiring",input,{}, {.leading=[&](WidgetBuilder& nested)->ii::core::Result<void>{
            auto result=nested.action_surface({},"remove",{.text="Remove"},[&]{good(host.clear_content());});
            if(!result)return std::unexpected(result.error());child=*result;return {};
        }});
        if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    good(child.activate());require(!item->root().snapshot(),"A leading view could not retire its parent safely");
    native::ListItemCompositeInput automatic;automatic.row.configuration.variant=ListItemVariant::navigation;
    automatic.row.configuration.label="Natural leading";
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto stack=builder.vstack({},"automatic-stack",{.spacing=0,.alignment=StackAlignment::leading});if(!stack)return std::unexpected(stack.error());
        auto value=native::NativeListItemComposite::create(builder,*stack,"automatic",automatic,{}, {.leading=[&](WidgetBuilder& nested)->ii::core::Result<void>{
            auto result=nested.action_surface({},"natural-leading",{.text="Natural",.width=80,.height=60},[]{});
            if(!result)return std::unexpected(result.error());child=*result;return {};
        }});
        if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    (void)take(item->root().snapshot());view=take(child.snapshot());frame=take(tree->drawing_snapshot({500,260}));
    if(!near(view.bounds.width,80)||!near(view.bounds.height,60)||!near(take(item->root().snapshot()).size.height,60)||
       !near(text_x(frame,"Natural leading"),90))
        throw std::runtime_error("Custom leading view did not use its rectangular natural size: "+
            std::to_string(view.bounds.width)+" x "+std::to_string(view.bounds.height)+", row height "+
            std::to_string(take(item->root().snapshot()).size.height)+", label x "+std::to_string(text_x(frame,"Natural leading")));
    auto sizing=take(item->options()),invalid=sizing;invalid.leading_size=std::numeric_limits<double>::quiet_NaN();
    require(!item->set_options(invalid)&&take(item->options())==sizing,"Invalid leading size replaced the current row");
    sizing.show_leading_icon=false;good(item->set_options(sizing));
    auto child_options=take(child.action_options());child_options.width=20;child_options.height=12;good(child.set_action_options(child_options));
    (void)take(item->root().snapshot());require(!take(child.interaction()).visible,"Hidden natural leading view remained visible");
    sizing.show_leading_icon=true;good(item->set_options(sizing));(void)take(item->root().snapshot());
    view=take(child.snapshot());frame=take(tree->drawing_snapshot({500,260}));
    require(near(view.bounds.width,20)&&near(view.bounds.height,12)&&near(take(item->root().snapshot()).size.height,44)&&
            near(text_x(frame,"Natural leading"),30),"Custom leading view did not update its layout after resizing");
    sizing.row.configuration.variant=ListItemVariant::detail_actions;good(item->set_options(sizing));
    child_options.width=70;child_options.height=60;good(child.set_action_options(child_options));(void)take(item->root().snapshot());view=take(child.snapshot());
    require(near(view.bounds.width,70)&&near(view.bounds.height,60)&&near(take(item->root().snapshot()).size.height,141),
            "Detail actions did not follow the natural leading view");
    sizing.row.configuration.variant=ListItemVariant::detail_settings;good(item->set_options(sizing));(void)take(item->root().snapshot());
    require(near(take(item->root().snapshot()).size.height,191),"Detail settings did not reserve the natural leading height");
    sizing.row.configuration.variant=ListItemVariant::navigation;sizing.leading_size=30;good(item->set_options(sizing));(void)take(item->root().snapshot());view=take(child.snapshot());
    require(near(view.bounds.width,30)&&near(view.bounds.height,30),"Explicit leading size did not override the natural dimensions");
    sizing.leading_size.reset();good(item->set_options(sizing));(void)take(item->root().snapshot());view=take(child.snapshot());
    require(near(view.bounds.width,70)&&near(view.bounds.height,60),"Removing leading size did not restore the latest natural dimensions");
    child_options.width=0;child_options.height=0;good(child.set_action_options(child_options));(void)take(item->root().snapshot());
    require(!take(child.interaction()).visible,"Zero-sized custom leading view remained visible");
    child_options.width=50;child_options.height=24;good(child.set_action_options(child_options));(void)take(item->root().snapshot());view=take(child.snapshot());
    require(take(child.interaction()).visible&&near(view.bounds.width,50)&&near(view.bounds.height,24),
            "Empty custom leading view did not recover its natural dimensions");
    sizing.row.configuration.height=30;good(item->set_options(sizing));child_options.height=80;good(child.set_action_options(child_options));
    (void)take(item->root().snapshot());view=take(child.snapshot());
    require(near(take(item->root().snapshot()).size.height,30)&&near(view.bounds.height,30),
            "Fixed item height did not clip the natural leading view");
    sizing.row.configuration.height=0;sizing.row.configuration.variant=ListItemVariant::detail_actions;good(item->set_options(sizing));
    child_options.height=8192;good(child.set_action_options(child_options));(void)take(item->root().snapshot());
    require(near(take(item->root().interaction()).implicit_size.height,8192)&&!take(item->failure()),
            "An oversized natural leading view failed instead of respecting the row size limit");
    ii::gui::Widget mixed_leading,mixed_trailing;native::ListItemCompositeInput mixed;
    mixed.row.configuration.variant=ListItemVariant::navigation;mixed.row.configuration.label="Dual";mixed.row.configuration.width=160;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto stack=builder.vstack({},"mixed-stack",{.spacing=0,.alignment=StackAlignment::leading});if(!stack)return std::unexpected(stack.error());
        auto value=native::NativeListItemComposite::create(builder,*stack,"mixed",mixed,{},
            {.leading=[&](WidgetBuilder& nested)->ii::core::Result<void>{
                auto result=nested.action_surface({},"mixed-leading",{.text="L",.width=70,.height=24},[]{});
                if(!result)return std::unexpected(result.error());mixed_leading=*result;return {};
            },.trailing=[&](WidgetBuilder& nested)->ii::core::Result<void>{
                auto result=nested.action_surface({},"mixed-trailing",{.text="R",.width=80,.height=24},[]{});
                if(!result)return std::unexpected(result.error());mixed_trailing=*result;return {};
            }});
        if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    (void)take(item->root().snapshot());auto first=take(mixed_leading.snapshot()),last=take(mixed_trailing.snapshot());
    frame=take(tree->drawing_snapshot({500,260}));
    require(first.bounds.x+first.bounds.width<=last.bounds.x&&text_x(frame,"Dual")<0,
            "Two natural views overlapped in a narrow row");
    auto first_options=take(mixed_leading.action_options()),last_options=take(mixed_trailing.action_options());
    first_options.width=30;last_options.width=30;good(mixed_leading.set_action_options(first_options));good(mixed_trailing.set_action_options(last_options));
    (void)take(item->root().snapshot());first=take(mixed_leading.snapshot());last=take(mixed_trailing.snapshot());
    frame=take(tree->drawing_snapshot({500,260}));
    require(near(first.bounds.width,30)&&near(last.bounds.width,30)&&near(text_x(frame,"Dual"),40)&&
            first.bounds.x+first.bounds.width<=last.bounds.x,"Two natural views did not settle after resizing together");
    tree->close();
}
