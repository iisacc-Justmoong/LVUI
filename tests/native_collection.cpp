#include <LVUI/NativeCollection.h>
#include <iostream>
#include <stdexcept>
#include <thread>
namespace {
using namespace lvui::native;
struct Item final {
    std::string id,title;
    friend bool operator==(const Item&,const Item&)=default;
};
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void good(ii::core::Result<void> result){if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}}
auto frame(std::vector<Item> values,CollectionOptions options={}){
    return collection_frame(values,[](const auto& item,std::size_t){return item.id;},options);
}
void identity(){
    KeyedRows<Item> rows;
    auto apply=[](const auto&)->ii::core::Result<void>{return {};};
    good(rows.apply(frame({{"a","A"},{"b","B"}}),apply));
    auto a=rows.find("a"),b=rows.find("b");
    int changes=0;
    auto lease=b->state.observe([&](const auto& value,std::uint64_t){
        check(value.index==0&&value.value.title=="Updated","Row index and payload were published separately");++changes;
    });
    good(rows.apply(frame({{"b","Updated"},{"a","A"},{"c","C"}}),apply));
    check(rows.find("a")==a&&rows.find("b")==b&&changes==1,"A retained key lost its row state or coherent update");
    check(a->state.get().index==1&&b->state.get().value.title=="Updated","Global row position or data was stale");
    good(rows.apply(frame({{"b","Updated"},{"a","A"},{"c","C"}}),apply));
    check(changes==1,"Unchanged row payload emitted another notification");
}
void rollback(){
    KeyedRows<Item> rows;
    auto good_apply=[](const auto&)->ii::core::Result<void>{return {};};
    good(rows.apply(frame({{"a","Old"}}),good_apply));
    auto a=rows.find("a");
    std::weak_ptr<KeyedRows<Item>::Row> failed_new;
    auto rejected=rows.apply(frame({{"a","Changed"},{"b","New"}}),[&](const auto& mounted)->ii::core::Result<void>{
        failed_new=mounted[1].row;return ii::core::fail(ii::core::ErrorCode::failed,"Rejected candidate");
    });
    check(!rejected&&failed_new.expired()&&rows.find("a")==a&&!rows.find("b")&&a->state.get().value.title=="Old",
          "A rejected structure committed row data or retained its new rows");
    auto thrown=rows.apply(frame({{"b","New"}}),[](const auto&)->ii::core::Result<void>{throw std::runtime_error("factory");});
    check(!thrown&&rows.find("a")==a,"A throwing structure callback changed the collection");
    good(rows.apply(frame({{"b","New"}}),good_apply));
    check(!rows.find("a")&&rows.find("b"),"Collection did not recover after rejection");
}
void range(){
    std::vector<Item> data;
    for(int n=0;n<1000;++n)data.push_back({std::to_string(n),"Row"});
    KeyedRows<Item> rows;
    std::size_t mounted=0;
    auto apply=[&](const auto& entries)->ii::core::Result<void>{mounted=entries.size();return {};};
    auto first=frame(data,{.first=100,.count=8});
    check(first.total==1000&&first.items.size()==8&&first.items[0].index==100,"A viewport frame materialized all rows or lost global positions");
    good(rows.apply(first,apply));
    auto kept=rows.find("102");std::weak_ptr<KeyedRows<Item>::Row> removed=rows.find("100");
    good(rows.apply(frame(data,{.first=102,.count=8}),apply));
    check(mounted==8&&rows.size()==8&&rows.find("102")==kept&&removed.expired(),"Range changes did not preserve overlap and retire off-range data");
    good(rows.apply(frame(data,{.first=2000,.count=8}),apply));
    check(mounted==0&&rows.size()==0,"An out-of-range viewport retained rows");
    data[999].id="0";
    bool duplicate=false;
    try{(void)frame(data,{.first=100,.count=8});}catch(const std::invalid_argument&){duplicate=true;}
    check(duplicate,"Duplicate keys outside the viewport were accepted");
}
void lifetime(){
    KeyedRows<Item> rows;
    auto initial=frame({{"a","A"}});
    bool foreign=false;
    std::thread worker([&]{foreign=!rows.apply(initial,[](const auto&)->ii::core::Result<void>{return {};});});worker.join();
    check(foreign&&rows.size()==0,"Foreign-thread state application was admitted");
    std::weak_ptr<KeyedRows<Item>::Row> staged;
    auto closed=rows.apply(initial,[&](const auto& entries)->ii::core::Result<void>{staged=entries[0].row;good(rows.close());return {};});
    check(!closed&&staged.expired()&&rows.size()==0,"Closing inside application published a retired row");
    check(!rows.apply(initial,[](const auto&)->ii::core::Result<void>{return {};}),"A closed collection admitted another application");
}
void diagnostics(){
    for(auto values:{std::vector<Item>{{"a","A"},{"a","B"}},std::vector<Item>{{"","A"}}}){
        bool rejected=false;try{(void)frame(values);}catch(const std::invalid_argument&){rejected=true;}
        check(rejected,"Invalid collection keys were accepted");
    }
    check(collection_key(12)=="12"&&collection_key(std::string("key"))=="key","Key conversion changed identity");
    check(collection_key(UINT64_MAX)=="18446744073709551615"&&collection_key(INT64_MIN)=="-9223372036854775808",
          "A native integer key lost identity through a floating-point conversion");
    auto words=list("A",std::string("B"));
    auto mixed=list(1,2.5);
    auto integers=list(UINT64_MAX,std::uint64_t(1));
    check(words==std::vector<std::string>{"A","B"}&&mixed==std::vector<double>{1,2.5}&&integers[0]==UINT64_MAX,
          "Native list inference lost String, mixed numeric or exact integer values");
    bool fraction=false;try{(void)collection_key(1.5);}catch(const std::domain_error&){fraction=true;}
    check(fraction,"A fractional numeric key was accepted");
    for(const auto& invalid:{std::string(1025,'x'),std::string("a\0b",3),std::string("\xff",1)}){
        bool rejected=false;try{(void)collection_key(invalid);}catch(const std::invalid_argument&){rejected=true;}
        check(rejected,"An invalid UTF-8, NUL-containing or oversized key was accepted");
    }
    std::vector<int> excessive(maximum_collection_items+1);
    bool bounded=false;
    try{(void)collection_frame(excessive,[](int,std::size_t index){return index;},{.count=0});}catch(const std::length_error&){bounded=true;}
    check(bounded,"An oversized collection source was accepted");
    KeyedRows<Item> rows;
    CollectionFrame<Item> forged;
    forged.total=1;forged.items.push_back({"a",{"a","A"},2});
    check(!rows.apply(forged,[](const auto&)->ii::core::Result<void>{return {};}),"A malformed external frame was accepted");
}
void sparse(){
    KeyedRows<Item> rows;
    CollectionFrame<Item> frame;frame.total=1000;frame.options={.first=1,.count=2,.sparse=true};
    frame.items={{"a",{"a","A"},1},{"b",{"b","B"},999}};
    auto apply=[](const auto&)->ii::core::Result<void>{return {};};
    good(rows.apply(frame,apply));
    check(rows.find("a")->state.get().index==1&&rows.find("b")->state.get().index==999,"Sparse rows lost their original model positions");
    frame.items[1].index=0;
    check(!rows.apply(frame,apply)&&rows.find("b")->state.get().index==999,"A non-monotonic sparse frame changed working row data");
}
}
int main(int argc,char** argv){try{
    const std::string name=argc>1?argv[1]:"identity";
    if(name=="identity")identity();else if(name=="rollback")rollback();else if(name=="range")range();
    else if(name=="lifetime")lifetime();else if(name=="diagnostics")diagnostics();else if(name=="sparse")sparse();else throw std::runtime_error("Unknown collection test");
    std::cout<<"Native collection "<<name<<" passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
