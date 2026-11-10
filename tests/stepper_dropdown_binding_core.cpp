#include <LVUI/NativeStepper.h>
#include <LVUI/NativeDropdownButton.h>
#include <concepts>
#include <stdexcept>
struct Image{};struct Decoder{using Image=::Image;};struct Widget{};struct Color{double red,green,blue,alpha;};
enum class Tone{primary,normal,borderless,destructive,disabled};enum class Arrow{up_down,up,down};
template<class P>struct Model{using Policy=P;};template<class P>struct Host{using Policy=P;};
int main(){
    using Stepper=lvui::native::StepperBinding<Model,Model,Host,Widget>;
    using Dropdown=lvui::native::DropdownBinding<Model,Model,Host,Decoder,Widget>;
    using Button=Model<lvui::native::ButtonHostModelPolicy>;
    static_assert(std::same_as<Stepper::Policy::Model,Model<lvui::native::StepperModelPolicy<Button>>>);
    static_assert(std::same_as<Dropdown::Policy::Input,lvui::native::DropdownInputValue<Image>>);
    if(lvui::native::stepper_arrow(Arrow::down)!=lvui::StepperArrow::down||
       lvui::native::stepper_tone(Tone::destructive)!=lvui::ButtonTone::destructive||
       lvui::native::dropdown_color(Color{.1,.2,.3,.4})!=lvui::StyleColor{.1,.2,.3,.4})
        throw std::runtime_error("Stepper or dropdown binding changed");
}
