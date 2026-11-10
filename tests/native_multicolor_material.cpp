#include <LVUI/NativeMaterialGradient.h>
#include <iiGUI/MaterialGradient.h>
#include <iiGUI/MaterialModel.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <utility>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template<class T> T take(ii::core::Result<T> value) {
    if (!value) throw std::runtime_error(value.error().message);
    return std::move(*value);
}
std::array<int,4> pixel(const ii::gui::RasterImage& image,unsigned x,unsigned y) {
    const auto rgba=image.rgba();const auto at=(std::size_t(y)*image.width()+x)*4;
    return {rgba[at],rgba[at+1],rgba[at+2],rgba[at+3]};
}
bool close(std::array<int,4> actual,std::array<int,4> expected,int tolerance) {
    for(std::size_t i=0;i<4;++i)if(std::abs(actual[i]-expected[i])>tolerance)return false;
    return true;
}
}

int main() {
    // Independent samples from the renamed LVUI library's 380 x 204
    // Material/Color/defaultPrimary component, in its sRGB document profile.
    constexpr auto center=lvui::StyleColor{.1557759940624237,.5030806064605713,.8999999761581421,1};
    lvui::MaterialConfiguration options;options.primary_color=center;options.motion_enabled=false;
    using MaterialModel=ii::gui::BasicMaterial<lvui::MaterialModelPolicy>;
    MaterialModel material;require(material.set_configuration(options,{}),"Figma material configuration was rejected");
    lvui::native::MaterialGradientBinding<ii::gui::BasicMaterialGradient,MaterialModel> cache;
    auto image=take(cache.render_colored(material,380,204,1));
    require(image&&image->width()==380&&image->height()==204,"Colored material extent is incorrect");
    for(const auto& sample:std::array{
            std::pair{std::pair{61u,74u},std::array{30,130,243,169}},
            std::pair{std::pair{120u,70u},std::array{27,129,251,123}},
            std::pair{std::pair{190u,102u},std::array{20,125,235,51}},
            std::pair{std::pair{300u,150u},std::array{26,129,251,69}}}) {
        require(close(pixel(*image,sample.first.first,sample.first.second),sample.second,8),
                "Colored radial fields differ from the Figma sRGB samples");
    }
    require(take(cache.render_colored(material,380,204,1))==image,
            "An unchanged colored material did not reuse its raster");
    options.primary_color=lvui::StyleColor{.9,.1,.2,1};
    require(material.set_configuration(options,{}),"Changing the material accent was rejected");
    auto recolored=take(cache.render_colored(material,380,204,1));
    require(recolored&&recolored!=image&&pixel(*recolored,61,74)!=pixel(*image,61,74),
            "A changed accent did not update the colored radial fields");
    auto padded=take(cache.render_colored(material,380,204,1,16,8));
    require(padded&&padded->width()==412&&padded->height()==220&&pixel(*padded,0,110)[3]>0&&
            take(cache.render_colored(material,380,204,1,16,8))==padded,
            "The colored material lost its outside blur input or cache");
    auto high=take(cache.render_colored(material,380,204,2,16,8));
    require(high&&high->width()==824&&high->height()==440,
            "The colored material did not follow the backing scale");
    require(!cache.render_colored(material,380,204,1,-1,0),"Invalid material padding was accepted");
    options.primary_color=lvui::StyleColor{.9,.1,.2,0};
    require(material.set_configuration(options,{}),"A transparent material accent was rejected");
    require(!take(cache.render_colored(material,380,204,1)),
            "A transparent accent did not hide the radial fields");
    require(!cache.render_colored(material,380,204,0),"An invalid backing scale was accepted");
}
