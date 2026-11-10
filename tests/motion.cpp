#include <LVUI/Motion.h>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>

using namespace std::chrono_literals;
namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
bool near(double a, double b) { return std::abs(a-b) < 1e-9; }
}

int main() {
    using Transition = lvui::MotionTransition<2>;
    const auto start = Transition::TimePoint{} + 10s;
    lvui::Motion linear{.duration=400ms, .curve=lvui::MotionCurve::linear};
    Transition transition({0, .2});
    require(bool(transition.retarget({1, .6}, linear, true, start)), "Linear motion rejected");
    require(transition.active() && near(transition.value()[0], 0), "Motion jumped at admission");
    require(transition.advance(start+100ms) && near(transition.value()[0], .25) && near(transition.value()[1], .3),
            "Motion did not interpolate at a frame");
    require(transition.advance(start+50ms) && near(transition.value()[0], .25), "Backward frame moved motion backwards");
    require(bool(transition.retarget({0, .2}, linear, true, start+100ms)), "Retarget failed");
    require(near(transition.value()[0], .25), "Retarget jumped from the displayed value");
    require(transition.advance(start+300ms) && near(transition.value()[0], .125), "Retarget used the old origin");
    require(!transition.advance(start+500ms) && near(transition.value()[0], 0), "Motion did not stop at the target");
    Transition exact({0, 0});
    linear.duration=320ms;
    const auto fractional_start=Transition::TimePoint{}+160ms;
    require(bool(exact.retarget({1, 0},linear,true,fractional_start))&&
            !exact.advance(fractional_start+320ms)&&near(exact.value()[0],1),
            "Exact duration did not settle after a fractional-second start");
    linear.duration=400ms;

    lvui::Motion out_cubic{.duration=400ms,.curve=lvui::MotionCurve::out_cubic};
    Transition cubic({0,0});require(bool(cubic.retarget({1,0},out_cubic,true,start)),"OutCubic rejected");
    require(cubic.advance(start+200ms)&&near(cubic.value()[0],.875),"OutCubic midpoint differs");
    require(!cubic.advance(start+400ms)&&near(cubic.value()[0],1),"OutCubic did not settle");

    lvui::Motion css_ease_out{.duration=400ms,.curve=lvui::MotionCurve::css_ease_out};
    Transition css({0,0});
    require(bool(css.retarget({1,0},css_ease_out,true,start)),"CSS ease-out rejected");
    require(css.advance(start+100ms)&&near(css.value()[0],.37813813082510966),"CSS ease-out quarter differs");
    require(css.advance(start+200ms)&&near(css.value()[0],.6846431874274606),"CSS ease-out midpoint differs");
    require(css.advance(start+300ms)&&near(css.value()[0],.9065353492811752),"CSS ease-out three-quarters differs");
    require(!css.advance(start+400ms)&&near(css.value()[0],1),"CSS ease-out did not settle");

    lvui::Motion out_back{.duration=400ms, .curve=lvui::MotionCurve::out_back, .overshoot=1.45};
    require(bool(transition.retarget({1, .2}, out_back, true, start+500ms)), "OutBack rejected");
    require(transition.advance(start+600ms) && near(transition.value()[0], .78203125), "OutBack curve differs");
    out_back.reduced_motion=true;
    require(bool(transition.retarget({1, .2}, out_back, true, start+600ms)) && !transition.active() &&
            near(transition.value()[0], 1), "Reduced motion did not settle immediately");

    const auto saved=transition.value();
    out_back.overshoot=std::numeric_limits<double>::quiet_NaN();
    require(!transition.retarget({0, 0}, out_back, true, start+700ms) && transition.value()==saved,
            "Invalid motion partially committed");
    out_back.overshoot=1.45;
    require(!transition.retarget({std::numeric_limits<double>::infinity(), 0}, out_back, true, start+700ms) &&
            transition.value()==saved, "Invalid target partially committed");
    require(!transition.reset({std::numeric_limits<double>::infinity(), 0}) && transition.value()==saved,
            "Invalid reset partially committed");
    out_back.reduced_motion=false;
    require(bool(transition.retarget({0, 0}, out_back, false, start+700ms)) && !transition.active() &&
            near(transition.value()[0], 0), "Disabled motion did not settle immediately");
    auto shared = std::make_shared<lvui::Motion>();
    shared->duration = 20ms;
    std::weak_ptr<lvui::Motion> weak = shared;
    require(lvui::motion_value(shared).duration == 20ms && lvui::motion_value(weak).duration == 20ms,
            "Motion handles did not supply their policy");
    shared.reset();
    require(lvui::motion_value(weak) == lvui::Motion{}, "Expired motion handle did not use the default");
}
