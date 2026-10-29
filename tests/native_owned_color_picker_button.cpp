#include <LVUI/NativeColorPickerButton.h>
#include <LVUI/ColorSelection.h>
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
struct Source final:lvui::ColorSource {
    lvui::StyleColor current{1,0,0,.5};mutable std::uint64_t version=0;mutable bool churn=false;
    mutable std::function<void()> on_read;
    [[nodiscard]] lvui::ColorSourceResult color()const override{
        auto callback=std::move(on_read);if(callback)callback();if(churn)++version;return current;
    }
    [[nodiscard]] std::uint64_t revision()const noexcept override{return version;}
};
const ii::gui::ImageRun* ring(const ii::gui::CanvasFrame& frame){
    const auto find=[](const auto& images)->const ii::gui::ImageRun*{auto it=std::ranges::find_if(images,[](const auto& image){return image.image&&image.image->width()==320;});return it==images.end()?nullptr:&*it;};
    if(auto found=find(frame.images))return found;
    for(const auto& layer:frame.overlays)if(auto found=find(layer.images))return found;return nullptr;
}
}
int main(){
    using namespace ii::gui;auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{240,100}));std::optional<lvui::native::NativeColorPickerButton> button;
    auto source=std::make_shared<Source>();lvui::native::ColorPickerButtonInput input;input.source=source;std::vector<std::string> events;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto stack=builder.vstack({},"stack");if(!stack)return std::unexpected(stack.error());
        auto result=lvui::native::NativeColorPickerButton::create(builder,*stack,"color",input,
            {.clicked=[&]{events.push_back("click");},.pressed=[&]{events.push_back("press");},.released=[&]{events.push_back("release");}});
        if(!result)return std::unexpected(result.error());button=*result;return {};
    }));
    auto snapshot=take(button->root().snapshot());require(snapshot.kind==WidgetKind::action_surface&&snapshot.bounds.width==28&&snapshot.bounds.height==28&&
        snapshot.accessibility_name=="Choose color"&&snapshot.accessibility_description=="#FF000080","Color button did not publish its source or accessibility");
    auto frame=take(tree->drawing_snapshot({240,100}));require(ring(frame)&&ring(frame)->image->rgba()[(160*320+160)*4+3]==0,
        "Color button did not submit the built-in hue ring");
    good(button->root().focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));good(tree->input({.kind=WidgetInputKind::key_up,.key=WidgetKey::enter}));
    require(events==std::vector<std::string>({"press","release","click"}),"Color button keyboard callbacks are out of order");
    source->current={0,1,0,1};++source->version;++input.source_revision;good(button->set_options(input));
    require(take(button->root().snapshot()).accessibility_description=="#00FF00FF","Color source update was not reflected");
    source->current.blue=2;++source->version;++input.source_revision;
    require(!button->set_options(input)&&take(button->root().snapshot()).accessibility_description=="#00FF00FF","Invalid color source changed the button");
    source->current.blue=0;source->churn=true;
    require(!button->set_options(input)&&take(button->root().snapshot()).accessibility_description=="#00FF00FF","Unstable color source changed the button");
    source->churn=false;good(button->set_options(input));
    auto nested=input;nested.source.reset();nested.configuration.current_color={0,0,1,1};
    source->on_read=[&]{good(button->set_options(nested));};++input.source_revision;good(button->set_options(input));
    require(take(button->root().snapshot()).accessibility_description=="#0000FFFF","Reentrant source read overwrote a newer button update");
    auto selection=std::make_shared<lvui::ColorSelection>(lvui::StyleColor{1,0,0,1});nested.source=selection;
    good(button->set_options(nested));auto edit=selection->begin();require(bool(edit),"Shared color selection did not start");
    require(bool(selection->edit(*edit,{0,1,0,1})),"Shared color selection edit failed");nested.source_revision=std::int64_t(selection->revision());
    good(button->set_options(nested));require(take(button->root().snapshot()).accessibility_description=="#00FF00FF","Color button did not read an edited selection");
    require(bool(selection->cancel(*edit)),"Shared color selection cancel failed");nested.source_revision=std::int64_t(selection->revision());
    good(button->set_options(nested));require(take(button->root().snapshot()).accessibility_description=="#FF0000FF","Color button did not reflect a canceled selection");
    nested.configuration.enabled=false;good(button->set_options(nested));snapshot=take(button->root().snapshot());
    auto disabled=take(tree->drawing_snapshot({240,100}));require(!snapshot.enabled&&ring(disabled)&&std::abs(ring(disabled)->opacity-.32f)<.001,
        "Disabled color button did not apply its design opacity");
    nested.configuration.show_hue_ring=false;good(button->set_options(nested));
    require(!ring(take(tree->drawing_snapshot({240,100}))),"Hidden hue ring remained visible");
    bool rejected=false;std::thread worker([&]{auto result=button->set_options(nested);rejected=!result&&result.error().code==ii::core::ErrorCode::wrong_executor;});worker.join();
    require(rejected,"Color button accepted a foreign-thread update");good(host.clear_content());require(!button->options(),"Retired color button accepted access");tree->close();
}
