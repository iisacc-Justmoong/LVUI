#pragma once
#include <iiColorManager/ColorValue.h>
#include <iiSDKCore/Core/Text/Number.h>
#include <concepts>
#include <stdexcept>
#include <type_traits>

namespace lvui::native {
template<class T> concept LongColorChannels=requires(const T& c){
    requires std::floating_point<std::remove_cvref_t<decltype(c.red)>>;
    requires std::floating_point<std::remove_cvref_t<decltype(c.green)>>;
    requires std::floating_point<std::remove_cvref_t<decltype(c.blue)>>;
    requires std::floating_point<std::remove_cvref_t<decltype(c.alpha)>>;
};
template<class T> concept ShortColorChannels=requires(const T& c){
    requires std::floating_point<std::remove_cvref_t<decltype(c.r)>>;
    requires std::floating_point<std::remove_cvref_t<decltype(c.g)>>;
    requires std::floating_point<std::remove_cvref_t<decltype(c.b)>>;
    requires std::floating_point<std::remove_cvref_t<decltype(c.a)>>;
};
template<class T> concept EncodedColor = requires {
    requires std::remove_cvref_t<T>::encoded_srgb;
} && (LongColorChannels<T> || ShortColorChannels<T>);
template<EncodedColor C> struct PanelColors final {
    std::optional<C> background,border;
    double width=0,radius=0;
    friend bool operator==(const PanelColors&,const PanelColors&)=default;
};
template<EncodedColor T> iiColorManager::Rgba color_channels(const T& value){
    iiColorManager::Rgba result;
    if constexpr(requires{value.red;})result={value.red,value.green,value.blue,value.alpha};
    else result={value.r,value.g,value.b,value.a};
    if(!iiColorManager::validRgba(result))throw std::domain_error("Color channels must be finite and within 0..1");
    return result;
}
template<EncodedColor To,EncodedColor From> To color(const From& value){
    const auto c=color_channels(value);
    if constexpr(LongColorChannels<To>)return To{static_cast<decltype(To::red)>(c.r),static_cast<decltype(To::green)>(c.g),static_cast<decltype(To::blue)>(c.b),static_cast<decltype(To::alpha)>(c.a)};
    else return To{static_cast<decltype(To::r)>(c.r),static_cast<decltype(To::g)>(c.g),static_cast<decltype(To::b)>(c.b),static_cast<decltype(To::a)>(c.a)};
}
inline std::optional<iiColorManager::Rgba> parse_color_text(std::string_view text,bool bare_hex=false){
    const auto trim=[](std::string_view s){
        const auto space=[](char c){return c==' '||c=='\t'||c=='\n'||c=='\r';};
        while(!s.empty()&&space(s.front()))s.remove_prefix(1);
        while(!s.empty()&&space(s.back()))s.remove_suffix(1);return s;
    };
    text=trim(text);
    if(text=="transparent")return iiColorManager::Rgba{0,0,0,0};
    if(text=="black")return iiColorManager::Rgba{0,0,0,1};
    if(text=="white")return iiColorManager::Rgba{1,1,1,1};
    if(text.starts_with('#')||(bare_hex&&(text.size()==6||text.size()==8)))return iiColorManager::parseHexValue(text);
    if(text.starts_with("rgba(")&&text.ends_with(')')){
        text=text.substr(5,text.size()-6);std::array<double,4> channels{};
        for(std::size_t i=0;i<4;++i){
            const auto comma=text.find(',');const auto n=ii::core::parse_decimal(trim(text.substr(0,comma)));
            if(!n||(i<3&&comma==text.npos)||(i==3&&comma!=text.npos))return {};
            channels[i]=*n/(i<3?255.:1.);if(comma!=text.npos)text.remove_prefix(comma+1);
        }
        iiColorManager::Rgba result{channels[0],channels[1],channels[2],channels[3]};
        if(iiColorManager::validRgba(result))return result;
    }
    return {};
}
template<EncodedColor To,class Text> requires std::convertible_to<const Text&,std::string_view>
To color(const Text& value){
    if constexpr(std::is_pointer_v<Text>)if(!value)throw std::invalid_argument("Color text cannot be null");
    const auto parsed=parse_color_text(std::string_view(value),true);
    if(!parsed)throw std::invalid_argument("Color requires encoded sRGB or valid color text");
    return color<To>(*parsed);
}
template<EncodedColor To,class... Channel> requires (sizeof...(Channel)==4 && ((std::is_arithmetic_v<Channel>&&!std::same_as<Channel,bool>)&&...))
To color(Channel... channels){return color<To>(iiColorManager::Rgba{static_cast<double>(channels)...});}
template<EncodedColor To,EncodedColor A,EncodedColor B> To composite_color(const A& a,const B& b){
    auto value=iiColorManager::compositeSrgb(color_channels(a),color_channels(b));
    if(!value)throw std::domain_error("Invalid Color composition");return color<To>(*value);
}
inline double linear_color_channel(double value){
    return value<=.04045?value/12.92:std::pow((value+.055)/1.055,2.4);
}
}
