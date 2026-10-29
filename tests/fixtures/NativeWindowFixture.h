#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <iiGUI/Appearance.h>
#include <string>
struct NativeWindowModel final {
    static inline int live=0;
    NativeWindowModel(){++live;}~NativeWindowModel(){--live;}
    ii::core::Observable<std::string> title{std::string("Review")},font{std::string("Pretendard")},other{std::string("Ready")};
    ii::core::Observable<double> width{720},height{500},minimumWidth{320},minimumHeight{220};
    ii::core::Observable<ii::gui::WidgetColor> color{ii::gui::WidgetColor{.1,.2,.3,1}};
    int title_reads=0;
    std::string caption(std::string value){++title_reads;return value;}
    unsigned file_revision=0;
};
