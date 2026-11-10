#include "native_input_field_composition.h"
#include "code_editor_test_binding.h"
#include <iiGUI/WaylandConnection.h>
#include <iiGUI/WidgetState.h>
#include "../../iiGUI/src/Widgets/AtSpiCoordinator.h"
#include "../../iiGUI/src/Widgets/AtSpiDbus.h"
#include "../../iiGUI/src/Widgets/AtSpiTextSource.h"
#include <gio/gio.h>
#include <sys/socket.h>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace ii;
using namespace ii::gui;
using namespace ii::gui::detail;
void fake_wayland_text_enter(WaylandConnection&);
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
template<class T>T take(lvui::CodeDocumentResult<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
struct Connected final {GDBusConnection* connection=nullptr;GError* error=nullptr;bool done=false;};
void connected(GObject*,GAsyncResult* result,gpointer data){auto& state=*static_cast<Connected*>(data);state.connection=g_dbus_connection_new_finish(result,&state.error);state.done=true;}
template<class Predicate>void wait(GMainContext* context,Predicate ready,const char* message="AT-SPI peer timed out"){
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(!ready()&&std::chrono::steady_clock::now()<deadline){
        while(g_main_context_pending(context))g_main_context_iteration(context,FALSE);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    check(ready(),message);
}
struct Peer final {
    GMainContext* context=g_main_context_new();GIOStream* streams[2]{};GDBusConnection* server=nullptr;GDBusConnection* client=nullptr;
    Peer(){
        g_main_context_push_thread_default(context);int fds[2]{};check(socketpair(AF_UNIX,SOCK_STREAM,0,fds)==0,"Cannot create peer sockets");
        for(int i=0;i<2;++i){GError* error=nullptr;auto* socket=g_socket_new_from_fd(fds[i],&error);check(socket&&!error,"Cannot adopt peer socket");
            streams[i]=G_IO_STREAM(g_socket_connection_factory_create_connection(socket));g_object_unref(socket);}
        auto* guid=g_dbus_generate_guid();Connected left,right;
        g_dbus_connection_new(streams[0],guid,GDBusConnectionFlags(G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_SERVER|G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_ALLOW_ANONYMOUS),nullptr,nullptr,connected,&left);
        g_dbus_connection_new(streams[1],nullptr,G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,nullptr,nullptr,connected,&right);g_free(guid);
        wait(context,[&]{return left.done&&right.done;});check(left.connection&&right.connection&&!left.error&&!right.error,"Cannot connect peers");
        server=left.connection;client=right.connection;g_dbus_connection_set_exit_on_close(server,FALSE);g_dbus_connection_set_exit_on_close(client,FALSE);
    }
    ~Peer(){if(client){g_dbus_connection_close_sync(client,nullptr,nullptr);g_object_unref(client);}if(server){g_dbus_connection_close_sync(server,nullptr,nullptr);g_object_unref(server);}
        for(auto* stream:streams)if(stream)g_object_unref(stream);g_main_context_pop_thread_default(context);g_main_context_unref(context);}
};
struct Event final {std::string name,detail,text;int position=0,length=0;};
void signal(GDBusConnection*,const gchar*,const gchar*,const gchar*,const gchar* name,GVariant* values,gpointer data){
    if(std::string_view(name)!="TextChanged"&&std::string_view(name)!="TextCaretMoved"&&std::string_view(name)!="TextSelectionChanged")return;
    auto& events=*static_cast<std::vector<Event>*>(data);const gchar* detail=nullptr;gint first=0,second=0;GVariant* payload=nullptr;GVariant* properties=nullptr;
    g_variant_get(values,"(&sii@v@a{sv})",&detail,&first,&second,&payload,&properties);auto* value=g_variant_get_variant(payload);
    const std::string text=g_variant_is_of_type(value,G_VARIANT_TYPE_STRING)?g_variant_get_string(value,nullptr):"";
    events.push_back({name,detail,text,first,second});g_variant_unref(value);g_variant_unref(payload);g_variant_unref(properties);
}
void input_notifications(bool code_editor){
    Peer peer;auto coordinator=AtSpiCoordinator::create("LVUI",[](std::function<void()> task){task();return true;});
    auto window=coordinator->open_window("Editor",{300,100});check(bool(window),"Cannot open accessibility window");
    std::shared_ptr<WidgetState> tree;bool published=false,sync_ok=true;int syncs=0,reads=0;
    auto sync=[&]{if(!published)return;++syncs;auto values=tree->accessibility_snapshots();if(!values||!coordinator->update(*window,*values))sync_ok=false;};
    WidgetFrameHooks hooks;hooks.accessibility_changed=sync;hooks.accessibility_value_changed=sync;
    tree=std::make_shared<WidgetState>([](runtime::Task task){task();},WidgetAppearance{},std::move(hooks));
    auto host=take(tree->content_host({},"host",{300,100}));std::optional<lvui::native::NativeInputField> field;
    std::optional<CodeEditorTestView> editor;std::shared_ptr<lvui::CodeMemoryDocument> document;
    if(code_editor)document=take(lvui::CodeMemoryDocument::create("A😀B"));
    good(host.set_content([&](WidgetBuilder& builder)->core::Result<void>{
        if(code_editor){lvui::native::CodeEditorInput options;options.source=document;options.editor=document;
            auto value=CodeEditorTestView::create(builder,{},"editor",options);if(!value)return std::unexpected(value.error());editor=*value;}
        else{lvui::InputFieldConfiguration options;options.text="A😀B";
            auto value=lvui::native::NativeInputField::create(builder,{},"field",options);if(!value)return std::unexpected(value.error());field=*value;}
        return {};
    }));
    WaylandConnection connection;connection.open({},{});connection.dispatch();fake_wayland_text_enter(connection);
    good(tree->attach(&connection));(void)take(tree->drawing_snapshot({300,100}));
    if(code_editor)good(editor->root().focus());else good(field->root().focus());
    const auto snapshots=take(tree->accessibility_snapshots());
    check(coordinator->publish(*window,snapshots,[tree](WidgetInput event){good(tree->input(std::move(event)));},
        [tree](const AtSpiTextTarget& target){return read_atspi_text(*tree,target);}),"Cannot publish LVUI field");published=true;
    AtSpiDbusBridge bridge(coordinator->model(),{},AtSpiDbusBridge::TextCallbacks{
        .read=[&](std::string_view path){++reads;return coordinator->text(path);},
        .input=[&](std::string path,WidgetInput value){return coordinator->request_text(std::move(path),std::move(value));}});
    check(bridge.attach(peer.server,":1.144"),"Cannot attach AT-SPI bridge");
    check(reads>0,"The initial AT-SPI projection did not expose the LVUI field");coordinator->on_changed([&](std::string){bridge.changed();});
    std::vector<Event> events;const auto subscription=g_dbus_connection_signal_subscribe(peer.client,nullptr,"org.a11y.atspi.Event.Object",nullptr,nullptr,nullptr,G_DBUS_SIGNAL_FLAGS_NONE,signal,&events,nullptr);
    good(tree->input({.kind=WidgetInputKind::text_selection,.replacement_range=TextRange{1,2}}));
    check(sync_ok&&syncs>0,"LVUI selection did not refresh the accessibility projection");
    wait(peer.context,[&]{return events.size()>=2;},"LVUI selection notifications timed out");
    check(events[0].name=="TextCaretMoved"&&events[0].position==2&&events[1].name=="TextSelectionChanged","LVUI selection did not reach AT-SPI");events.clear();
    good(tree->input({.kind=WidgetInputKind::text,.text="한"}));
    wait(peer.context,[&]{std::size_t count=0;for(const auto& event:events)if(event.name=="TextChanged")++count;return count>=2;},"LVUI text replacement notifications timed out");
    std::vector<Event> changes;for(const auto& event:events)if(event.name=="TextChanged")changes.push_back(event);
    check(sync_ok&&changes.size()==2&&changes[0].detail=="delete"&&changes[0].position==1&&changes[0].length==1&&changes[0].text=="😀"&&
        changes[1].detail=="insert"&&changes[1].position==1&&changes[1].length==1&&changes[1].text=="한","LVUI edit did not publish the exact AT-SPI replacement");
    if(code_editor){auto snapshot=take(document->snapshot());check(take(snapshot->read({0,snapshot->length()},32))=="A한B","LVUI code document did not preserve the edit");}
    else check(take(field->snapshot()).text=="A한B","LVUI field did not preserve the edit");
    g_dbus_connection_signal_unsubscribe(peer.client,subscription);coordinator->on_changed({});bridge.detach();published=false;
    check(coordinator->unpublish(*window),"Cannot unpublish LVUI field");tree->close();
}
}
int main(){input_notifications(false);input_notifications(true);}
