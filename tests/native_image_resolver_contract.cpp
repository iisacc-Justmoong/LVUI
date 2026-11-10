#include <LVUI/NativeImageResolver.h>
#include <memory>
#include <stdexcept>
#include <string_view>
namespace {
struct Raster final { int marker=7; };
struct Resolver final:lvui::native::ImageResolver<Raster>{
    mutable unsigned calls=0;
    std::shared_ptr<const Raster> image(std::string_view,std::uint32_t width,std::uint32_t height)const override{
        ++calls;if(width!=10||height!=5)throw std::runtime_error("Unexpected extent");return std::make_shared<const Raster>();
    }
};
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main(){
    using namespace lvui::native;
    auto source=std::make_shared<Resolver>();
    std::shared_ptr<const ImageResolver<Raster>> resolver=source;
    auto image=resolve_image<Raster>(std::string_view{"image"},resolver,9.5,4.5);
    require(image&&image->marker==7&&source->calls==1,"Typed image resolution failed");
    require(resolve_image<Raster>(image,resolver,1,1)==image,"Pixel identity changed");
    require(!resolve_image<Raster>(std::string_view{},resolver,1,1)&&source->calls==1,"Empty source invoked resolver");
    bool invalid=false;try{(void)resolve_image<Raster>(std::string_view{"bad"},resolver,9000,1);}catch(const std::invalid_argument&){invalid=true;}
    require(invalid&&source->calls==1,"Invalid extent reached the resolver");
    ImageServices<Raster> services;
    services.resolver.set(resolver);
    require(resolve_scoped_image<Raster>(std::string_view{"image"},[&]{return services.resolver.get();},10,5)!=nullptr,
            "Scoped image service did not supply its resolver");
}
