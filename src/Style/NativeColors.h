#pragma once

#include <array>
#include <cmath>
#include <concepts>
#include <limits>
#include <locale>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace lvui::native {

// Unassociated, encoded sRGB; all channels must be finite and normalized.
struct EncodedSrgb final {
    static constexpr bool encoded_srgb = true;
    double r = 0, g = 0, b = 0, a = 1;
    bool operator==(const EncodedSrgb&) const = default;
};

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

namespace color_detail {
inline bool valid(EncodedSrgb value) noexcept {
    const auto channel=[](double v){return std::isfinite(v)&&v>=0&&v<=1;};
    return channel(value.r)&&channel(value.g)&&channel(value.b)&&channel(value.a);
}
inline std::string_view trim(std::string_view text) noexcept {
    const auto space=[](char c){return c==' '||c=='\t'||c=='\n'||c=='\r';};
    while(!text.empty()&&space(text.front()))text.remove_prefix(1);
    while(!text.empty()&&space(text.back()))text.remove_suffix(1);
    return text;
}
inline std::optional<EncodedSrgb> hex(std::string_view text) noexcept {
    if(text.starts_with('#'))text.remove_prefix(1);
    const auto size=text.size();
    if(size!=3&&size!=4&&size!=6&&size!=8)return {};
    std::array<int,8> digits{};
    for(std::size_t i=0;i<size;++i){
        const auto c=text[i];
        digits[i]=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;
        if(digits[i]<0)return {};
    }
    const auto channel=[&](std::size_t i){return (size<5?digits[i]*17:digits[i*2]*16+digits[i*2+1])/255.;};
    return EncodedSrgb{channel(0),channel(1),channel(2),size==4||size==8?channel(3):1.};
}
// A classic-locale stream also works on supported Apple platforms whose
// libc++ does not provide floating-point from_chars before macOS 26.
inline std::optional<double> decimal(std::string_view text) {
    if(text.empty()||text.size()>1024)return {};
    std::size_t i=text.front()=='-'?1:0;
    bool digit=false,nonzero=false;
    const auto digits=[&]{while(i<text.size()&&text[i]>='0'&&text[i]<='9'){
        digit=true;nonzero=nonzero||text[i]!='0';++i;
    }};
    digits();
    if(i<text.size()&&text[i]=='.'){++i;digits();}
    if(!digit)return {};
    if(i<text.size()&&(text[i]=='e'||text[i]=='E')){
        ++i;
        if(i<text.size()&&(text[i]=='+'||text[i]=='-'))++i;
        const auto start=i;
        while(i<text.size()&&text[i]>='0'&&text[i]<='9')++i;
        if(i==start)return {};
    }
    if(i!=text.size())return {};
    if(!nonzero)return text.front()=='-'?-0.:0.;
    std::istringstream stream{std::string(text)};
    stream.imbue(std::locale::classic());
    double result=0;
    stream>>std::noskipws>>result;
    const bool subnormal=result!=0&&std::abs(result)<(std::numeric_limits<double>::min)();
    if(stream.bad()||(stream.fail()&&!subnormal)||!stream.eof()||!std::isfinite(result)||result==0)return {};
    return result;
}
} // namespace color_detail

template<EncodedColor T> EncodedSrgb color_channels(const T& value){
    EncodedSrgb result;
    if constexpr(LongColorChannels<T>)result={value.red,value.green,value.blue,value.alpha};
    else result={value.r,value.g,value.b,value.a};
    if(!color_detail::valid(result))throw std::domain_error("Color channels must be finite and within 0..1");
    return result;
}
template<EncodedColor To,EncodedColor From> To color(const From& value){
    const auto c=color_channels(value);
    if constexpr(LongColorChannels<To>)return To{static_cast<decltype(To::red)>(c.r),static_cast<decltype(To::green)>(c.g),static_cast<decltype(To::blue)>(c.b),static_cast<decltype(To::alpha)>(c.a)};
    else return To{static_cast<decltype(To::r)>(c.r),static_cast<decltype(To::g)>(c.g),static_cast<decltype(To::b)>(c.b),static_cast<decltype(To::a)>(c.a)};
}
inline std::optional<EncodedSrgb> parse_color_text(std::string_view text,bool bare_hex=false){
    text=color_detail::trim(text);
    if(text=="transparent")return EncodedSrgb{0,0,0,0};
    if(text=="black")return EncodedSrgb{0,0,0,1};
    if(text=="white")return EncodedSrgb{1,1,1,1};
    if(text.starts_with('#')||(bare_hex&&(text.size()==6||text.size()==8)))return color_detail::hex(text);
    if(text.starts_with("rgba(")&&text.ends_with(')')){
        text=text.substr(5,text.size()-6);std::array<double,4> channels{};
        for(std::size_t i=0;i<4;++i){
            const auto comma=text.find(',');const auto n=color_detail::decimal(color_detail::trim(text.substr(0,comma)));
            if(!n||(i<3&&comma==text.npos)||(i==3&&comma!=text.npos))return {};
            channels[i]=*n/(i<3?255.:1.);if(comma!=text.npos)text.remove_prefix(comma+1);
        }
        EncodedSrgb result{channels[0],channels[1],channels[2],channels[3]};
        if(color_detail::valid(result))return result;
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
To color(Channel... channels){return color<To>(EncodedSrgb{static_cast<double>(channels)...});}
template<EncodedColor To,EncodedColor A,EncodedColor B> To composite_color(const A& a,const B& b){
    const auto front=color_channels(a),back=color_channels(b);
    const auto alpha=front.a+back.a*(1-front.a);
    if(alpha==0)return color<To>(EncodedSrgb{0,0,0,0});
    const auto channel=[&](double x,double y){return (x*front.a+y*back.a*(1-front.a))/alpha;};
    return color<To>(EncodedSrgb{channel(front.r,back.r),channel(front.g,back.g),channel(front.b,back.b),alpha});
}
inline double linear_color_channel(double value){
    return value<=.04045?value/12.92:std::pow((value+.055)/1.055,2.4);
}
}
