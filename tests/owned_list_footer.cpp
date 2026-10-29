#include <LVUI/ListFooter.h>

#include <limits>
#include <stdexcept>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
}
int main() {
    using namespace lvui;
    ListFooterConfiguration value; auto natural = ListFooter::geometry(value);
    require(natural && natural->width == 88 && natural->height == 26 && natural->slots[0].width == 22 &&
            natural->slots[1].x == 24 && natural->slots[2].width == 40 && value.slots[2].menu_indicator,
            "List footer defaults or menu geometry changed");
    value.slots[1].visible = false; auto hidden = ListFooter::geometry(value);
    require(hidden && hidden->width == 66 && hidden->slots[1].width == 0 && hidden->slots[2].x == 24,
            "A hidden footer slot still occupied layout space");
    value.width = 20; value.height = 10; auto narrow = ListFooter::geometry(value);
    require(narrow && narrow->slots[0].width > 0 && narrow->slots[2].x + narrow->slots[2].width <= 18.0001 &&
            narrow->slots[0].height == 6, "Narrow footer slots escaped the available bounds");
    value = {}; value.slots[0].text = "Open"; auto labeled = ListFooter::geometry(value, {30, 0, 0});
    require(labeled && labeled->width == 122, "Footer layout did not include its measured label");
    value = {}; value.spacing = std::numeric_limits<double>::infinity(); require(!ListFooter::geometry(value), "Invalid footer spacing was accepted");
    value = {}; value.slots[0].icon_name = std::string("bad\xff", 4); require(!ListFooter::validate(value), "Invalid footer icon UTF-8 was accepted");
    value = {}; require(!ListFooter::geometry(value, {std::numeric_limits<double>::infinity(), 0, 0}), "Invalid footer text measurement was accepted");
}
