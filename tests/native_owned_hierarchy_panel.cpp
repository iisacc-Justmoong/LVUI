#include <LVUI/NativeHierarchyPanel.h>
#include <iiGUI/WidgetState.h>

#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
    return std::move(*value);
}
template<class T> T take(lvui::HierarchyResult<T> value) {
    if (!value) throw std::runtime_error(value.error().message); return std::move(*value);
}
void good(ii::core::Result<void> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
}
}
int main() {
    using namespace lvui; using namespace ii::gui;
    std::vector<HierarchyEntry> entries{{.key="root",.label="Root"},{.key="child",.parent="root",.label="Child"}};
    for (int i=0;i<100;++i) entries.push_back({.key=std::to_string(i),.label="Item " + std::to_string(i)});
    auto document=std::make_shared<HierarchyMemory>(entries); auto controller=std::make_shared<HierarchyController>(document);
    const ToolbarButtonId tool{std::string("home")}; native::HierarchyPanelInput input;
    input.appearance.width=240; input.appearance.height=160; input.appearance.footer_visible=true;
    input.list.controller=controller; input.toolbar.items={{.id=tool,.icon_name="home"}};
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{300,200}));
    std::optional<native::NativeHierarchyPanel> panel; int activated=0,tools=0,footer=0;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto column=builder.vstack({},"column",{.spacing=0,.alignment=StackAlignment::leading}); if(!column)return std::unexpected(column.error());
        auto value=native::NativeHierarchyPanel::create(builder,*column,"panel",input,
            {.item_activated=[&](const auto&){++activated;},.toolbar_triggered=[&](const auto&){++tools;},
             .footer_clicked=[&](std::size_t,const auto&){++footer;}});
        if(!value)return std::unexpected(value.error()); panel=*value; return {};
    }));
    (void)take(tree->drawing_snapshot({300,200}));
    auto geometry=take(panel->geometry()); require(geometry.toolbar.height==26 && geometry.list.height==108 && geometry.footer.height==26,
        "Composed hierarchy panel lost its regions");
    auto list=panel->list(); auto toolbar=panel->toolbar(); auto footer_view=panel->footer();
    auto keys=take(list.mounted_keys());
    if(keys.empty()){
        auto failure=take(panel->failure()); auto list_failure=take(list.failure()); auto snapshot=take(list.root().snapshot());
        throw std::runtime_error("Empty hierarchy panel list; panel="+(failure?failure->message:std::string("none"))+
            ", list="+(list_failure?list_failure->message:std::string("none"))+", size="+
            std::to_string(snapshot.size.width)+"x"+std::to_string(snapshot.size.height));
    }
    require(keys.size()<=7 && take(footer_view.root().interaction()).visible,
            "Hierarchy panel did not virtualize its list or show the footer");
    good(take(list.row("root")).row().activate()); good(take(toolbar.slot(tool)).activate()); good(take(footer_view.slot(0)).activate());
    require(activated==1 && tools==1 && footer==1 && take(document->snapshot())->revision==0,
            "Hierarchy panel lost child events or modified the source during selection");
    auto retained=take(list.row("root")).root(); auto next=input; next.appearance.width=200; next.appearance.height=120;
    next.appearance.footer_visible=false; good(panel->set_options(next)); geometry=take(panel->geometry());
    require(geometry.list.height==94 && !take(footer_view.root().interaction()).visible && retained.snapshot(),
            "Hierarchy panel resizing rebuilt its rows or left the footer visible");
    auto invalid=next; invalid.footer.configuration.slots[0].icon_name="missing-icon"; invalid.toolbar.appearance.slot_size=24;
    require(!panel->set_options(invalid) && take(panel->options())==next && retained.snapshot(),
            "Invalid hierarchy panel input replaced its committed state");
    bool wrong=false; std::thread worker([&]{auto result=panel->set_options(next);wrong=!result&&result.error().code==ii::core::ErrorCode::wrong_executor;});
    worker.join(); require(wrong,"Hierarchy panel accepted a foreign-thread update");
    good(host.clear_content()); require(!panel->set_options(next) && !retained.snapshot(),"Retired hierarchy panel accepted an update"); tree->close();
}
