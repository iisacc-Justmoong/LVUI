#include <LVUI/NativeInputField.h>
#include <iiGUI/WidgetState.h>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}

void field(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{500,200}));
    std::optional<lvui::native::NativeInputField> field;std::vector<std::string> edits;unsigned submitted=0;std::string clipboard;
    lvui::InputFieldConfiguration options;options.text="A😀Z";options.accessibility_name="Example";
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto root=builder.vstack({},"root");if(!root)return std::unexpected(root.error());
        auto value=lvui::native::NativeInputField::create(builder,*root,"field",options,
            {.edited=[&](std::string text){edits.push_back(std::move(text));},.submitted=[&]{++submitted;}},
            {.write=[&](std::string text)->ii::core::Result<void>{clipboard=std::move(text);return {};}});
        if(!value)return std::unexpected(value.error());field=*value;return {};
    }));
    auto native=take(field->root().snapshot());
    require(native.text_input&&!native.password&&native.text.empty()&&native.text_length==4&&native.accessibility_name=="Example",
        "Owned field did not expose bounded text input metadata");
    good(field->root().focus());good(field->set_selection(1,3));
    require(take(field->root().text_content({1,2})).text=="😀","Owned field did not expose UTF-16 text ranges");
    good(tree->input({.kind=WidgetInputKind::composition,.text="ㅎ"}));
    good(tree->input({.kind=WidgetInputKind::composition,.text="하"}));
    require(take(field->snapshot()).text=="A하Z"&&edits.empty(),"Composition was committed prematurely");
    good(tree->input({.kind=WidgetInputKind::text,.text="한"}));
    require(take(field->snapshot()).text=="A한Z"&&edits==std::vector<std::string>{"A한Z"},
        "Korean composition was not committed exactly once");
    good(field->set_selection(1,2));good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::copy}));
    require(clipboard=="한","Owned field did not copy the selected Unicode text");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::cut}));
    require(take(field->snapshot()).text=="AZ"&&edits.back()=="AZ","Owned field did not cut the selected text");
    auto content=std::make_shared<ClipboardContent>();content->text="한";
    good(tree->input({.kind=WidgetInputKind::text,.content=content}));
    require(take(field->snapshot()).text=="A한Z","Owned field did not paste delivered text");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::space}));
    good(tree->input({.kind=WidgetInputKind::text,.text=" "}));
    require(take(field->snapshot()).text=="A한 Z","Space key and text delivery inserted more than one space");
    good(field->set_selection(0,0));const auto before_undo=edits.size();
    good(tree->input({.kind=WidgetInputKind::composition,.text="ㅎ"}));
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::backspace}));
    require(take(field->snapshot()).text=="A한 Z"&&edits.size()==before_undo,
        "A composition followed by deletion emitted a nonexistent committed change");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));
    require(submitted==1,"Single-line Enter did not submit");
    auto drawing=take(tree->drawing_snapshot({500,200}));
    bool image=!drawing.images.empty();for(const auto& layer:drawing.overlays)image|=!layer.images.empty();
    require(image,"Owned field did not submit shaped text drawing");
    options.placeholder="Different";good(field->set_options(options));
    require(take(field->snapshot()).text=="A한 Z","Unchanged external text replaced user edits");
    good(field->root().set_text("A😀Z"));
    require(take(field->snapshot()).text=="A😀Z","Explicit widget text update did not restore the original value");
    options.text="New";options.password=true;good(field->set_options(options));
    native=take(field->root().snapshot());require(native.password&&native.text.empty(),"Secure custom field leaked its value into the snapshot");
    good(field->set_selection(0,3));clipboard="unchanged";
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::cut}));
    require(clipboard=="unchanged"&&take(field->snapshot()).text=="New","Secure cut exposed or deleted a password");
    options.read_only=true;good(field->set_options(options));
    good(tree->input({.kind=WidgetInputKind::text,.text="x"}));
    require(take(field->snapshot()).text=="New","Read-only field accepted text");
    bool rejected=false;std::thread worker([&]{auto result=field->set_options(options);rejected=!result&&result.error().code==ii::core::ErrorCode::wrong_executor;});worker.join();
    require(rejected,"A foreign thread changed an owned input field");
    good(host.clear_content());require(!field->set_options(options),"Retired input field accepted an update");tree->close();
}
void nested_commit(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"outer",{400,200}));std::optional<lvui::native::NativeInputField> field;unsigned edits=0;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto container=builder.absolute_stack({},"container",{400,200});if(!container)return std::unexpected(container.error());
        auto child=builder.content_host(*container,"child",{250,100});if(!child)return std::unexpected(child.error());
        if(auto positioned=child->set_frame({60,20,250,100});!positioned)return positioned;
        return child->set_content([&](auto& inner)->ii::core::Result<void>{
            auto value=lvui::native::NativeInputField::create(inner,{},"nested",{},
                {.edited=[&](std::string){++edits;}});
            if(!value)return std::unexpected(value.error());field=*value;return {};
        });
    }));
    auto bounds=take(field->root().snapshot()).bounds;
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=bounds.x+10,.y=bounds.y+10}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=bounds.x+10,.y=bounds.y+10}));
    good(tree->input({.kind=WidgetInputKind::composition,.text="한"}));
    good(tree->input({.kind=WidgetInputKind::commit_composition}));
    require(take(field->snapshot()).text=="한"&&!take(field->snapshot()).marked&&edits==1,
        "Nested input lost composition commit or focus");
    tree->close();
}
void focus_notifications(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{400,100}));
    std::optional<lvui::native::NativeInputField> field;Widget other;std::vector<std::string> events;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto root=builder.hstack({},"root");if(!root)return std::unexpected(root.error());
        auto value=lvui::native::NativeInputField::create(builder,*root,"field",{},
            {.edited=[&](std::string text){events.push_back("edit:"+text);},
             .focus_changed=[&](bool focused){events.push_back(focused?"focus+":"focus-");}});
        if(!value)return std::unexpected(value.error());field=*value;
        auto button=builder.action_surface(*root,"other",{.text="Other",.width=60,.height=22});
        if(!button)return std::unexpected(button.error());other=*button;return {};
    }));
    good(field->root().focus());
    good(tree->input({.kind=WidgetInputKind::composition,.text="한"}));
    good(other.focus());
    require(events==std::vector<std::string>{"focus+","edit:한","focus-"},
        "Focus loss did not synchronously commit composition before notifying the owner");
    good(field->root().focus());good(other.focus());
    require(events==std::vector<std::string>{"focus+","edit:한","focus-","focus+","focus-"},
        "Focus changes without drawing were lost or duplicated");
    tree->close();
}
}
int main(){field();nested_commit();focus_notifications();}
