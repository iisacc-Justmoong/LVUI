#include <LVUI/NativeToggleSwitch.h>
#include <chrono>
#include <concepts>
#include <stdexcept>
namespace {
template<class P>struct Model{};
template<class P>struct Host{using Policy=P;};
struct Color{double red,green,blue,alpha;};
void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
}
int main(){
    using namespace std::chrono_literals;
    using Binding=lvui::native::ToggleSwitchBinding<Model,Host>;
    static_assert(std::same_as<Binding::Policy::Model,Model<lvui::native::ToggleSwitchModelPolicy>>);
    require(lvui::native::ToggleSwitchModelPolicy::valid_text("Toggle")&&!lvui::native::ToggleSwitchModelPolicy::valid_text(std::string_view{"x\0y",3}),"text contract");
    require(lvui::native::toggle_color(Color{.1,.2,.3,.4})==lvui::StyleColor{.1,.2,.3,.4},"generic color conversion");
    auto motion=lvui::native::toggle_motion_duration({},320);
    require(motion.duration==320ms&&lvui::native::toggle_motion_duration(motion,1.5).duration<0ms,"motion duration contract");
}
