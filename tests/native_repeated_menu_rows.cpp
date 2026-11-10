#include "native_menu_composition.h"
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
lvui::native::MenuItemPort port(std::string key){
    lvui::native::MenuItemPort result;
    result.entry=[key=std::move(key)]()->ii::core::Result<lvui::MenuEntry>{lvui::MenuEntry entry;entry.key=key;return entry;};
    return result;
}
}
int main(){
    using namespace lvui::native;using ii::core::ErrorCode;
    MenuCapture capture;good(capture.append(port("captured")));
    bool capture_wrong_thread=false;std::thread capture_worker([&]{auto result=capture.append(port("foreign"));
        capture_wrong_thread=!result&&result.error().code==ErrorCode::wrong_executor;});capture_worker.join();
    require(capture_wrong_thread&&take(capture.release()).size()==1&&!capture.append(port("late"))&&!capture.release(),
        "A captured menu generation accepted a foreign thread or a second publication");
    {
        auto bundle=std::make_shared<MenuCapture>();good(bundle->append(port("before")));
        auto first=take(bundle->slot());good(bundle->append(port("middle")));auto second=take(bundle->slot());
        good(bundle->append(port("after")));good(first({port("a"),port("b")}));good(second({port("c")}));
        std::vector<std::string> result;int calls=0;bool reject=false,reenter=false,busy=false;
        MenuSectionPort publish=[&](std::vector<MenuItemPort> values)->ii::core::Result<void>{
            ++calls;if(reenter){auto nested=first({port("nested")});busy=!nested&&nested.error().code==ErrorCode::busy;}
            if(reject)return ii::core::fail(ErrorCode::failed,"Rejected section");
            std::vector<std::string> next;for(auto& value:values)next.push_back(take(value.entry()).key);result=std::move(next);return {};
        };
        require(calls==0,"A staged menu slot published before its parent");good(bundle->publish(publish));
        require(result==std::vector<std::string>({"before","a","b","middle","c","after"})&&!bundle->publish(publish)&&!bundle->append(port("late")),
            "A menu composition lost slot order or accepted staging after publication");
        good(first({port("b"),port("a")}));require(result==std::vector<std::string>({"before","b","a","middle","c","after"}),
            "Replacing one slot changed its siblings or their order");
        reject=true;require(!second({port("rejected")})&&result.back()=="after"&&result[4]=="c",
            "A rejected child slot changed the published composition");reject=false;
        reenter=true;good(second({port("updated")}));reenter=false;
        require(busy&&result==std::vector<std::string>({"before","b","a","middle","updated","after"}),
            "A reentrant child update changed an in-flight publication");
        require(!second(std::vector<MenuItemPort>(4097)),"An oversized menu slot was accepted");
        bool wrong=false;std::thread worker([&]{auto update=first({});wrong=!update&&update.error().code==ErrorCode::wrong_executor;});worker.join();
        require(wrong,"A nested menu slot accepted a foreign thread");
        good(bundle->close());require(!first({})&&!second({}),"Closed menu slots remained usable");
        MenuCapture unowned;require(!unowned.slot(),"An unowned menu composition admitted a slot");
    }
    {
        auto bundle=std::make_shared<MenuCapture>();auto slot=take(bundle->slot());good(slot({port("old")}));
        bool retire=false;MenuSectionPort publish=[&](auto)->ii::core::Result<void>{if(retire)good(bundle->close());return {};};
        good(bundle->publish(publish));retire=true;require(!slot({port("new")})&&!slot({}),
            "A child publication revived a menu composition retired by its callback");
    }
    {
        auto bundle=std::make_shared<MenuCapture>();auto first=take(bundle->slot());auto second=take(bundle->slot());
        good(first(std::vector<MenuItemPort>(4095)));good(bundle->append(port("last")));
        require(!second({port("excess")})&&!bundle->append(port("excess")),
            "A composed menu accepted more items than the complete generation permits");
        good(first({port("kept")}));good(second({port("new")}));
        std::size_t count=0;good(bundle->publish([&](auto items)->ii::core::Result<void>{count=items.size();return {};}));
        require(count==3,"A rejected oversized update corrupted the staged item count");
    }
    using Rows=MenuRepeatedRows;std::vector<std::string> published;int calls=0;
    bool reject=false,throws=false,reenter=false,busy=false;
    std::shared_ptr<Rows::Row> a;
    auto rows=std::make_shared<Rows>([&](Rows::Ports ports)->ii::core::Result<void>{
        ++calls;
        if(reenter){auto result=a->replace({port("reentrant")});busy=!result&&result.error().code==ErrorCode::busy;}
        if(throws)throw std::runtime_error("observer failed");
        if(reject)return ii::core::fail(ErrorCode::failed,"rejected candidate");
        std::vector<std::string> next;for(auto& item:ports)next.push_back(take(item.entry()).key);published=std::move(next);return {};
    });
    a=take(rows->row("a"));auto b=take(rows->row("b"));
    require(!rows->row(std::string(1025,'k'))&&!a->replace(Rows::Ports(4097)),"Oversized staged row was accepted");
    good(a->append(port("A")));good(b->replace({port("B")}));
    require(calls==0,"Staged row published before the keyed candidate");
    good(rows->publish({{"a",a},{"b",b}}));require(published==std::vector<std::string>({"A","B"})&&!a->add_group({}),"Initial rows were not published or accepted a late group");
    good(rows->publish({{"b",b},{"a",a}}));require(published==std::vector<std::string>({"B","A"})&&take(rows->row("a"))==a,"Reorder lost row identity");
    good(a->replace({port("A2")}));require(published==std::vector<std::string>({"B","A2"}),"Active row replacement lost sibling order");
    reject=true;require(!b->replace({port("invalid")})&&published==std::vector<std::string>({"B","A2"}),"Rejected row changed its committed ports");
    auto c=take(rows->row("c"));good(c->append(port("C")));
    require(!rows->publish({{"a",a},{"c",c}})&&take(rows->row("b"))==b,"Rejected candidate changed the active row map");
    reject=false;good(rows->publish({{"a",a},{"c",c}}));
    require(published==std::vector<std::string>({"A2","C"})&&!b->replace({port("retired")}),"Removed row remained active");
    reenter=true;good(c->replace({port("C2")}));reenter=false;
    require(busy&&published==std::vector<std::string>({"A2","C2"}),"Reentrant replacement was not rejected safely");
    throws=true;require(!a->replace({port("exception")})&&published==std::vector<std::string>({"A2","C2"}),"Thrown publication changed the committed rows");throws=false;
    require(!rows->publish({{"a",a},{"a",a}}),"Duplicate row keys were accepted");
    require(!rows->publish(std::vector<Rows::Mount>(4097,{"a",a})),"Oversized row publication was accepted");
    auto foreign=std::make_shared<Rows>([](auto)->ii::core::Result<void>{return {};});auto foreign_row=take(foreign->row("f"));
    require(!rows->publish({{"f",foreign_row}}),"A foreign row was accepted");
    bool wrong_thread=false;std::thread worker([&]{auto result=rows->row("other");wrong_thread=!result&&result.error().code==ErrorCode::wrong_executor;});worker.join();
    require(wrong_thread,"Repeated row coordination did not enforce its owner thread");
    good(rows->close());require(!a->replace({port("closed")})&&!rows->row("a"),"Closed rows remained usable");
    Rows unowned([](auto)->ii::core::Result<void>{return {};});require(!unowned.row("a"),"An unowned coordinator admitted a row");
}
