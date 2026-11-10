#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <iiGUI/NativeWidgets.h>
#include <LVUI/Tooltip.h>
#include <LVUI/Material.h>
#include <memory>
#include <string>

struct NativeOwnedTooltipModel final {
    ii::core::Observable<std::string> text{std::string("Help")};
    ii::core::Observable<std::string> name{std::string("Tooltip help")};
    ii::core::Observable<double> width{300},padding{10},delay{0},timeout{0},opacity{.5},anchorX{8},animation_duration{400};
    ii::core::Observable<bool> automatic{true},scroll{true},motion_enabled{true};
    ii::core::Observable<std::shared_ptr<ii::gui::MotionPreferences>> motion{std::shared_ptr<ii::gui::MotionPreferences>{}};
    ii::core::Observable<lvui::TooltipPlacement> placement{lvui::TooltipPlacement::below};
    ii::core::Observable<lvui::MaterialBackdrop> backdrop{lvui::MaterialBackdrop::content};
    ii::core::Observable<ii::gui::WidgetColor> surface{ii::gui::WidgetColor{.1,.2,.3,1}};
    ii::core::Observable<ii::gui::Widget> direct_target{ii::gui::Widget{}};
    ii::core::Observable<ii::gui::Rect> bounds{ii::gui::Rect{0,0,640,420}};
    ii::gui::Widget next_target;
    int opened_count=0,closed_count=0;
    bool adjust_padding_on_open=false,schedule_timeout_on_open=false,switch_target_on_open=false;
    void opened(){++opened_count;if(adjust_padding_on_open){adjust_padding_on_open=false;padding.set(16);}
        if(schedule_timeout_on_open){schedule_timeout_on_open=false;timeout.set(60000);}
        if(switch_target_on_open){switch_target_on_open=false;direct_target.set(next_target);}}
    void closed(){++closed_count;}
};
