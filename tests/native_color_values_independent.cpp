#include "../src/Style/NativeColors.h"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
struct ExternalColor {
    static constexpr bool encoded_srgb = true;
    double red, green, blue, alpha;
};
struct ByteColor {
    static constexpr bool encoded_srgb = true;
    unsigned char r, g, b, a;
};
struct UnmarkedColor { double r, g, b, a; };
static_assert(lvui::native::EncodedColor<ExternalColor>);
static_assert(!lvui::native::EncodedColor<ByteColor>);
static_assert(!lvui::native::EncodedColor<UnmarkedColor>);

void require(bool condition) {
    if (!condition) throw std::runtime_error("independent color contract failed");
}
}

int main() {
    using namespace lvui::native;
    using Color = EncodedSrgb;
    require(color<Color>(ExternalColor{.25, .5, .75, 1}) == Color{.25, .5, .75, 1});
    require(color<Color>("#123") == Color{17/255., 34/255., 51/255., 1});
    require(color<Color>("#1234") == Color{17/255., 34/255., 51/255., 68/255.});
    require(color<Color>("#112233") == Color{17/255., 34/255., 51/255., 1});
    require(color<Color>("11223344") == Color{17/255., 34/255., 51/255., 68/255.});
    require(color<Color>(" rgba(2.55e2, 0, 127.5, .25) ") == Color{1, 0, .5, .25});
    require(color<Color>("transparent") == Color{0, 0, 0, 0});
    require(composite_color<Color>(Color{1, 0, 0, .5}, Color{0, 0, 1, 1}) == Color{.5, 0, .5, 1});
    require(composite_color<Color>(Color{1, 0, 0, 0}, Color{0, 0, 1, 0}) == Color{0, 0, 0, 0});
    for (const char* text : {"#12345", "#zz0000", "rgba(+1,0,0,1)", "rgba(nan,0,0,1)",
                             "rgba(1,2,3,1,2)", "rgba(256,0,0,1)", "rgba(1e-999,0,0,1)"})
        require(!parse_color_text(text, true));
    require(!parse_color_text(std::string("rgba(1\0,0,0,1)", 14), true));
    bool rejected = false;
    try { (void)color<Color>(ExternalColor{0, 0, 0, std::numeric_limits<double>::infinity()}); }
    catch (const std::domain_error&) { rejected = true; }
    require(rejected);
}
