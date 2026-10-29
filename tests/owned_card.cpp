#include <LVUI/Card.h>

#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
bool inside(lvui::CardRect rect, double width, double height) {
    return rect.x >= 0 && rect.y >= 0 && rect.width >= 0 && rect.height >= 0 &&
        rect.x + rect.width <= width + 1e-8 && rect.y + rect.height <= height + 1e-8;
}
}
int main() {
    using namespace lvui;
    Card card; auto c = card.configuration();
    require(card.intrinsic_size().width == 210 && card.intrinsic_size().height == 240, "Default file card metrics changed");
    c.size = CardSize::small; require(card.set_configuration(c) && card.intrinsic_size().width == 140 && card.intrinsic_size().height == 160,
                                     "Small file card metrics changed");
    c.size = CardSize::large; require(card.set_configuration(c) && card.intrinsic_size().width == 360 && card.intrinsic_size().height == 280,
                                     "Large file card metrics changed");
    c.type = CardType::file_preview; require(card.set_configuration(c) && card.intrinsic_size().width == 480 && card.intrinsic_size().height == 320 &&
                                            !card.can_select() && !card.shows_menu() && !card.shows_action(), "File preview defaults changed");
    c.preview_title = "Preview"; c.show_menu = true; c.show_action = true;
    require(card.set_configuration(c), "Custom preview actions were rejected");
    const auto preview = card.geometry(480,320);
    require(preview && preview->preview.width == 480 && preview->title.width > 0 && preview->menu.width > 0 && preview->action.width > 0,
            "Preview title or explicit actions were omitted");
    c.show_menu.reset(); c.show_action.reset(); c.preview_title.clear();
    c.type = CardType::project; c.title = "My project"; c.description = "Description"; c.status_text = "Ready";
    c.progress_label = "Milestones"; c.progress_text = "50%"; c.progress = .5;
    c.rows = {{"Owner", "A"}, {"Format", "B"}};
    require(card.set_configuration(c) && card.intrinsic_size().width == 256 && card.intrinsic_size().height >= 280 &&
            card.can_select() && card.shows_menu() && card.shows_action(), "Project card defaults changed");
    auto geometry = card.geometry(256, 280);
    require(geometry && geometry->title.width > 0 && geometry->progress_track.width > 0 &&
            std::abs(geometry->progress_fill.width * 2 - geometry->progress_track.width) < 1e-8 &&
            geometry->menu.width > 0 && geometry->action.width > 0, "Project card omitted its content or progress");
    require(card.visual({.hovered=true}).hovered && !card.visual({.hovered=true}).selected, "Automatic card hover failed");
    require(card.toggle_selection() == true && card.visual({}).selected && card.toggle_selection() == false, "Card selection did not toggle");
    c = card.configuration(); c.display_state = CardDisplayState::normal; c.selected = true;
    require(card.set_configuration(c) && !card.visual({.hovered=true}).selected && !card.visual({.hovered=true}).hovered,
            "Explicit card display state was ignored");
    c.display_state = CardDisplayState::selected; c.enabled = false;
    require(card.set_configuration(c) && card.visual({}).selected && !card.toggle_selection(), "Disabled card accepted selection");
    c.enabled = true; c.display_state = CardDisplayState::automatic; c.selectable = false;
    require(card.set_configuration(c) && !card.toggle_selection(), "Nonselectable card accepted selection");
    for (int kind = 0; kind != 8; ++kind) {
        c.type = static_cast<CardType>(kind); c.selectable.reset(); require(card.set_configuration(c), "Known card type was rejected");
        for (int width = 0; width <= 40; width += 2) for (int height = 0; height <= 40; height += 2) {
            auto g = card.geometry(width, height);
            require(g && inside(g->surface,width,height) && inside(g->preview,width,height) && inside(g->icon,width,height) &&
                    inside(g->title,width,height) && inside(g->description,width,height) && inside(g->status,width,height) &&
                    inside(g->progress_track,width,height) && inside(g->progress_fill,width,height) &&
                    inside(g->action,width,height) && inside(g->menu,width,height), "Card geometry escaped a small host extent");
            for (const auto& row : g->rows) require(inside(row.label,width,height) && inside(row.value,width,height), "Card row escaped its host extent");
        }
    }
    const auto previous = card.configuration(); c = previous; c.progress = std::numeric_limits<double>::quiet_NaN();
    require(!card.set_configuration(c) && card.configuration() == previous, "Invalid progress replaced committed card options");
    c = previous; c.rows.resize(129); require(!card.set_configuration(c), "Unbounded card rows were accepted");
    c = previous; c.type = static_cast<CardType>(99); require(!card.set_configuration(c), "Unknown card type was accepted");
    require(!card.geometry(-1, 20) && !card.geometry(20, std::numeric_limits<double>::infinity()), "Invalid card extent was accepted");
}
