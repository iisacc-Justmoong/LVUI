#include <LVUI/TooltipOutline.h>
#include <iiGUI/Image.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void raster(){
    lvui::Tooltip tooltip;auto config=tooltip.configuration();
    for(auto side:{lvui::TooltipPlacement::above,lvui::TooltipPlacement::below,lvui::TooltipPlacement::left,lvui::TooltipPlacement::right}){
        config.preferred=side;require(tooltip.set_configuration(config),"Tooltip direction rejected");
        auto layout=tooltip.layout({0,0,300,200},{150,100},{100,30});require(layout&&layout->placement==side,"Tooltip did not fit its preferred direction");
        auto path=lvui::tooltip_outline(*layout);require(bool(path),"Tooltip outline missing");
        const auto width=static_cast<std::uint32_t>(std::ceil(layout->frame.width)),height=static_cast<std::uint32_t>(std::ceil(layout->frame.height));
        const auto svg="<svg xmlns='http://www.w3.org/2000/svg' width='"+std::to_string(layout->frame.width)+"' height='"+
            std::to_string(layout->frame.height)+"' viewBox='0 0 "+std::to_string(layout->frame.width)+" "+
            std::to_string(layout->frame.height)+"'><path fill='white' d='"+*path+"'/></svg>";
        auto image=take(ii::gui::svg_image(svg,width,height));const auto alpha=[&](double x,double y){
            const auto px=std::clamp<int>(static_cast<int>(x),0,int(width)-1),py=std::clamp<int>(static_cast<int>(y),0,int(height)-1);
            return image->rgba()[(std::size_t(py)*width+std::size_t(px))*4+3];
        };
        const auto local_x=layout->body.x-layout->frame.x,local_y=layout->body.y-layout->frame.y;
        require(alpha(local_x+layout->body.width/2,local_y+layout->body.height/2)>240,"Tooltip body was not filled");
        const auto bx=(layout->tail_start.x+layout->tail_end.x)/2-layout->frame.x;
        const auto by=(layout->tail_start.y+layout->tail_end.y)/2-layout->frame.y;
        const auto tx=layout->tip.x-layout->frame.x,ty=layout->tip.y-layout->frame.y;
        require(alpha(bx+.4*(tx-bx),by+.4*(ty-by))>180,"Tooltip tail was not filled");
        require(alpha(0,0)<32,"Tooltip outline filled an exterior corner");
    }
}
}
int main(){raster();}
