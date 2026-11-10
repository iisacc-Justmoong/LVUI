#include "native_input_field_composition.h"
#include <iiGUI/WidgetState.h>
#if defined(IIGUI_TEXT_REPLACEMENT_SELECTION_API) && defined(IIGUI_TEXT_REVISION_API)
#include "android_ime_fixture.h"
#include "../../iiGUI/src/Platform/Win32ImeProtocol.h"
#include "../../iiGUI/src/Platform/WaylandImeProtocol.h"
#endif
#if defined(IIGUI_TEXT_GEOMETRY_API) && defined(IIGUI_TEXT_REVISION_API)
#include "../../iiGUI/src/Platform/UIKitTextInputProtocol.h"
#endif
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
template<class T>T take(lvui::TextInputResult<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}

#if defined(IIGUI_TEXT_REPLACEMENT_SELECTION_API) && defined(IIGUI_TEXT_REVISION_API)
void wayland_ime_composition(){
    using namespace ii::gui;
    AndroidImeFixture host;auto container=take(host.tree->content_host({},"host",{300,100}));
    std::optional<lvui::native::NativeInputField> field;unsigned edits=0;
    lvui::InputFieldConfiguration options;options.text="A😀Z";
    good(container.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=lvui::native::NativeInputField::create(builder,{},"field",options,{.edited=[&](std::string){++edits;}});
        if(!value)return std::unexpected(value.error());field=*value;return {};
    }));
    (void)take(host.tree->drawing_snapshot({300,100}));good(field->root().focus());
    good(host.tree->input({.kind=WidgetInputKind::text_selection,.replacement_range=TextRange{1,2}}));
    detail::WaylandImeProtocol ime;good(ime.start(host));detail::WaylandImeBatch batch;
    batch.preedit="ㅎ";batch.cursor_begin=3;batch.cursor_end=3;good(ime.apply(host,batch));
    require(take(field->snapshot()).text=="AㅎZ"&&edits==0,
            "Wayland preedit did not preserve the field's pending edit");
    batch={};batch.committed="한";batch.preedit="글";batch.cursor_begin=3;batch.cursor_end=3;
    good(ime.apply(host,batch));auto value=take(field->snapshot());
    require(value.text=="A한글Z"&&value.marked==lvui::TextInputRange{2,1}&&edits==1,
            "Wayland commit and next preedit did not reach the LVUI field");
    batch={};good(ime.apply(host,batch));good(host.tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::undo}));
    require(take(field->snapshot()).text=="A😀Z","Wayland input did not form one undo step");
    host.tree->close();
}
void win32_ime_composition(){
    using namespace ii::gui;
    AndroidImeFixture host;auto container=take(host.tree->content_host({},"host",{300,100}));
    std::optional<lvui::native::NativeInputField> field;unsigned edits=0;
    lvui::InputFieldConfiguration options;options.text="A😀Z";
    good(container.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=lvui::native::NativeInputField::create(builder,{},"field",options,{.edited=[&](std::string){++edits;}});
        if(!value)return std::unexpected(value.error());field=*value;return {};
    }));
    (void)take(host.tree->drawing_snapshot({300,100}));good(field->root().focus());
    good(host.tree->input({.kind=WidgetInputKind::text_selection,.replacement_range=TextRange{1,2}}));
    detail::Win32ImeProtocol ime;good(ime.start(host));detail::Win32ImeMessage first;first.composing=u"ㅎ";first.cursor=1;
    good(ime.apply(host,first));detail::Win32ImeMessage next;next.committed=u"한";next.composing=u"글";next.cursor=1;
    good(ime.apply(host,next));auto value=take(field->snapshot());
    require(value.text=="A한글Z"&&value.marked==lvui::TextInputRange{2,1}&&edits==1,
            "Windows IME result and next candidate did not reach the LVUI field");
    good(ime.end(host));value=take(field->snapshot());
    require(value.text=="A한Z"&&!value.marked&&edits==1,"Windows IME end lost the committed field text");
    good(host.tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::undo}));
    require(take(field->snapshot()).text=="A😀Z","Windows IME result did not form one undo step");
    host.tree->close();
}
#endif
#if defined(IIGUI_TEXT_GEOMETRY_API) && defined(IIGUI_TEXT_REVISION_API)
void uikit_composition(){
    using namespace ii::gui;
    AndroidImeFixture host;auto container=take(host.tree->content_host({},"host",{300,100}));
    std::optional<lvui::native::NativeInputField> field;unsigned edits=0;
    lvui::InputFieldConfiguration options;options.text="A😀Z";
    good(container.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=lvui::native::NativeInputField::create(builder,{},"field",options,{.edited=[&](std::string){++edits;}});
        if(!value)return std::unexpected(value.error());field=*value;return {};
    }));
    (void)take(host.tree->drawing_snapshot({300,100}));good(field->root().focus());
    detail::UIKitTextInputProtocol input;const auto generation=take(input.state(host)).generation;
    require(take(input.move(host,generation,1,detail::UIKitTextDirection::right,1))==3,
            "UIKit field movement split an emoji");
    good(input.select(host,generation,{1,2}));good(input.compose(host,generation,"하나",{0,1}));
    auto projected=take(field->snapshot());
    require(projected.text=="A하나Z"&&projected.marked==lvui::TextInputRange{1,2}&&
            projected.selection==lvui::TextInputRange{1,1}&&edits==0,"UIKit field composition published or selected the wrong candidate");
    good(input.compose(host,generation,"한",{1,0}));good(input.finish(host,generation));
    require(take(field->snapshot()).text=="A한Z"&&edits==1,"UIKit field candidate did not commit exactly once");
    good(host.tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::undo}));
    require(take(field->snapshot()).text=="A😀Z","UIKit field composition did not form one undo step");
    host.tree->close();
}
void uikit_outside_selection(){
    using namespace ii::gui;
    AndroidImeFixture host;auto container=take(host.tree->content_host({},"host",{300,100}));
    std::optional<lvui::native::NativeInputField> field;unsigned edits=0;
    lvui::InputFieldConfiguration options;options.text="A😀Z";
    good(container.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=lvui::native::NativeInputField::create(builder,{},"field",options,{.edited=[&](std::string){++edits;}});
        if(!value)return std::unexpected(value.error());field=*value;return {};
    }));
    (void)take(host.tree->drawing_snapshot({300,100}));good(field->root().focus());
    detail::UIKitTextInputProtocol input;const auto generation=take(input.state(host)).generation;
    good(input.select(host,generation,{1,2}));good(input.compose(host,generation,"하나",{1,0}));
    good(input.select(host,generation,{0,1}));
    auto value=take(field->snapshot());
    require(value.text=="A하나Z"&&!value.marked&&value.selection==lvui::TextInputRange{0,1}&&edits==1,
            "UIKit field outside selection failed to commit its candidate once");
    good(host.tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::undo}));
    require(take(field->snapshot()).text=="A😀Z","UIKit outside selection split the composition undo transaction");
    host.tree->close();
    AndroidImeFixture limited_host;auto limited_container=take(limited_host.tree->content_host({},"host",{300,100}));
    std::optional<lvui::native::NativeInputField> limited;options.text="AZ";options.maximum_length=2;
    good(limited_container.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto next=lvui::native::NativeInputField::create(builder,{},"limited",options);
        if(!next)return std::unexpected(next.error());limited=*next;return {};
    }));
    (void)take(limited_host.tree->drawing_snapshot({300,100}));good(limited->root().focus());
    detail::UIKitTextInputProtocol limited_input;const auto token=take(limited_input.state(limited_host)).generation;
    good(limited_input.select(limited_host,token,{1,0}));good(limited_input.compose(limited_host,token,"하나",{1,0}));
    auto before=take(limited->snapshot());auto rejected=limited_input.select(limited_host,token,{4,0});auto after=take(limited->snapshot());
    require(!rejected&&after.text==before.text&&after.marked==before.marked&&after.selection==before.selection,
            "A bounded UIKit field partially committed an invalid outside selection");
    limited_host.tree->close();
}
void uikit_composition_replacement(){
    using namespace ii::gui;
    struct Case{TextRange range;const char* text;const char* expected;std::size_t caret;};
    const Case cases[]={{{0,1},"X","X한글CDE",1},{{4,1},"Q","A한글CQE",5},
                        {{2,1},"나","A한나CDE",3},{{0,4},"K","KDE",1}};
    for(const auto& c:cases){
        AndroidImeFixture host;auto container=take(host.tree->content_host({},"host",{300,100}));
        std::optional<lvui::native::NativeInputField> field;unsigned edits=0;lvui::InputFieldConfiguration options;options.text="ABCDE";
        good(container.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
            auto next=lvui::native::NativeInputField::create(builder,{},"field",options,{.edited=[&](std::string){++edits;}});
            if(!next)return std::unexpected(next.error());field=*next;return {};
        }));
        (void)take(host.tree->drawing_snapshot({300,100}));good(field->root().focus());
        detail::UIKitTextInputProtocol input;const auto generation=take(input.state(host)).generation;
        good(input.select(host,generation,{1,1}));good(input.compose(host,generation,"한글",{1,0}));
        good(input.replace(host,generation,c.range,c.text));const auto value=take(field->snapshot());
        require(value.text==c.expected&&!value.marked&&value.selection==lvui::TextInputRange{c.caret,0}&&edits==1,
                "UIKit field replacement did not atomically commit the projected edit");
        good(host.tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::undo}));
        require(take(field->snapshot()).text=="ABCDE","UIKit replacement split the field undo transaction");
        host.tree->close();
    }
}
#if defined(IIGUI_TEXT_CHANGE_SCOPE_API)
void uikit_external_changes(){
    using namespace ii::gui;
    std::optional<lvui::native::NativeInputField> field;std::vector<std::string> notices;bool replace_during_commit=false,replace_during_will=false;
    detail::WidgetFrameHooks hooks;
    hooks.text_input_change=[&](bool value)->ii::runtime::Task{
        notices.push_back(std::string(value?"will-text:":"will-selection:")+take(field->snapshot()).text);
        if(replace_during_will){replace_during_will=false;good(field->set_text("nested"));}
        return [&]{notices.push_back("did:"+take(field->snapshot()).text);};
    };
    AndroidImeFixture host;host.tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();},WidgetAppearance{},hooks);
    auto container=take(host.tree->content_host({},"host",{300,100}));lvui::InputFieldConfiguration options;options.text="A";
    good(container.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=lvui::native::NativeInputField::create(builder,{},"field",options,{.edited=[&](std::string){
            if(replace_during_commit){replace_during_commit=false;good(field->set_text("external"));}
        }});
        if(!value)return std::unexpected(value.error());field=*value;return {};
    }));
    (void)take(host.tree->drawing_snapshot({300,100}));good(field->root().focus());
    good(field->set_text("B"));
    require(notices==std::vector<std::string>({"will-text:A","did:B"}),"Programmatic field change notified after its mutation");
    notices.clear();detail::UIKitTextInputProtocol input;const auto generation=take(input.state(host)).generation;
    good(input.select(host,generation,{1,0}));good(input.compose(host,generation,"한",{1,0}));
    require(notices.empty(),"A system-originated field edit notified the input delegate");
    replace_during_commit=true;good(input.finish(host,generation));
    require(notices==std::vector<std::string>({"will-text:B한","did:external"}),
            "A reentrant programmatic field edit was hidden by a system edit");
    notices.clear();replace_during_will=true;auto stale=field->set_text("outer");
    require(!stale&&stale.error().code==ii::core::ErrorCode::busy&&take(field->snapshot()).text=="nested"&&
            notices==std::vector<std::string>({"will-text:external","will-text:external","did:nested","did:nested"}),
            "A notification reentry overwrote the newer field state or lost its completion");
    host.tree->close();
}
#endif
#endif

#if defined(IIGUI_TEXT_GEOMETRY_API)
void text_geometry(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{300,120}));
    std::optional<lvui::native::NativeInputField> field;
    lvui::InputFieldConfiguration options;options.text="A😀B\n한Z";options.multiline=true;options.width=200;options.height=80;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=lvui::native::NativeInputField::create(builder,{},"field",options);
        if(!value)return std::unexpected(value.error());field=*value;return {};
    }));
    (void)take(tree->drawing_snapshot({300,120}));good(field->root().focus());
    auto single=take(field->root().text_geometry({1,2}));
    require(single.start_caret.width>0&&single.end_caret.width>0&&single.end_caret.x>single.start_caret.x&&
            single.selection.size()==1&&single.selection[0].width>0,"Field geometry lost its shaped emoji");
    auto multiple=take(field->root().text_geometry({0,6}));
    require(multiple.selection.size()>=2&&multiple.start_caret.y<multiple.end_caret.y,
            "Field geometry lost its line break");
    require(!field->root().text_geometry({2,1})&&!field->root().text_geometry({7,1}),
            "Field geometry accepted an invalid UTF-16 boundary");
    good(field->set_selection(3,3));auto caret=take(field->root().text_geometry({3,0}));auto current=take(field->root().snapshot());
    require(std::abs(caret.start_caret.x-current.caret_bounds.x)<.01&&std::abs(caret.start_caret.y-current.caret_bounds.y)<.01,
            "Field geometry disagreed with the displayed caret");
    auto bidi=take(field->options());bidi.text="ab אבג cd";bidi.multiline=false;bidi.width=200;bidi.height=30;
    good(field->set_options(std::move(bidi)));good(field->set_selection(0,0));
    auto runs=take(field->root().text_geometry({1,4}));
    require(runs.selection.size()>=2,"Field geometry merged distinct bidirectional text runs");
    auto narrow=take(field->options());narrow.text=std::string(120,'w');narrow.multiline=false;narrow.width=45;narrow.height=30;
    good(field->set_options(std::move(narrow)));good(field->set_selection(120,120));
    (void)take(tree->drawing_snapshot({300,120}));
    auto hidden=take(field->root().text_geometry({0,1}));auto shown=take(field->root().text_geometry({119,1}));
    require(hidden.selection.empty()&&hidden.start_caret.width==0&&shown.end_caret.width>0,
            "Field geometry ignored horizontal scrolling or clipping");
    tree->close();
}
#endif

#if defined(IIGUI_TEXT_SELECTION_API)
#if defined(IIGUI_TEXT_REVISION_API)
void android_text_revision(){
    using namespace ii::gui;
    AndroidImeFixture host;auto container=take(host.tree->content_host({},"host",{300,100}));
    std::optional<lvui::native::NativeInputField> field;lvui::InputFieldConfiguration options;options.text="A";
    good(container.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=lvui::native::NativeInputField::create(builder,{},"field",options);
        if(!value)return std::unexpected(value.error());field=*value;return {};
    }));
    (void)take(host.tree->drawing_snapshot({300,100}));good(field->root().focus());
    detail::AndroidImeProtocol ime;const auto gen=take(ime.state(host)).generation;
    good(ime.composition(host,gen,"한",1));const auto before=take(field->root().snapshot());
    good(ime.composition(host,gen,"글",1));const auto after=take(field->root().snapshot());
    require(before.text_revision&&after.text_revision&&before.text_revision!=after.text_revision&&
            before.text_length==after.text_length&&before.text_selection==after.text_selection&&
            before.marked_range==after.marked_range&&take(field->snapshot()).text=="A글",
            "A same-length field candidate reused its input revision");
    host.tree->close();
}
#endif
#if defined(IIGUI_TEXT_SURROUNDING_DELETE_API) && defined(IIGUI_TEXT_REVISION_API)
void android_composing_deletion(){
    using namespace ii::gui;
    AndroidImeFixture host;auto container=take(host.tree->content_host({},"host",{300,100}));
    std::optional<lvui::native::NativeInputField> field;std::vector<std::string> edits;
    lvui::InputFieldConfiguration options;options.text="A😀xy한Z";
    good(container.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=lvui::native::NativeInputField::create(builder,{},"field",options,{.edited=[&](std::string text){
            edits.push_back(text);if(field){auto mirrored=take(field->options());mirrored.text=std::move(text);good(field->set_options(std::move(mirrored)));}
        }});
        if(!value)return std::unexpected(value.error());field=*value;return {};
    }));
    (void)take(host.tree->drawing_snapshot({300,100}));good(field->root().focus());
    detail::AndroidImeProtocol ime;const auto gen=take(ime.state(host)).generation;
    good(ime.select(host,gen,5,4));good(ime.composing_region(host,gen,3,6));
    good(ime.erase_surrounding_codepoints(host,gen,2,1));
    auto value=take(field->snapshot());
    require(value.text=="AyZ"&&value.marked==lvui::TextInputRange{1,1}&&value.anchor==2&&value.caret==1&&
            edits==std::vector<std::string>{"Axy한Z"},"Android composing field deletion published an uncommitted candidate");
    good(ime.finish(host,gen));require(edits==std::vector<std::string>{"Axy한Z","AyZ"},"Shortened field composition failed to commit");
    good(host.tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::undo}));
    require(take(field->snapshot()).text=="Axy한Z","Undo failed to restore the candidate source");
    good(host.tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::undo}));
    require(take(field->snapshot()).text=="A😀xy한Z","Undo failed to restore the surrounding field deletion");
    host.tree->close();
}
#endif
#if defined(IIGUI_TEXT_COMPOSITION_REGION_API) && defined(IIGUI_TEXT_REVISION_API)
void android_composition_region(){
    using namespace ii::gui;
    AndroidImeFixture host;auto container=take(host.tree->content_host({},"host",{300,100}));
    std::optional<lvui::native::NativeInputField> field;unsigned edits=0;
    lvui::InputFieldConfiguration options;options.text="A😀Z";
    good(container.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=lvui::native::NativeInputField::create(builder,{},"field",options,{.edited=[&](std::string){++edits;}});
        if(!value)return std::unexpected(value.error());field=*value;return {};
    }));
    (void)take(host.tree->drawing_snapshot({300,100}));good(field->root().focus());
    detail::AndroidImeProtocol ime;const auto gen=take(ime.state(host)).generation;
    good(ime.select(host,gen,3,1));good(ime.composing_region(host,gen,3,1));
    auto value=take(field->snapshot());
    require(value.text=="A😀Z"&&value.marked==lvui::TextInputRange{1,2}&&value.anchor==3&&value.caret==1&&edits==0,
            "Android region marking changed the field or its selection");
    good(ime.composition(host,gen,"한",1));good(ime.select(host,gen,3,0));good(ime.composing_region(host,gen,2,0));
    value=take(field->snapshot());
    require(value.text=="A한Z"&&value.marked==lvui::TextInputRange{0,2}&&value.anchor==3&&value.caret==0&&edits==1,
            "Android region update lost the committed field candidate");
    good(ime.commit(host,gen,"K",1));require(take(field->snapshot()).text=="KZ"&&edits==2,"Marked field replacement failed");
    good(host.tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::undo}));
    value=take(field->snapshot());require(value.text=="A한Z"&&value.anchor==3&&value.caret==0,"Undo lost the new region selection");
    good(host.tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::undo}));
    value=take(field->snapshot());require(value.text=="A😀Z"&&value.anchor==3&&value.caret==1,"Undo lost the prior composition");
    host.tree->close();
}
#endif
#if defined(IIGUI_TEXT_REPLACEMENT_SELECTION_API) && defined(IIGUI_TEXT_REVISION_API)
void android_selection_deletion(){
    using namespace ii::gui;
    AndroidImeFixture host;auto container=take(host.tree->content_host({},"host",{300,100}));
    std::optional<lvui::native::NativeInputField> field;unsigned edits=0;
    lvui::InputFieldConfiguration options;options.text="A😀B한Z";
    good(container.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=lvui::native::NativeInputField::create(builder,{},"field",options,{.edited=[&](std::string){++edits;}});
        if(!value)return std::unexpected(value.error());field=*value;return {};
    }));
    (void)take(host.tree->drawing_snapshot({300,100}));good(field->root().focus());
    detail::AndroidImeProtocol ime;const auto generation=take(ime.state(host)).generation;
    good(ime.select(host,generation,5,3));good(ime.erase_surrounding_codepoints(host,generation,1,1));
    auto value=take(field->snapshot());
    require(value.text=="AB한"&&value.anchor==3&&value.caret==1&&edits==1,
            "Android surrounding deletion did not preserve the selected field text");
    good(host.tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::undo}));
    value=take(field->snapshot());
    require(value.text=="A😀B한Z"&&value.anchor==5&&value.caret==3,
            "Undo did not restore both surrounding deletions and the reverse selection");
    good(host.tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::redo}));
    value=take(field->snapshot());
    require(value.text=="AB한"&&value.anchor==3&&value.caret==1,
            "Redo did not restore the surrounding deletion and selection");
    host.tree->close();
}
#endif
void text_service_selection(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{300,100}));
    std::optional<lvui::native::NativeInputField> field;unsigned edits=0;
    lvui::InputFieldConfiguration options;options.text="A😀Z";
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto value=lvui::native::NativeInputField::create(builder,{},"field",options,
            {.edited=[&](std::string){++edits;}});
        if(!value)return std::unexpected(value.error());field=*value;return {};
    }));
    (void)take(tree->drawing_snapshot({300,100}));good(field->root().focus());
    auto select=[&](std::size_t start,std::size_t length){return tree->input({.kind=WidgetInputKind::text_selection,.replacement_range=TextRange{start,length}});};
    good(select(1,2));require(take(field->snapshot()).selection==lvui::TextInputRange{1,2},"Text service did not select Unicode text");
#if defined(IIGUI_DIRECTIONAL_TEXT_SELECTION_API)
    good(tree->input({.kind=WidgetInputKind::text_selection,.replacement_range=TextRange{1,2},.selection_reversed=true}));
    require(take(field->snapshot()).anchor==3&&take(field->snapshot()).caret==1&&take(field->root().snapshot()).text_selection_reversed,
            "Text service lost directional field selection");
#endif
    require(!select(2,0)&&take(field->snapshot()).selection==lvui::TextInputRange{1,2},"A split scalar changed the selection");
    good(tree->input({.kind=WidgetInputKind::composition,.text="한"}));
#if defined(IIGUI_DIRECTIONAL_TEXT_SELECTION_API)
    good(tree->input({.kind=WidgetInputKind::text_selection,.replacement_range=TextRange{0,2},.selection_reversed=true}));
    require(take(field->snapshot()).anchor==2&&take(field->snapshot()).caret==0&&take(field->snapshot()).marked.has_value(),
            "Directional selection disturbed field composition");
#endif
    good(select(0,0));const auto marked=take(field->snapshot());
    require(marked.text=="A한Z"&&marked.marked.has_value()&&marked.selection==lvui::TextInputRange{0,0}&&edits==0,
            "Text service selection disturbed marked input");
    good(tree->input({.kind=WidgetInputKind::commit_composition}));
    require(take(field->snapshot()).text=="A한Z"&&take(field->snapshot()).selection==lvui::TextInputRange{0,0}&&edits==1,
            "Selection lost the composition commit or caret");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::undo}));
    require(take(field->snapshot()).text=="A😀Z","Composition and selection broke undo");
#if defined(IIGUI_TEXT_REPLACEMENT_CURSOR_API)
    good(select(1,2));
    good(tree->input({.kind=WidgetInputKind::composition,.text="한",.text_cursor_offset=-1}));
    require(take(field->snapshot()).text=="A한Z"&&take(field->snapshot()).caret==0,"Composition cursor did not move before its candidate");
    good(tree->input({.kind=WidgetInputKind::composition,.text="하",.text_cursor_offset=2}));
    require(take(field->snapshot()).caret==3,"Composition cursor did not move beyond its candidate");
    good(tree->input({.kind=WidgetInputKind::text,.text="한",.text_cursor_offset=2}));
    require(take(field->snapshot()).text=="A한Z"&&take(field->snapshot()).caret==3,"Committed cursor ignored its requested offset");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::undo}));
    good(select(0,1));auto invalid=tree->input({.kind=WidgetInputKind::composition,.text="K",.text_cursor_offset=2});
    require(!invalid&&take(field->snapshot()).text=="A😀Z"&&!take(field->snapshot()).marked&&
            take(field->snapshot()).selection==lvui::TextInputRange{0,1},"A split cursor partially applied composition");
    good(select(1,2));good(tree->input({.kind=WidgetInputKind::composition,.text_cursor_offset=1}));
    require(take(field->snapshot()).text=="AZ"&&take(field->snapshot()).marked.has_value(),"Empty composition restored the selected text");
    good(tree->input({.kind=WidgetInputKind::commit_composition}));
    require(take(field->snapshot()).text=="AZ","Empty composition did not commit deletion");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::undo}));
    require(take(field->snapshot()).text=="A😀Z","Empty composition did not preserve undo");
#if defined(IIGUI_TEXT_SERVICE_INPUT_API)
    std::string target;
    for(const auto& node:take(tree->accessibility_snapshots()))if(node.focused&&node.text_input)target=node.id;
    require(!target.empty(),"Native field did not expose its text-service identity");
    good(tree->input({.kind=WidgetInputKind::text_selection,.target=target,.replacement_range=TextRange{1,2},.text_service=true}));
    good(tree->input({.kind=WidgetInputKind::composition,.text="한",.target=target,.text_cursor_offset=-1,.text_service=true}));
    require(take(field->snapshot()).text=="A한Z"&&take(field->snapshot()).caret==0,"Targeted field composition was not delivered");
    good(tree->input({.kind=WidgetInputKind::commit_composition,.target=target,.text_service=true}));
    require(take(field->snapshot()).text=="A한Z"&&!take(field->snapshot()).marked,"Targeted field composition did not commit");
#endif
#endif
    tree->close();
}
#endif
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
    const auto composition=take(field->snapshot());good(field->set_accessibility_name("입력"));
    const auto renamed=take(field->snapshot());
    require(renamed.text==composition.text&&renamed.marked==composition.marked&&renamed.selection==composition.selection&&
            take(field->root().snapshot()).accessibility_name=="입력"&&take(field->options()).accessibility_name=="입력"&&edits.empty(),
            "Renaming a field interrupted input composition or emitted an edit");
    auto palette=take(field->options()).palette;palette.focus=lvui::figma_defaults::accent_purple_default;
    good(field->set_palette(palette));
    const auto recolored=take(field->snapshot());
    require(recolored.text==composition.text&&recolored.marked==composition.marked&&recolored.selection==composition.selection&&
            take(field->options()).palette==palette&&edits.empty(),"Recoloring a field interrupted input composition");
    auto invalid_palette=palette;invalid_palette.focus.red=2;
    require(!field->set_palette(invalid_palette)&&take(field->options()).palette==palette&&take(field->snapshot()).marked==composition.marked,
            "An invalid field palette changed live input state");
    require(!field->set_accessibility_name(std::string("bad\xff"))&&take(field->options()).accessibility_name=="입력",
            "An invalid field name changed the live configuration");
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
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::character,.text="z",.meta=true}));
    require(take(field->snapshot()).text=="A한Z"&&edits.back()=="A한Z","Command-Z did not undo the field edit");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::character,.shift=true,.text="z",.meta=true}));
    require(take(field->snapshot()).text=="A한 Z"&&edits.back()=="A한 Z","Command-Shift-Z did not redo the field edit");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::character,.text="z",.control=true}));
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::character,.text="y",.control=true}));
    require(take(field->snapshot()).text=="A한 Z","Control-Y did not redo the field edit");
    good(field->set_selection(0,0));const auto before_undo=edits.size();
    good(tree->input({.kind=WidgetInputKind::composition,.text="ㅎ"}));
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::backspace}));
    require(take(field->snapshot()).text=="A한 Z"&&edits.size()==before_undo,
        "A composition followed by deletion emitted a nonexistent committed change");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::undo}));
    require(take(field->snapshot()).text=="A한Z", "A canceled composition left a phantom undo entry");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::redo}));
    require(take(field->snapshot()).text=="A한 Z", "Redo did not recover the previous edit after composition");
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
    good(field->set_selection(3,3));good(tree->input({.kind=WidgetInputKind::text,.text="!"}));
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::undo}));
    require(take(field->snapshot()).text=="New!","Secure input retained undo history");
    options.password=false;good(field->set_options(options));
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::undo}));
    require(take(field->snapshot()).text=="New!","Leaving password mode exposed old input history");
    options.read_only=true;good(field->set_options(options));
#if defined(IIGUI_TEXT_SELECTION_API)
    good(tree->input({.kind=WidgetInputKind::text_selection,.replacement_range=TextRange{1,2}}));
    require(take(field->snapshot()).selection==lvui::TextInputRange{1,2},"Read-only field rejected text-service selection");
#endif
    good(tree->input({.kind=WidgetInputKind::text,.text="x"}));
    require(take(field->snapshot()).text=="New!","Read-only field accepted text");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::undo}));
    require(take(field->snapshot()).text=="New!","Read-only field accepted undo");
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
void double_click_selection(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{400,100}));
    std::optional<lvui::native::NativeInputField> field;unsigned edits=0;
    lvui::InputFieldConfiguration options;options.text="hello world";
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto value=lvui::native::NativeInputField::create(builder,{},"field",options,
            {.edited=[&](std::string){++edits;}});
        if(!value)return std::unexpected(value.error());field=*value;return {};
    }));
    auto bounds=take(field->root().snapshot()).bounds;
    const auto x=bounds.x+12,y=bounds.y+bounds.height/2;
    const auto click=[&](double time,std::uint32_t native_count=0){
        good(tree->input({.kind=WidgetInputKind::pointer_down,.x=x,.y=y,.timestamp=time,.buttons=1,.native_click_count=native_count}));
        good(tree->input({.kind=WidgetInputKind::pointer_up,.x=x,.y=y,.timestamp=time+.03}));
    };
    click(1);click(1.2);
    require(take(field->snapshot()).selection==lvui::TextInputRange{0,5},
            "Double click did not select the complete word or mouse-up collapsed it");
    require(edits==0,"Word selection emitted a text edit");
    good(field->set_selection(0,0));
    click(3);click(4);
    require(take(field->snapshot()).selection.length==0,"Separated clicks were mistaken for a double click");
    click(5);
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=x,.y=y,.timestamp=5.2,.buttons=1}));
    good(tree->input({.kind=WidgetInputKind::pointer_move,.x=x+90,.y=y,.timestamp=5.25,.buttons=1}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=x+90,.y=y,.timestamp=5.3}));
    require(take(field->snapshot()).selection==lvui::TextInputRange{0,11},"Double-click drag did not extend by words");
    require(edits==0,"Word drag emitted a text edit");
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=x+90,.y=y,.timestamp=6,.buttons=1}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=x+90,.y=y,.timestamp=6.03}));
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=x+90,.y=y,.timestamp=6.2,.buttons=1}));
    good(tree->input({.kind=WidgetInputKind::pointer_move,.x=x,.y=y,.timestamp=6.25,.buttons=1}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=x,.y=y,.timestamp=6.3}));
    const auto reverse=take(field->snapshot());
    require(reverse.selection==lvui::TextInputRange{0,11} && reverse.anchor==11 && reverse.caret==0,
            "Reverse word drag did not preserve directional selection");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::undo}));
    require(take(field->snapshot()).text=="hello world" && edits==0,"Word selection created an undo edit");
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=x,.y=y,.timestamp=6.4,.buttons=1}));
    good(tree->input({.kind=WidgetInputKind::pointer_move,.x=x+90,.y=y,.timestamp=6.45,.buttons=1}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=x+90,.y=y,.timestamp=6.5}));
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=x+90,.y=y,.timestamp=6.6,.buttons=1}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=x+90,.y=y,.timestamp=6.63}));
    require(take(field->snapshot()).selection.length==0,"A drag armed an unrelated double click");
    auto settings=take(field->options());settings.double_click_interval=.1;settings.double_click_distance=2;
    good(field->set_options(settings));good(field->set_selection(0,0));
    click(7);click(7.2);
    require(take(field->snapshot()).selection.length==0,"Configured double-click timing was ignored");
    click(9,1);click(10.2,2);
    require(take(field->snapshot()).selection==lvui::TextInputRange{0,5},"Native double-click timing was not honored");
    good(field->set_selection(0,0));
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=x,.y=y,.timestamp=11,.buttons=1,.native_click_count=1}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=x,.y=y,.timestamp=11.4}));click(11.5,2);
    require(take(field->snapshot()).selection==lvui::TextInputRange{0,5},"Native click was rejected by fallback press duration");
    good(field->set_selection(0,0));click(12,1);click(12.05,1);
    require(take(field->snapshot()).selection.length==0,"A native single-click decision was overridden by fallback timing");
    settings.use_native_click_count=false;good(field->set_options(settings));
    click(13,1);click(13.2,2);
    require(take(field->snapshot()).selection.length==0,"Opting out of native click decisions was ignored");
    click(15,1);click(15.05,1);
    require(take(field->snapshot()).selection==lvui::TextInputRange{0,5},"Configured click timing was not used after opting out");
    settings.use_native_click_count=true;settings.double_click_interval=0;good(field->set_options(settings));
    good(field->set_selection(0,0));click(17,1);click(17.05,2);
    require(take(field->snapshot()).selection.length==0,"A disabled double-click was accepted from the host");
    settings.double_click_interval=.5;good(field->set_options(settings));
    click(18,1);click(18.1,3);
    require(take(field->snapshot()).selection.length==0,"A native triple-click was treated as a double-click");
    good(tree->input({.kind=WidgetInputKind::focus_lost}));click(19,2);
    require(take(field->snapshot()).selection.length==0,"A native count selected without a matching press in this field");
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=x,.y=y,.device=PointerDevice::touch,.timestamp=20,.device_id=101,.buttons=1,.native_click_count=1}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=x,.y=y,.device=PointerDevice::touch,.timestamp=20.03,.device_id=101}));
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=x,.y=y,.device=PointerDevice::touch,.timestamp=21,.device_id=102,.buttons=1,.native_click_count=2}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=x,.y=y,.device=PointerDevice::touch,.timestamp=21.03,.device_id=102}));
    require(take(field->snapshot()).selection==lvui::TextInputRange{0,5},"Native touch taps lost their sequence across touch identities");
    settings.double_click_interval=std::numeric_limits<double>::quiet_NaN();
    require(!field->set_options(settings),"Invalid double-click preferences were accepted");
    settings.double_click_interval=.5;settings.double_click_distance=4;settings.text="one two";settings.password=true;
    good(field->set_options(settings));click(8);click(8.2);
    require(take(field->snapshot()).selection==lvui::TextInputRange{0,7},
            "Secure double click exposed word boundaries instead of selecting the whole value");
    auto unicode=take(lvui::native::TextInput::create(lvui::native::native_text_boundary<ii::gui::TextIndex>,"A😀 한글 e\xcc\x81"));
    require(take(unicode.word_range(1))==lvui::TextInputRange{1,2},"Emoji word hit split a surrogate pair");
    require(take(unicode.word_range(4))==lvui::TextInputRange{4,2},"Korean word hit did not retain its complete word");
    require(take(unicode.word_range(7))==lvui::TextInputRange{7,2},"Word hit split a combining grapheme");
    require(!unicode.word_range(2),"Word hit accepted an interior surrogate position");
    tree->close();
}
void displayed_text_notifications(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{400,100}));
    std::optional<lvui::native::NativeInputField> field;
    std::vector<std::string> displayed,edits;
    lvui::native::NativeInputFieldCallbacks callbacks;
    callbacks.displayed_changed=[&](std::string text){
        displayed.push_back(text);
        if(text=="trigger")good(field->set_text("replacement"));
    };
    callbacks.edited=[&](std::string text){edits.push_back(std::move(text));};
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto value=lvui::native::NativeInputField::create(builder,{},"displayed",{},std::move(callbacks));
        if(!value)return std::unexpected(value.error());field=*value;return {};
    }));
    good(field->root().focus());
    good(tree->input({.kind=WidgetInputKind::composition,.text="ㅎ"}));
    good(tree->input({.kind=WidgetInputKind::composition,.text="한"}));
    require(displayed==std::vector<std::string>({"ㅎ","한"})&&edits.empty(),
            "Displayed text did not notify during uncommitted composition");
    good(tree->input({.kind=WidgetInputKind::composition,.text=""}));
    require(displayed.back().empty()&&edits.empty(),"Cancel did not report the restored display text");
    good(field->set_text("A"));
    require(displayed.back()=="A"&&edits.empty(),"Programmatic text did not report its visible change");
    good(tree->input({.kind=WidgetInputKind::text,.text="B"}));
    require(displayed.back()=="AB"&&edits==std::vector<std::string>{"AB"},
            "Committed typing did not deliver display and edit notifications");
    good(field->clear_as_user());
    const auto before=displayed.size();good(field->set_text(""));
    require(displayed.size()==before&&edits==std::vector<std::string>({"AB",""}),
            "An unchanged value produced another display or edit notification");
    good(tree->input({.kind=WidgetInputKind::text,.text="trigger"}));
    require(take(field->snapshot()).text=="replacement"&&displayed.back()=="replacement"&&
            edits==std::vector<std::string>({"AB",""}),
            "A reentrant display update was overwritten or emitted a stale edit");
    tree->close();

    auto second=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto second_host=take(second->content_host({},"host",{200,60}));
    std::optional<lvui::native::NativeInputField> retiring;unsigned stale_edits=0;
    lvui::native::NativeInputFieldCallbacks retiring_callbacks;
    retiring_callbacks.displayed_changed=[&](std::string){good(second_host.clear_content());};
    retiring_callbacks.edited=[&](std::string){++stale_edits;};
    good(second_host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto value=lvui::native::NativeInputField::create(builder,{},"retiring",{},std::move(retiring_callbacks));
        if(!value)return std::unexpected(value.error());retiring=*value;return {};
    }));
    good(retiring->root().focus());
    good(second->input({.kind=WidgetInputKind::text,.text="x"}));
    require(stale_edits==0&&!retiring->root().snapshot(),"A retired display observer received a stale edit");
    second->close();
}
void user_clear(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{400,100}));
    std::optional<lvui::native::NativeInputField> field;
    std::vector<std::string> edits;
    lvui::InputFieldConfiguration options;options.text="A😀Z";
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto value=lvui::native::NativeInputField::create(builder,{},"clearable",options,
            {.edited=[&](std::string text){edits.push_back(std::move(text));}});
        if(!value)return std::unexpected(value.error());field=*value;return {};
    }));
    good(field->root().focus());
    good(field->clear_as_user());
    require(take(field->snapshot()).text.empty()&&edits==std::vector<std::string>{""},
            "Clear did not produce one user edit");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::undo}));
    require(take(field->snapshot()).text=="A😀Z"&&edits.back()=="A😀Z","Undo did not restore the cleared Unicode text");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::redo}));
    require(take(field->snapshot()).text.empty()&&edits.size()==3&&edits.back().empty(),"Redo did not restore the clear edit");
    good(field->clear_as_user());
    require(edits.size()==3,"Clearing an empty field emitted a redundant edit");
    good(field->set_text("Original"));
    good(tree->input({.kind=WidgetInputKind::composition,.text="한"}));
    require(take(field->snapshot()).marked.has_value(),"The composition fixture was not marked");
    good(field->clear_as_user());
    require(take(field->snapshot()).text.empty()&&!take(field->snapshot()).marked&&edits.size()==4,
            "Clear did not replace the full composition as one committed edit");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::undo}));
    require(take(field->snapshot()).text=="Original"&&edits.size()==5,
            "Undo after clear did not restore the text before composition");
    auto current=take(field->options());current.read_only=true;good(field->set_options(current));
    good(field->clear_as_user());
    require(take(field->snapshot()).text=="Original"&&edits.size()==5,"Clear changed a read-only field");
    current.read_only=false;current.enabled=false;good(field->set_options(current));
    good(field->clear_as_user());
    require(take(field->snapshot()).text=="Original"&&edits.size()==5,"Clear changed a disabled field");
    bool wrong_thread=false;std::thread worker([&]{auto r=field->clear_as_user();wrong_thread=!r&&r.error().code==ii::core::ErrorCode::wrong_executor;});worker.join();
    require(wrong_thread,"Clear accepted a foreign thread");
    tree->close();require(!field->clear_as_user(),"Clear accepted a retired widget");
}
void soft_wrap(){
    using namespace ii::gui;using namespace lvui::native;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{400,220}));
    std::optional<NativeInputField> field;unsigned edits=0;
    lvui::InputFieldConfiguration options;options.text="alpha beta gamma delta";options.width=80;options.height=160;
    options.horizontal_padding=4;options.vertical_padding=4;options.font_family="Monospace";
    options.font_size=16;options.line_height=22;options.multiline=true;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto root=builder.absolute_stack({},"root",{400,220});if(!root)return std::unexpected(root.error());
        auto value=NativeInputField::create(builder,*root,"field",options,{.edited=[&](std::string){++edits;}});
        if(!value)return std::unexpected(value.error());field=*value;return field->root().set_frame({0,0,80,160});
    }));
    (void)take(tree->drawing_snapshot({400,220}));good(field->root().focus());
    const auto model=take(NativeTextLayout::create(options.text,{.font_family="Monospace",.font_size=16,.line_height=22},false,72));
    require(model.lines().size()>1,"Multiline fixture did not wrap");
    const auto split=model.lines()[0].last;const auto upper=take(model.caret(split,NativeTextAffinity::upstream));
    const auto lower=take(model.caret(split,NativeTextAffinity::downstream));
    auto bounds=take(field->root().snapshot()).bounds;
    const auto click=[&](double x,double y,double time){
        good(tree->input({.kind=WidgetInputKind::pointer_down,.x=bounds.x+4+x,.y=bounds.y+4+y,.timestamp=time,.buttons=1}));
        good(tree->input({.kind=WidgetInputKind::pointer_up,.x=bounds.x+4+x,.y=bounds.y+4+y,.timestamp=time+.02}));
    };
    click(upper.x+2,upper.y+upper.height/2,1);
    auto state=take(field->snapshot());auto caret=take(field->root().snapshot()).caret_bounds;
    require(state.caret==split&&state.affinity==NativeTextAffinity::upstream&&std::abs(caret.y-(bounds.y+4+upper.y))<1,
            "Click at the end of a wrapped line moved the caret to the next line");
    click(lower.x,lower.y+lower.height/2,3);
    state=take(field->snapshot());caret=take(field->root().snapshot()).caret_bounds;
    require(state.caret==split&&state.affinity==NativeTextAffinity::downstream&&std::abs(caret.y-(bounds.y+4+lower.y))<1,
            "Click at the start of a wrapped line lost its visual caret affinity");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::left}));
    require(take(field->snapshot()).caret==split&&take(field->snapshot()).affinity==NativeTextAffinity::upstream,
            "Left did not move across both visual sides of a soft break");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::right}));
    require(take(field->snapshot()).caret==split&&take(field->snapshot()).affinity==NativeTextAffinity::downstream&&edits==0,
            "Right changed text instead of moving to the other side of a soft break");
    good(field->set_selection(2,2));
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::down}));
    require(take(field->snapshot()).caret>=split,"Down did not move to the next visual line");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::up}));
    require(take(field->snapshot()).caret==2,"Up did not preserve the preferred horizontal caret position");
    good(field->set_selection(split,split));
    good(tree->input({.kind=WidgetInputKind::composition,.text="한"}));
    require(take(field->snapshot()).marked.has_value()&&edits==0,"Wrapping committed IME composition prematurely");
    good(tree->input({.kind=WidgetInputKind::commit_composition}));
    require(edits==1&&take(field->snapshot()).text.find("한")!=std::string::npos,"Wrapped composition did not commit");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::undo}));
    require(take(field->snapshot()).text==options.text,"Wrapped input lost its undo history");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::redo}));
    require(take(field->snapshot()).text.find("한")!=std::string::npos,"Wrapped input lost its redo history");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::undo}));
    auto current=take(field->options());current.wrap_enabled=false;good(field->set_options(current));
    good(field->set_selection(split,split));
    const auto unwrapped=take(field->root().snapshot()).caret_bounds;
    require(std::abs(unwrapped.y-(bounds.y+4))<1,"Disabled wrapping still used a visual second line");
    current.wrap_enabled=true;good(field->set_options(current));good(field->root().set_frame({0,0,160,160}));
    (void)take(tree->drawing_snapshot({400,220}));bounds=take(field->root().snapshot()).bounds;
    good(field->set_selection(split,split));
    require(bounds.width==160&&std::abs(take(field->root().snapshot()).caret_bounds.y-(bounds.y+4))<1&&
            take(field->snapshot()).text==options.text,"Parent resize did not reflow without changing text");
    current.horizontal_padding=0;good(field->set_options(current));good(field->root().set_frame({0,0,1,160}));
    (void)take(tree->drawing_snapshot({400,220}));
    const auto long_text=std::string(NativeTextLayout::maximum_lines,'a')+"\n";
    good(field->set_text(long_text));
    require(take(field->snapshot()).wrapping_fallback&&take(field->snapshot()).text==long_text,
            "Exceeding the visual-line budget discarded input instead of falling back");
    good(field->root().set_frame({0,0,160,160}));
    (void)take(tree->drawing_snapshot({400,220}));
    require(!take(field->snapshot()).wrapping_fallback&&take(field->snapshot()).text==long_text,
            "A wider field did not recover wrapping while preserving input");
    tree->close();
}
void scroll_wrapped_read_only(){
    using namespace ii::gui;using namespace lvui::native;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{240,100}));
    std::optional<NativeInputField> field;
    lvui::InputFieldConfiguration options;options.width=180;options.height=52;
    options.multiline=true;options.read_only=true;options.font_size=14;options.line_height=20;
    for(int i=0;i<30;++i)options.text+="A long report line for wrapping and scrolling.\n";
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=NativeInputField::create(builder,{},"report",options);
        if(!value)return std::unexpected(value.error());field=*value;return {};
    }));
    (void)take(tree->drawing_snapshot({240,100}));
    const auto initial=take(field->snapshot());
    require(initial.selection.location==0&&initial.selection.length==0&&initial.vertical_scroll==0,
            "A read-only multiline field did not initially show the beginning of its text");
    good(field->set_selection(0,0));
    const auto before=take(field->snapshot());const auto rect=take(field->root().snapshot()).bounds;
    const auto first_hit=take(field->root().text_position(8,8));
    good(tree->input({.kind=WidgetInputKind::scroll,.x=rect.x+8,.y=rect.y+8,.scroll_y=45}));
    const auto after=take(field->snapshot());
    const auto scrolled_hit=take(field->root().text_position(8,8));
    if(!(after.vertical_scroll>before.vertical_scroll && scrolled_hit>first_hit))
        std::fprintf(stderr,"scroll %.2f -> %.2f hit %zu -> %zu bounds %.1f %.1f %.1f %.1f\n",before.vertical_scroll,after.vertical_scroll,first_hit,scrolled_hit,rect.x,rect.y,rect.width,rect.height);
    require(after.vertical_scroll>before.vertical_scroll && scrolled_hit>first_hit &&
            after.text==options.text && after.selection==before.selection,
            "Wheel scrolling did not move the wrapped read-only text without editing it");
    good(tree->input({.kind=WidgetInputKind::scroll,.x=rect.x+8,.y=rect.y+8,.scroll_y=100000}));
    const auto bottom=take(field->snapshot()).vertical_scroll;
    good(tree->input({.kind=WidgetInputKind::scroll,.x=rect.x+8,.y=rect.y+8,.scroll_y=10}));
    require(bottom>0&&take(field->snapshot()).vertical_scroll==bottom,"Wrapped text scrolled past its end");
    options.text="Updated report\n"+options.text;good(field->set_options(options));
    const auto updated=take(field->snapshot());
    require(updated.selection.location==0&&updated.selection.length==0&&updated.vertical_scroll==0,
            "A replaced read-only multiline document did not show its beginning");
    good(tree->input({.kind=WidgetInputKind::scroll,.x=rect.x+8,.y=rect.y+8,.scroll_y=100}));
    good(tree->input({.kind=WidgetInputKind::scroll,.x=rect.x+8,.y=rect.y+8,.scroll_y=-100000}));
    require(take(field->snapshot()).vertical_scroll==0,"Wrapped text scrolled past the beginning");
    tree->close();
}
void maximum_length(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{400,150}));
    std::optional<lvui::native::NativeInputField> field;std::vector<std::string> edits;
    lvui::InputFieldConfiguration options;options.text="A😀ZQ";options.maximum_length=3;
    options.length_unit=lvui::TextLengthUnit::unicode_scalars;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto value=lvui::native::NativeInputField::create(builder,{},"limited",options,
            {.edited=[&](std::string value){edits.push_back(std::move(value));}});
        if(!value)return std::unexpected(value.error());field=*value;return {};
    }));
    require(take(field->snapshot()).text=="A😀Z", "Owned field did not apply initial scalar limit");
    good(field->root().focus());
    good(tree->input({.kind=WidgetInputKind::text,.text="Q"}));
    require(edits.empty()&&take(field->snapshot()).text=="A😀Z", "Rejected full-field input emitted an edit");
    good(field->set_selection(1,3));
    good(tree->input({.kind=WidgetInputKind::text,.text="한글"}));
    require(edits==std::vector<std::string>{"A한Z"}, "Bounded replacement did not emit its actual text");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::undo}));
    require(take(field->snapshot()).text=="A😀Z", "Owned bounded field did not undo");
    options.maximum_length=2;good(field->set_options(options));
    require(take(field->snapshot()).text=="A😀", "Owned field did not apply a changed limit");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::redo}));
    require(take(field->snapshot()).text=="A😀", "Length change retained incompatible history");
    good(field->set_text("A"));
    good(tree->input({.kind=WidgetInputKind::composition,.text="😀한"}));
    require(take(field->snapshot()).text=="A😀한", "Owned field truncated preedit");
    good(tree->input({.kind=WidgetInputKind::commit_composition}));
    require(take(field->snapshot()).text=="A😀"&&edits.back()=="A😀", "Owned field did not commit bounded composition");
    tree->close();
}
void clear_button(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"clear-host",{300,80}));
    std::optional<lvui::native::NativeInputField> field;std::vector<std::string> edits;
    lvui::InputFieldConfiguration options;lvui::apply_input_field_size(options,lvui::InputFieldSize::compact);
    options.text="A😀";options.clear_button_visible=true;options.clear_accessibility_name="Clear document name";
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto parent=builder.absolute_stack({},"parent",{300,80});if(!parent)return std::unexpected(parent.error());
        auto value=lvui::native::NativeInputField::create(builder,*parent,"input",options,
            {.edited=[&](std::string text){edits.push_back(std::move(text));}});
        if(!value)return std::unexpected(value.error());field=*value;return field->root().set_frame({0,0,206,22});
    }));
    const auto find=[&](std::string_view suffix){
        const auto nodes=take(tree->snapshots());
        const auto found=std::ranges::find_if(nodes,[&](const auto& n){return n.id==suffix||n.id.ends_with(std::string("/")+std::string(suffix));});
        if(found==nodes.end())throw std::runtime_error("Input clear action is missing");
        return take(tree->find(found->id));
    };
    auto clear=find("input.clear"),parent=find("parent");
    auto bounds=take(clear.snapshot()).bounds;
    require(take(clear.snapshot()).visible&&take(clear.snapshot()).accessibility_name=="Clear document name"&&
            std::abs(bounds.x-187)<.01&&std::abs(bounds.width-12)<.01,
            "Input clear action did not use Figma geometry and accessible naming");
    good(field->set_text(std::string(200,'x')));good(field->root().focus());
    const auto caret=take(field->root().snapshot()).caret_bounds;
    require(caret.x+caret.width<=bounds.x-lvui::search_field_defaults::gap+.01,
            "Long input overlapped the clear action");
    good(field->set_text("A😀"));
    good(field->root().focus());
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=bounds.x+bounds.width/2,.y=bounds.y+bounds.height/2,.buttons=1}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=bounds.x+bounds.width/2,.y=bounds.y+bounds.height/2}));
    require(take(field->snapshot()).text.empty()&&edits==std::vector<std::string>{""}&&
            !take(clear.snapshot()).visible&&take(field->root().snapshot()).focused,
            "Input clear did not emit one edit and preserve focus");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::undo}));
    require(take(field->snapshot()).text=="A😀"&&take(clear.snapshot()).visible,
            "Undo did not restore a cleared input and action");
    good(tree->input({.kind=WidgetInputKind::composition,.text="한"}));
    auto marked_options=take(field->options());marked_options.clear_button_visible=false;
    good(field->set_options(marked_options));
    require(take(field->snapshot()).marked.has_value()&&!take(clear.snapshot()).visible,
            "Changing clear visibility committed or discarded marked text");
    marked_options.clear_button_visible=true;good(field->set_options(marked_options));
    require(take(field->snapshot()).marked.has_value()&&take(clear.snapshot()).visible,
            "Restoring clear visibility interrupted marked text");
    const auto count=edits.size();good(clear.activate());
    require(take(field->snapshot()).text.empty()&&edits.size()==count+1&&edits.back().empty(),
            "Clearing marked text emitted an extra edit or retained the draft");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::undo}));
    require(take(field->snapshot()).text=="A😀"&&!take(field->snapshot()).marked,
            "Undo did not restore the committed value after clearing composition");
    good(tree->input({.kind=WidgetInputKind::text,.text="Q"}));
    const auto after_draft=take(field->snapshot()).text;
    good(field->set_palette(take(field->options()).palette));
    require(take(field->snapshot()).text==after_draft&&after_draft!="A😀",
            "Restyling replayed an old composition over a later edit");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::undo}));
    require(take(field->snapshot()).text=="A😀","Restyling lost the later edit's undo entry");
    auto current=take(field->options());current.read_only=true;good(field->set_options(current));
    require(!take(clear.snapshot()).visible&&take(field->snapshot()).text=="A😀","Read-only input exposed a clear action");
    current.read_only=false;good(field->set_options(current));good(parent.set_enabled(false));
    require(!take(clear.snapshot()).visible&&take(field->snapshot()).text=="A😀","Disabled ancestor exposed a clear action");
    good(parent.set_enabled(true));require(take(clear.snapshot()).visible,"Re-enabled ancestor did not restore the clear action");
    current.clear_button_visible=false;good(field->set_options(current));
    require(!take(clear.snapshot()).visible&&take(field->snapshot()).text=="A😀","Disabling clear changed input state");
    current.clear_button_visible=true;good(field->set_options(current));
    good(field->root().set_frame({0,0,20,22}));(void)take(tree->drawing_snapshot({300,80}));
    require(!take(clear.snapshot()).visible,"Narrow input exposed an overlapping clear action");
    good(field->root().set_frame({0,0,206,22}));(void)take(tree->drawing_snapshot({300,80}));
    require(take(clear.snapshot()).visible,"Widened input did not restore its clear action");
    current.clear_accessibility_name=std::string("bad\xff");
    require(!field->set_options(current)&&take(field->snapshot()).text=="A😀","Invalid clear name changed the input");
    auto secret=take(field->options());secret.text="sensitive";secret.password=true;good(field->set_options(secret));
    good(field->root().focus());good(clear.activate());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::undo}));
    require(take(field->snapshot()).text.empty(),"Password clear exposed prior secret through undo");
    tree->close();
}
void retirement_during_clear(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"retire-host",{300,80}));
    std::optional<lvui::native::NativeInputField> field;unsigned edits=0;
    lvui::InputFieldConfiguration options;options.text="draft";options.clear_button_visible=true;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto value=lvui::native::NativeInputField::create(builder,{},"retiring",options,
            {.edited=[&](std::string text){if(text.empty()){++edits;good(host.clear_content());}}});
        if(!value)return std::unexpected(value.error());field=*value;return {};
    }));
    const auto nodes=take(tree->snapshots());
    const auto found=std::ranges::find_if(nodes,[](const auto& n){return n.id.ends_with("/retiring.clear");});
    require(found!=nodes.end(),"Retiring input clear action is missing");auto clear=take(tree->find(found->id));
    good(field->root().focus());good(clear.activate());
    require(edits==1&&!field->root().snapshot(),"Clear callback could not safely retire its field");
    tree->close();
}
void rounded_material(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"material-host",{320,80}));
    std::optional<lvui::native::NativeInputField> field;Widget parent;
    lvui::InputFieldConfiguration options;lvui::apply_input_field_size(options,lvui::InputFieldSize::compact);
    options.text="AB";options.surface=lvui::InputFieldSurface::rounded;options.corner_radius=0;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto container=builder.absolute_stack({},"material-parent",{320,80});if(!container)return std::unexpected(container.error());
        parent=*container;
        auto value=lvui::native::NativeInputField::create(builder,parent,"material-input",options);
        if(!value)return std::unexpected(value.error());field=*value;return field->root().set_frame({0,0,206,22});
    }));
    good(field->root().focus());good(field->set_selection(0,2));
    const auto material=[](const CanvasFrame& frame)->std::size_t{
        for(std::size_t i=0;i<frame.overlays.size();++i)if(frame.overlays[i].backdrop)return i;
        throw std::runtime_error("Rounded input did not submit its backdrop");
    };
    auto drawing=take(tree->drawing_snapshot({320,80}));auto index=material(drawing);
    require(std::abs(drawing.overlays[index].backdrop->blur_radius-8)<.001&&
            drawing.overlays[index].images.size()==1&&drawing.overlays[index].images.front().image,
            "Rounded input lost the shared Figma material");
    const auto enabled=drawing.overlays[index].images.front().image;
    bool ink_after=false,rounded_focus=false,selected_accent=false,caret_description=false;
    const auto accent=ii::gui::srgb_to_linear(options.palette.focus);
    const auto caret_ink=ii::gui::srgb_to_linear(lvui::search_field_defaults::description);
    for(std::size_t i=index+1;i<drawing.overlays.size();++i){
        ink_after|=drawing.overlays[i].panels.size()>=2&&!drawing.overlays[i].images.empty();
        for(const auto& panel:drawing.overlays[i].panels)
            if(std::abs(panel.bounds.width-206)<.01&&std::abs(panel.bounds.height-22)<.01&&panel.radius==8)
                rounded_focus=true;
            else if(panel.stroke_width==0&&panel.bounds.width>1&&panel.top==accent)
                selected_accent=true;
            else if(panel.stroke_width==0&&panel.bounds.width==1&&panel.top==caret_ink)
                caret_description=true;
    }
    require(ink_after,"Input selection, caret, or shaped text was drawn beneath the material");
    require(rounded_focus,"Rounded input focus ring did not use the LVUI radius");
    require(selected_accent&&caret_description,"Rounded input lost the Figma selection or cursor color");
    good(parent.set_enabled(false));drawing=take(tree->drawing_snapshot({320,80}));
    require(std::ranges::none_of(drawing.overlays,[](const auto& layer){return layer.backdrop.has_value();}),
            "Disabled rounded input retained the Figma backdrop blur");
    good(parent.set_enabled(true));good(field->root().focus());
    drawing=take(tree->drawing_snapshot({320,80}));
    const auto restored=drawing.overlays[material(drawing)].images.front().image;
    require(std::ranges::equal(restored->rgba(),enabled->rgba()),
            "Re-enabled rounded input did not restore the enabled material");
    good(tree->input({.kind=WidgetInputKind::composition,.text="한"}));
    auto current=take(field->options());current.surface=lvui::InputFieldSurface::cylinder;good(field->set_options(current));
    require(take(field->snapshot()).marked.has_value(),"Switching to cylinder discarded composition");
    drawing=take(tree->drawing_snapshot({320,80}));index=material(drawing);
    require(std::abs(drawing.overlays[index].backdrop->radius-11)<.001&&
            !std::ranges::equal(drawing.overlays[index].images.front().image->rgba(),enabled->rgba()),
            "Cylinder input did not use the Figma capsule geometry");
    bool cylinder_focus=false;
    for(std::size_t i=index+1;i<drawing.overlays.size();++i)for(const auto& panel:drawing.overlays[i].panels)
        if(std::abs(panel.bounds.width-206)<.01&&std::abs(panel.bounds.height-22)<.01&&panel.radius==11)
            cylinder_focus=true;
    require(cylinder_focus,"Cylinder focus outline did not follow its material radius");
    current.surface=lvui::InputFieldSurface::inline_;good(field->set_options(current));
    drawing=take(tree->drawing_snapshot({320,80}));index=material(drawing);
    require(take(field->snapshot()).marked.has_value()&&drawing.overlays[index].backdrop->radius==0&&
            std::abs(drawing.overlays[index].backdrop->blur_radius-6)<.001,
            "Inline input lost the Figma material or marked text");
    current.surface=lvui::InputFieldSurface::flat;good(field->set_options(current));
    require(take(field->snapshot()).marked.has_value(),"Switching input surface discarded composition");
    drawing=take(tree->drawing_snapshot({320,80}));
    require(std::ranges::none_of(drawing.overlays,[](const auto& layer){return layer.backdrop.has_value();}),
            "Flat input retained the rounded backdrop");
    current.surface=lvui::InputFieldSurface::rounded;good(field->set_options(current));
    require(take(field->snapshot()).marked.has_value(),"Restoring input surface discarded composition");
    (void)material(take(tree->drawing_snapshot({320,80})));
    current.surface=static_cast<lvui::InputFieldSurface>(99);
    require(!field->set_options(current)&&take(field->snapshot()).marked.has_value(),
            "Invalid input surface changed the active field");
    current=take(field->options());current.text.clear();current.placeholder="Hint";good(field->set_options(current));
    const auto placeholder=[](const CanvasFrame& frame){
        for(const auto& layer:frame.overlays)for(const auto& run:layer.texts)if(run.text=="Hint")return run.color.alpha;
        throw std::runtime_error("Rounded input placeholder is missing");
    };
    require(std::abs(placeholder(take(tree->drawing_snapshot({320,80})))-.9)<.001,
            "Enabled rounded input placeholder did not use the LVUI text token");
    good(parent.set_enabled(false));
    require(std::abs(placeholder(take(tree->drawing_snapshot({320,80})))-.3)<.001,
            "Disabled rounded input placeholder did not use the LVUI disabled text token");
    tree->close();
}
}
int main(){
#if defined(IIGUI_TEXT_REPLACEMENT_SELECTION_API) && defined(IIGUI_TEXT_REVISION_API)
    wayland_ime_composition();
    win32_ime_composition();
#endif
#if defined(IIGUI_TEXT_GEOMETRY_API) && defined(IIGUI_TEXT_REVISION_API)
    uikit_composition();
    uikit_outside_selection();
    uikit_composition_replacement();
#if defined(IIGUI_TEXT_CHANGE_SCOPE_API)
    uikit_external_changes();
#endif
#endif
#if defined(IIGUI_TEXT_GEOMETRY_API)
    text_geometry();
#endif
#if defined(IIGUI_TEXT_REVISION_API)
android_text_revision();
#endif
#if defined(IIGUI_TEXT_SURROUNDING_DELETE_API) && defined(IIGUI_TEXT_REVISION_API)
android_composing_deletion();
#endif
#if defined(IIGUI_TEXT_COMPOSITION_REGION_API) && defined(IIGUI_TEXT_REVISION_API)
android_composition_region();
#endif
#if defined(IIGUI_TEXT_REPLACEMENT_SELECTION_API) && defined(IIGUI_TEXT_REVISION_API)
android_selection_deletion();
#endif
#if defined(IIGUI_TEXT_SELECTION_API)
text_service_selection();
#endif
field();nested_commit();focus_notifications();double_click_selection();displayed_text_notifications();user_clear();soft_wrap();scroll_wrapped_read_only();maximum_length();clear_button();retirement_during_clear();rounded_material();}
