#include "table_host_test_binding.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <optional>
#include <stdexcept>

namespace {
using namespace ii;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}

// Simulate a platform which cannot construct a text editor. Other errors must
// retain their existing fail-stop behavior.
struct UnavailableField final {
    inline static core::ErrorCode code=core::ErrorCode::unsupported;
    static core::Result<UnavailableField> create(gui::WidgetBuilder&,gui::Widget,std::string_view,
            lvui::InputFieldConfiguration,lvui::native::NativeInputFieldCallbacks={},lvui::native::NativeInputFieldClipboard={}) {
        return core::fail(code,"Text editor unavailable");
    }
    core::Result<lvui::native::NativeInputFieldSnapshotValue<gui::NativeTextAffinity>> snapshot()const{return core::fail(code,"No editor");}
    core::Result<lvui::InputFieldConfiguration> options()const{return core::fail(code,"No editor");}
    core::Result<void> set_palette(lvui::InputFieldPalette)const{return core::fail(code,"No editor");}
    core::Result<void> set_accessibility_name(std::string)const{return core::fail(code,"No editor");}
    gui::Widget root()const{return {};}
};
using Table=lvui::native::TableBinding<gui::BasicNativeTable,UnavailableField,
    lvui::native::TableTestMenu,lvui::native::TableTestMenuItem,lvui::native::TableTestHeader,
    lvui::native::TableTestGroup,gui::WidgetBuilder,gui::Rect,gui::RasterImage,
    lvui::TableViewportBinding<gui::BasicTableViewportSource>>;
}

int main(){
    using namespace ii::gui;
    auto data=std::make_shared<lvui::TableMemory>(
        std::vector<lvui::TableColumn>{{.key="name",.title="Name",.width=120}},
        std::vector<lvui::TableRow>{{.key="a",.cells={{.text="First"}}}});
    lvui::native::TableInput options;options.controller=std::make_shared<lvui::TableController>(data);
    options.editor=data;options.editable=true;options.appearance.width=180;options.appearance.height=100;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{200,130}));std::optional<Table> table;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=Table::create(builder,{},"table",options);if(!value)return std::unexpected(value.error());
        table=*value;return {};
    }));
    (void)take(table->frame());
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=40,.y=34,.buttons=1}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=40,.y=34}));
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));
    auto failure=take(table->failure());require(failure&&failure->code==ii::core::ErrorCode::unsupported,"Unavailable editor was not reported");
    require(!table->editor()&&bool(table->frame()),"Unavailable editor damaged the table");
    auto direct=table->begin_edit();require(!direct&&direct.error().code==ii::core::ErrorCode::unsupported,"Direct edit lost the unsupported result");
    const auto accessible=take(tree->accessibility_snapshots());
    const auto cell=std::ranges::find_if(accessible,[](const auto& value){return value.text=="First";});
    require(cell!=accessible.end(),"Table cell accessibility was not published");
    good(tree->input({.kind=WidgetInputKind::activate,.target=cell->id}));
    failure=take(table->failure());require(failure&&failure->code==ii::core::ErrorCode::unsupported,"Accessible activation lost the unsupported result");
    UnavailableField::code=ii::core::ErrorCode::failed;
    auto failed=tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter});
    require(!failed&&failed.error().code==ii::core::ErrorCode::failed,"Unexpected editor failure was swallowed");
    tree->close();
}
