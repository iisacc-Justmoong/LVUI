#include <iiGUI/NativeWidgets.h>
#if defined(IIGUI_KEY_CANCEL_API)
#include "native_input_field_composition.h"
#include <iiGUI/WidgetState.h>
#include "Platform/AndroidKeyboardDispatch.h"
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
using namespace ii;
using namespace ii::gui;
using namespace ii::gui::detail;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
struct FakeSurface {
    std::shared_ptr<WidgetState> tree;
    std::optional<lvui::native::NativeInputField>* field;
    std::string clipboard;
    std::optional<WidgetSnapshot> focus(){
        std::optional<WidgetSnapshot> result;
        for(auto& value:take(tree->accessibility_snapshots()))if(value.focused&&value.visible&&value.enabled&&!value.input_blocked){
            if(value.text_input)return value;result=std::move(value);
        }
        return result;
    }
    bool send(WidgetInput event){good(tree->input(event));return true;}
    core::Result<TextContent> read(std::string_view id,TextRange range){auto widget=tree->find(id);if(!widget)return std::unexpected(widget.error());return widget->text_content(range);}
    core::Result<ClipboardContent> read_clipboard(){return ClipboardContent{.text=clipboard};}
    core::Result<void> write_clipboard(const ClipboardContent& content){clipboard=content.text;return {};}
};
AndroidKeyboardFrame key(char c){AndroidKeyboardFrame event;event.down=true;event.key=WidgetKey::character;event.character=c;event.shortcut=c;event.device=1;return event;}
std::uint32_t combine(std::uint32_t accent,std::uint32_t value){return accent=='\''&&value=='e'?0xe9:0;}
}
int main(){
    auto tree=std::make_shared<WidgetState>([](runtime::Task task){task();});
    auto root=take(tree->content_host({},"host",{300,100}));
    std::optional<lvui::native::NativeInputField> field;std::vector<std::string> edits;
    FakeSurface surface{tree,&field};
    good(root.set_content([&](WidgetBuilder& builder)->core::Result<void>{
        auto parent=take(builder.absolute_stack({},"root",{300,100}));
        auto result=lvui::native::NativeInputField::create(builder,parent,"field",{},
            {.edited=[&](std::string value){edits.push_back(std::move(value));}},
            {.write=[&](std::string value)->core::Result<void>{surface.clipboard=std::move(value);return {};}});
        if(!result)return std::unexpected(result.error());field=*result;return {};
    }));
    (void)take(tree->drawing_snapshot({300,100}));good(field->root().focus());
    AndroidKeyboardDispatcher keyboard;
    check(keyboard.input(key('a'),surface,combine),"Android character was not accepted");
    auto space=key(' ');space.key=WidgetKey::space;keyboard.input(space,surface,combine);
    auto accent=key('\'');accent.character=0x80000000u|'\'';keyboard.input(accent,surface,combine);
    keyboard.input(key('e'),surface,combine);
    check(take(field->snapshot()).text=="a é"&&edits.size()==3,"Android character or dead-key edit did not reach LVUI exactly once");
    auto undo=key('z');undo.control=true;keyboard.input(undo,surface,combine);
    check(take(field->snapshot()).text=="a ","Android Control-Z did not reach LVUI undo");
    auto redo=undo;redo.shift=true;keyboard.input(redo,surface,combine);
    check(take(field->snapshot()).text=="a é","Android Control-Shift-Z did not reach LVUI redo");
    auto select=key('a');select.control=true;keyboard.input(select,surface,combine);
    auto copy=key('c');copy.control=true;keyboard.input(copy,surface,combine);
    check(surface.clipboard=="a é","Android Copy lost the LVUI selection");
    surface.clipboard="한😀";auto paste=key('v');paste.control=true;keyboard.input(paste,surface,combine);
    check(take(field->snapshot()).text=="한😀","Android paste did not replace the LVUI selection");
    keyboard.input(undo,surface,combine);
    check(take(field->snapshot()).text=="a é","Android pasted edit was not undoable");
    tree->close();
}
#else
int main(){return 77;}
#endif
