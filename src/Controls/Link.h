#pragma once
#include <LVUI/Button.h>
#include <LVUI/Navigation.h>
#include <algorithm>
#include <cmath>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace lvui {
struct LinkEvent final {NavigationRequest request;};
using LinkMethod=std::function<void(const LinkEvent&)>;
struct LinkPalette final {
    StyleColor link{figma_defaults::accent_blue_default};
    StyleColor hover{1,1,1,.9};
    std::optional<StyleColor> pressed;
    StyleColor disabled{1,1,1,.6};
    StyleColor focus{figma_defaults::accent_blue_default};
};
struct LinkConfiguration final {
    std::string text,accessibility_name,href="/";
    ii::core::Value params{ii::core::Value::Object{}};
    std::weak_ptr<NavigationTarget> router,navigator;
    std::shared_ptr<const NavigationPage> target;
    bool replace=false,enabled=true,underline=false,show_focus_ring=true;
    ButtonTone tone=ButtonTone::borderless;
    double horizontal_padding=14,vertical_padding=10,minimum_height=36,corner_radius=8;
    std::optional<double> width,height;
    double font_size=12,line_height=12;
    unsigned font_weight=600;
    LinkPalette palette;
};
struct LinkInteraction final {bool enabled=true,hovered=false,pressed=false,focused=false;};
struct LinkVisual final {bool enabled=false,focused=false,underline=false;StyleColor text,focus;};
struct LinkRect final {double x=0,y=0,width=0,height=0;};
struct LinkGeometry final {LinkRect text;double radius=0;bool draw_text=false;};

struct LinkModelPolicy final {
    using Configuration=LinkConfiguration;using Visual=LinkVisual;using Interaction=LinkInteraction;
    using Geometry=LinkGeometry;using Tone=ButtonTone;using Color=StyleColor;
    using Target=NavigationTarget;using Request=NavigationRequest;
    [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return valid_accessibility_text(value);}
    [[nodiscard]] static bool valid_params(const ii::core::Value& value)noexcept{return valid_navigation_parameters(value);}
};
} // namespace lvui
