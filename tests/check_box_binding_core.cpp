#include <LVUI/NativeCheckBox.h>
#include <concepts>
#include <memory>
#include <stdexcept>
#include <string_view>
namespace {
void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<class Policy>struct Model {using PolicyType=Policy;};
template<class Policy>struct Host {using PolicyType=Policy;};
struct Decoder {
    struct Image {};inline static int calls=0;
    static ii::core::Result<std::shared_ptr<const Image>> decode(std::shared_ptr<const void> identity,std::string_view svg,std::uint32_t size){
        if(!identity||svg.empty()||size!=68)return ii::core::fail(ii::core::ErrorCode::invalid_argument,"bad image");
        ++calls;return std::shared_ptr<const Image>(std::make_shared<Image>());
    }
};
struct Color {double red,green,blue,alpha;};
}
int main(){
    using Binding=lvui::native::CheckBoxBinding<Model,Host,Decoder>;
    using HostPolicy=Binding::PolicyType;
    static_assert(std::same_as<HostPolicy::Model,Model<lvui::native::CheckBoxModelPolicy>>);
    lvui::CheckBoxConfiguration c;c.checked=true;
    require(lvui::native::CheckBoxModelPolicy::authored_mark(c),"default color did not select authored mark");
    c.palette.checked={.1,.2,.3,1};require(!lvui::native::CheckBoxModelPolicy::authored_mark(c),"custom color selected stock mark");
    c.use_figma_checked_assets=true;require(lvui::native::CheckBoxModelPolicy::authored_mark(c),"explicit asset selection ignored");
    auto a=HostPolicy::authored_image(true),b=HostPolicy::authored_image(false);
    require(a&&b&&*a&&*b&&Decoder::calls==2,"independent decoder did not receive authored assets");
    require(lvui::native::checkbox_color(Color{.1,.2,.3,.4})==lvui::StyleColor{.1,.2,.3,.4},"generic color translation failed");
}
