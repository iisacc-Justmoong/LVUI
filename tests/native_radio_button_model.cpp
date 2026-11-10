#include <LVUI/NativeRadioButton.h>
#include <iiGUI/RadioButtonModel.h>
using RadioButtonModel=ii::gui::BasicRadioButton<lvui::native::RadioButtonModelPolicy>;
#include <iiGUI/RadioGroup.h>
#include <chrono>
#include <cmath>
#include <limits>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool near(double a,double b){return std::abs(a-b)<1e-8;}
struct Member final:ii::gui::RadioGroupMember {
    std::string name;bool checked=false,active=false,displayed=false;int updates=0;
    std::function<void()> during;
    std::string_view group()const noexcept override{return name;}
    bool selected()const noexcept override{return checked;}
    bool participating()const noexcept override{return active;}
    void assign(bool value)noexcept override{checked=value;}
    void publish()noexcept override{if(displayed==checked)return;displayed=checked;++updates;if(during){auto callback=during;callback();}}
};
struct NavigationMember final:ii::gui::RadioGroupMember,ii::gui::RadioGroupNavigation {
    std::string name;bool active=true,checked=false;int visits=0;std::shared_ptr<const void> domain=std::make_shared<int>(1);
    std::string_view group()const noexcept override{return name;}
    bool selected()const noexcept override{return checked;}
    bool participating()const noexcept override{return active;}
    void assign(bool value)noexcept override{checked=value;}
    void publish()noexcept override{}
    std::optional<ii::gui::RadioNavigationPosition> navigation_position()const override{return ii::gui::RadioNavigationPosition{1,2,domain};}
    bool navigate_to(std::string_view expected)override{if(!active||name!=expected)return false;++visits;return true;}
};
void model(){
    using namespace std::chrono_literals;RadioButtonModel button;lvui::RadioButtonConfiguration value;
    value.text="Label";value.motion.duration=180ms;value.motion.curve=lvui::MotionCurve::linear;
    const auto start=RadioButtonModel::TimePoint{};
    require(button.set_configuration(value,start)&&!button.checked(),"Initial radio configuration failed");
    auto geometry=button.geometry(59,18,33,13);
    require(geometry&&near(geometry->indicator.width,18)&&near(geometry->text.x,26)&&
            near(geometry->text.y,2.5)&&near(geometry->indicator_radius,9),"Radio geometry is incorrect");
    auto changed=button.activate(start);require(changed&&*changed&&button.checked()&&button.animating(),"Radio did not select");
    require(button.visual({}).indicator==value.palette.off,"Radio indicator color jumped at selection");
    require(button.advance(start+65ms),"Radio color animation ended too early");
    const auto halfway_color=button.visual({}).indicator;
    require(near(halfway_color.red,value.palette.off.red+(value.palette.on.red-value.palette.off.red)*.875)&&
            near(halfway_color.green,value.palette.off.green+(value.palette.on.green-value.palette.off.green)*.875),
            "Radio indicator did not follow its 130 ms OutCubic color transition");
    require(button.advance(start+90ms)&&near(button.dot_scale(),.5),"Radio dot did not animate");
    require(button.advance(start+130ms)&&button.visual({}).indicator==value.palette.on,"Radio indicator did not settle independently of its dot");
    require(!button.advance(start+180ms)&&near(button.dot_scale(),1),"Radio motion did not settle at its deadline");
    geometry=button.geometry(59,18,33,13);
    require(geometry&&near(geometry->dot.x,5)&&near(geometry->dot.y,5)&&near(geometry->dot.width,8),"Radio dot geometry is incorrect");
    require(!button.activate(start+180ms)&&button.checked(),"Selected radio toggled off");
    value.checked=true;value.enabled=false;require(button.set_configuration(value,start+180ms)&&!button.activate(start+181ms),"Disabled radio activated");
    require(button.visual({}).indicator==value.palette.disabled_on&&button.visual({}).dot==value.palette.disabled_dot,"Disabled palette is incorrect");
    auto invalid=value,before=button.configuration();invalid.dot_size=19;
    require(!button.set_configuration(invalid,start)&&button.configuration()==before,"Invalid dot size changed state");
    invalid=value;invalid.palette.on.red=std::numeric_limits<double>::quiet_NaN();
    require(!button.set_configuration(invalid,start),"Invalid color was accepted");
    invalid=value;invalid.group=std::string("x\0y",3);
    require(!button.set_configuration(invalid,start),"Invalid group name was accepted");
    require(!button.geometry(-1,18,5,13),"Invalid geometry was accepted");
}
void default_dot_motion(){
    using namespace std::chrono_literals;RadioButtonModel button;lvui::RadioButtonConfiguration value;
    const auto start=RadioButtonModel::TimePoint{}+1s;
    require(value.motion.duration==360ms&&value.motion.curve==lvui::MotionCurve::out_back&&near(value.motion.overshoot,1.45),
            "Default radio dot motion does not match the LVRS release transition");
    require(button.set_configuration(value,start)&&button.set_checked(true,start),"Default radio selection failed");
    require(button.advance(start+180ms)&&near(button.dot_scale(),1.05625)&&button.visual({}).draw_dot,
            "Default radio dot did not follow the 360 ms OutBack curve");
    require(button.set_checked(false,start+180ms)&&button.dot_scale()>0&&!button.visual({}).draw_dot,
            "Deselection did not immediately hide the radio dot");
    require(button.advance(start+200ms)&&!button.visual({}).draw_dot,
            "An unchecked radio drew its still-animating dot");
    require(button.set_checked(true,start+200ms)&&button.visual({}).draw_dot,
            "Reselection did not restore the in-flight radio dot");
}
void indicator_motion(){
    using namespace std::chrono_literals;RadioButtonModel button;lvui::RadioButtonConfiguration value;
    const auto start=RadioButtonModel::TimePoint{}+1s;
    value.palette.off={0,0,0,0};value.palette.on={1,.5,.25,1};value.motion.duration=0ms;
    require(button.set_configuration(value,start)&&button.set_checked(true,start)&&button.animating()&&near(button.dot_scale(),1),
            "Indicator motion did not run independently from dot motion");
    require(button.advance(start+65ms),"Indicator transition stopped before its deadline");
    auto color=button.visual({}).indicator;
    require(near(color.red,.875)&&near(color.green,.4375)&&near(color.blue,.21875)&&near(color.alpha,.875),
            "Indicator did not interpolate all color channels");
    require(button.set_checked(false,start+65ms)&&button.visual({}).indicator==color,
            "Indicator reversal did not retain the displayed color");
    require(button.advance(start+130ms)&&near(button.visual({}).indicator.red,.109375),
            "Indicator reversal did not start at the displayed color");
    require(!button.advance(start+195ms)&&button.visual({}).indicator==value.palette.off,
            "Indicator reversal did not settle");
    value.palette.off={.2,.4,.6,.8};
    require(button.set_configuration(value,start+195ms)&&button.animating()&&button.visual({}).indicator.red==0,
            "Palette change did not retain the displayed indicator");
    require(button.advance(start+260ms)&&near(button.visual({}).indicator.alpha,.7),
            "Palette change did not animate alpha");
    value.enabled=false;
    require(button.set_configuration(value,start+260ms)&&!button.animating()&&
            button.visual({}).indicator==value.palette.disabled_off,
            "Disabling did not settle indicator motion immediately");
    value.enabled=true;value.motion.reduced_motion=true;
    require(button.set_configuration(value,start+260ms)&&!button.animating()&&
            button.visual({}).indicator==value.palette.off,"Reduced motion did not settle the indicator");
    value.motion.reduced_motion=false;value.motion_enabled=false;
    require(button.set_configuration(value,start+260ms)&&button.set_checked(true,start+260ms)&&!button.animating()&&
            button.visual({}).indicator==value.palette.on,"Disabled motion did not settle the indicator");
}
void grouping(){
    auto groups=std::make_shared<ii::gui::RadioGroups>();auto a=std::make_shared<Member>();auto b=std::make_shared<Member>();auto c=std::make_shared<Member>();
    a->name=b->name=c->name="choice";a->checked=true;
    require(groups->prepare(a)&&groups->prepare(b)&&groups->prepare(c),"Group preparation failed");
    a->active=b->active=true;require(groups->refresh(a)&&groups->refresh(b),"Initial group failed");
    require(a->displayed&&!b->displayed,"Initial selection failed");
    require(groups->select(b,true)&&!a->checked&&b->checked&&!a->displayed&&b->displayed,"Exclusive selection failed");
    c->checked=true;require(groups->select(b,true)&&!c->active&&c->checked&&b->checked,"Unpublished candidate affected live group");
    c->active=true;require(groups->refresh(c)&&!b->checked&&c->checked&&c->displayed,"Committed candidate did not take selection");
    c->during=[&]{c->during={};(void)groups->select(a,true);};
    require(groups->select(b,true)&&a->checked&&!b->checked&&!c->checked&&a->displayed&&!b->displayed&&!c->displayed,
            "Reentrant selection did not settle all group members");
    b->name="other";require(groups->select(b,true)&&a->checked&&b->checked,"Separate groups interfered");
    c->active=false;groups->retire(c);require(groups->select(a,false)&&!a->checked&&b->checked,"Retirement or explicit clear failed");
    auto x=std::make_shared<Member>(),y=std::make_shared<Member>();x->active=y->active=true;x->checked=y->checked=true;
    require(groups->prepare(x)&&groups->prepare(y)&&groups->refresh(x)&&groups->refresh(y)&&x->checked&&y->checked,
            "Unnamed radios were incorrectly coupled");
    auto nav=std::make_shared<NavigationMember>(),foreign=std::make_shared<NavigationMember>(),inactive=std::make_shared<NavigationMember>();
    nav->name=inactive->name="choice";foreign->name="other";inactive->active=false;
    require(groups->prepare(nav)&&groups->prepare(foreign)&&groups->prepare(inactive),"Navigation participants were not registered");
    auto choices=groups->navigation_members("choice");
    require(choices.size()==1&&choices.front()->navigate_to("choice")&&nav->visits==1&&groups->navigation_members("").empty(),
            "Optional navigation ports did not respect group and publication boundaries");
    groups->retire(nav);require(groups->navigation_members("choice").empty(),"A retired navigation member remained available");
}
}
int main(){model();default_dot_motion();indicator_motion();grouping();}
