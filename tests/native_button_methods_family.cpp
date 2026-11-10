#include <LVUI/NativeAlertButton.h>
#include <iiGUI/NativeAlertButtonView.h>
#include <LVUI/NativeButton.h>
#include <LVUI/NativeComboBox.h>
#include <iiGUI/NativeComboBoxView.h>
#include <LVUI/NativeDropdownButton.h>
#include <LVUI/NativeStepper.h>
#include <iiGUI/NativeStepperView.h>
#include <iiGUI/NativeDropdownButtonView.h>
#include <iiGUI/SvgImageCache.h>
#include <iiGUI/WidgetState.h>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace lvui::native {
using NativeComboBox=ComboBoxBinding<ii::gui::BasicComboBox,ii::gui::BasicNativeComboBox,ii::gui::Widget>;
using ButtonEvent=ButtonEventValue<ii::gui::Widget>;
using ButtonMethod=ButtonMethodValue<ButtonEvent>;
using ButtonMethodRegistry=ButtonMethodRegistryValue<ButtonEvent>;
using ButtonInput=ButtonInputValue<ii::gui::RasterImage>;
using ButtonModel=ii::gui::BasicButton<ButtonHostModelPolicy>;
using NativeButton=ButtonBinding<ii::gui::BasicButton,ii::gui::BasicNativeButton,ii::gui::CachedSvgDecoder,ii::gui::Widget>;
using AlertButtonEvent=ButtonEventValue<ii::gui::Widget>;
using NativeAlertButtonCallbacks=NativeAlertButtonCallbacksValue<ii::gui::Widget>;
using AlertButtonModel=ii::gui::BasicAlertButton<AlertButtonModelPolicy<ButtonModel>>;
using NativeAlertButton=AlertButtonBinding<ii::gui::BasicButton,ii::gui::BasicNativeButton,
    ii::gui::BasicAlertButton,ii::gui::BasicNativeAlertButton,ii::gui::CachedSvgDecoder,ii::gui::Widget>;
using StepperModel=ii::gui::BasicStepper<StepperModelPolicy<ButtonModel>>;
using NativeStepper=StepperBinding<ii::gui::BasicButton,ii::gui::BasicStepper,ii::gui::BasicNativeStepper,ii::gui::Widget>;
using DropdownInput=DropdownInputValue<ii::gui::RasterImage>;
using DropdownButtonModel=ii::gui::BasicDropdownButton<DropdownModelPolicy<ButtonModel>>;
using NativeDropdownButton=DropdownBinding<ii::gui::BasicButton,ii::gui::BasicDropdownButton,ii::gui::BasicNativeDropdownButton,ii::gui::CachedSvgDecoder,ii::gui::Widget>;
}
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> result){if(!result)throw std::runtime_error(result.error().message);return std::move(*result);}
void good(ii::core::Result<void> result){if(!result)throw std::runtime_error(result.error().message);}

void clipped_programmatic_activation(){
    using namespace ii::gui;using lvui::native::NativeButton;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"clip-host",{100,100}));Widget root;
    std::optional<NativeButton> button;std::vector<std::string> events;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        root=take(builder.absolute_stack({},"root",{100,100}));good(root.set_clip_content(true));
        button=take(NativeButton::create(builder,root,"outside",{},[&]{events.emplace_back("clicked");}));
        return button->root().set_frame({10,140,60,30});
    }));
    (void)take(tree->drawing_snapshot({100,100}));
    require(!take(button->root().snapshot()).visible&&take(button->root().interaction()).visible,
            "Expected a logically visible button outside the clipping region");
    good(button->set_method([&](const auto&){events.emplace_back("method");}));
    good(button->root().activate());
    require(events==std::vector<std::string>{"method","clicked"},"Clipping suppressed a programmatic button action");
    good(root.set_visible(false));require(!button->root().activate(),"A hidden button accepted programmatic activation");
    require(events==std::vector<std::string>{"method","clicked"},"A hidden button delivered a stale action");
    tree->close();
}

template<class Control,class Factory,class Activate>
void contract(Factory factory,Activate activate,bool stepped){
    using namespace ii::gui;using lvui::native::ButtonEvent;using lvui::native::ButtonMethod;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{180,100}));std::optional<Control> control;
    std::vector<std::string> events;bool replace=true;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        control=take(factory(builder,[&]{events.emplace_back("clicked");},[&]{events.emplace_back("stepped");}));return {};
    }));
    (void)take(tree->drawing_snapshot({180,100}));
    const auto fire=[&]{good(activate(*control));};
    good(control->set_method([&](const ButtonEvent& event){
        require(event.source.snapshot().has_value()&&event.trigger=="clicked"&&event.enabled&&event.effective_enabled&&event.tone.has_value(),
                "Button-family method event is invalid");events.emplace_back("method");
        if(replace){replace=false;good(control->set_methods({[&](const auto&){events.emplace_back("new");}}));}
    }));
    good(control->set_methods({[&](const auto&){events.emplace_back("old1");},[&](const auto&){events.emplace_back("old2");}}));
    require(take(control->has_methods()),"Button-family registry did not retain its methods");fire();
    auto expected=std::vector<std::string>{"method","old1","old2","clicked"};if(stepped)expected.emplace_back("stepped");
    require(events==expected,"Reentrant button-family replacement altered the active dispatch");
    events.clear();fire();expected={"method","new","clicked"};if(stepped)expected.emplace_back("stepped");
    require(events==expected,"Reentrant button-family replacement did not apply to the next dispatch");
    require(!control->set_methods(std::vector<ButtonMethod>(257,[](const auto&){})),"Button-family registry accepted too many methods");
    events.clear();fire();require(events==expected,"Rejected button-family methods replaced the live registry");
    bool foreign=false;std::thread worker([&]{auto result=control->set_method({});foreign=!result&&result.error().code==ii::core::ErrorCode::wrong_executor;});
    worker.join();require(foreign,"Button-family registry accepted a foreign thread");
    events.clear();good(control->set_method([&](const auto&){events.emplace_back("disable");good(control->root().set_enabled(false));}));
    fire();require(events==std::vector<std::string>{"disable"},"A disabled button delivered stale methods or click");
    events.clear();good(control->set_method([&](const ButtonEvent& event){
        require(event.trigger=="manual test"&&event.enabled&&!event.effective_enabled&&event.tone.has_value(),
                "Disabled manual button event is invalid");events.emplace_back("manual method");}));
    good(control->set_methods({[&](const auto&){events.emplace_back("manual list");}}));
    good(control->invoke_methods("manual test"));
    require(events==std::vector<std::string>{"manual method","manual list"},
            "Manual methods clicked, stepped, or rejected a disabled button");
    events.clear();good(control->set_method([&](const ButtonEvent& event){
        require(event.trigger=="clicked"&&!event.effective_enabled,"Default manual button event is invalid");
        events.emplace_back("default manual");}));
    good(control->set_methods({}));good(control->invoke_methods());
    require(events==std::vector<std::string>{"default manual"},"Default manual invocation delivered an unexpected callback");
    bool foreign_invoke=false;std::thread manual_worker([&]{auto result=control->invoke_methods();
        foreign_invoke=!result&&result.error().code==ii::core::ErrorCode::wrong_executor;});
    manual_worker.join();require(foreign_invoke,"Manual button invocation accepted a foreign thread");
    good(control->root().set_enabled(true));events.clear();
    good(control->set_method([&](const auto&){events.emplace_back("retire");good(host.clear_content());}));
    fire();require(events==std::vector<std::string>{"retire"},"A retired button delivered stale methods or click");
    require(!control->set_methods({}),"A retired button accepted a method list");
    require(!control->invoke_methods(),"A retired button accepted manual invocation");tree->close();
}
}
int main(){
    using namespace lvui::native;
    clipped_programmatic_activation();
    const auto click=[](auto& control){return control.root().activate();};
    contract<NativeButton>([](auto& builder,auto clicked,auto){return NativeButton::create(builder,{},"button",{},std::move(clicked));},click,false);
    contract<NativeAlertButton>([](auto& builder,auto clicked,auto){NativeAlertButtonCallbacks callbacks;callbacks.clicked=std::move(clicked);
        return NativeAlertButton::create(builder,{},"alert",{},std::move(callbacks));},click,false);
    contract<NativeDropdownButton>([](auto& builder,auto clicked,auto){NativeDropdownCallbacks callbacks;callbacks.clicked=std::move(clicked);
        return NativeDropdownButton::create(builder,{},"dropdown",{},std::move(callbacks));},click,false);
    contract<NativeComboBox>([](auto& builder,auto clicked,auto){NativeComboBoxCallbacks callbacks;callbacks.clicked=std::move(clicked);
        return NativeComboBox::create(builder,{},"combo",{},std::move(callbacks));},click,false);
    contract<NativeStepper>([](auto& builder,auto clicked,auto stepped){NativeStepperCallbacks callbacks;callbacks.clicked=std::move(clicked);
        callbacks.stepped=[stepped=std::move(stepped)](int){stepped();};return NativeStepper::create(builder,{},"stepper",{},std::move(callbacks));},
        [](auto& control){return control.root().step(1);},true);
}
