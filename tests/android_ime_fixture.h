#pragma once

#include "../../iiGUI/src/Platform/AndroidImeProtocol.h"
#include <iiGUI/WidgetState.h>

// Headless bridge fixture: the same owner-thread protocol and widget APIs used
// by the Android surface, without starting an application or platform IME.
struct AndroidImeFixture final {
    std::shared_ptr<ii::gui::detail::WidgetState> tree=
        std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    std::optional<ii::gui::WidgetSnapshot> focus(){
        auto snapshots=tree->accessibility_snapshots();if(!snapshots)return {};
        for(auto& item:*snapshots)
            if(item.focused&&item.text_input&&item.visible&&item.enabled&&!item.input_blocked)return item;
        return {};
    }
    ii::core::Result<ii::gui::TextContent> read(std::string_view id,ii::gui::TextRange range){
        auto widget=tree->find(id);if(!widget)return std::unexpected(widget.error());return widget->text_content(range);
    }
#if defined(IIGUI_TEXT_GEOMETRY_API)
    ii::core::Result<ii::gui::TextInputGeometry> geometry(std::string_view id,ii::gui::TextRange range){
        auto widget=tree->find(id);if(!widget)return std::unexpected(widget.error());return widget->text_geometry(range);
    }
    ii::core::Result<std::size_t> position(std::string_view id,double x,double y){
        auto widget=tree->find(id);if(!widget)return std::unexpected(widget.error());return widget->text_position(x,y);
    }
#endif
    ii::core::Result<void> send(ii::gui::WidgetInput input){return tree->input(input);}
};
