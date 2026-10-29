#pragma once
#include <iiGUI/Image.h>
#include <iiGUI/Appearance.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <memory>
#include <string>

struct NativeOwnedImageModel final {
    ii::core::Observable<std::shared_ptr<const ii::gui::RasterImage>> image{
        std::shared_ptr<const ii::gui::RasterImage>{}};
    ii::core::Observable<double> width{64}, height{48};
    int reads = 0;
    std::shared_ptr<const ii::gui::RasterImage> current_image() {
        ++reads;
        return image.get();
    }
};

struct NativeOwnedImageAdvancedModel final {
    ii::core::Observable<std::shared_ptr<const ii::gui::RasterImage>> image{};
    ii::core::Observable<double> width{200}, height{100}, padding{10}, radius{40}, border_width{2};
    ii::core::Observable<ii::gui::ImageFit> fit{ii::gui::ImageFit::cover};
    ii::core::Observable<bool> circular{true}, allow_empty{true};
    ii::core::Observable<std::string> name{std::string("Art preview")};
    ii::core::Observable<ii::gui::WidgetColor> background{ii::gui::WidgetColor{.5, .5, .5, 1}};
    ii::core::Observable<ii::gui::WidgetColor> border{ii::gui::WidgetColor{1, 0, 0, 1}};
};
