#include <LVUI/NativeEditorTimeline.h>
#include <stdexcept>
struct Color {double red=0,green=0,blue=0,alpha=0;};
struct Palette {Color surface,divider,text,clip,selected,accent;};
int main(){auto p=lvui::native::editor_timeline_palette<Palette>();auto c=lvui::figma_defaults::panel_surface_solid;
 if(p.surface.red!=c.red||p.surface.green!=c.green||p.surface.blue!=c.blue||p.surface.alpha!=c.alpha)throw std::runtime_error("Timeline surface token changed");
 auto a=lvui::figma_defaults::accent_blue_default;if(p.accent.red!=a.red||p.accent.green!=a.green||p.accent.blue!=a.blue||p.accent.alpha!=a.alpha)throw std::runtime_error("Timeline accent token changed");}
