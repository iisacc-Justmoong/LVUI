#include <LVUI/NavigationHistory.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
lvui::NavigationEntry entry(std::string path){return {.path=std::move(path)};}
void transactions(){
    using Op=lvui::NavigationOperation;lvui::NavigationHistory history;
    auto first=take(history.prepare(Op::push,entry(" home/ ")));
    require(take(history.state()).depth==0&&first.changes()&&first.destination()->path=="/home","Preparation changed history or did not normalize");
    require(take(history.commit(first))&&take(history.state()).depth==1,"Root push failed");
    require(!history.commit(first),"A committed plan was replayed");
    auto stale=take(history.prepare(Op::push,entry("/stale")));
    auto second=take(history.prepare(Op::push,entry("/reports")));require(take(history.commit(second)),"Second push failed");
    require(!history.commit(stale)&&take(history.state()).current->path=="/reports","Stale plan changed the committed history");
    auto replacement=take(history.prepare(Op::replace,entry("/settings")));require(take(history.commit(replacement)),"Replace failed");
    auto state=take(history.state());require(state.depth==2&&state.current->path=="/settings","Replace changed the wrong entry");
    auto back=take(history.prepare(Op::pop));require(back.destination()->path=="/home"&&take(history.commit(back)),"Back did not expose the previous page");
    const auto revision=take(history.state()).revision;auto root_back=take(history.prepare(Op::pop));
    require(!root_back.changes()&&!take(history.commit(root_back))&&take(history.state()).revision==revision,"Root back was not a no-op");
    auto root=take(history.prepare(Op::set_root,entry("/new-root")));require(take(history.commit(root))&&take(history.state()).depth==1,"Root replacement failed");
    auto same=take(history.prepare(Op::replace,entry("/new-root")));const auto stable=take(history.state()).revision;
    require(!same.changes()&&!take(history.commit(same))&&take(history.state()).revision==stable,"Identical replacement changed history");
    auto third=take(history.prepare(Op::push,entry("/third")));require(take(history.commit(third)),"Third push failed");
    auto fourth=take(history.prepare(Op::push,entry("/fourth")));require(take(history.commit(fourth)),"Fourth push failed");
    auto top=take(history.prepare(Op::pop_to_root));require(top.destination()->path=="/new-root"&&take(history.commit(top))&&
        take(history.entries()).size()==1,"Pop to root failed");
    auto reset=take(history.prepare(Op::clear));require(take(history.commit(reset))&&take(history.state()).depth==0,"History clear failed");
    lvui::NavigationHistory other;auto foreign=take(other.prepare(Op::push,entry("/foreign")));
    require(!history.commit(foreign),"A different history accepted a plan");
}
void validation_and_threads(){
    using Op=lvui::NavigationOperation;lvui::NavigationHistory history;auto root=take(history.prepare(Op::set_root,entry("/")));require(take(history.commit(root)),"Initial root failed");
    auto invalid=entry(std::string("bad\0path",8));const auto before=take(history.state());
    require(!history.prepare(Op::push,std::move(invalid))&&take(history.state()).revision==before.revision,"Invalid route altered history");
    auto oversized=entry("/large");oversized.params=ii::core::Value(std::string(1024*1024+1,'x'));
    require(!history.prepare(Op::push,std::move(oversized))&&take(history.state()).depth==1,"Oversized parameters altered history");
    require(!history.prepare(Op::pop,entry("/unrelated"))&&!history.prepare(Op::push),"Wrong operation arguments were accepted");
    lvui::NavigationEntry null_params=entry("/empty");null_params.params=ii::core::Value{};
    auto normalized=take(history.prepare(Op::push,std::move(null_params)));
    require(normalized.destination()->params.kind()==ii::core::ValueKind::object,"Null parameters did not become an empty object");
    bool rejected=false;std::thread worker([&]{auto state=history.state();rejected=!state&&state.error().code==ii::core::ErrorCode::wrong_executor;});worker.join();
    require(rejected,"Foreign thread read history");
}
void budget(){
    using Op=lvui::NavigationOperation;lvui::NavigationHistory history;bool rejected=false;
    for(int i=0;i<32;++i){auto value=entry("/large/"+std::to_string(i));value.params=ii::core::Value(std::string(900*1024,'x'));
        const auto before=take(history.state());auto plan=history.prepare(Op::push,std::move(value));
        if(!plan){require(plan.error().code==ii::core::ErrorCode::out_of_range&&take(history.state()).revision==before.revision&&
            take(history.state()).depth==before.depth,"History budget failure changed committed entries");rejected=true;break;}
        require(take(history.commit(*plan)),"Budgeted history entry was not committed");}
    require(rejected&&take(history.state()).retained_bytes<=16*1024*1024,"History total budget was not enforced");
}
struct RetiringPage final:lvui::NavigationPage {
    lvui::NavigationHistory* owner=nullptr;bool* busy=nullptr;
    ~RetiringPage()override{auto state=owner->state();*busy=!state&&state.error().code==ii::core::ErrorCode::busy;}
};
void retirement(){
    using Op=lvui::NavigationOperation;lvui::NavigationHistory history;bool busy=false;
    auto root=take(history.prepare(Op::push,entry("/")));require(take(history.commit(root)),"Root setup failed");
    auto page=std::make_shared<RetiringPage>();page->owner=&history;page->busy=&busy;
    {lvui::NavigationEntry value;value.page=page;value.params=ii::core::Value("typed payload");
        auto push=take(history.prepare(Op::push,std::move(value)));require(push.destination()->path.empty()&&take(history.commit(push)),"Typed page identity or payload changed");}
    page.reset();auto pop=take(history.prepare(Op::pop));require(take(history.commit(pop))&&busy,"Page retirement exposed an in-progress history");
}
}
int main(){transactions();validation_and_threads();budget();retirement();}
