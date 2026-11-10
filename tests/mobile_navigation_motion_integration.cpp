#include <LVUI/MobileNavigationMotion.h>
#include <iiGUI/MobileNavigationMotion.h>
namespace lvui {using MobileNavigationMotion=MobileNavigationMotionBinding<ii::gui::BasicMobileNavigationMotion>;}
#include <chrono>
#include <cmath>
#include <stdexcept>
using namespace std::chrono_literals;
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
int main(){
    lvui::MobileNavigationMotion motion;const auto t=lvui::MobileNavigationMotion::TimePoint{};
    check(motion.reset(0,56),"Initial indicator rejected");
    check(motion.retarget(2,56,true,360ms,t)&&motion.active(),"Selection did not start motion");
    check(motion.advance(t+140ms),"Indicator stopped before reaching its destination");auto stretched=motion.geometry();
    check(stretched.left>0&&stretched.right>56&&stretched.right-stretched.left>56&&stretched.height<48,
          "Indicator did not stretch toward the new tab");
    check(!motion.advance(t+360ms)&&!motion.active(),"Indicator did not stop on the final frame");
    auto end=motion.geometry();check(std::abs(end.left-112)<.001&&std::abs(end.right-168)<.001&&end.height==48,
          "Indicator ended at the wrong tab");
    check(motion.retarget(0,56,false,360ms,t+361ms)&&!motion.active()&&motion.geometry().left==0,
          "Reduced motion did not settle immediately");
    auto before=motion.geometry();check(!motion.retarget(0,-1,true,360ms,t)&&motion.geometry()==before,
          "Invalid geometry changed the indicator");
}
