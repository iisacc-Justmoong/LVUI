#include <LVUI/NativeCheckBox.h>
#include <iiGUI/CheckBoxModel.h>
using CheckBoxModel=ii::gui::BasicCheckBox<lvui::native::CheckBoxModelPolicy>;
#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool near(double a,double b){return std::abs(a-b)<1e-9;}
void motion(){
    using namespace std::chrono_literals;using namespace lvui;
    CheckBoxModel box;CheckBoxConfiguration c;c.use_figma_checked_assets=false;
    c.palette.unchecked={0,0,0,1};c.palette.checked={1,.5,.25,1};
    const auto t=CheckBoxModel::TimePoint{}+1s;
    require(c.motion.duration==360ms&&c.motion.curve==MotionCurve::out_back&&near(c.motion.overshoot,1.45),
            "Checkbox default mark transition differs from its design");
    require(box.set_configuration(c,t)&&!box.animating()&&near(box.mark_scale(),.55),"Initial checkbox mark was not settled");
    auto selected=box.activate(t);
    require(selected&&*selected&&box.checked()&&box.visual({}).draw_mark&&box.animating()&&near(box.mark_scale(),.55)&&
            box.visual({}).fill==c.palette.unchecked,"Selection did not start from the displayed state");
    require(box.advance(t+65ms)&&near(box.visual({}).fill.red,.875)&&near(box.visual({}).fill.green,.4375),
            "Checkbox fill did not follow its 130 ms OutCubic transition");
    require(box.advance(t+180ms)&&near(box.mark_scale(),1.0253125)&&box.visual({}).fill==c.palette.checked,
            "Checkbox mark did not follow the 360 ms OutBack transition");
    require(!box.advance(t+360ms)&&near(box.mark_scale(),1),"Checkbox mark did not settle on time");
    auto cleared=box.activate(t+360ms);
    require(cleared&&!*cleared&&!box.visual({}).draw_mark&&box.animating(),"Unchecked checkbox did not hide its mark immediately");
    require(box.advance(t+450ms),"Unchecked mark transition ended too early");
    const auto interrupted=box.mark_scale();auto reselected=box.activate(t+450ms);
    require(reselected&&*reselected&&near(box.mark_scale(),interrupted)&&box.visual({}).draw_mark,
            "Reselecting did not preserve the in-flight mark scale");
    auto invalid=c;invalid.motion.duration=-1ms;const auto previous=box.configuration();
    require(!box.set_configuration(invalid,t+450ms)&&box.configuration()==previous&&box.animating(),
            "Invalid motion changed the committed checkbox");

    CheckBoxModel hover;CheckBoxConfiguration colors;colors.use_figma_checked_assets=false;colors.motion.duration=0ms;
    colors.palette.unchecked={.8,.6,.4,.8};require(hover.set_configuration(colors,t),"Color transition setup failed");
    CheckBoxInteraction over{.hovered=true};require(hover.set_interaction(over,t)&&hover.animating()&&hover.visual(over).fill==colors.palette.unchecked,
            "Hover did not begin at the current color");
    require(hover.advance(t+65ms),"Hover color ended early");const auto halfway=hover.visual(over).fill;
    require(near(halfway.red,.8+(.8/1.06-.8)*.875)&&near(halfway.alpha,.8),"Hover color interpolation was wrong");
    CheckBoxInteraction down{.hovered=true,.pressed=true};
    require(hover.set_interaction(down,t+65ms)&&hover.visual(down).fill==halfway,"Pressed retarget jumped from the current color");
    require(hover.advance(t+130ms)&&near(hover.visual(down).fill.red,halfway.red+(.8/1.18-halfway.red)*.875),
            "Pressed color did not retarget from the visible value");
    colors.enabled=false;require(hover.set_configuration(colors,t+130ms)&&!hover.animating()&&
        hover.visual(down).fill==colors.palette.disabled_unchecked,"Disabling did not settle color motion");
    colors.enabled=true;colors.motion.reduced_motion=true;
    require(hover.set_configuration(colors,t+130ms)&&hover.set_interaction({},t+130ms)&&!hover.animating()&&
        hover.visual({}).fill==colors.palette.unchecked,"Reduced motion did not immediately settle the color");
    colors.motion.reduced_motion=false;colors.motion_enabled=false;
    require(hover.set_configuration(colors,t+130ms)&&hover.activate(t+130ms)&&!hover.animating()&&near(hover.mark_scale(),1),
            "Disabled motion did not settle checkbox selection");
}
void stock_motion(){
    using namespace std::chrono_literals;CheckBoxModel box;lvui::CheckBoxConfiguration c;
    c.palette.unchecked={0,0,0,1};const auto t=CheckBoxModel::TimePoint{}+1s;
    require(box.set_configuration(c,t)&&box.activate(t)&&box.visual({}).figma_mark&&
            box.visual({}).fill==c.palette.unchecked&&near(box.mark_scale(),.55),
            "Stock asset did not preserve the animated underlying color");
    require(box.advance(t+130ms)&&box.visual({}).fill==c.palette.checked&&box.animating(),
            "Stock asset did not finish its underlying color before the mark motion");
}
}
int main(){
    CheckBoxModel box;
    lvui::CheckBoxConfiguration value;value.text="Label";value.motion_enabled=false;
    require(box.set_configuration(value)&&!box.checked(),"Initial checkbox configuration failed");
    auto geometry=box.geometry(57,18,33,13);
    require(geometry&&near(geometry->box.x,.5)&&near(geometry->box.y,.5)&&
            near(geometry->box.width,17)&&near(geometry->text.x,23.5)&&
            near(geometry->text.y,2.5)&&near(geometry->radius,3.5),"Checkbox geometry differs from its design");
    require(box.visual({}).fill==value.palette.unchecked && !box.visual({}).draw_mark && box.visual({}).draw_inner_shadow,
            "Unchecked appearance is incorrect");
    auto changed=box.activate();
    require(changed&&*changed&&box.checked()&&box.visual({}).draw_mark&&box.visual({}).figma_mark&&
            box.visual({}).fill==value.palette.checked&&
            !box.visual({}).draw_inner_shadow,"Default checked asset appearance is incorrect");
    require(box.visual({.pressed=true}).fill.red<box.visual({.hovered=true}).fill.red,
            "The underlying checked surface ignored pressed state");
    value.checked=true;value.checked_border_width=2;value.palette.checked_border={1,0,0,1};
    require(box.set_configuration(value)&&near(box.visual({}).border_width,2),"The underlying checked border was discarded");
    value.checked=true;value.use_figma_checked_assets=false;require(box.set_configuration(value),"Custom mark override was rejected");
    require(!box.visual({}).figma_mark&&near(box.visual({}).border_width,2)&&box.visual({.pressed=true}).fill.red<box.visual({.hovered=true}).fill.red,
            "Custom checked appearance did not restore state colors");
    value.use_figma_checked_assets=true;value.shape_style=lvui::CheckBoxShape::cylinder;
    require(box.set_configuration(value)&&!box.visual({}).figma_mark&&near(box.geometry(57,18,33,13)->radius,8.5),
            "Cylindrical checkbox did not use a round indicator and custom mark");
    auto invalid_shape=value;invalid_shape.shape_style=static_cast<lvui::CheckBoxShape>(42);
    require(!box.set_configuration(invalid_shape)&&box.configuration()==value,"Invalid checkbox shape replaced the current state");
    value.shape_style=lvui::CheckBoxShape::round_rect;
    require(box.mark_coverage(.44,.67)>0&&box.mark_coverage(.05,.05)==0,
            "The independent check mark geometry is incorrect");
    value.use_figma_checked_assets=std::nullopt;value.checked=true;value.enabled=false;
    require(box.set_configuration(value)&&!box.activate()&&box.checked()&&
            box.visual({}).fill==value.palette.disabled_checked&&
            box.visual({}).figma_mark&&box.visual({}).draw_inner_shadow,
            "Disabled checkbox admitted activation or lost its checked asset and shadow");
    auto before=box.configuration(),invalid=value;invalid.box_size=0;
    require(!box.set_configuration(invalid)&&box.configuration()==before,"Invalid size changed the checkbox");
    invalid=value;invalid.mark_stroke_width=std::numeric_limits<double>::infinity();
    require(!box.set_configuration(invalid),"Invalid mark width was accepted");
    invalid=value;invalid.palette.checked.red=2;
    require(!box.set_configuration(invalid),"Invalid color was accepted");
    invalid=value;invalid.text=std::string("x\0y",3);
    require(!box.set_configuration(invalid),"Invalid text was accepted");
    require(!box.geometry(-1,18,3,13),"Invalid allocated extent was accepted");
    motion();
    stock_motion();
}
