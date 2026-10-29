#include <LVUI/PageRouter.h>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
struct Page final:lvui::NavigationPage {};
struct Presenter final:lvui::NavigationPresenter {
    bool fail_before=false,fail_after=false,skip_commit=false,reentrant_busy=false;
    bool double_commit=false,double_rejected=false,foreign_commit=false,foreign_rejected=false;
    lvui::PageRouter* router=nullptr;
    std::shared_ptr<lvui::PageRouter>* release_owner=nullptr;
    std::vector<lvui::NavigationEntry> visible;
    std::function<ii::core::Result<bool>()> saved_commit;
    ii::core::Result<void> present(const lvui::NavigationEntry& entry,std::function<ii::core::Result<bool>()> commit)override {
        if(fail_before)return ii::core::fail(ii::core::ErrorCode::failed,"Candidate build failed");
        if(router){auto nested=router->navigate({.href="/"});reentrant_busy=!nested&&nested.error().code==ii::core::ErrorCode::busy;}
        saved_commit=commit;if(skip_commit)return {};
        if(release_owner)release_owner->reset();
        if(foreign_commit){std::thread worker([&]{auto result=commit();foreign_rejected=!result&&result.error().code==ii::core::ErrorCode::wrong_executor;});worker.join();}
        auto applied=commit();if(!applied)return std::unexpected(applied.error());
        if(double_commit){auto again=commit();double_rejected=!again&&again.error().code==ii::core::ErrorCode::busy;}
        if(fail_after)return ii::core::fail(ii::core::ErrorCode::failed,"Presentation failed after commit");
        visible.push_back(entry);return {};
    }
};
void routing(){
    auto home=std::make_shared<Page>(),report=std::make_shared<Page>(),fallback=std::make_shared<Page>();
    auto router=take(lvui::PageRouter::create({{"/",home},{"/reports/[id]",report}},fallback));
    auto presenter=std::make_shared<Presenter>();good(router->set_presenter(presenter));presenter->router=router.get();
    require(take(router->set_root("/"))&&presenter->reentrant_busy&&take(router->state()).history.depth==1,"Initial page or reentry guard failed");
    ii::core::Value params(ii::core::Value::Object{{"id",ii::core::Value("explicit")},{"filter",ii::core::Value(true)}});
    require(take(router->navigate({.href="/reports/42",.params=params}))&&presenter->visible.back().page==report,
        "Resolved report was not presented");
    auto id=take(presenter->visible.back().params.find("id"));
    require(take(id.get().get<std::string>()).get()=="explicit"&&take(router->state()).history.depth==2,
        "Explicit parameters did not override captured values");
    require(take(router->navigate({.href="/missing"}))&&presenter->visible.back().page==fallback&&
        presenter->visible.back().path=="/missing","Unmatched fallback lost the requested path");
    require(take(router->back())&&presenter->visible.back().page==report&&take(router->state()).history.depth==2,
        "Back did not rebuild the previous entry");
    require(take(router->pop_to_root())&&take(router->state()).history.depth==1&&presenter->visible.back().page==home,
        "Pop to root did not present the root");
    const auto count=presenter->visible.size();require(!take(router->back())&&presenter->visible.size()==count,"Root back unnecessarily rebuilt a page");
    auto direct=std::make_shared<Page>();require(take(router->navigate({.href="/reports/9",.params=ii::core::Value("payload"),.target=direct}))&&
        presenter->visible.back().page==direct&&presenter->visible.back().path.empty(),"Direct page did not override the path");
}
void failures(){
    auto page=std::make_shared<Page>();auto router=take(lvui::PageRouter::create({{"/",page},{"/ok",page}}));
    auto presenter=std::make_shared<Presenter>();good(router->set_presenter(presenter));presenter->double_commit=true;presenter->foreign_commit=true;
    require(take(router->set_root("/"))&&presenter->double_rejected&&presenter->foreign_rejected,"Initial root or commit protections failed");
    presenter->double_commit=false;presenter->foreign_commit=false;
    const auto before=take(router->state()).history;
    auto candidate=std::make_shared<Presenter>();candidate->foreign_commit=true;candidate->double_commit=true;
    require(take(router->synchronize(candidate))&&candidate->visible.back().page==page&&candidate->foreign_rejected&&candidate->double_rejected&&
        take(router->state()).history.revision==before.revision,"Candidate host synchronization changed history or broke commit isolation");
    candidate->fail_before=true;require(!router->synchronize(candidate)&&take(router->state()).history.revision==before.revision,
        "Failed host synchronization changed committed history");
    auto invalid=router->set_routes({{"/bad",{}}});require(!invalid&&take(router->state()).routes==2,
        "Invalid route replacement changed routing configuration");
    bool wrong_thread=false;std::thread worker([&]{auto result=router->back();wrong_thread=!result&&result.error().code==ii::core::ErrorCode::wrong_executor;});worker.join();
    require(wrong_thread,"Foreign thread navigated the page router");
    presenter->fail_before=true;auto failed=router->navigate({.href="/ok"});require(!failed&&take(router->state()).history.revision==before.revision,
        "A failed candidate changed history");presenter->fail_before=false;
    presenter->skip_commit=true;failed=router->navigate({.href="/ok"});require(!failed&&take(router->state()).history.revision==before.revision,
        "Presenter success without commit was accepted");presenter->skip_commit=false;
    require(!presenter->saved_commit()&&take(router->state()).history.revision==before.revision,"Expired commit callback remained usable");
    require(!router->navigate({.href="/ok",.params=ii::core::Value("scalar")})&&take(router->state()).history.revision==before.revision,
        "Non-object path parameters changed history");
    presenter->fail_after=true;failed=router->navigate({.href="/ok"});require(!failed&&take(router->state()).history.revision==before.revision+1,
        "Failure after commit hid the committed history");presenter->fail_after=false;
    auto old=take(router->state()).history;require(!take(router->navigate({.href="/unknown"}))&&take(router->state()).history.revision==old.revision,
        "Unknown route changed history");
    presenter.reset();require(!router->navigate({.href="/ok"}),"Retired presenter remained usable");
}
void lifetime(){
    struct RetiringPage final:lvui::NavigationPage {
        lvui::PageRouter* router;bool* rejected;
        RetiringPage(lvui::PageRouter* value,bool* out):router(value),rejected(out){}
        ~RetiringPage()override{auto state=router->state();*rejected=!state&&state.error().code==ii::core::ErrorCode::busy;}
    };
    auto router=take(lvui::PageRouter::create({}));bool route_busy=false,fallback_busy=false;
    auto route=std::make_shared<RetiringPage>(router.get(),&route_busy);good(router->set_routes({{"/old",route}}));route.reset();
    good(router->set_routes({}));require(route_busy,"Retired route page reentered a changing router");
    auto fallback=std::make_shared<RetiringPage>(router.get(),&fallback_busy);good(router->set_fallback(fallback));fallback.reset();
    good(router->set_fallback({}));require(fallback_busy,"Retired fallback page reentered a changing router");
    auto page=std::make_shared<Page>();good(router->set_routes({{"/",page}}));auto presenter=std::make_shared<Presenter>();good(router->set_presenter(presenter));
    std::weak_ptr<lvui::PageRouter> weak=router;presenter->release_owner=&router;
    auto result=router->set_root("/");require(result&&*result&&weak.expired(),"Navigation did not safely retain and release its owner");
}
}
int main(){routing();failures();lifetime();}
