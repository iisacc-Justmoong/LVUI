#include <LVUI/Link.h>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
bool near(double a,double b){return std::abs(a-b)<1e-7;}
struct Page final:lvui::NavigationPage {};
struct Target final:lvui::NavigationTarget {
    std::vector<lvui::NavigationRequest> calls;
    lvui::Link* nested=nullptr;
    bool accepted=true,fail=false,nested_busy=false;
    ii::core::Result<bool> navigate(const lvui::NavigationRequest& request)override {
        calls.push_back(request);
        if(nested){auto result=nested->navigate();nested_busy=!result&&result.error().code==ii::core::ErrorCode::busy;}
        if(fail)return ii::core::fail(ii::core::ErrorCode::failed,"navigation failed");
        return accepted;
    }
};
void appearance(){
    lvui::Link link;lvui::LinkConfiguration value;value.text="Reports";
    lvui::LinkMethod method=[](const lvui::LinkEvent&){};method({});
    require(link.set_configuration(value),"Valid link configuration rejected");
    auto visual=link.visual({});require(visual.enabled&&visual.text==value.palette.link&&!visual.underline,"Default link style changed");
    const auto pressed=link.visual({.pressed=true}).text;
    require(link.visual({.hovered=true}).text==value.palette.hover&&near(pressed.red,value.palette.link.red/1.2)&&
        near(pressed.blue,value.palette.link.blue/1.2),
        "Link interaction colors changed");
    value.enabled=false;value.underline=true;require(link.set_configuration(value),"Disabled link rejected");
    visual=link.visual({.focused=true});require(!visual.enabled&&!visual.focused&&visual.underline&&visual.text==value.palette.disabled,
        "Disabled link style or focus changed");
    auto geometry=link.geometry(120,36,42,12,false);require(geometry&&near(geometry->text.x,14)&&near(geometry->text.y,12)&&
        near(geometry->text.width,42)&&geometry->draw_text,"Link text geometry changed");
    require(!link.geometry(120,36,42,12,true)->draw_text,"Child content did not suppress text fallback");
    auto before=link.configuration(),invalid=before;invalid.href=std::string("x\0y",3);
    require(!link.set_configuration(invalid)&&link.configuration().href==before.href,"Invalid path changed link");
    invalid=before;invalid.palette.link.red=2;require(!link.set_configuration(invalid),"Invalid link color accepted");
    invalid=before;invalid.horizontal_padding=-1;require(!link.set_configuration(invalid),"Invalid link padding accepted");
    invalid=before;invalid.params=ii::core::Value(ii::core::Value::Array(4096,ii::core::Value(true)));
    require(!link.set_configuration(invalid),"Unbounded navigation parameter tree was accepted");
    ii::core::Value deep;for(int i=0;i<66;++i){ii::core::Value::Array one;one.push_back(std::move(deep));deep=ii::core::Value(std::move(one));}
    invalid=before;invalid.params=std::move(deep);require(!link.set_configuration(invalid),"Excessively deep navigation parameters were accepted");
}
void navigation(){
    lvui::Link link;lvui::LinkConfiguration config;auto router=std::make_shared<Target>(),navigator=std::make_shared<Target>();
    auto context=std::make_shared<Target>();auto page=std::make_shared<Page>();
    config.href="/reports";config.params=ii::core::Value(ii::core::Value::Object{{"id",ii::core::Value(42)}});
    config.router=router;config.navigator=navigator;config.replace=true;
    require(link.set_configuration(config),"Navigation options rejected");auto result=link.navigate(context);
    require(result&&result->has_value()&&**result&&router->calls.size()==1&&navigator->calls.empty()&&
        router->calls[0].href=="/reports"&&router->calls[0].replace&&router->calls[0].params==config.params,
        "Explicit router or navigation request lost");
    config.target=page;require(link.set_configuration(config),"Page target rejected");router->nested=&link;
    result=link.navigate(context);require(result&&router->nested_busy&&router->calls.back().target==page,
        "Component target or reentry protection lost");router->nested=nullptr;
    router.reset();result=link.navigate(context);require(result&&navigator->calls.size()==1,"Expired router did not fall back to navigator");
    config.router.reset();require(link.set_configuration(config),"Fallback configuration rejected");
    config.navigator.reset();require(link.set_configuration(config),"Default target configuration rejected");
    result=link.navigate(context);require(result&&context->calls.size()==1,"Context fallback failed");
    context->fail=true;result=link.navigate(context);require(!result&&result.error().code==ii::core::ErrorCode::failed,"Navigation failure was swallowed");
    config.target.reset();config.href="";require(link.set_configuration(config),"Empty destination rejected");
    result=link.navigate(context);require(result&&!result->has_value(),"Empty destination navigated");
}
}
int main(){appearance();navigation();}
