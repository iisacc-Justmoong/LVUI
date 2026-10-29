#include <LVUI/Route.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
void require(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
template<class T>T take(ii::core::Result<T> result){if(!result)throw std::runtime_error(result.error().message);return std::move(*result);}
void good(ii::core::Result<void> result){if(!result)throw std::runtime_error(result.error().message);}
void normalization(){
    require(take(lvui::normalize_route(" \t reports/42/// \n"))=="/reports/42","Route trim or trailing slash changed");
    require(take(lvui::normalize_route("\xC2\xA0" "reports//42" "\xE3\x80\x80"))=="/reports//42","Unicode trim or inner empty segment changed");
    require(take(lvui::normalize_route(""))=="/"&&take(lvui::normalize_route("////"))=="/","Root normalization changed");
    require(!lvui::normalize_route(std::string("bad\0path",8))&&!lvui::normalize_route(std::string("\xC0\xAF",2))&&
        !lvui::normalize_route(std::string(65537,'x')),"Invalid route accepted");
    std::string segments;for(int i=0;i<4096;++i)segments+="/x";
    require(bool(lvui::normalize_route(segments))&&!lvui::normalize_route(segments+"/x"),"Route segment limit changed");
}
void matching(){
    auto pattern=take(lvui::RoutePattern::compile("/reports/[id]/[id]"));
    auto match=take(pattern.match("reports/first/last/"));require(match.matched&&match.params.at("id")=="last","Repeated route capture changed");
    auto partial=take(pattern.match("/reports/first"));require(!partial.matched&&partial.params.empty(),"Partial route leaked captures");
    auto empty=take(lvui::RoutePattern::compile("/reports/[id]"));
    require(take(empty.match("/reports//detail")).matched==false,"A single capture consumed extra segments");
    auto rest=take(lvui::RoutePattern::compile("/logs/[...parts]/ignored"));
    auto path=take(rest.match("/logs/a//b"));require(path.matched&&path.params.at("parts")=="a//b","Remainder capture changed");
    path=take(rest.match("/logs"));require(path.matched&&path.params.at("parts").empty(),"Empty remainder did not match");
    require(!take(take(lvui::RoutePattern::compile("")).match("/")).matched&&
        take(take(lvui::RoutePattern::compile("  ")).match("/")).matched,"Inactive and root patterns were conflated");
    require(!pattern.match(std::string("bad\0path",8)),"Invalid candidate route was accepted");
}
void resolver(){
    lvui::RouteResolver resolver;good(resolver.set_patterns({"", "/reports/[id]", "/reports/fixed", "/logs/[...parts]"}));
    auto a=take(resolver.resolve("reports/fixed/"));require(a->matched&&a->index==1&&a->params.at("id")=="fixed","Declaration order changed");
    auto cached=take(resolver.resolve("/reports/fixed"));require(a==cached&&take(resolver.state()).cache_hits==1,"Route cache did not reuse an immutable result");
    good(resolver.set_cache_capacity(2));require(take(resolver.state()).cache_capacity==16,"Cache minimum changed");
    for(int i=0;i<24;++i)(void)take(resolver.resolve("/reports/"+std::to_string(i)));
    require(take(resolver.state()).cached<=16&&take(resolver.resolve("/reports/fixed"))!=a,"Bounded least-recently-used eviction failed");
    const auto previous=take(resolver.state());require(!resolver.set_patterns(std::vector<std::string>(65537,"x"))&&
        take(resolver.state()).revision==previous.revision,"Invalid route update replaced the live resolver");
    require(!resolver.set_patterns({"/valid",std::string("bad\0path",8)})&&take(resolver.state()).revision==previous.revision&&
        take(resolver.resolve("/reports/42"))->matched,"Partially prepared route update changed the resolver");
    require(!resolver.set_cache_capacity(65537)&&take(resolver.state()).cache_capacity==16,"Invalid cache capacity changed configuration");
    good(resolver.set_patterns({"/other"}));auto unmatched=take(resolver.resolve("/reports/fixed"));
    require(!unmatched->matched&&a->matched&&a->params.at("id")=="fixed","Cache invalidation or retained result lifetime failed");
    bool rejected=false;std::thread worker([&]{auto result=resolver.resolve("/");rejected=!result&&result.error().code==ii::core::ErrorCode::wrong_executor;});worker.join();
    require(rejected,"Foreign thread accessed the route cache");
}
}
int main(){normalization();matching();resolver();}
