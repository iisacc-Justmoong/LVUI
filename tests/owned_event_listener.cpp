#include <LVUI/EventListener.h>
#include <limits>
#include <stdexcept>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main(){
    lvui::EventListener listener;auto initial=listener.configuration();
    require(initial.width==640&&initial.height==480,"Event listener defaults changed");
    auto next=initial;next.width=320;next.height=180;require(listener.set_configuration(next),"Valid extent was rejected");
    auto bad=next;bad.width=0;require(!listener.set_configuration(bad)&&listener.configuration()==next,"Zero extent changed the listener");
    bad=next;bad.height=8193;require(!listener.set_configuration(bad),"Oversized listener was accepted");
    bad=next;bad.width=std::numeric_limits<double>::quiet_NaN();require(!listener.set_configuration(bad),"NaN listener extent was accepted");
}
