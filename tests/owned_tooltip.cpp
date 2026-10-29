#include <LVUI/Tooltip.h>
#include <LVUI/TooltipOutline.h>
#include <LVUI/Material.h>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
using namespace std::chrono_literals;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool near(double a,double b){return std::abs(a-b)<1e-7;}
void geometry(){
    lvui::Tooltip tooltip;
    auto layout=tooltip.layout({0,0,300,200},{150,100},{100,30});
    require(layout&&layout->placement==lvui::TooltipPlacement::above&&near(layout->body.x,88)&&
        near(layout->body.y,36)&&near(layout->body.width,124)&&near(layout->body.height,54)&&
        near(layout->frame.height,64)&&layout->tip==lvui::TooltipPoint{150,100},"Default tooltip layout is incorrect");
    require(near(layout->tail_start.x,142)&&near(layout->tail_end.x,158)&&near(layout->radius,12),"Tooltip tail geometry is incorrect");
    auto options=tooltip.configuration();options.preferred=lvui::TooltipPlacement::below;require(tooltip.set_configuration(options),"Preferred placement rejected");
    layout=tooltip.layout({0,0,300,200},{150,100},{100,30});
    require(layout&&layout->placement==lvui::TooltipPlacement::below&&near(layout->body.y,110),"Preferred placement was ignored");
    options.preferred=lvui::TooltipPlacement::left;require(tooltip.set_configuration(options),"Left placement rejected");
    layout=tooltip.layout({0,0,300,200},{150,100},{100,30});
    require(layout&&layout->placement==lvui::TooltipPlacement::left&&near(layout->body.x,16),"Left placement was ignored");
    options.preferred=lvui::TooltipPlacement::right;require(tooltip.set_configuration(options),"Right placement rejected");
    layout=tooltip.layout({0,0,300,200},{150,100},{100,30});
    require(layout&&layout->placement==lvui::TooltipPlacement::right&&near(layout->body.x,160),"Right placement was ignored");
    options.preferred=lvui::TooltipPlacement::above;require(tooltip.set_configuration(options),"Above placement rejected");
    layout=tooltip.layout({0,0,300,200},{150,20},{100,30});
    require(layout&&layout->placement==lvui::TooltipPlacement::below,"Tooltip did not choose a fully fitting alternative");
    layout=tooltip.layout({-100,-50,100,80},{-95,-45},{500,500});
    require(layout&&layout->body.width>0&&layout->body.height>0&&layout->frame.x>=-100&&layout->frame.y>=-50&&
        layout->frame.x+layout->frame.width<=0&&layout->frame.y+layout->frame.height<=30,"Tooltip escaped its available rectangle");
    require(!tooltip.layout({0,0,100,100},{-1,20},{10,10})&&!tooltip.layout({0,0,10,10},{5,5},{10,10})&&
        !tooltip.layout({0,0,100,100},{20,20},{std::numeric_limits<double>::infinity(),1}),"Invalid tooltip geometry was accepted");
    for(int x=0;x<=300;x+=15)for(int y=0;y<=200;y+=10){
        auto candidate=tooltip.layout({0,0,300,200},{double(x),double(y)},{280,120});if(!candidate)continue;
        require(candidate->body.x>=8-1e-7&&candidate->body.y>=8-1e-7&&candidate->body.x+candidate->body.width<=292+1e-7&&
            candidate->body.y+candidate->body.height<=192+1e-7&&candidate->radius<=candidate->body.width/2+1e-7&&
            candidate->radius<=candidate->body.height/2+1e-7,"A tooltip candidate violated its layout bounds");
        auto outline=lvui::tooltip_outline(*candidate);
        require(outline&&lvui::valid_material_outline(*outline),"Tooltip layout did not yield a valid outline");
    }
    lvui::TooltipLayout invalid;invalid.frame={0,0,100,100};invalid.body={10,10,80,80};invalid.tip={50,0};
    invalid.tail_start={-100,0};invalid.tail_end={100,0};
    require(!lvui::tooltip_outline(invalid),"Invalid tooltip tail produced an outline");
}
void timing(){
    lvui::Tooltip tooltip;auto config=tooltip.configuration();
    require(config.delay==500ms&&config.timeout==5000ms&&config.hide_delay==120ms&&config.animation_duration==420ms,"Tooltip default timing is incorrect");
    config.delay=100ms;config.hide_delay=50ms;config.timeout=200ms;
    require(tooltip.set_configuration(config),"Tooltip timing configuration rejected");
    lvui::TooltipInteraction input{.target_present=true,.target_available=true,.target_engaged=true};
    require(tooltip.advance(0,input)&&tooltip.phase()==lvui::TooltipPhase::waiting_to_open&&tooltip.next_deadline()==100,"Tooltip open was not delayed");
    require(tooltip.advance(99,input)&&!tooltip.visible(),"Tooltip opened too early");
    auto opened=tooltip.advance(100,input);require(opened&&opened->opened&&tooltip.visible()&&tooltip.next_deadline()==300,"Tooltip failed to open on deadline");
    input.target_engaged=false;input.bubble_hovered=true;
    require(tooltip.advance(110,input)&&tooltip.phase()==lvui::TooltipPhase::shown,"Bubble hover did not retain tooltip");
    input.bubble_hovered=false;require(tooltip.advance(150,input)&&tooltip.phase()==lvui::TooltipPhase::waiting_to_hide&&tooltip.next_deadline()==200,"Tooltip hide was not delayed");
    input.target_engaged=true;require(tooltip.advance(175,input)&&tooltip.phase()==lvui::TooltipPhase::shown&&tooltip.next_deadline()==300,"Reentry did not cancel delayed hide");
    auto expired=tooltip.advance(300,input);require(expired&&expired->closed&&!tooltip.visible(),"Tooltip timeout did not close");
    require(tooltip.advance(301,input)&&tooltip.phase()==lvui::TooltipPhase::hidden,"Tooltip reopened without target disengagement");
    input.target_engaged=false;require(bool(tooltip.advance(302,input)),"Tooltip disengagement rejected");input.target_engaged=true;
    require(tooltip.advance(303,input)&&tooltip.next_deadline()==403,"Tooltip did not rearm after disengagement");
    require(tooltip.advance(403,input)->opened,"Rearmed tooltip did not open");input.target_available=false;
    require(tooltip.advance(404,input)->closed&&!tooltip.visible(),"Unavailable target did not close tooltip");
    require(!tooltip.advance(403,input)&&!tooltip.visible(),"Backward clock changed tooltip state");
    auto invalid=config;invalid.delay=-1ms;require(!tooltip.set_configuration(invalid)&&tooltip.configuration()==config,"Invalid timing replaced tooltip configuration");
    invalid=config;invalid.preferred=static_cast<lvui::TooltipPlacement>(88);require(!tooltip.set_configuration(invalid),"Invalid placement accepted");
}
void manual_and_reconfiguration(){
    lvui::Tooltip default_manual;require(default_manual.open(0)->opened,"Default manual tooltip did not open");
    lvui::TooltipInteraction no_target{.target_present=false};
    require(default_manual.advance(500,no_target)&&default_manual.visible()&&default_manual.next_deadline()==5000,
        "Manual tooltip was dismissed by automatic hover logic");
    lvui::Tooltip tooltip;auto options=tooltip.configuration();options.automatic=false;options.timeout=50ms;
    require(tooltip.set_configuration(options),"Manual tooltip rejected");
    auto shown=tooltip.open(0);require(shown&&shown->opened&&tooltip.visible()&&tooltip.next_deadline()==50,"Manual tooltip did not open");
    lvui::TooltipInteraction targetless{.target_present=false};require(tooltip.advance(49,targetless)&&tooltip.visible(),"Targetless tooltip closed too early");
    require(tooltip.advance(50,targetless)->closed,"Manual tooltip timeout failed");
    require(tooltip.open(60)->opened&&tooltip.close(61)->closed&&!tooltip.visible(),"Manual tooltip close failed");
    require(!tooltip.open(std::numeric_limits<std::int64_t>::max()),"Unsafe tooltip clock accepted");
    options.automatic=true;options.timeout=0ms;options.delay=100ms;require(tooltip.set_configuration(options),"Automatic tooltip rejected");
    lvui::TooltipInteraction active{.target_available=true,.target_engaged=true};require(tooltip.advance(70,active)&&tooltip.next_deadline()==170,"Pending open deadline incorrect");
    options.delay=20ms;require(tooltip.set_configuration(options)&&tooltip.next_deadline()==90,"New delay did not reschedule pending open");
    require(tooltip.advance(90,active)->opened,"Rescheduled tooltip did not open");
}
void canceled_and_hidden(){
    lvui::Tooltip tooltip;auto options=tooltip.configuration();options.delay=100ms;options.hide_delay=50ms;
    require(tooltip.set_configuration(options),"Delay configuration rejected");
    lvui::TooltipInteraction input{.target_available=true,.target_engaged=true};
    require(tooltip.advance(0,input)&&tooltip.next_deadline()==100,"Open delay not scheduled");
    input.target_engaged=false;require(tooltip.advance(50,input)&&tooltip.phase()==lvui::TooltipPhase::hidden&&!tooltip.next_deadline(),"Pending open was not canceled");
    input.target_engaged=true;require(tooltip.advance(70,input)&&tooltip.next_deadline()==170,"Open delay was not restarted");
    require(tooltip.advance(170,input)->opened,"Restarted open did not fire");
    input.target_engaged=false;require(tooltip.advance(180,input)&&tooltip.next_deadline()==230,"Hide delay not scheduled");
    require(tooltip.advance(229,input)&&tooltip.visible(),"Tooltip hid before its deadline");
    auto hidden=tooltip.advance(230,input);require(hidden&&hidden->closed&&!tooltip.visible()&&!tooltip.next_deadline(),"Hide deadline did not close tooltip");
}
}
int main(){geometry();timing();manual_and_reconfiguration();canceled_and_hidden();}
