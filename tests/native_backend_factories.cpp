#include <LVUI/NativeBackendFactories.h>
#include <iostream>
#include <stdexcept>
namespace {
using namespace lvui::native;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Default final {int value=1;};
struct Backend final {
    explicit Backend(int value):value(value){}
    Backend(const Backend&)=delete;Backend(Backend&&)=delete;
    int value;
};
struct Wrong final {int operator()(){return 0;}};
template<class F>concept Accepts=requires(BackendFactories factories,F factory){factories.template with<Backend>(std::move(factory));};
static_assert(!Accepts<Wrong>);
static_assert(!OwnedBackend<Backend&>);
void creation(){
    BackendFactories empty;check(empty.create<Default>().value==1,"Default construction changed");
    int calls=0;auto factories=empty.with<Backend>([&calls,amount=std::make_unique<int>(7)]{++calls;return Backend(*amount);});
    check(calls==0&&factories.size()==1&&empty.size()==0,"Registering a constructor created an object or changed an older snapshot");
    auto first=factories.create<Backend>();auto second=factories.create<Backend>();
    check(calls==2&&first.value==7&&second.value==7,"A nonmovable backend was copied or shared between activations");
}
void snapshots(){
    const auto before=BackendFactories{}.with<Backend>([]{return Backend(2);});
    const auto after=before.with<Backend>([]{return Backend(3);});
    check(before.create<Backend>().value==2&&after.create<Backend>().value==3&&after.size()==1,"Replacing a construction policy changed existing activation inputs");
    auto qualified=before.with<const Backend>([]()->const Backend{return Backend(4);});
    auto constant=qualified.create<const Backend>();
    check(constant.value==4&&qualified.create<Backend>().value==2&&qualified.size()==2,"Constructor policies conflated distinct C++ const types");
    bool missing=false;try{auto value=BackendFactories{}.create<Backend>("Domain.Backend");(void)value;}catch(const std::logic_error& error){missing=std::string(error.what()).find("Domain.Backend")!=std::string::npos;}
    check(missing,"A required constructor dependency silently defaulted or lost its diagnostic name");
}
void failure(){
    auto factories=BackendFactories{}.with<Backend>([]()->Backend{throw std::runtime_error("Original construction failure");});
    bool preserved=false;try{auto value=factories.create<Backend>();(void)value;}catch(const std::runtime_error& error){preserved=std::string(error.what())=="Original construction failure";}
    check(preserved,"A failing native constructor lost its original error");
}
void sites(){
    auto factories=BackendFactories{}.with<Backend>([](const BackendRequest& request){
        check(request.type=="Domain.Backend","C++ construction lost its declared type");
        return Backend(request.binding=="left"?1:2);
    });
    auto left=factories.create<Backend>("Domain.Backend","left");auto right=factories.create<Backend>("Domain.Backend","right");
    check(left.value==1&&right.value==2,"Independent import names could not receive different constructor arguments");
}
}
int main(int argc,char** argv){try{
    const std::string name=argc>1?argv[1]:"creation";
    if(name=="creation")creation();else if(name=="snapshots")snapshots();else if(name=="failure")failure();else if(name=="sites")sites();else throw std::runtime_error("Unknown backend construction test");
    std::cout<<"Native backend construction "<<name<<" passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
