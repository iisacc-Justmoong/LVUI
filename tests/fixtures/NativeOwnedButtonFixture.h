#pragma once
#include <LVUI/Button.h>
#include <LVUI/NativeButtonMethods.h>
#include <iiGUI/NativeWidgets.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <memory>
#include <string>
#include <stdexcept>
#include <vector>

struct NativeOwnedButtonModel final {
    struct InvokeTarget final {std::vector<std::string>* events;void invoke(const lvui::native::ButtonEventValue<ii::gui::Widget>&){events->emplace_back("object invoke");}};
    struct TriggerTarget final {std::vector<std::string>* events;void trigger(){events->emplace_back("object trigger");}};
    ii::core::Observable<std::string> text{std::string("Save")},name{std::string("Save document")};
    ii::core::Observable<bool> enabled{true},motion_enabled{true},enter_enabled{true};
    ii::core::Observable<double> height{22},padding{8};
    ii::core::Observable<lvui::ButtonTone> tone{lvui::ButtonTone::primary};
    ii::core::Observable<lvui::ButtonSelectionStyle> selection_style{lvui::ButtonSelectionStyle::segmented};
    ii::core::Observable<ii::gui::ButtonTone> native_tone{ii::gui::ButtonTone::normal};
    ii::core::Observable<bool> selected{false};
    ii::core::Observable<ii::gui::WidgetColor> selected_color{{.2,.4,.6,1}};
    ii::core::Observable<ii::gui::WidgetColor> danger_hover{{.1,.6,.3,1}}, danger_pressed{{.2,.4,.8,1}};
    ii::core::Observable<std::shared_ptr<const ii::gui::RasterImage>> icon{};
    int saves=0,tools=0;
    std::vector<std::string> events;
    InvokeTarget invoke_target{&events};
    TriggerTarget trigger_target{&events};
    std::vector<lvui::native::ButtonMethodValue<lvui::native::ButtonEventValue<ii::gui::Widget>>> methods;
    std::vector<std::vector<lvui::native::ButtonMethodValue<lvui::native::ButtonEventValue<ii::gui::Widget>>>> nested_methods;
    void save(){++saves;events.emplace_back("clicked");}
    void tool(){++tools;}
    void press(){events.emplace_back("pressed");}
    void release(){events.emplace_back("released");}
    void cancel(){events.emplace_back("canceled");}
    void method(const lvui::native::ButtonEventValue<ii::gui::Widget>& event){
        if(!event.source.snapshot()||event.trigger!="clicked"||!event.enabled||!event.effective_enabled||event.tone!=tone.get())
            throw std::runtime_error("Invalid button method event");
        events.emplace_back("method");
    }
    void after(){events.emplace_back("after");}
    void last(const lvui::native::ButtonEventValue<ii::gui::Widget>&){events.emplace_back("last");}
};
