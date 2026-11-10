#include "native_list_item_segments_composition.h"
#include <iiGUI/WidgetState.h>

#include <memory>
#include <functional>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
struct Command final:lvui::ListFooterCommand{int* count=nullptr;std::function<void()> action;void invoke()const override{++*count;if(action)action();}};
}
int main(){
    using namespace lvui;using namespace ii::gui;
    native::ListItemSegmentsInput input;int commands=0;auto method=std::make_shared<Command>();method->count=&commands;
    input.configuration.options={{"Original",true,{}},{"Unavailable",false,{}},{"Edited",true,method}};
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{400,100}));std::optional<native::NativeListItemSegments> segments;
    std::vector<int> edits;bool replace=false;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto stack=builder.vstack({},"stack",{.spacing=0,.alignment=StackAlignment::leading});if(!stack)return std::unexpected(stack.error());
        auto value=native::NativeListItemSegments::create(builder,*stack,"segments",input,[&](int index){edits.push_back(index);
            auto next=take(segments->options());next.configuration.selected_index=index;
            if(replace){next.configuration.options={{"Replacement",true,{}}};next.configuration.selected_index=0;}
            good(segments->set_options(next));});
        if(!value)return std::unexpected(value.error());segments=*value;return {};
    }));
    auto first=take(segments->segment(0)),disabled=take(segments->segment(1)),last=take(segments->segment(2));
    require(take(first.snapshot()).checked&&!take(last.snapshot()).checked&&take(first.action_options()).role==ActionRole::radio_button,
            "Initial segment selection or accessible role changed");
    good(first.activate());require(take(first.snapshot()).checked&&edits.empty(),"Reactivating the selected segment toggled it off");
    good(disabled.activate());require(edits.empty(),"Disabled segment produced an edit");
    good(last.activate());require(edits==std::vector<int>{2}&&commands==1&&take(last.snapshot()).checked&&!take(first.snapshot()).checked,
            "Segment selection or its method was lost");
    good(last.activate());require(commands==2&&edits.size()==1&&take(last.snapshot()).checked,"Repeated selected action lost its method or selection");
    good(last.focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::left}));
    require(edits==std::vector<int>({2,0})&&take(first.snapshot()).checked,"Keyboard navigation did not skip disabled segments");
    auto accented=take(segments->options());accented.accent=WindowAccentPalette{.primary=figma_defaults::accent_purple_default,
        .selected=figma_defaults::accent_purple_selected};good(segments->set_options(accented));
    require(take(first.snapshot()).checked&&take(first.snapshot()).focused&&take(segments->selected_index())==0&&
            take(first.snapshot()).id==take(take(segments->segment(0)).snapshot()).id,
            "Restyling segments lost selection, focus, or identity");
    auto invalid_accent=accented;invalid_accent.accent->primary.red=-1;
    require(!segments->set_options(invalid_accent)&&take(segments->options())==accented,"Invalid segment accent was committed");
    accented.accent.reset();good(segments->set_options(accented));
    require(take(first.snapshot()).checked&&take(first.snapshot()).focused&&take(segments->selected_index())==0,
            "Restoring the default palette disturbed segment state");
    replace=true;good(last.activate());replace=false;auto replacement=take(segments->segment(0));
    require(!first.snapshot()&&take(replacement.action_options()).text=="Replacement", "Reentrant options replacement retained the old generation");
    auto command_input=take(segments->options());command_input.configuration.options[0].command=method;good(segments->set_options(command_input));
    auto old_selected=take(segments->segment(0));method->action=[&]{auto changed=take(segments->options());
        changed.configuration.options={{"After command",true,{}}};good(segments->set_options(changed));};
    good(old_selected.activate());auto after_command=take(segments->segment(0));method->action={};
    require(!old_selected.snapshot()&&take(after_command.action_options()).text=="After command",
            "A selected segment command replaced its active generation before returning");
    auto next=take(segments->options());auto invalid=next;invalid.configuration.selected_index=3;
    require(!segments->set_options(invalid)&&take(segments->options())==next,"Invalid segments replaced the previous input");
    bool wrong=false;std::thread worker([&]{auto r=segments->set_options(next);wrong=!r&&r.error().code==ii::core::ErrorCode::wrong_executor;});worker.join();
    require(wrong,"Segments accepted a foreign owner thread");good(host.clear_content());require(!segments->set_options(input),"Retired segments accepted an update");
    std::optional<native::NativeListItemSegments> retiring;const auto before=commands;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=native::NativeListItemSegments::create(builder,{},"retiring",input,[&](int){good(host.clear_content());});
        if(!value)return std::unexpected(value.error());retiring=*value;return {};
    }));
    good(take(retiring->segment(2)).activate());
    require(!retiring->root().snapshot()&&commands==before,"A removed segment delivered a stale command");tree->close();
}
