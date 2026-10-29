#pragma once
#include <LVUI/ColorPickerButton.h>
#include <iiGUI/NativeWidgets.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <memory>
class NativeColorButtonSource final:public lvui::ColorSource {
    lvui::StyleColor value_{1,0,0,1};std::uint64_t revision_=1;
public:
    [[nodiscard]] lvui::ColorSourceResult color()const override{return value_;}
    [[nodiscard]] std::uint64_t revision()const noexcept override{return revision_;}
    void set(lvui::StyleColor color){value_=color;++revision_;}
};
struct NativeOwnedColorButtonModel final {
    std::shared_ptr<NativeColorButtonSource> owner=std::make_shared<NativeColorButtonSource>();
    ii::core::Observable<std::shared_ptr<const lvui::ColorSource>> source{owner};
    ii::core::Observable<std::int64_t> revision{1},size{28};
    ii::core::Observable<bool> ring{true};
    ii::core::Observable<ii::gui::WidgetColor> color{{0,0,1,1}};
    int clicks=0,presses=0,releases=0;
    void clicked(){++clicks;}
    void pressed(){++presses;}
    void released(){++releases;}
};
