#pragma once

#include <LVUI/StyleTokens.h>
#include <LVUI/Text.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lvui {

enum class CardType { file, file_preview, folder, project, device, model, member, link };
enum class CardSize { small, medium, large };
enum class CardDetail { brief, detailed };
enum class CardDisplayState { automatic, normal, hover, selected };
struct CardRow final {
    std::string label, value;
    friend bool operator==(const CardRow&, const CardRow&) = default;
};
struct CardPalette final {
    StyleColor surface{figma_defaults::panel_surface_solid};
    StyleColor hover{figma_defaults::panel_surface_alt};
    StyleColor selected{figma_defaults::panel_sub_surface};
    StyleColor border{1,1,1,.12}, selected_border{figma_defaults::accent_blue_default};
    StyleColor text{1,1,1,.9}, secondary{1,1,1,.6}, disabled_text{1,1,1,.3};
    StyleColor progress{figma_defaults::accent_blue_default}, progress_track{1,1,1,.12};
    StyleColor status{.24,.77,.45,1}, focus{figma_defaults::accent_blue_default};
    friend bool operator==(const CardPalette&, const CardPalette&) = default;
};
struct CardConfiguration final {
    CardType type = CardType::file;
    CardSize size = CardSize::medium;
    CardDetail detail = CardDetail::brief;
    CardDisplayState display_state = CardDisplayState::automatic;
    std::string title, description, details, metadata, status_text, summary, footnote, domain, preview_title;
    std::string progress_text, menu_accessibility_name = "More actions", icon_name;
    std::optional<std::string> action_text, progress_label;
    std::vector<CardRow> rows;
    std::optional<bool> selectable, show_menu, show_action;
    bool selected = false, enabled = true, visible = true, show_description = true, show_status = true;
    double width = 0, height = 0, progress = 0, padding = 18;
    // Unset follows the card type; explicit values override the default.
    std::optional<double> corner_radius;
    CardPalette palette;
    friend bool operator==(const CardConfiguration&, const CardConfiguration&) = default;
};
struct CardExtent final { double width = 0, height = 0; };
struct CardRect final { double x = 0, y = 0, width = 0, height = 0; };
struct CardRowGeometry final { std::size_t index = 0; CardRect label, value; };
struct CardGeometry final {
    CardRect surface, preview, icon, title, description, details, metadata, status, summary, footnote, domain;
    CardRect progress_label, progress_text, progress_track, progress_fill, action, menu;
    std::vector<CardRowGeometry> rows;
};
struct CardInteraction final { bool hovered = false, pressed = false, focused = false; };
struct CardVisual final {
    bool selected = false, hovered = false, enabled = true, focused = false;
    StyleColor surface, border, text, secondary, focus;
};

struct CardModelPolicy final {
    using Configuration=CardConfiguration;using Type=CardType;using Size=CardSize;using Detail=CardDetail;using DisplayState=CardDisplayState;
    using Extent=CardExtent;using Rect=CardRect;using Geometry=CardGeometry;using Interaction=CardInteraction;using Visual=CardVisual;using Color=StyleColor;
    [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return valid_accessibility_text(value);}
    [[nodiscard]] static std::string_view action_label(CardType type)noexcept{switch(type){case CardType::folder:return "Open";case CardType::project:return "View";
        case CardType::device:return "Manage";case CardType::model:return "Use";case CardType::member:return "Profile";case CardType::link:return "Visit";default:return {};}}
    [[nodiscard]] static std::string_view progress_label(CardType type)noexcept{return type==CardType::device?"Storage":"Milestones";}
};
} // namespace lvui
