#include <LVUI/NativeValues.h>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
using Color=iiColorManager::Rgba;
struct UiColor final {static constexpr bool encoded_srgb=true;double red=0,green=0,blue=0,alpha=1;bool operator==(const UiColor&)const=default;};
struct LinearColor final {double red=0,green=0,blue=0,alpha=1;};
struct FloatColor final {static constexpr bool encoded_srgb=true;float red=0,green=0,blue=0,alpha=1;bool operator==(const FloatColor&)const=default;};
struct ByteColor final {static constexpr bool encoded_srgb=true;unsigned char r=0,g=0,b=0,a=255;};
static_assert(lvui::native::EncodedColor<Color>&&lvui::native::EncodedColor<UiColor>&&!lvui::native::EncodedColor<LinearColor>);
static_assert(lvui::native::EncodedColor<FloatColor>&&!lvui::native::EncodedColor<ByteColor>);
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<class F>void rejected(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed,"Invalid native Color accepted");}
void values(){
    using namespace lvui::native;
    check(color<UiColor>(Color{.2,.4,.6,.5})==UiColor{.2,.4,.6,.5},"SDK and widget channels changed");
    check(color<FloatColor>(Color{.25,.5,.75,1})==FloatColor{.25f,.5f,.75f,1},"Explicit floating Color conversion narrowed incorrectly");
    check(invoke_callback<FloatColor()>([]{return FloatColor{.25f,.5f,.75f,1};})==FloatColor{.25f,.5f,.75f,1},"Imported float Color result was rejected");
    check(color<Color>("#1234")==Color{17/255.,34/255.,51/255.,68/255.},"HEX channels changed");
    check(color<Color>("ff000080")==Color{1,0,0,128/255.},"Bare hex changed");
    check(color<Color>("rgba(255, 0, 127.5, .25)")==Color{1,0,.5,.25},"RGBA text changed");
    check(color<Color>("transparent")==Color{0,0,0,0},"Named transparent changed");
    check(composite_color<Color>(Color{1,0,0,.5},Color{0,0,1,1})==Color{.5,0,.5,1},"Source-over changed");
    check(equal(Color{1,0,0,1},UiColor{1,0,0,1})&&!equal(Color{1,0,0,1},UiColor{}),"Color equality changed");
    check(linear_color_channel(.04045)==.04045/12.92,"sRGB decode boundary changed");
}
void bounds(){using namespace lvui::native;
    rejected([]{(void)color<Color>(2.,0.,0.,1.);});rejected([]{(void)color<Color>(Color{0,0,0,std::numeric_limits<double>::quiet_NaN()});});
    for(auto text:{"#zz0000","rgba(255,0,0,2)","rgba(255,0,0)","rgba(1,2,3,1,2)","rgba(nan,0,0,1)","xyz"})rejected([=]{(void)color<Color>(text);});
    rejected([]{const char* missing=nullptr;(void)color<Color>(missing);});
    ii::core::Observable<Color> state{Color{1,0,0,1}};rejected([&]{(void)assign_value(state,Color{2,0,0,1});});
    check(state.get()==Color{1,0,0,1},"Invalid assignment replaced valid color state");
}
void reactive(){using namespace lvui::native;
    Bindings graph;ii::core::Observable<bool> selected{true};ii::core::Observable<Color> a{Color{1,0,0,1}},b{Color{0,0,1,1}};Color shown;int updates=0;
    auto id=graph.bind([&](ReadContext& c){return color<Color>(choose(c.read(selected),[&]{return c.read(a);},[&]{return c.read(b);}));},[&](Color value)->ii::core::Result<void>{shown=value;++updates;return {};});
    check(id&&shown==a.get(),"Initial reactive Color failed");b.set({0,1,0,1});check(updates==1,"Inactive branch remained subscribed");selected.set(false);check(shown==b.get(),"Selected branch retained old Color");
    b.set({2,0,0,1});check(shown==Color{0,1,0,1}&&!graph.failures()->empty(),"Invalid source did not preserve last valid display");
    b.set({.3,.4,.5,.6});check(shown==b.get(),"Color binding did not recover");
}
void callbacks(){using namespace lvui::native;
    auto result=invoke_callback<UiColor()>([]{return Color{.2,.3,.4,.5};});check(result==UiColor{.2,.3,.4,.5},"Function result did not normalize Color");
    UiColor received;invoke_callback<void(UiColor)>([&](UiColor c){received=c;},Color{1,0,0,.5});check(received==UiColor{1,0,0,.5},"Event Color did not normalize");
    rejected([]{(void)invoke_callback<UiColor()>([]{return Color{2,0,0,1};});});
}
void lifetime(){using namespace lvui::native;
    Bindings graph;ii::core::Observable<Color> source{Color{1,0,0,1}};int delivered=0;
    auto id=graph.bind([&](ReadContext& c){return color<Color>(c.read(source));},[&](Color)->ii::core::Result<void>{++delivered;return {};});
    check(id&&graph.close(),"Color binding did not close");source.set({0,0,1,1});check(delivered==1,"Closed Color subscription delivered");
}
void threading(){using namespace lvui::native;
    Bindings graph;std::vector<std::thread> workers;std::atomic<int> complete=0;
    for(int i=0;i<4;++i)workers.emplace_back([&]{for(int j=0;j<100;++j)check(color<Color>("rgba(255,0,0,.5)")==Color{1,0,0,.5},"Concurrent Color parsing changed");auto r=graph.failures();check(!r&&r.error().code==ii::core::ErrorCode::wrong_executor,"Color binding allowed foreign-domain inspection");++complete;});
    for(auto& t:workers)t.join();check(complete==4&&graph.close(),"Concurrent Color value work did not finish");
}
int main(int argc,char** argv){try{std::string s=argc>1?argv[1]:"values";if(s=="values")values();else if(s=="bounds")bounds();else if(s=="reactive")reactive();else if(s=="callbacks")callbacks();else if(s=="lifetime")lifetime();else if(s=="threading")threading();else throw std::runtime_error("Unknown Color test");std::cout<<"Native Color "<<s<<" passed\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
