#include "native_input_field_composition.h"
#include "code_editor_test_binding.h"
#include <iiGUI/WaylandConnection.h>
#include <iiGUI/WidgetState.h>
#include <memory>
#include <stdexcept>

using namespace ii;
using namespace ii::gui;
using namespace ii::gui::detail;
void fake_wayland_text_enter(WaylandConnection&);
std::uint32_t fake_wayland_text_serial(const WaylandConnection&);
std::uint64_t fake_wayland_text_epoch(const WaylandConnection&);
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(core::Result<T> result){if(!result)throw std::runtime_error(result.error().message);return std::move(*result);}
template<class T>T take(lvui::CodeDocumentResult<T> result){if(!result)throw std::runtime_error(result.error().message);return std::move(*result);}
template<class T>T take(lvui::TextInputResult<T> result){if(!result)throw std::runtime_error(result.error().message);return std::move(*result);}
void good(core::Result<void> result){if(!result)throw std::runtime_error(result.error().message);}
std::size_t panels(const CanvasFrame& frame){std::size_t count=frame.panels.size();for(const auto& layer:frame.overlays)count+=layer.panels.size();return count;}
void done(const std::shared_ptr<WidgetState>& tree,WaylandConnection& connection,WaylandImeBatch batch){
    WaylandImeEvent ime{.kind=WaylandImeEventKind::done,.epoch=fake_wayland_text_epoch(connection),
                        .serial=fake_wayland_text_serial(connection),.batch=std::move(batch)};
    const WaylandSurfaceEvent event{WaylandEventKind::ime,nullptr,&ime};check(tree->native_event(&event),"Wayland event was rejected");
}
void field(){
    WaylandConnection connection;connection.open({},{});connection.dispatch();fake_wayland_text_enter(connection);
    auto tree=std::make_shared<WidgetState>([](runtime::Task task){task();});auto container=take(tree->content_host({},"host",{300,100}));
    std::optional<lvui::native::NativeInputField> input;unsigned edits=0;lvui::InputFieldConfiguration options;options.text="A😀Z";
    good(container.set_content([&](WidgetBuilder& builder)->core::Result<void>{
        auto value=lvui::native::NativeInputField::create(builder,{},"field",options,{.edited=[&](std::string){++edits;}});
        if(!value)return std::unexpected(value.error());input=*value;return {};
    }));
    good(tree->attach(&connection));good(input->root().focus());
    good(tree->input({.kind=WidgetInputKind::text_selection,.replacement_range=TextRange{1,2}}));tree->present();
    WaylandImeBatch batch;batch.preedit="ㅎ";batch.cursor_begin=3;batch.cursor_end=3;done(tree,connection,batch);
    check(take(input->snapshot()).text=="AㅎZ"&&edits==0,"Wayland surface did not project LVUI field composition");
#if defined(IIGUI_TEXT_COMPOSITION_PRESENTATION_API)
    batch.preedit="한😀";batch.cursor_begin=7;batch.cursor_end=3;done(tree,connection,batch);
    auto selected=take(input->snapshot());check(selected.anchor==4&&selected.caret==2,"Wayland field lost reverse preedit selection");
    batch.cursor_begin=-1;batch.cursor_end=-1;done(tree,connection,batch);auto hidden=take(tree->drawing_snapshot({300,100}));
    batch.cursor_begin=7;batch.cursor_end=7;done(tree,connection,batch);auto shown=take(tree->drawing_snapshot({300,100}));
    check(panels(shown)==panels(hidden)+1,"Wayland field did not hide and restore the composition caret");
#endif
    batch={};batch.committed="한";done(tree,connection,batch);
    check(take(input->snapshot()).text=="A한Z"&&edits==1,"Wayland surface did not commit LVUI field input");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::undo}));
    check(take(input->snapshot()).text=="A😀Z","Wayland field composition did not undo as one edit");tree->close();
}
void editor(){
    WaylandConnection connection;connection.open({},{});connection.dispatch();fake_wayland_text_enter(connection);
    auto document=take(lvui::CodeMemoryDocument::create("A한😀Z"));lvui::native::CodeEditorInput config;config.source=document;config.editor=document;
    auto tree=std::make_shared<WidgetState>([](runtime::Task task){task();});auto container=take(tree->content_host({},"host",{300,120}));
    std::optional<CodeEditorTestView> input;
    good(container.set_content([&](WidgetBuilder& builder)->core::Result<void>{
        auto value=CodeEditorTestView::create(builder,{},"editor",config);if(!value)return std::unexpected(value.error());input=*value;return {};
    }));
    good(tree->attach(&connection));good(input->root().focus());
    good(tree->input({.kind=WidgetInputKind::text_selection,.replacement_range=TextRange{2,0}}));tree->present();
    WaylandImeBatch batch;batch.preedit="글";batch.cursor_begin=3;batch.cursor_end=3;done(tree,connection,batch);
#if defined(IIGUI_TEXT_COMPOSITION_PRESENTATION_API)
    batch.preedit="한😀";batch.cursor_begin=7;batch.cursor_end=3;done(tree,connection,batch);
    auto selected=take(input->snapshot());check(selected.selection.anchor==5&&selected.selection.caret==3,"Wayland code editor lost reverse preedit selection");
    batch.cursor_begin=-1;batch.cursor_end=-1;done(tree,connection,batch);auto hidden=take(tree->drawing_snapshot({300,120}));
    batch.cursor_begin=7;batch.cursor_end=7;done(tree,connection,batch);auto shown=take(tree->drawing_snapshot({300,120}));
    check(panels(shown)==panels(hidden)+1,"Wayland code editor did not hide and restore the composition caret");
#endif
    batch={};batch.before=3;batch.after=4;batch.committed="B";done(tree,connection,batch);
    auto snapshot=take(document->snapshot());
    check(take(snapshot->read({0,snapshot->length()},32))=="ABZ","Wayland surface did not update the LVUI code document");tree->close();
}
}
int main(){field();editor();}
