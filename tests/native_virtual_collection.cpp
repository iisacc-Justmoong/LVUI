#include <LVUI/NativeVirtualCollection.h>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <thread>
namespace {
using namespace lvui::native;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
auto data(){std::vector<int> values(1000);std::iota(values.begin(),values.end(),0);return values;}
auto key=[](int value,std::size_t){return value;};
void vertical(){
    auto values=data();
    auto frame=virtual_collection_frame(values,key,{.item_height=40,.spacing=4,.overscan=1},{.width=320,.height=120,.y=441});
    check(frame.collection.total==1000&&frame.collection.options.first==9&&frame.collection.items.size()==5,
          "The native viewport did not bound its retained rows");
    check(frame.content_height==43996&&frame.cells.front().y==-45&&frame.cells.front().height==40&&frame.cells.front().width==320,
          "Logical scroll extent or fractional viewport-relative geometry is incorrect");
    auto empty=virtual_collection_frame(values,key,{.item_height=40},{});
    check(empty.collection.items.empty()&&empty.content_height==40000,"An unmeasured viewport constructed speculative row scopes");
    auto last=virtual_collection_frame(values,key,{.item_height=40,.spacing=4},{.width=320,.height=120,.y=1e12});
    check(last.y==43876&&last.collection.items.back().index==999,"Scrolling beyond the end did not clamp to the final row");
    values[999]=0;bool duplicate=false;
    try{(void)virtual_collection_frame(values,key,{.item_height=40},{.width=320,.height=120});}catch(const std::invalid_argument&){duplicate=true;}
    check(duplicate,"Virtualization skipped validation of offscreen keys");
}
void grid(){
    auto values=data();
    auto frame=virtual_collection_frame(values,key,{.item_height=80,.item_width=150,.spacing=10,.overscan=0},{.width=480,.height=170,.y=90});
    check(frame.columns==3&&frame.collection.options.first==3&&frame.collection.items.size()==6&&frame.cells[1].x>163,
          "Responsive columns did not use the native viewport width");
    check(frame.content_height==30050&&frame.cells[0].y==0&&frame.cells[0].width>153,
          "Grid geometry or the incomplete final row changed the logical extent");
    auto horizontal=virtual_collection_frame(values,key,{.item_height=40,.item_width=60,.spacing=4,.horizontal=true,.overscan=0},{.width=140,.height=90,.x=65});
    check(horizontal.collection.options.first==1&&horizontal.collection.items.size()==3&&horizontal.cells[0].x==-1&&
          horizontal.content_width==63996&&horizontal.content_height==90,"Horizontal virtualization used the vertical range or translation");
}
void anchor(){
    auto values=data();VirtualViewport viewport;
    viewport.observe({.width=320,.height=120,.y=441});
    auto initial=virtual_collection_frame(values,key,{.item_height=40,.spacing=4},viewport.current(),viewport.anchor());
    viewport.commit(initial);
    values.insert(values.begin(),1001);
    auto appended=virtual_collection_frame(values,key,{.item_height=40,.spacing=4},viewport.current(),viewport.anchor());
    check(appended.y==485&&appended.anchor.key=="10","A prepended result moved the review anchor");
    viewport.commit(appended);
    viewport.observe({.width=480,.height=170,.y=485});
    auto resized=virtual_collection_frame(values,key,{.item_height=80,.item_width=150,.spacing=10},viewport.current(),viewport.anchor());
    check(resized.anchor.key=="10"&&resized.y==271,"Resizing lost the anchored item or its intra-row position");
    viewport.commit(resized);
    viewport.observe({.width=480,.height=170,.y=0});
    auto moved=virtual_collection_frame(values,key,{.item_height=80,.item_width=150,.spacing=10},viewport.current(),viewport.anchor());
    check(moved.y==0&&moved.anchor.key=="1001","A user scroll was overridden by a stale anchor");
}
void lifetime(){
    VirtualViewport viewport;bool rejected=false;
    std::thread worker([&]{try{viewport.observe({.width=20,.height=20});}catch(const std::logic_error&){rejected=true;}});worker.join();
    check(rejected,"A foreign thread changed native viewport state");
    viewport.close();
    viewport.observe({.width=20,.height=20});
    check(viewport.current().width==0,"A retired viewport accepted a late layout callback");
    VirtualViewport live;live.observe({.width=320,.height=120,.y=441});
    auto frame=virtual_collection_frame(data(),key,{.item_height=40,.spacing=4},live.current(),live.anchor());
    live.observe({.width=480,.height=120,.y=500});
    check(!live.commit(frame)&&live.current().y==500&&live.anchor().key.empty(),"A stale row activation overwrote newer native viewport input");
}
void diagnostics(){
    auto values=data();
    for(auto layout:{VirtualCollectionLayout{.item_height=0},VirtualCollectionLayout{.item_height=20,.spacing=-1},
                     VirtualCollectionLayout{.item_height=20,.columns=65537},VirtualCollectionLayout{.item_height=20,.overscan=65537},
                     VirtualCollectionLayout{.item_height=20,.horizontal=true}}){
        bool bad=false;try{(void)virtual_collection_frame(values,key,layout,{.width=320,.height=120});}catch(const std::invalid_argument&){bad=true;}
        check(bad,"Invalid virtual layout was accepted");
    }
    auto tiny=virtual_collection_frame(values,key,{.item_height=std::numeric_limits<double>::denorm_min(),.item_width=std::numeric_limits<double>::denorm_min()},
                                        {.width=8192,.height=8192});
    check(tiny.columns==maximum_collection_items&&tiny.collection.items.size()==1000,"Extreme finite cell sizes overflowed native index arithmetic");
    auto overflow=VirtualViewportState{.width=8193,.height=120};bool rejected=false;
    try{(void)virtual_collection_frame(values,key,{.item_height=20},overflow);}catch(const std::invalid_argument&){rejected=true;}
    check(rejected,"An unbounded viewport was admitted");
}
void indexed(){
    auto values=data();std::size_t key_reads=0;
    auto source=virtual_collection_source(values,[&](int value,std::size_t){++key_reads;return value;});
    for(int n=0;n<200;++n){
        auto frame=virtual_collection_frame(*source,{.item_height=40,.spacing=4},{.width=320,.height=120,.y=double(n)});
        check(frame.collection.items.size()<=6,"Indexed scrolling retained too many rows");
    }
    check(key_reads==1000,"Viewport movement rescanned every model key");
    values[0]=2000;
    auto stable=virtual_collection_frame(*source,{.item_height=40},{.width=320,.height=120});
    check(stable.collection.items.front().value==0&&source->find("100")==100,"A source snapshot borrowed a changing source range or lost its key index");
}
void reveal(){
    auto values=data();VirtualViewport viewport;viewport.observe({.width=320,.height=120});
    viewport.reveal("900");
    auto frame=virtual_collection_frame(values,key,{.item_height=40,.spacing=4},viewport.current(),viewport.anchor());
    check(frame.revealed_key=="900"&&frame.y==39520&&frame.collection.items.back().index>=900,
          "Revealing an unmounted logical key did not make its whole row visible");
    check(viewport.commit(frame)&&viewport.current().reveal_key.empty(),"An applied reveal remained pending");
    viewport.reveal("10",CollectionReveal::center);
    auto centered=virtual_collection_frame(values,key,{.item_height=40,.spacing=4},viewport.current(),viewport.anchor());
    check(centered.y==400,"Center reveal used the old scroll anchor");
    viewport.observe({.width=320,.height=120,.y=100});
    check(viewport.current().reveal_key.empty(),"A user scroll did not supersede pending programmatic focus");
    VirtualViewport unmeasured;unmeasured.reveal("500");
    auto pending=virtual_collection_frame(values,key,{.item_height=40,.item_width=150},unmeasured.current(),unmeasured.anchor());
    check(unmeasured.commit(pending)&&unmeasured.current().reveal_key=="500","A focus request was acknowledged before a real viewport existed");
}
}
int main(int argc,char** argv){try{
    std::string name=argc>1?argv[1]:"vertical";
    if(name=="vertical")vertical();else if(name=="grid")grid();else if(name=="anchor")anchor();else if(name=="lifetime")lifetime();else if(name=="diagnostics")diagnostics();else if(name=="indexed")indexed();else if(name=="reveal")reveal();
    else throw std::runtime_error("Unknown native virtual collection test");
    std::cout<<"Native virtual collection "<<name<<" passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
