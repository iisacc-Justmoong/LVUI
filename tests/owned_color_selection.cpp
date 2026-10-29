#include <LVUI/ColorSelection.h>
#include <atomic>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(lvui::ColorSelectionResult<T> value){if(!value)throw std::runtime_error(value.error().message);return *value;}
}
int main(){
    const lvui::StyleColor red{1,0,0,1},green{0,1,0,.5},blue{0,0,1,1};lvui::ColorSelection selection(red);
    require(selection.snapshot().color==red&&selection.revision()==0,"Initial color selection changed");
    auto first=take(selection.begin());require(selection.snapshot().editing==first&&selection.snapshot().baseline==red&&!selection.begin(),
        "Color selection admitted overlapping edit sessions");
    (void)take(selection.edit(first,green));require(selection.snapshot().color==green,"Color edit was not stored");
    (void)take(selection.cancel(first));require(selection.snapshot().color==red&&!selection.snapshot().editing&&!selection.cancel(first),
        "Cancel did not restore its baseline or reject a stale token");
    auto second=take(selection.begin());(void)take(selection.edit(second,green));(void)take(selection.accept(second));
    require(selection.snapshot().color==green&&!selection.snapshot().editing&&!selection.edit(second,blue),"Accept lost its color or retained the token");
    auto third=take(selection.begin());(void)take(selection.edit(third,red));(void)take(selection.set_color(blue));
    require(!selection.cancel(third)&&selection.snapshot().color==blue,"A stale cancel replaced an external color");
    auto fourth=take(selection.begin());const auto revision=selection.revision();
    require(take(selection.edit(fourth,blue))==revision,"An unchanged edit published a revision");
    (void)take(selection.set_color(blue));
    require(!selection.cancel(fourth)&&selection.revision()==revision+1,
        "An equal external replacement did not invalidate the edit");
    auto invalid=blue;invalid.alpha=std::numeric_limits<double>::quiet_NaN();const auto before=selection.snapshot();
    require(!selection.set_color(invalid)&&selection.snapshot().color==before.color&&selection.revision()==before.revision,
        "Invalid color changed the selection");
    bool invalid_initial=false;try{lvui::ColorSelection bad(invalid);}catch(const std::invalid_argument&){invalid_initial=true;}
    require(invalid_initial,"Invalid initial color was accepted");
    auto fifth=take(selection.begin());const auto active=selection.snapshot();
    require(!selection.edit(fifth,invalid)&&selection.snapshot().editing==active.editing&&selection.revision()==active.revision,
        "An invalid edit changed the active session");
    (void)take(selection.edit_if_current(active.revision,green));
    require(selection.snapshot().editing==fifth&&!selection.edit_if_current(active.revision,red),
        "A surface edit lost the session or accepted a stale revision");
    (void)take(selection.cancel(fifth));
    require(selection.snapshot().color==blue,"A shared surface edit changed the rollback baseline");
    auto sixth=take(selection.begin());(void)take(selection.edit(sixth,green));
    auto completed=take(selection.complete(sixth,true));
    require(completed.color==green&&completed.baseline==green&&!completed.editing&&
            completed.last_completion==lvui::ColorEditCompletion{sixth,lvui::ColorEditTermination::accepted}&&
            completed.revision==selection.revision(),"Atomic color completion returned inconsistent state");
    (void)take(selection.set_color(blue));
    require(completed.color==green&&selection.snapshot().color==blue,
            "A later external update changed the completed color snapshot");
    const auto latest=selection.snapshot();
    require(!selection.begin_if_current(completed.revision)&&!selection.snapshot().editing,
            "A stale edit start captured a later external color");
    const auto started=take(selection.begin_if_current(latest.revision));
    require(started.editing&&started.color==blue&&started.baseline==blue&&started.revision==latest.revision+1,
            "Conditional edit start did not return an atomic baseline and token");
    (void)take(selection.complete(*started.editing,false));
    std::atomic<bool> failed=false;std::vector<std::thread> workers;
    for(int index=0;index<4;++index)workers.emplace_back([&,index]{for(int n=0;n<200;++n){
        auto color=lvui::StyleColor{double(index)/3,double(n%2),0,1};if(!selection.set_color(color))failed=true;
        auto snapshot=selection.snapshot();if(!lvui::valid_color_value(snapshot.color))failed=true;
    }});
    for(auto& worker:workers)worker.join();require(!failed&&selection.color().has_value(),"Concurrent color selection produced an invalid state");
}
