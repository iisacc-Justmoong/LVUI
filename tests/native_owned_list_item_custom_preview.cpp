#include <LVUI/NativeListItemComposite.h>
#include "list_item_composite_test_binding.h"
#include <iiGUI/WidgetState.h>

#include <cmath>
#include <limits>
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
    native::ListItemCompositeInput input;input.row.configuration.variant=ListItemVariant::resource;
    input.row.configuration.label="Clip";input.preview_icon_name="missing-custom-preview-icon";
    input.preview=take(ii::gui::rgba_image(1,1,{255,0,0,255}));
    ii::gui::Widget child;unsigned clicks=0;
    const WidgetFactory preview=[&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto result=builder.action_surface({},"custom-preview",{.text="Preview",.width=48,.height=48},[&]{++clicks;});
        if(!result)return std::unexpected(result.error());child=*result;return {};
    };
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto stack=builder.vstack({},"stack",{.spacing=0,.alignment=StackAlignment::leading});if(!stack)return std::unexpected(stack.error());
        auto value=native::NativeListItemComposite::create(builder,*stack,"item",input,{}, {.preview=preview});
        if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    const auto drawing=take(tree->drawing_snapshot({500,300}));auto root=take(item->root().snapshot()),visual=take(child.snapshot());
    require(near(root.size.height,129)&&near(visual.bounds.x,12)&&near(visual.bounds.width,48)&&near(visual.bounds.height,48),
            "Custom resource preview did not occupy the preview slot");
    for(const auto& image:drawing.images)require(image.image!=input.preview,"Custom preview still painted the default image");
    for(const auto& layer:drawing.overlays)for(const auto& image:layer.images)
        require(image.image!=input.preview,"Custom preview still painted the default overlay image");
    good(child.activate());require(clicks==1,"Custom preview did not receive input");
    auto next=take(item->options());next.row.configuration.enabled=false;good(item->set_options(next));
    good(child.activate());require(clicks==1,"Disabled item delivered preview input");
    next.row.configuration.enabled=true;next.show_preview=false;good(item->set_options(next));(void)take(tree->drawing_snapshot({500,300}));
    require(!take(child.interaction()).visible,"Hidden custom preview remained visible");
    next.show_preview=true;next.preview_size=80;good(item->set_options(next));(void)take(tree->drawing_snapshot({500,300}));
    require(near(take(item->root().snapshot()).size.height,161)&&near(take(child.snapshot()).bounds.width,80),
            "Preview size change did not update the custom slot");
    next.row.configuration.variant=ListItemVariant::media;good(item->set_options(next));(void)take(tree->drawing_snapshot({500,300}));
    require(near(take(item->root().snapshot()).size.height,145)&&take(child.interaction()).visible,
            "Custom preview did not survive a resource-to-media change");
    next.row.configuration.variant=ListItemVariant::form;good(item->set_options(next));(void)take(tree->drawing_snapshot({500,300}));
    require(!take(child.interaction()).visible,"A non-preview row displayed the custom preview");
    next.row.configuration.variant=ListItemVariant::resource;next.preview_size=0;good(item->set_options(next));(void)take(tree->drawing_snapshot({500,300}));
    require(!take(child.interaction()).visible,"A zero-sized preview was still visible");
    next.preview_size=48;good(item->set_options(next));(void)take(tree->drawing_snapshot({500,300}));
    require(take(child.interaction()).visible,"Restoring preview size did not restore the retained view");
    auto old=item->root();auto failed=host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=native::NativeListItemComposite::create(builder,{},"bad",input,{}, {.preview=[](WidgetBuilder&)->ii::core::Result<void>{
            return ii::core::fail(ii::core::ErrorCode::failed,"Preview rejected");}});
        if(!value)return std::unexpected(value.error());return {};
    });
    require(!failed&&old.snapshot(),"Failed preview construction replaced the active generation");
    good(host.clear_content());require(!child.snapshot()&&!item->set_options(input),"Retired preview or item remained usable");
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto stack=builder.vstack({},"outer",{.spacing=0,.alignment=StackAlignment::leading});if(!stack)return std::unexpected(stack.error());
        auto fixed=input;fixed.preview_size=48;
        auto value=native::NativeListItemComposite::create(builder,*stack,"clipped",fixed,{}, {.preview=[&](WidgetBuilder& nested)->ii::core::Result<void>{
            auto container=nested.absolute_stack({},"oversized",{200,200});if(!container)return std::unexpected(container.error());
            auto result=nested.action_surface(*container,"wide",{.text="Wide",.width=200,.height=200},[&]{++clicks;});if(!result)return std::unexpected(result.error());
            child=*result;return child.set_frame({0,0,200,200});
        }});
        if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    (void)take(tree->drawing_snapshot({500,300}));const auto before=clicks;
    const auto click=[&](float x,float y){good(tree->input({.kind=WidgetInputKind::pointer_down,.x=x,.y=y}));
        good(tree->input({.kind=WidgetInputKind::pointer_up,.x=x,.y=y}));};
    click(80,20);require(clicks==before,"Preview input escaped its clipped bounds");
    click(20,20);require(clicks==before+1,"Clipped preview did not receive input inside its bounds");
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=native::NativeListItemComposite::create(builder,{},"self-retiring",input,{}, {.preview=[&](WidgetBuilder& nested)->ii::core::Result<void>{
            auto result=nested.action_surface({},"remove",{.text="Remove"},[&]{good(host.clear_content());});
            if(!result)return std::unexpected(result.error());child=*result;return {};
        }});
        if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    good(child.activate());require(!item->root().snapshot(),"A preview could not retire its parent safely");
    native::ListItemCompositeInput automatic;automatic.row.configuration.variant=ListItemVariant::resource;
    automatic.row.configuration.label="Natural preview";
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto stack=builder.vstack({},"automatic-stack",{.spacing=0,.alignment=StackAlignment::leading});if(!stack)return std::unexpected(stack.error());
        auto value=native::NativeListItemComposite::create(builder,*stack,"automatic",automatic,{}, {.preview=[&](WidgetBuilder& nested)->ii::core::Result<void>{
            auto result=nested.action_surface({},"natural-preview",{.text="Natural",.width=70,.height=90},[]{});
            if(!result)return std::unexpected(result.error());child=*result;return {};
        }});
        if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    (void)take(item->root().snapshot());visual=take(child.snapshot());
    require(near(visual.bounds.width,70)&&near(visual.bounds.height,90)&&near(take(item->root().snapshot()).size.height,171),
            "Custom preview did not use its rectangular natural size");
    auto sizing=take(item->options()),invalid=sizing;invalid.preview_size=std::numeric_limits<double>::quiet_NaN();
    require(!item->set_options(invalid)&&take(item->options())==sizing,"Invalid preview size replaced the current row");
    sizing.row.configuration.variant=ListItemVariant::media;good(item->set_options(sizing));(void)take(item->root().snapshot());
    require(near(take(item->root().snapshot()).size.height,155)&&take(child.interaction()).visible,
            "Natural preview dimensions were lost in a media row");
    sizing.row.configuration.variant=ListItemVariant::resource;sizing.show_preview=false;good(item->set_options(sizing));
    auto child_options=take(child.action_options());child_options.width=110;child_options.height=24;good(child.set_action_options(child_options));
    (void)take(item->root().snapshot());require(!take(child.interaction()).visible,"Hidden natural preview remained visible");
    sizing.show_preview=true;good(item->set_options(sizing));(void)take(item->root().snapshot());visual=take(child.snapshot());
    require(near(visual.bounds.width,110)&&near(visual.bounds.height,24)&&near(take(item->root().snapshot()).size.height,120),
            "Custom preview did not update its layout after the child resized");
    sizing.preview_size=60;good(item->set_options(sizing));(void)take(item->root().snapshot());visual=take(child.snapshot());
    require(near(visual.bounds.width,60)&&near(visual.bounds.height,60)&&near(take(item->root().snapshot()).size.height,141),
            "Explicit preview size did not override both natural dimensions");
    sizing.preview_size.reset();good(item->set_options(sizing));(void)take(item->root().snapshot());visual=take(child.snapshot());
    require(near(visual.bounds.width,110)&&near(visual.bounds.height,24),"Removing preview size did not restore the latest natural dimensions");
    child_options.width=0;child_options.height=0;good(child.set_action_options(child_options));(void)take(item->root().snapshot());
    require(!take(child.interaction()).visible,"Zero-sized custom preview remained visible");
    child_options.width=64;child_options.height=35;good(child.set_action_options(child_options));(void)take(item->root().snapshot());visual=take(child.snapshot());
    require(take(child.interaction()).visible&&near(visual.bounds.width,64)&&near(visual.bounds.height,35),
            "Empty custom preview did not recover its natural dimensions");
    sizing.row.configuration.height=50;good(item->set_options(sizing));child_options.height=80;good(child.set_action_options(child_options));
    (void)take(item->root().snapshot());visual=take(child.snapshot());
    require(near(take(item->root().snapshot()).size.height,50)&&near(visual.bounds.height,38),
            "Fixed item height did not clip the natural preview");
    sizing.row.configuration.height=0;good(item->set_options(sizing));child_options.height=8192;good(child.set_action_options(child_options));
    (void)take(item->root().snapshot());
    require(near(take(item->root().interaction()).implicit_size.height,8192)&&!take(item->failure()),
            "An oversized natural preview failed instead of respecting the row size limit");
    tree->close();
}
