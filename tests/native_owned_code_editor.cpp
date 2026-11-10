#include "code_editor_test_binding.h"
#include <iiGUI/WidgetState.h>
#if defined(IIGUI_TEXT_REPLACEMENT_SELECTION_API) && defined(IIGUI_TEXT_REVISION_API)
#include "android_ime_fixture.h"
#include "../../iiGUI/src/Platform/Win32ImeProtocol.h"
#include "../../iiGUI/src/Platform/WaylandImeProtocol.h"
#endif
#if defined(IIGUI_TEXT_GEOMETRY_API) && defined(IIGUI_TEXT_REVISION_API)
#include "../../iiGUI/src/Platform/UIKitTextInputProtocol.h"
#endif

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
    return std::move(*value);
}
template<class T> T take(lvui::CodeDocumentResult<T> value) {
    if (!value) throw std::runtime_error(value.error().message); return std::move(*value);
}
void good(ii::core::Result<void> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
}
#if defined(IIGUI_TEXT_REPLACEMENT_SELECTION_API) && defined(IIGUI_TEXT_REVISION_API)
void wayland_ime_composition(){
    using namespace ii::gui;using namespace lvui;
    auto document=take(CodeMemoryDocument::create("A한😀Z"));native::CodeEditorInput input;input.source=document;input.editor=document;
    AndroidImeFixture host;auto container=take(host.tree->content_host({},"host",{300,120}));
    std::optional<CodeEditorTestView> editor;unsigned edits=0;
    good(container.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=CodeEditorTestView::create(builder,{},"editor",input,{.edited=[&](const auto&){++edits;}});
        if(!value)return std::unexpected(value.error());editor=*value;return {};
    }));
    (void)take(host.tree->drawing_snapshot({300,120}));good(editor->root().focus());
    good(host.tree->input({.kind=WidgetInputKind::text_selection,.replacement_range=TextRange{2,0}}));
    detail::WaylandImeProtocol ime;good(ime.start(host));detail::WaylandImeBatch batch;
    batch.preedit="글";batch.cursor_begin=3;batch.cursor_end=3;good(ime.apply(host,batch));
    batch={};batch.before=3;batch.after=4;batch.committed="B";good(ime.apply(host,batch));
    auto snapshot=take(document->snapshot());
    require(take(snapshot->read({0,snapshot->length()},32))=="ABZ"&&!take(editor->snapshot()).marked&&edits>0,
            "Wayland deletion around preedit did not reach the LVUI code editor");
    host.tree->close();
}
void win32_ime_composition(){
    using namespace ii::gui;using namespace lvui;
    auto document=take(CodeMemoryDocument::create("A😀Z"));native::CodeEditorInput input;input.source=document;input.editor=document;
    AndroidImeFixture host;auto container=take(host.tree->content_host({},"host",{300,120}));
    std::optional<CodeEditorTestView> editor;unsigned edits=0;
    good(container.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=CodeEditorTestView::create(builder,{},"editor",input,{.edited=[&](const auto&){++edits;}});
        if(!value)return std::unexpected(value.error());editor=*value;return {};
    }));
    (void)take(host.tree->drawing_snapshot({300,120}));good(editor->root().focus());
    good(host.tree->input({.kind=WidgetInputKind::text_selection,.replacement_range=TextRange{1,2}}));
    detail::Win32ImeProtocol ime;good(ime.start(host));detail::Win32ImeMessage first;first.composing=u"ㅎ";first.cursor=1;
    good(ime.apply(host,first));detail::Win32ImeMessage next;next.committed=u"한";next.composing=u"글";next.cursor=1;
    good(ime.apply(host,next));auto value=take(editor->snapshot());
    require(value.marked&&value.marked->location==2&&value.marked->length==1&&
            take(document->snapshot())->revision()==1&&edits==1,
            "Windows IME result and next candidate did not reach the LVUI code editor");
    good(ime.end(host));value=take(editor->snapshot());
    require(!value.marked&&take(take(document->snapshot())->read({0,3},16))=="A한Z"&&edits==1,
            "Windows IME end lost or repeated the committed code edit");
    host.tree->close();
}
#endif
#if defined(IIGUI_TEXT_GEOMETRY_API) && defined(IIGUI_TEXT_REVISION_API)
void uikit_composition(){
    using namespace ii::gui;using namespace lvui;
    auto document=take(CodeMemoryDocument::create("A😀Z\nnext"));native::CodeEditorInput input;input.source=document;input.editor=document;
    AndroidImeFixture host;auto container=take(host.tree->content_host({},"host",{300,120}));
    std::optional<CodeEditorTestView> editor;unsigned edits=0;
    good(container.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=CodeEditorTestView::create(builder,{},"editor",input,{.edited=[&](const auto&){++edits;}});
        if(!value)return std::unexpected(value.error());editor=*value;return {};
    }));
    (void)take(host.tree->drawing_snapshot({300,120}));good(editor->root().focus());
    detail::UIKitTextInputProtocol text;const auto generation=take(text.state(host)).generation;
    require(take(text.move(host,generation,1,detail::UIKitTextDirection::down,1))>=5,
            "UIKit code movement did not follow the next visual line");
    good(text.select(host,generation,{1,2}));good(text.compose(host,generation,"하나",{0,1}));
    auto projected=take(editor->snapshot());
    require(projected.marked&&projected.marked->location==1&&projected.marked->length==2&&
            projected.selection==CodeSelection{1,2}&&take(document->snapshot())->revision()==0&&edits==0,
            "UIKit code composition changed the source or selected the wrong candidate");
    good(text.compose(host,generation,"한",{1,0}));good(text.finish(host,generation));
    require(take(take(document->snapshot())->read({0,3},16))=="A한Z"&&edits==1,
            "UIKit code candidate did not commit exactly once");
    host.tree->close();
}
void uikit_outside_selection(){
    using namespace ii::gui;using namespace lvui;
    auto document=take(CodeMemoryDocument::create("A😀Z"));native::CodeEditorInput input;input.source=document;input.editor=document;
    AndroidImeFixture host;auto container=take(host.tree->content_host({},"host",{300,120}));
    std::optional<CodeEditorTestView> editor;unsigned edits=0;
    good(container.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=CodeEditorTestView::create(builder,{},"editor",input,{.edited=[&](const auto&){++edits;}});
        if(!value)return std::unexpected(value.error());editor=*value;return {};
    }));
    (void)take(host.tree->drawing_snapshot({300,120}));good(editor->root().focus());
    detail::UIKitTextInputProtocol text;const auto generation=take(text.state(host)).generation;
    good(text.select(host,generation,{1,2}));good(text.compose(host,generation,"하나",{1,0}));
    good(text.select(host,generation,{0,1}));auto value=take(editor->snapshot());
    require(!value.marked&&value.selection==CodeSelection{0,1}&&take(document->snapshot())->revision()==1&&edits==1&&
            take(take(document->snapshot())->read({0,4},32))=="A하나Z",
            "UIKit code outside selection did not commit the source and selection together");
    good(text.select(host,generation,{1,2}));good(text.compose(host,generation,"글",{1,0}));
    const auto before=take(editor->snapshot());
    (void)take(document->replace({.revision=1,.range={0,0},.text="external"}));
    auto rejected=text.select(host,generation,{0,0});const auto after=take(editor->snapshot());
    require(!rejected&&after.marked&&before.marked&&after.marked->location==before.marked->location&&
            after.marked->length==before.marked->length&&after.selection==before.selection&&
            take(document->snapshot())->revision()==2&&edits==1,
            "A stale code candidate was committed or its selection moved");
    host.tree->close();
}
void uikit_composition_replacement(){
    using namespace ii::gui;using namespace lvui;
    struct Case{TextRange range;const char* text;const char* expected;std::size_t length,caret;};
    const Case cases[]={{{0,1},"X","X한글CDE",6,1},{{4,1},"Q","A한글CQE",6,5},
                        {{2,1},"나","A한나CDE",6,3},{{0,4},"K","KDE",3,1}};
    for(const auto& c:cases){
        auto document=take(CodeMemoryDocument::create("ABCDE"));native::CodeEditorInput input;input.source=document;input.editor=document;
        AndroidImeFixture host;auto container=take(host.tree->content_host({},"host",{300,120}));
        std::optional<CodeEditorTestView> editor;unsigned edits=0;
        good(container.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
            auto next=CodeEditorTestView::create(builder,{},"editor",input,{.edited=[&](const auto&){++edits;}});
            if(!next)return std::unexpected(next.error());editor=*next;return {};
        }));
        (void)take(host.tree->drawing_snapshot({300,120}));good(editor->root().focus());
        detail::UIKitTextInputProtocol text;const auto generation=take(text.state(host)).generation;
        good(text.select(host,generation,{1,1}));good(text.compose(host,generation,"한글",{1,0}));
        good(text.replace(host,generation,c.range,c.text));const auto value=take(editor->snapshot());const auto view=take(document->snapshot());
        require(!value.marked&&value.selection==CodeSelection{c.caret,c.caret}&&view->revision()==1&&edits==1&&
                take(view->read({0,c.length},32))==c.expected,
                "UIKit code replacement did not commit one projected source edit");
        host.tree->close();
    }
}
#if defined(IIGUI_TEXT_CHANGE_SCOPE_API)
void uikit_external_changes(){
    using namespace ii::gui;using namespace lvui;
    auto document=take(CodeMemoryDocument::create("AB"));native::CodeEditorInput input;input.source=document;input.editor=document;
    std::optional<CodeEditorTestView> editor;std::vector<std::string> notices;
    detail::WidgetFrameHooks hooks;
    hooks.text_input_change=[&](bool value)->ii::runtime::Task{
        const auto selection=take(editor->snapshot()).selection;
        notices.push_back(std::string(value?"will-text:":"will-selection:")+std::to_string(selection.caret));
        return [&]{notices.push_back("did:"+std::to_string(take(editor->snapshot()).selection.caret));};
    };
    AndroidImeFixture host;host.tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();},WidgetAppearance{},hooks);
    auto container=take(host.tree->content_host({},"host",{300,120}));
    good(container.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=CodeEditorTestView::create(builder,{},"editor",input);
        if(!value)return std::unexpected(value.error());editor=*value;return {};
    }));
    (void)take(host.tree->drawing_snapshot({300,120}));good(editor->root().focus());
    good(editor->set_selection(1,1));
    require(notices==std::vector<std::string>({"will-selection:0","did:1"}),"Programmatic code selection notified after mutation");
    notices.clear();good(editor->insert("한"));
    require(notices==std::vector<std::string>({"will-text:1","did:2"}),"Programmatic code insertion notified after mutation");
    notices.clear();detail::UIKitTextInputProtocol text;const auto generation=take(text.state(host)).generation;
    good(text.compose(host,generation,"글",{1,0}));good(text.finish(host,generation));
    require(notices.empty(),"A system-originated code edit notified the input delegate");
    host.tree->close();
}
#endif
#endif
#if defined(IIGUI_TEXT_GEOMETRY_API)
void text_geometry(){
    using namespace ii::gui;using namespace lvui;
    std::string text="A😀B\n한Z";for(int line=0;line<80;++line)text+="\nrow";
    auto document=take(CodeMemoryDocument::create(text));native::CodeEditorInput input;input.source=document;input.editor=document;
    input.appearance.width=200;input.appearance.height=80;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{300,120}));std::optional<CodeEditorTestView> editor;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=CodeEditorTestView::create(builder,{},"editor",input);
        if(!value)return std::unexpected(value.error());editor=*value;return {};
    }));
    (void)take(tree->drawing_snapshot({300,120}));good(editor->root().focus());
    auto single=take(editor->root().text_geometry({1,2}));
    require(single.start_caret.width>0&&single.end_caret.width>0&&single.end_caret.x>single.start_caret.x&&
            single.selection.size()==1&&single.selection[0].width>0,"Code geometry lost its shaped emoji");
    auto multiple=take(editor->root().text_geometry({0,6}));
    require(multiple.selection.size()>=2&&multiple.start_caret.y<multiple.end_caret.y,
            "Code geometry lost its visible line break");
    const auto length=take(document->snapshot())->length();
    require(!editor->root().text_geometry({2,1})&&!editor->root().text_geometry({length,1}),
            "Code geometry accepted an invalid UTF-16 boundary");
    good(editor->set_selection(length,length));
    auto hidden=take(editor->root().text_geometry({0,1}));auto shown=take(editor->root().text_geometry({length-1,1}));
    require(hidden.selection.empty()&&hidden.start_caret.width==0&&shown.end_caret.width>0,
            "Code geometry read the wrong viewport after scrolling");
    tree->close();
}
#endif
#if defined(IIGUI_TEXT_REVISION_API)
void android_text_revision(){
    using namespace ii::gui;using namespace lvui;
    auto document=take(CodeMemoryDocument::create("A"));native::CodeEditorInput input;input.source=document;input.editor=document;
    AndroidImeFixture host;auto container=take(host.tree->content_host({},"host",{300,120}));
    std::optional<CodeEditorTestView> editor;
    good(container.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=CodeEditorTestView::create(builder,{},"editor",input);
        if(!value)return std::unexpected(value.error());editor=*value;return {};
    }));
    (void)take(host.tree->drawing_snapshot({300,120}));good(editor->root().focus());
    detail::AndroidImeProtocol ime;const auto gen=take(ime.state(host)).generation;
    good(ime.select(host,gen,1,1));
    good(ime.composition(host,gen,"한",1));const auto before=take(editor->root().snapshot());
    good(ime.composition(host,gen,"글",1));const auto after=take(editor->root().snapshot());
    require(before.text_revision&&after.text_revision&&before.text_revision!=after.text_revision&&
            before.text_length==after.text_length&&before.text_selection==after.text_selection&&before.marked_range==after.marked_range&&
            take(ime.read(host,gen,{0,2})).text=="A글","A same-length code candidate reused its input revision");
    host.tree->close();
}
#endif
#if defined(IIGUI_TEXT_SURROUNDING_DELETE_API) && defined(IIGUI_TEXT_REVISION_API)
void android_composing_deletion(){
    using namespace ii::gui;using namespace lvui;
    auto document=take(CodeMemoryDocument::create("A😀xy한Z"));native::CodeEditorInput input;input.source=document;input.editor=document;
    AndroidImeFixture host;auto container=take(host.tree->content_host({},"host",{300,120}));
    std::optional<CodeEditorTestView> editor;unsigned edits=0;
    good(container.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=CodeEditorTestView::create(builder,{},"editor",input,{.edited=[&](const auto&){
            ++edits;if(editor)good(editor->sync_source());
        }});
        if(!value)return std::unexpected(value.error());editor=*value;return {};
    }));
    (void)take(host.tree->drawing_snapshot({300,120}));good(editor->root().focus());
    detail::AndroidImeProtocol ime;const auto gen=take(ime.state(host)).generation;
    good(ime.select(host,gen,5,4));good(ime.composing_region(host,gen,3,6));
    good(ime.erase_surrounding_codepoints(host,gen,2,1));
    auto value=take(editor->snapshot());
    require(value.marked&&value.marked->location==1&&value.marked->length==1&&value.selection==CodeSelection{2,1}&&
            take(ime.read(host,gen,{0,3})).text=="AyZ"&&take(take(document->snapshot())->read({0,5},16))=="Axy한Z"&&edits==1,
            "Android composing code deletion lost source or selection");
    good(ime.finish(host,gen));
    require(take(take(document->snapshot())->read({0,3},16))=="AyZ"&&edits==2,
            "The shortened code candidate failed to commit");
    host.tree->close();
}
#endif
#if defined(IIGUI_TEXT_COMPOSITION_REGION_API) && defined(IIGUI_TEXT_REVISION_API)
void android_composition_region(){
    using namespace ii::gui;using namespace lvui;
    auto document=take(CodeMemoryDocument::create("A😀Z"));native::CodeEditorInput input;input.source=document;input.editor=document;
    AndroidImeFixture host;auto container=take(host.tree->content_host({},"host",{300,120}));
    std::optional<CodeEditorTestView> editor;unsigned edits=0;
    good(container.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=CodeEditorTestView::create(builder,{},"editor",input,{.edited=[&](const auto&){++edits;}});
        if(!value)return std::unexpected(value.error());editor=*value;return {};
    }));
    (void)take(host.tree->drawing_snapshot({300,120}));good(editor->root().focus());
    detail::AndroidImeProtocol ime;const auto gen=take(ime.state(host)).generation;
    good(ime.select(host,gen,3,1));good(ime.composing_region(host,gen,3,1));
    auto value=take(editor->snapshot());
    require(value.marked&&value.marked->location==1&&value.marked->length==2&&value.selection==CodeSelection{3,1}&&edits==0,
            "Android region marking changed code selection or emitted an edit");
    good(ime.composition(host,gen,"한",1));good(ime.select(host,gen,3,0));good(ime.composing_region(host,gen,2,0));
    value=take(editor->snapshot());
    require(value.marked&&value.marked->location==0&&value.marked->length==2&&value.selection==CodeSelection{3,0}&&
            take(document->snapshot())->revision()==1&&edits==1,"Android region update lost the committed code candidate");
    good(ime.commit(host,gen,"K",1));
    require(take(take(document->snapshot())->read({0,2},16))=="KZ"&&take(document->snapshot())->revision()==2&&edits==2,
            "Marked code replacement was not committed once");
    host.tree->close();
}
#endif
#if defined(IIGUI_TEXT_REPLACEMENT_SELECTION_API) && defined(IIGUI_TEXT_REVISION_API)
void android_selection_deletion(){
    using namespace ii::gui;using namespace lvui;
    auto document=take(CodeMemoryDocument::create("A😀B한Z"));
    native::CodeEditorInput input;input.source=document;input.editor=document;
    AndroidImeFixture host;auto container=take(host.tree->content_host({},"host",{300,120}));
    std::optional<CodeEditorTestView> editor;unsigned edits=0;
    good(container.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=CodeEditorTestView::create(builder,{},"editor",input,{.edited=[&](const auto&){++edits;}});
        if(!value)return std::unexpected(value.error());editor=*value;return {};
    }));
    (void)take(host.tree->drawing_snapshot({300,120}));good(editor->root().focus());
    detail::AndroidImeProtocol ime;const auto generation=take(ime.state(host)).generation;
    good(ime.select(host,generation,5,3));good(ime.erase_surrounding_codepoints(host,generation,1,1));
    require(take(take(document->snapshot())->read({0,3},16))=="AB한"&&
            take(editor->snapshot()).selection==CodeSelection{3,1}&&edits==1,
            "Android surrounding deletion did not preserve code selection in one edit");
    host.tree->close();
}
#endif
#if defined(IIGUI_TEXT_REPLACEMENT_CURSOR_API) && defined(IIGUI_TEXT_SERVICE_INPUT_API)
void targeted_text_service(){
    using namespace ii::gui;using namespace lvui;
    auto document=take(CodeMemoryDocument::create("A😀Z"));
    native::CodeEditorInput input;input.source=document;input.editor=document;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{300,120}));std::optional<CodeEditorTestView> editor;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=CodeEditorTestView::create(builder,{},"editor",input);
        if(!value)return std::unexpected(value.error());editor=*value;return {};
    }));
    (void)take(tree->drawing_snapshot({300,120}));good(editor->root().focus());std::string target;
    for(const auto& node:take(tree->accessibility_snapshots()))if(node.focused&&node.text_input)target=node.id;
    require(!target.empty(),"Code editor did not expose its text-service identity");
    good(tree->input({.kind=WidgetInputKind::text_selection,.target=target,.replacement_range=TextRange{1,2},.text_service=true}));
    good(tree->input({.kind=WidgetInputKind::composition,.text="한",.target=target,.text_cursor_offset=-1,.text_service=true}));
    require(take(editor->snapshot()).selection==CodeSelection{0,0}&&take(editor->snapshot()).marked.has_value()&&
            take(document->snapshot())->revision()==0,"Targeted code composition lost its projected selection");
    good(tree->input({.kind=WidgetInputKind::text,.text="하",.target=target,.text_cursor_offset=2,.text_service=true}));
    require(take(editor->snapshot()).selection==CodeSelection{3,3}&&take(document->snapshot())->revision()==1&&
            take(take(document->snapshot())->read({0,3},16))=="A하Z","Targeted code commit lost its text or cursor");
    tree->close();
}
#endif
}
int main() {
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
    using namespace ii::gui; using namespace lvui;
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
#if defined(IIGUI_TEXT_REPLACEMENT_CURSOR_API) && defined(IIGUI_TEXT_SERVICE_INPUT_API)
    targeted_text_service();
#endif
    auto document = take(CodeMemoryDocument::create("A😀\n두번째"));
    native::CodeEditorInput input; input.source = document; input.editor = document; input.appearance.width = 240; input.appearance.height = 100;
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {300, 120}));
    std::optional<CodeEditorTestView> editor; std::vector<CodeDocumentChange> edits; std::string clipboard;
    bool checked_reentrancy = false;
    good(host.set_content([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        auto column = builder.vstack({}, "column", {.spacing=0, .alignment=StackAlignment::leading});
        if (!column) return std::unexpected(column.error());
        auto value = CodeEditorTestView::create(builder, *column, "editor", input,
            {.edited = [&](const auto& change) { edits.push_back(change);
                good(editor->sync_source()); auto nested = editor->set_selection(0, 0);
                checked_reentrancy = !nested && nested.error().code == ii::core::ErrorCode::busy; }},
            {.write = [&](std::string text) -> ii::core::Result<void> { clipboard = std::move(text); return {}; }});
        if (!value) return std::unexpected(value.error()); editor = *value; return {};
    }));
    require(take(editor->frame()).lines.size() == 2 && take(editor->root().snapshot()).size.width == 240,
            "Native code editor did not render its initial visible lines");
    auto drawing = take(tree->drawing_snapshot({300, 120}));
    require(!drawing.images.empty() || !drawing.overlays.empty(), "Native code editor produced no shaped text drawing");
    auto appearance = take(editor->appearance()); appearance.width = 210; good(editor->set_appearance(appearance));
    require(take(editor->root().snapshot()).size.width == 210, "Code editor did not apply a changed appearance");
    auto invalid = appearance; invalid.line_height = 0;
    require(!editor->set_appearance(invalid) && take(editor->appearance()) == appearance,
            "An invalid appearance replaced the working code editor");
    good(editor->root().focus());
#if defined(IIGUI_TEXT_SELECTION_API)
    good(tree->input({.kind=WidgetInputKind::text_selection,.replacement_range=TextRange{1,2}}));
    require(take(editor->snapshot()).selection==CodeSelection{1,3}&&take(document->snapshot())->revision()==0,
            "Text service selection did not reach the code editor");
#if defined(IIGUI_DIRECTIONAL_TEXT_SELECTION_API)
    good(tree->input({.kind=WidgetInputKind::text_selection,.replacement_range=TextRange{1,2},.selection_reversed=true}));
    require(take(editor->snapshot()).selection==CodeSelection{3,1}&&take(editor->root().snapshot()).text_selection_reversed,
            "Text service lost directional code selection");
#endif
    auto invalid_selection=tree->input({.kind=WidgetInputKind::text_selection,.replacement_range=TextRange{2,0}});
    const auto retained_selection=take(editor->snapshot()).selection.range();
    require(!invalid_selection&&retained_selection.location==1&&retained_selection.length==2,"A split scalar changed the code selection");
#else
    good(editor->set_selection(1, 3));
#endif
    good(tree->input({.kind=WidgetInputKind::composition, .text="ㅎ", .composition_selection=TextRange{1, 0}}));
#if defined(IIGUI_TEXT_SELECTION_API)
    good(tree->input({.kind=WidgetInputKind::text_selection,.replacement_range=TextRange{0,0}}));
    require(take(editor->snapshot()).selection==CodeSelection{0,0}&&take(editor->snapshot()).marked.has_value()&&
            take(document->snapshot())->revision()==0,"Text selection outside code composition lost the candidate");
#if defined(IIGUI_DIRECTIONAL_TEXT_SELECTION_API)
    good(tree->input({.kind=WidgetInputKind::text_selection,.replacement_range=TextRange{0,2},.selection_reversed=true}));
    require(take(editor->snapshot()).selection==CodeSelection{2,0}&&take(editor->root().snapshot()).text_selection_reversed&&
            take(editor->snapshot()).marked.has_value(),"Directional code selection lost the candidate");
#endif
#endif
    require(take(document->snapshot())->revision() == 0 && take(editor->snapshot()).marked.has_value() &&
            take(editor->root().text_content({0, 2})).text == "Aㅎ", "Native code composition changed the document or exposed stale text");
    good(tree->input({.kind=WidgetInputKind::composition, .text="한", .composition_selection=TextRange{1, 0}}));
    good(tree->input({.kind=WidgetInputKind::commit_composition}));
    require(take(document->snapshot())->revision() == 1 && edits.size() == 1 && !take(editor->snapshot()).marked &&
            take(take(document->snapshot())->read({0, 3}, 16)) == "A한\n", "Native composition did not commit once");
    good(tree->input({.kind=WidgetInputKind::key_down, .key=WidgetKey::backspace}));
    good(tree->input({.kind=WidgetInputKind::text, .text="B"}));
    require(take(document->snapshot())->revision() == 3 && edits.size() == 3 && checked_reentrancy &&
            take(take(document->snapshot())->read({0, 3}, 16)) == "AB\n", "Native keyboard editing did not reach the independent source");
    good(editor->insert(""));
    require(take(document->snapshot())->revision() == 3 && edits.size() == 3, "An empty code insertion emitted a false edit");
    good(editor->set_selection(0, 2)); good(editor->insert("AB"));
    require(take(document->snapshot())->revision() == 3 && edits.size() == 3 &&
            take(editor->snapshot()).selection == CodeSelection{2, 2}, "Identical replacement emitted an edit or failed to collapse selection");
    good(editor->set_selection(0, 2)); good(tree->input({.kind=WidgetInputKind::key_down, .key=WidgetKey::copy}));
    require(clipboard == "AB" && take(document->snapshot())->revision() == 3, "Code copy changed the document or lost selected text");
    good(tree->input({.kind=WidgetInputKind::key_down, .key=WidgetKey::cut}));
    require(take(document->snapshot())->revision() == 4 && clipboard == "AB", "Code cut did not commit the copied selection");
    good(editor->insert("AB"));
    (void)take(document->replace({.revision=5, .range={0, 0}, .text="external "}));
    require(!editor->insert("stale") && take(document->snapshot())->revision() == 6, "A stale native editor overwrote an external change");
    good(editor->refresh()); good(editor->insert("!")); require(take(document->snapshot())->revision() == 7, "Native editor did not recover after refresh");
    bool wrong_thread = false; std::thread worker([&] { auto value = editor->set_selection(0, 0); wrong_thread = !value && value.error().code == ii::core::ErrorCode::wrong_executor; });
    worker.join(); require(wrong_thread, "Native code editor accepted a foreign-thread mutation");
    good(host.clear_content()); require(!editor->set_selection(0, 0), "A retired code editor accepted an update"); tree->close();

    std::string lines; for (int i = 0; i < 10000; ++i) lines += "line " + std::to_string(i) + "\n";
    auto large = take(CodeMemoryDocument::create(std::move(lines))); native::CodeEditorInput many;
    many.source = large; many.appearance.width = 180; many.appearance.height = 80;
    auto next_tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto next_host = take(next_tree->content_host({}, "host", {200, 100})); std::optional<CodeEditorTestView> reader;
    good(next_host.set_content([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        auto column = builder.vstack({}, "column", {.spacing=0, .alignment=StackAlignment::leading});
        if (!column) return std::unexpected(column.error());
        auto value = CodeEditorTestView::create(builder, *column, "reader", many);
        if (!value) return std::unexpected(value.error()); reader = *value; return {};
    }));
    good(reader->scroll_to_line(9000)); const auto frame = take(reader->frame());
    require(!frame.lines.empty() && frame.lines.front().line == 9000 && frame.lines.size() <= 7 &&
            !reader->insert("x") && take(large->snapshot())->revision() == 0,
            "Read-only large code view materialized unrelated lines or edited the source");
    (void)take(next_tree->drawing_snapshot({200, 100}));
#if defined(IIGUI_TEXT_SELECTION_API)
    good(reader->root().focus());
    good(next_tree->input({.kind=WidgetInputKind::text_selection,.replacement_range=TextRange{1,3}}));
    require(take(reader->snapshot()).selection==CodeSelection{1,4}&&take(large->snapshot())->revision()==0,
            "Read-only code view rejected text-service selection");
#endif
    good(next_host.clear_content()); next_tree->close();

    // The clipboard must receive the complete selected source, even when a
    // document read is bounded to 4 MiB and a chunk boundary crosses a scalar.
    std::string report(65535, 'a'); report += "😀한\n";
    for (int i = 0; i < 4500; ++i) { report.append(1024, 'b'); report += '\n'; }
    auto report_document = take(CodeMemoryDocument::create(report));
    native::CodeEditorInput report_input; report_input.source = report_document;
    auto report_tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto report_host = take(report_tree->content_host({}, "host", {200, 100}));
    std::optional<CodeEditorTestView> report_reader;
    std::string copied_report;
    good(report_host.set_content([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        auto value = CodeEditorTestView::create(builder, {}, "report", report_input, {},
            {.write = [&](std::string text) -> ii::core::Result<void> { copied_report = std::move(text); return {}; }});
        if (!value) return std::unexpected(value.error()); report_reader = *value; return {};
    }));
    good(report_reader->root().focus());
    good(report_tree->input({.kind=WidgetInputKind::key_down, .key=WidgetKey::select_all}));
    good(report_tree->input({.kind=WidgetInputKind::key_down, .key=WidgetKey::copy}));
    require(copied_report == report && take(report_document->snapshot())->revision() == 0,
            "Large Unicode selection was truncated or modified during copy");
    good(report_host.clear_content()); report_tree->close();

    auto retired_document=take(CodeMemoryDocument::create("a"));std::weak_ptr<CodeMemoryDocument> weak_document=retired_document;
    native::CodeEditorInput retiring;retiring.source=retired_document;retiring.editor=retired_document;
    auto retiring_tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto retiring_host=take(retiring_tree->content_host({},"retiring",{200,100}));std::optional<CodeEditorTestView> retiring_editor;
    auto captured=std::make_shared<int>(1);std::weak_ptr<int> weak_callback=captured;
    good(retiring_host.set_content([&](WidgetBuilder& b)->ii::core::Result<void>{
        auto value=CodeEditorTestView::create(b,{},"retiring-editor",retiring,{.edited=[&,held=captured](const auto&){good(retiring_host.clear_content());}});
        if(!value)return std::unexpected(value.error());retiring_editor=*value;return {};
    }));
    retiring.source.reset();retiring.editor.reset();captured.reset();good(retiring_editor->insert("x"));
    require(take(retired_document->snapshot())->revision()==1,"Retiring an editor lost its committed edit");
    retired_document.reset();require(weak_callback.expired()&&weak_document.expired()&&!retiring_editor->snapshot(),
        "Retired code editor retained its callbacks or document");retiring_tree->close();
}
