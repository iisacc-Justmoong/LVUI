#include <LVUI/NativeMeasuredCollection.h>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <thread>
namespace {
using namespace lvui::native;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
auto source(std::vector<int> values={0,1,2,3,4,5}){return virtual_collection_source(values,[](int value,std::size_t){return value;});}
void index(){
    VirtualLineIndex index;index.reset({10,20,0,40},2);
    check(index.start(0)==0&&index.start(2)==34&&index.extent()==76,"Native line prefix sums lost spacing or zero rows");
    check(index.at(12)==1&&index.end(32)==2&&index.end(35)==3,"Native line lookup lost exclusive boundaries");
    index.set(1,30);
    check(index.start(2)==44&&index.extent()==86&&index.height(1)==30,"A measured height failed to update subsequent row offsets");
    index.reset({},0);check(index.extent()==0&&index.at(0)==0&&index.end(1)==0,"An empty index retained prior extents");
}
void measured(){
    auto values=source();VirtualMeasurements<int> measured;
    const auto layout=VirtualCollectionLayout{.item_height=40,.spacing=2,.overscan=0,.measure_height=true};
    auto initial=measured.plan(values,layout,{.width=320,.height=70});
    check(initial.collection.items.size()==2&&initial.content_height==250,"Initial variable rows lost their estimate");
    check(measured.report(initial.measurement_generation,{{"0",320,10},{"1",320,60}}),"Actual heights were not accepted");
    auto next=measured.plan(values,layout,{.width=320,.height=70});
    check(next.cells[0].height==10&&next.cells[1].y==12&&next.cells[1].height==60&&next.content_height==240,
          "Measured geometry did not replace estimated row heights");
    check(!measured.report(next.measurement_generation,{{"0",320,10}}),"Unchanged sizes caused another native invalidation");
    auto scrolled=measured.plan(values,layout,{.width=320,.height=70,.y=72});
    check(scrolled.collection.items.front().index==1&&scrolled.cells.front().y==-60,"Measured visible-range lookup ignored prior variable heights");
}
void grid(){
    VirtualMeasurements<int> measured;auto values=source();
    auto layout=VirtualCollectionLayout{.item_height=40,.item_width=150,.spacing=10,.overscan=0,.measure_height=true};
    auto initial=measured.plan(values,layout,{.width=320,.height=140});
    measured.report(initial.measurement_generation,{{"0",155,20},{"1",155,90},{"2",155,30},{"3",155,50}});
    auto next=measured.plan(values,layout,{.width=320,.height=140});
    check(next.cells[0].height==90&&next.cells[1].height==90&&next.cells[2].y==100&&next.cells[2].height==50&&next.content_height==200,
          "Grid row extents did not use the tallest measured cell");
    auto resized=measured.plan(values,layout,{.width=480,.height=140});
    check(resized.columns==3&&resized.content_height==90&&resized.measurement_generation!=initial.measurement_generation,
          "A width change reused measurements made under a different wrapping constraint");
    check(!measured.report(initial.measurement_generation,{{"0",155,300}}),"A stale measurement changed a newer grid generation");
}
void anchor(){
    VirtualMeasurements<int> measured;auto values=source();
    auto layout=VirtualCollectionLayout{.item_height=40,.spacing=2,.overscan=0,.measure_height=true};
    auto first=measured.plan(values,layout,{.width=320,.height=40,.y=85});
    measured.report(first.measurement_generation,{{"0",320,100}});
    auto changed=measured.plan(values,layout,{.width=320,.height=40,.y=85},first.anchor);
    check(changed.anchor.key=="2"&&changed.y==145&&changed.cells.front().y==-1,
          "A measurement above the viewport moved the anchored content");
    auto reordered=measured.plan(source({5,0,1,2,3,4}),layout,{.width=320,.height=40,.y=145},changed.anchor);
    check(reordered.y==187&&reordered.anchor.key=="2","Reordering discarded keyed measurements or the visible anchor");
}
void lifetime(){
    VirtualMeasurements<int> measured;auto values=source();
    auto frame=measured.plan(values,{.item_height=40,.measure_height=true},{.width=320,.height=70});
    bool rejected=false;std::thread foreign([&]{try{measured.report(frame.measurement_generation,{{"0",320,10}});}catch(const std::logic_error&){rejected=true;}});foreign.join();
    check(rejected,"A foreign thread published UI measurements");
    measured.close();check(!measured.report(frame.measurement_generation,{{"0",320,10}}),"A retired measurement controller accepted a late report");
}
void diagnostics(){
    VirtualMeasurements<int> measured;auto values=source();
    auto frame=measured.plan(values,{.item_height=40,.measure_height=true},{.width=320,.height=70});
    bool rejected=false;try{measured.report(frame.measurement_generation,{{"0",320,-1}});}catch(const std::invalid_argument&){rejected=true;}
    check(rejected&&measured.plan(values,{.item_height=40,.measure_height=true},{.width=320,.height=70}).content_height==240,
          "Invalid natural geometry partially changed the layout index");
    check(!measured.report(frame.measurement_generation,{{"0",100,10}}),"A result for an obsolete width was accepted");
}
void ambient(){
    VirtualMeasurements<int> measured;auto values=source();
    auto layout=VirtualCollectionLayout{.item_height=40,.measure_height=true};
    auto epoch=std::make_shared<const int>(0);
    auto first=measured.plan(values,layout,{.width=320,.height=70},{},epoch);
    measured.report(first.measurement_generation,{{"0",320,0},{"1",320,0},{"2",320,0},{"3",320,0},{"4",320,0},{"5",320,0}});
    check(measured.plan(values,layout,{.width=320,.height=70},{},epoch).content_height==0,"Zero-height measurements retained phantom rows");
    auto changed=measured.plan(values,layout,{.width=320,.height=70},{},std::make_shared<const int>(0));
    check(changed.content_height==240&&changed.measurement_generation!=first.measurement_generation,"An ancestor change kept stale offscreen measurements");
}
void sparse(){
    std::vector<int> data(1000);std::iota(data.begin(),data.end(),0);
    auto values=source(data);VirtualMeasurements<int> measured;
    auto layout=VirtualCollectionLayout{.item_height=40,.overscan=0,.measure_height=true};
    auto first=measured.plan(values,layout,{.width=320,.height=70});
    std::vector<VirtualRowMeasurement> sizes;
    for(int n=0;n<1000;++n)sizes.push_back({std::to_string(n),320,n==1?10.:n==998?20.:0.});
    measured.report(first.measurement_generation,sizes);
    auto next=measured.plan(values,layout,{.width=320,.height=70});
    check(next.collection.options.sparse&&next.collection.items.size()==2&&next.collection.items.front().index==1&&
          next.collection.items.back().index==998&&next.cells.back().y==10&&next.content_height==30,
          "Invisible rows between visible content materialized the whole model or changed model indices");
}
}
int main(int argc,char** argv){try{
    std::string name=argc>1?argv[1]:"index";
    if(name=="index")index();else if(name=="measured")measured();else if(name=="grid")grid();else if(name=="anchor")anchor();else if(name=="lifetime")lifetime();else if(name=="diagnostics")diagnostics();else if(name=="ambient")ambient();else if(name=="sparse")sparse();else throw std::runtime_error("Unknown measured collection test");
    std::cout<<"Native measured collection "<<name<<" passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
