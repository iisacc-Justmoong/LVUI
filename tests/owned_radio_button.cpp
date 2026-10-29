#include <LVUI/RadioButton.h>
#include <LVUI/RadioGroup.h>
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
struct Member final:lvui::RadioGroupMember {
    std::string name;bool checked=false,active=false,displayed=false;int updates=0;
    std::function<void()> during;
    std::string_view group()const noexcept override{return name;}
    bool selected()const noexcept override{return checked;}
    bool participating()const noexcept override{return active;}
    void assign(bool value)noexcept override{checked=value;}
    void publish()noexcept override{if(displayed==checked)return;displayed=checked;++updates;if(during){auto callback=during;callback();}}
};
void model(){
    using namespace std::chrono_literals;lvui::RadioButton button;lvui::RadioButtonConfiguration value;
    value.text="Label";value.motion.curve=lvui::MotionCurve::linear;
    const auto start=lvui::RadioButton::TimePoint{};
    require(button.set_configuration(value,start)&&!button.checked(),"Initial radio configuration failed");
    auto geometry=button.geometry(59,18,33,13);
    require(geometry&&near(geometry->indicator.width,18)&&near(geometry->text.x,26)&&
            near(geometry->text.y,2.5)&&near(geometry->indicator_radius,9),"Radio geometry is incorrect");
    auto changed=button.activate(start);require(changed&&*changed&&button.checked()&&button.animating(),"Radio did not select");
    require(button.advance(start+90ms)&&near(button.dot_scale(),.5),"Radio dot did not animate");
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
void grouping(){
    auto groups=std::make_shared<lvui::RadioGroups>();auto a=std::make_shared<Member>();auto b=std::make_shared<Member>();auto c=std::make_shared<Member>();
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
}
}
int main(){model();grouping();}
