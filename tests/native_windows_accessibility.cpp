#include "NativeOwnedButton.generated.h"
#include "NativeOwnedInputField.generated.h"
#include <iiGUI/WidgetState.h>
#include "../../iiGUI/src/Widgets/WindowsAccessibility.h"
#include <windows.h>
#include <oleacc.h>
#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace ii;
using namespace ii::gui;
using namespace ii::gui::detail;
namespace {
std::mutex messages_mutex;std::vector<MSG> messages;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
VARIANT child(LONG value){VARIANT result{};result.vt=VT_I4;result.lVal=value;return result;}
}
HRESULT CoInitializeEx(void*,DWORD){return S_OK;}void CoUninitialize(){}
BSTR SysAllocStringLen(const wchar_t* value,UINT size){auto out=new(std::nothrow) wchar_t[size+1];if(!out)return nullptr;if(value)std::copy_n(value,size,out);out[size]=0;return out;}
UINT SysStringLen(BSTR value){return value?UINT(std::wcslen(value)):0;}
void SysFreeString(BSTR value){delete[] value;}
int MultiByteToWideChar(UINT,DWORD,const char* text,int length,wchar_t* out,int capacity){
    if(!out)return length;if(capacity<length)return 0;for(int i=0;i<length;++i)out[i]=static_cast<unsigned char>(text[i]);return length;
}
BOOL GetClientRect(HWND,RECT* out){*out={0,0,640,480};return 1;}BOOL ClientToScreen(HWND,POINT* out){*out={0,0};return 1;}
UINT GetDpiForWindow(HWND){return 96;}int GetWindowTextLengthW(HWND){return 4;}
int GetWindowTextW(HWND,wchar_t* out,int size){if(size<5)return 0;std::copy_n(L"LVUI",5,out);return 4;}
BOOL PostMessageW(HWND window,UINT type,WPARAM w,LPARAM l){std::lock_guard lock(messages_mutex);messages.push_back({window,type,w,l,0,{}});return 1;}
HWND SetFocus(HWND value){return value;}void NotifyWinEvent(DWORD,HWND,LONG,LONG){}
LRESULT LresultFromObject(REFIID,WPARAM,IUnknown* object){object->AddRef();return reinterpret_cast<LRESULT>(object);}
HRESULT AccessibleObjectFromWindow(HWND,DWORD,REFIID,void** result){*result=nullptr;return E_NOTIMPL;}

namespace {
IAccessible* find_peer(IAccessible& root,std::wstring_view expected,unsigned depth=0){
    if(depth>32)return nullptr;
    LONG count=0;check(root.get_accChildCount(&count)==S_OK,"Cannot count accessible buttons");
    for(LONG i=1;i<=count;++i){IDispatch* dispatch=nullptr;if(root.get_accChild(child(i),&dispatch)!=S_OK||!dispatch)continue;
        auto* peer=static_cast<IAccessible*>(dispatch);BSTR name=nullptr;
        const auto status=peer->get_accName(child(0),&name);const auto actual=name?std::wstring_view(name):std::wstring_view{};
        const bool matches=status==S_OK&&name&&actual==expected;
        if(name)SysFreeString(name);if(matches)return peer;
        auto* nested=find_peer(*peer,expected,depth+1);peer->Release();if(nested)return nested;}
    return nullptr;
}
void generated_button_lifetime(){
    using View=lvui_generated::NativeOwnedButton;auto tree=std::make_shared<WidgetState>([](runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{640,480}));auto image=take(rgba_image(1,1,std::vector<std::uint8_t>{255,255,255,255}));
    auto mount=[&](std::shared_ptr<View::Scope>& scope){
        good(host.set_content([&](WidgetBuilder& builder){return View::build(builder,{},[&](auto& value){value.backend_0.icon.set(image);},&scope);}));
        (void)take(tree->drawing_snapshot({640,480}));
    };
    std::shared_ptr<View::Scope> first,second;mount(first);
    WidgetSurfaceCallbacks callbacks;
    callbacks.accessibility=[tree]{auto nodes=take(tree->accessibility_snapshots());
        // The test has no OS window; emulate an attached surface for projection.
        for(auto& node:nodes)node.attached=true;return nodes;};
    callbacks.input=[tree](WidgetInput input){good(tree->input(std::move(input)));};
    auto bridge=std::make_unique<WindowsAccessibility>(reinterpret_cast<void*>(1),callbacks);bridge->refresh();
    MSG request{reinterpret_cast<void*>(1),WM_GETOBJECT,0,OBJID_CLIENT,0,{}};auto result=bridge->result(&request);
    check(result&&*result,"Cannot obtain MSAA root");auto* root=reinterpret_cast<IAccessible*>(*result);auto* old=find_peer(*root,L"Save document");
    check(old,"Generated LVUI save button was not exposed");
    std::atomic<HRESULT> submitted{E_FAIL};std::thread worker([&]{submitted=old->accDoDefaultAction(child(0));});worker.join();
    check(submitted==S_OK&&!messages.empty()&&first->backend_0.saves==0,"Generated button action was not queued");const auto pending=messages.back();
    good(host.clear_content());bridge->refresh();mount(second);bridge->refresh();
    check(bridge->input(&pending)&&first->backend_0.saves==0&&second->backend_0.saves==0,"Retired generated button activated a replacement");
    BSTR name=nullptr;check(old->get_accName(child(0),&name)==CO_E_OBJNOTCONNECTED&&!name,"Retired generated button stayed accessible");
    auto* fresh=find_peer(*root,L"Save document");check(fresh,"Replacement LVUI save button was not exposed");
    std::thread replacement([&]{submitted=fresh->accDoDefaultAction(child(0));});replacement.join();
    check(submitted==S_OK&&bridge->input(&messages.back())&&second->backend_0.saves==1,"Current generated button did not receive the MSAA action");
    fresh->Release();old->Release();root->Release();bridge.reset();good(host.clear_content());tree->close();
}
void generated_input_replacement(){
    using View=lvui_generated::NativeOwnedInputField;
    auto tree=std::make_shared<WidgetState>([](runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{640,480}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](WidgetBuilder& builder){return View::build(builder,{}, {},&scope);}));
    (void)take(tree->drawing_snapshot({640,480}));
    auto nodes=take(tree->accessibility_snapshots());auto other=std::ranges::find_if(nodes,[](const auto& node){return node.id.ends_with("/multi");});
    check(other!=nodes.end(),"Generated second editor is missing");auto second=take(tree->find(other->id));good(second.focus());
    WidgetSurfaceCallbacks callbacks;
    callbacks.accessibility=[tree]{auto result=take(tree->accessibility_snapshots());for(auto& node:result)node.attached=true;return result;};
    callbacks.lookup=[tree](std::string_view id){auto value=take(tree->accessibility_snapshot(id));value.attached=true;return value;};
    callbacks.text_content=[tree](std::string_view id,TextRange range)->core::Result<TextContent>{auto widget=tree->find(id);if(!widget)return std::unexpected(widget.error());return widget->text_content(range);};
    callbacks.accessibility_text=[tree](std::string_view id,const TextInputRevision& revision,std::string text){return tree->replace_accessibility_text(id,revision,std::move(text));};
    auto bridge=std::make_unique<WindowsAccessibility>(reinterpret_cast<void*>(1),callbacks);bridge->refresh();
    MSG request{reinterpret_cast<void*>(1),WM_GETOBJECT,0,OBJID_CLIENT,0,{}};auto result=bridge->result(&request);
    check(result&&*result,"Cannot obtain MSAA input root");auto* root=reinterpret_cast<IAccessible*>(*result);
    auto* peer=find_peer(*root,L"Text field");check(peer,"Generated LVUI field was not exposed");
    auto value=SysAllocStringLen(L"Accessible",10);check(value,"Cannot allocate input text");
    check(peer->put_accValue(child(0),value)==S_OK&&scope->backend_0.text.get()=="Accessible"&&
          scope->backend_0.edits.size()==1&&take(second.snapshot()).focused,
          "MSAA replacement did not reach the generated LVUI field and its model");SysFreeString(value);
    nodes=take(tree->accessibility_snapshots());auto first=std::ranges::find_if(nodes,[](const auto& node){return node.id.ends_with("/field");});
    check(first!=nodes.end(),"Generated edited field is missing");auto field=take(tree->find(first->id));good(field.focus());
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::undo}));
    check(scope->backend_0.text.get()=="Draft","Accessibility replacement did not enter the LVUI undo history");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::redo}));
    check(scope->backend_0.text.get()=="Accessible","Accessibility replacement did not enter the LVUI redo history");
    peer->Release();root->Release();bridge.reset();good(host.clear_content());tree->close();
}
}
int main(){generated_button_lifetime();generated_input_replacement();}
