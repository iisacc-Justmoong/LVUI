#pragma once
#include <LVUI/Avatar.h>
#include <iiGUI/Image.h>
#include <iiGUI/Appearance.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <memory>
#include <string>

struct NativeOwnedAvatarModel final {
    ii::core::Observable<lvui::AvatarSize> size{lvui::AvatarSize::medium};
    ii::core::Observable<std::shared_ptr<const ii::gui::RasterImage>> image{};
    ii::core::Observable<double> width{0},height{0};
    ii::core::Observable<bool> enabled{true};
    ii::core::Observable<std::string> name{std::string("Profile")};
    ii::core::Observable<ii::gui::WidgetColor> background{ii::gui::WidgetColor{.5,.5,.5,1}};
};
