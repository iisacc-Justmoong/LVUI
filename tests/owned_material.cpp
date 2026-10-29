#include <LVUI/Material.h>
#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
void require(bool ok, const char* reason) { if (!ok) throw std::runtime_error(reason); }
bool near(double a, double b) { return std::abs(a-b)<1e-6; }
void defaults() {
    lvui::Material value; lvui::MaterialConfiguration config;
    config.kind=lvui::MaterialKind::panel;
    require(value.set_configuration(config, {}), "Panel material rejected");
    auto p=value.resolved();
    require(near(p.width,380)&&near(p.height,204)&&near(p.radius,12)&&near(p.blur_radius,16)&&
            near(p.tint.alpha,.25)&&near(p.border.alpha,.22)&&near(p.inner_highlight_opacity,.14), "Glass defaults changed");
    config.kind=lvui::MaterialKind::window;
    require(value.set_configuration(config, {}), "Window material rejected");
    auto w=value.resolved();
    require(near(w.width,500)&&near(w.height,272)&&near(w.radius,16)&&near(w.blur_radius,64)&&
            near(w.tint.alpha,.5)&&near(w.border.alpha,.12)&&near(w.shadow_radius,48)&&near(w.shadow_offset_y,16), "Window defaults changed");
    config.density=lvui::MaterialDensity::glass25; config.tint_opacity=.7;config.blur_enabled=false;
    require(value.set_configuration(config, {}), "Material overrides rejected");w=value.resolved();
    require(near(w.tint.alpha,.7)&&near(w.blur_radius,0)&&near(w.border.alpha,.22), "Density and overrides were not resolved independently");
}
void gradients_and_validation() {
    lvui::Material value;lvui::MaterialConfiguration config;config.kind=lvui::MaterialKind::panel;
    require(value.set_configuration(config, {}), "Initial material rejected");
    const auto a=value.gradient_alpha(48,32,380,204), b=value.gradient_alpha(190,102,380,204);
    require(a>0&&a<=1&&b>0&&b<=1&&!near(a,b), "Radial material lost its spatial variation");
    require(!near(value.gradient_alpha(330,100,380,204),value.gradient_alpha(330,100,760,204)), "Radial positions did not follow resizing");
    config.motion_enabled=false;config.primary_color=lvui::StyleColor{1,0,0,.5};
    require(value.set_configuration(config,{})&&value.gradient_alpha(48,32,380,204)>.5*a&&
            value.gradient_alpha(48,32,380,204)<a,"Accent alpha was not applied independently to overlapping gradients");
    config.intense_opacity=0;config.faint_opacity=0;require(value.set_configuration(config, {}), "Zero gradients rejected");
    require(near(value.gradient_alpha(48,32,380,204),0), "Zero gradients still contribute");
    auto before=value.configuration(), invalid=before;invalid.blur_radius=65;
    require(!value.set_configuration(invalid,{})&&value.configuration()==before,"Invalid blur changed material");
    invalid=before;invalid.tint_opacity=std::numeric_limits<double>::quiet_NaN();require(!value.set_configuration(invalid,{}),"NaN opacity accepted");
    invalid=before;invalid.density=static_cast<lvui::MaterialDensity>(90);require(!value.set_configuration(invalid,{}),"Invalid density accepted");
    invalid=before;invalid.width=-1;require(!value.set_configuration(invalid,{}),"Negative size accepted");
    for(auto path:{"M0 0L100 0L100 50L0 50Z","m2 3 10 0 0 10z","M.25-2.5e1L+3 4z","M0 0 C1 2 3 4 5 6 S7 8 9 10 Q11 12 13 14 T15 16 A5 6 30 0 1 20 30 Z"})
        require(lvui::valid_material_outline(path),"Valid SVG outline was rejected");
    for(auto path:{"L0 0","M0","M1e 2","M+-1 0","M0 0 L1","M0 0 A-5 6 30 0 1 20 30","M0 0 A5 6 30 2 1 20 30","M0 0 Z 1","M0 0\" onload=\"alert(1)","M0 0 X1 2"})
        require(!lvui::valid_material_outline(path),"Invalid or unsafe SVG outline was accepted");
    invalid=before;invalid.outline_path="M0";require(!value.set_configuration(invalid,{}),"Invalid outline changed material");
}
void motion() {
    using namespace std::chrono_literals;lvui::Material value;lvui::MaterialConfiguration config;
    config.kind=lvui::MaterialKind::panel;config.motion={.duration=200ms,.curve=lvui::MotionCurve::linear};
    config.primary_color=lvui::StyleColor{0,0,0,1};config.color=lvui::StyleColor{0,0,0,1};config.tint_opacity=1;
    const auto t=lvui::Material::TimePoint{};require(value.set_configuration(config,t),"Initial motion material rejected");
    config.primary_color=lvui::StyleColor{1,0,0,1};config.color=lvui::StyleColor{1,1,1,1};
    require(value.set_configuration(config,t)&&value.animating(),"Material color change did not animate");
    require(value.advance(t+100ms)&&near(value.displayed_primary().red,.5)&&near(value.displayed_tint().red,.5),"Material intermediate colors are wrong");
    require(!value.advance(t+200ms)&&near(value.displayed_primary().red,1),"Material motion did not settle");
    config.motion_enabled=false;config.primary_color=lvui::StyleColor{0,1,0,1};
    require(value.set_configuration(config,t+200ms)&&!value.animating()&&near(value.displayed_primary().green,1),"Disabled motion did not update immediately");
}
}
int main(){defaults();gradients_and_validation();motion();}
