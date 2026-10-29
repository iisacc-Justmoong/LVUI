#include <LVUI/Image.h>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
bool near(double left, double right) { return std::abs(left - right) < 1e-9; }

void layout() {
    lvui::Image image;
    lvui::ImageConfiguration options;
    options.width = options.height = 200;
    options.fit = lvui::ImageFit::contain;
    require(image.set_configuration(options, lvui::ImageSize{400, 200}), "contain configuration failed");
    auto view = image.geometry(200, 200);
    require(view && view->draw_image && near(view->image.x, 0) && near(view->image.y, 50) &&
            near(view->image.width, 200) && near(view->image.height, 100) && near(view->source.width, 1),
            "contain geometry is incorrect");
    options.fit = lvui::ImageFit::cover;
    require(image.set_configuration(options, lvui::ImageSize{400, 200}), "cover configuration failed");
    view = image.geometry(200, 200);
    require(view && near(view->image.width, 200) && near(view->image.height, 200) &&
            near(view->source.x, .25) && near(view->source.width, .5) && near(view->source.height, 1),
            "cover did not crop the source centrally");
    options.fit = lvui::ImageFit::stretch;
    require(image.set_configuration(options, lvui::ImageSize{400, 200}), "stretch configuration failed");
    view = image.geometry(200, 200);
    require(view && near(view->image.height, 200) && near(view->source.width, 1), "stretch cropped the source");
    options.width = 200; options.height = 100; options.circular = true; options.padding = 10;
    options.border_width = 200; options.fit = lvui::ImageFit::cover;
    require(image.set_configuration(options, lvui::ImageSize{400, 200}), "circular configuration failed");
    view = image.geometry(200, 100);
    require(view && near(view->outer.x, 50) && near(view->outer.width, 100) &&
            near(view->image.x, 60) && near(view->image.width, 80) &&
            near(view->radius, 50) && near(view->image_radius, 40) && near(view->border_width, 50),
            "circular geometry, padding, or border clamp is incorrect");
    require(image.geometry(0, 100) && !image.geometry(0, 100)->draw_image, "zero allocation should draw no image");
    require(!image.geometry(-1, 100) && !image.geometry(std::numeric_limits<double>::infinity(), 1),
            "invalid allocated extent was accepted");
}

void validation() {
    lvui::Image image;
    lvui::ImageConfiguration options;
    options.accessibility_name = "미리보기 \xF0\x9F\x8E\xA8";
    require(image.set_configuration(options, std::nullopt), "valid empty image failed");
    const auto before = image.configuration();
    auto invalid = options; invalid.width = -1;
    require(!image.set_configuration(invalid, std::nullopt) && image.configuration() == before,
            "invalid update did not preserve the last configuration");
    invalid = options; invalid.padding = 17;
    require(!image.set_configuration(invalid, std::nullopt), "excessive padding was accepted");
    invalid = options; invalid.allow_empty = false;
    require(!image.set_configuration(invalid, std::nullopt), "required image was absent");
    require(!image.set_configuration(options, lvui::ImageSize{0, 1}), "zero source dimension was accepted");
    invalid = options; invalid.fit = static_cast<lvui::ImageFit>(99);
    require(!image.set_configuration(invalid, std::nullopt), "invalid fit was accepted");
    invalid = options; invalid.background = lvui::StyleColor{2, 0, 0, 1};
    require(!image.set_configuration(invalid, std::nullopt), "invalid color was accepted");
    for (const auto& name : {std::string("x\0y", 3), std::string("\xC0\x80", 2),
                             std::string("\xED\xA0\x80", 3), std::string("\xF4\x90\x80\x80", 4),
                             std::string("\xF0\x9F", 2), std::string(65537, 'a')}) {
        invalid = options; invalid.accessibility_name = name;
        require(!image.set_configuration(invalid, std::nullopt), "invalid accessibility text was accepted");
    }
}
}
int main() { layout(); validation(); }
