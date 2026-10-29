#include <LVUI/NativeCollectionNavigation.h>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <thread>
namespace {
using namespace lvui::native;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
auto source(std::vector<int> values={0,1,2,3,4,5,6,7,8,9}){return virtual_collection_source(values,[](int value,std::size_t){return value;});}
void navigation(){
    auto values=source();CollectionNavigation nav;
    auto first=nav.command(*values,CollectionCommand::down,{.columns=3,.page_rows=2});
    check(first.handled&&first.index==0&&nav.state.get().current_key=="0","An unfocused collection did not enter its first item");
    nav.command(*values,CollectionCommand::down,{.columns=3,.page_rows=2});
    check(nav.state.get().current_key=="3","Grid down did not advance by a complete row");
    nav.command(*values,CollectionCommand::page_next,{.columns=3,.page_rows=2});
    check(nav.state.get().current_key=="9","Page navigation failed to reach an unmounted logical item");
    nav.command(*values,CollectionCommand::right,{.columns=3});
    check(nav.state.get().current_key=="9","Moving beyond the end wrapped into another row");
    nav.command(*values,CollectionCommand::home,{.columns=3});
    check(nav.state.get().current_key=="0","Home did not reach the collection start");
    auto activated=nav.command(*values,CollectionCommand::activate);
    check(activated.activated&&activated.index==0,"Activation lost its current logical target");
}
void selection(){
    auto values=source();CollectionNavigation nav;nav.configure({.mode=CollectionSelectionMode::multiple});
    nav.focus_at(*values,1);
    nav.focus_at(*values,4,{.range=true});
    check(nav.state.get().selected_keys==std::vector<std::string>{"1","2","3","4"},"Range selection did not use its anchor");
    nav.focus_at(*values,7,{.additive=true});
    check(nav.is_selected("1")&&nav.is_selected("7")&&nav.state.get().selected_keys.size()==5,"An additive click discarded existing selection");
    nav.command(*values,CollectionCommand::previous,{}, {.additive=true});
    check(nav.state.get().current_key=="6"&&nav.state.get().selected_keys.size()==5,"An additive arrow changed selection instead of only focus");
    nav.command(*values,CollectionCommand::toggle);
    check(nav.is_selected("6"),"Space-style toggle failed to select the current item");
    nav.command(*values,CollectionCommand::clear);
    check(nav.state.get().selected_keys.empty()&&nav.state.get().current_key=="6","Clearing selection discarded focus");
}
void projection(){
    CollectionNavigation nav;nav.configure({.mode=CollectionSelectionMode::multiple});
    auto full=source();nav.focus_at(*full,4);
    auto filtered=source({0,1,2});
    check(!nav.current_index(*filtered)&&nav.is_selected("4"),"Filtering destroyed stable-key focus or selection");
    nav.command(*filtered,CollectionCommand::select_all);
    check(nav.is_selected("4")&&nav.state.get().selected_keys.size()==4,"Selecting visible results erased a hidden selected key");
    auto reordered=source({9,8,7,6,5,4,3,2,1,0});
    check(nav.current_index(*reordered)==5,"Reordering converted a focused key into an obsolete row position");
}
void eligibility(){
    auto values=source();CollectionNavigation nav;
    auto even=[](int value,std::size_t){return value%2==0;};
    nav.command(*values,CollectionCommand::end,{}, {},even);
    check(nav.state.get().current_key=="8","End focused an ineligible item");
    nav.command(*values,CollectionCommand::previous,{}, {},even);
    check(nav.state.get().current_key=="6","Navigation did not skip an ineligible item");
    const auto revision=nav.state.revision();
    auto missing=nav.command(*values,CollectionCommand::next,{}, {},[](int,std::size_t){return false;});
    check(!missing.handled&&nav.state.revision()==revision,"An empty eligible projection mutated selection state");
}
void lifetime(){
    auto values=source();CollectionNavigation nav;bool rejected=false;
    std::thread worker([&]{try{nav.command(*values,CollectionCommand::home);}catch(const std::logic_error&){rejected=true;}});worker.join();
    check(rejected,"A foreign thread changed logical navigation");
    nav.close();bool closed=false;try{nav.command(*values,CollectionCommand::home);}catch(const std::logic_error&){closed=true;}
    check(closed,"A retired navigation model accepted more commands");nav.close();
}
void modes(){
    auto values=source();CollectionNavigation nav;
    nav.configure({.mode=CollectionSelectionMode::none});nav.command(*values,CollectionCommand::home);
    check(nav.state.get().current_key=="0"&&nav.state.get().selected_keys.empty(),"Focus-only navigation created a selection");
    nav.configure({.mode=CollectionSelectionMode::multiple,.select_on_move=false});nav.command(*values,CollectionCommand::next);
    check(nav.state.get().current_key=="1"&&nav.state.get().selected_keys.empty(),"select_on_move=false did not keep focus and selection separate");
    nav.command(*values,CollectionCommand::toggle);nav.command(*values,CollectionCommand::next,{}, {.range=true});
    check(nav.state.get().selected_keys==std::vector<std::string>{"1","2"},"An explicit range failed in focus-only movement mode");
    nav.configure({.mode=CollectionSelectionMode::single});check(nav.state.get().selected_keys==std::vector<std::string>{"2"},"Switching to single selection retained multiple keys");
    nav.configure({.mode=CollectionSelectionMode::multiple,.select_on_move=false,.anchor_on_unselected_focus=true});
    nav.focus_at(*values,7,{},true);nav.focus_at(*values,1,{},false);
    check(nav.state.get().anchor_key=="1"&&nav.is_selected("7"),"Focus-only movement lost selection or did not establish the requested anchor");
    nav.focus_at(*values,3,{.range=true},true);
    check(nav.state.get().selected_keys==std::vector<std::string>{"1","2","3"},"A subsequent range did not use the focus-only anchor");
}
void restore(){
    CollectionNavigation nav;nav.configure({.mode=CollectionSelectionMode::multiple});
    auto values=source();nav.focus_at(*values,1);nav.focus_at(*values,4,{.range=true});
    int notices=0;auto subscription=nav.state.observe([&](const auto&,std::uint64_t){++notices;});
    check(nav.set_state({.current_key="7",.anchor_key="4",.selected_keys={"7","4","4"}}),"A changed authoritative state was ignored");
    check(nav.state.get().current_key=="7"&&nav.state.get().anchor_key=="4"&&
          nav.state.get().selected_keys==std::vector<std::string>{"4","7"}&&notices==1,
          "Restoration failed to atomically publish canonical keys");
    check(!nav.set_state({.current_key="7",.anchor_key="4",.selected_keys={"4","7"}})&&notices==1,
          "Restoring an unchanged state caused another notification");
    nav.focus_at(*values,8,{.range=true,.additive=true});
    check(nav.state.get().selected_keys==std::vector<std::string>{"4","5","6","7","8"},
          "A restored anchor did not drive subsequent range selection");
    nav.set_state({.current_key="7",.anchor_key="4",.selected_keys={"4","7"}});
    const auto saved=nav.state.get();const auto revision=nav.state.revision();
    for(const auto& invalid:{std::string{},std::string(1025,'x'),std::string("a\0b",3)}) {
        bool rejected=false;try{nav.set_state({.selected_keys={invalid}});}catch(const std::invalid_argument&){rejected=true;}
        check(rejected&&nav.state.get()==saved&&nav.state.revision()==revision,"Invalid selection partially mutated the state");
    }
    for(bool current:{false,true}){
        bool rejected=false;CollectionSelectionState input=saved;
        if(current)input.current_key=std::string(1025,'x');else input.anchor_key=std::string("a\0b",3);
        try{nav.set_state(std::move(input));}catch(const std::invalid_argument&){rejected=true;}
        check(rejected&&nav.state.get()==saved&&nav.state.revision()==revision,"Invalid focus or anchor partially mutated the state");
    }
    bool oversized=false;try{nav.set_state({.selected_keys=std::vector<std::string>(maximum_collection_items+1,"1")});}
    catch(const std::length_error&){oversized=true;}
    check(oversized&&nav.state.get()==saved&&nav.state.revision()==revision,"Oversized input was not rejected atomically");
    bool overBytes=false;try{nav.set_state({.selected_keys=std::vector<std::string>(maximum_collection_key_bytes/1024+1,std::string(1024,'x'))});}
    catch(const std::length_error&){overBytes=true;}
    check(overBytes&&nav.state.get()==saved&&nav.state.revision()==revision,"An oversized key payload changed selection");
    nav.configure({.mode=CollectionSelectionMode::single});const auto single=nav.state.get();
    bool rejected=false;try{nav.set_state({.selected_keys={"1","2"}});}catch(const std::invalid_argument&){rejected=true;}
    check(rejected&&nav.state.get()==single,"Single-selection mode accepted a multi-selection");
    nav.configure({.mode=CollectionSelectionMode::none});const auto none=nav.state.get();rejected=false;
    try{nav.set_state({.selected_keys={"1"}});}catch(const std::invalid_argument&){rejected=true;}
    check(rejected&&nav.state.get()==none,"Focus-only mode accepted a selection");
    bool wrongThread=false;std::thread worker([&]{try{nav.set_state({});}catch(const std::logic_error&){wrongThread=true;}});worker.join();
    check(wrongThread&&nav.state.get()==none,"Another thread replaced navigation state");
    nav.close();rejected=false;try{nav.set_state({});}catch(const std::logic_error&){rejected=true;}
    check(rejected,"A retired navigation accepted restored state");
}
}
int main(int argc,char** argv){try{
    const std::string name=argc>1?argv[1]:"navigation";
    if(name=="navigation")navigation();else if(name=="selection")selection();else if(name=="projection")projection();else if(name=="eligibility")eligibility();else if(name=="lifetime")lifetime();else if(name=="modes")modes();else if(name=="restore")restore();else throw std::runtime_error("Unknown collection navigation test");
    std::cout<<"Native collection navigation "<<name<<" passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
