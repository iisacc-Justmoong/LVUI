#include <LVUI/ColorSurface.h>
#include <iiGUI/ColorSurfaceModel.h>
#include <stdexcept>
int main(){ii::gui::BasicColorSurface<lvui::ColorSurfaceModelPolicy> m;auto c=m.configuration();
 if(m.position(-10,50,100,100)!=lvui::ColorSurfacePoint{0,.5})throw std::runtime_error("Pointer clipping changed");
 c.enabled=false;if(!m.set_configuration(c)||m.position(1,1,100,100))throw std::runtime_error("Disabled surface accepted input");
 c.width=0;if(m.set_configuration(c)||m.configuration().width!=192)throw std::runtime_error("Invalid configuration changed state");}
