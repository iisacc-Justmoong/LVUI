#pragma once
#include <LVUI/Motion.h>
#include <LVUI/MaterialRadialDefaults.generated.h>
#include <LVUI/StyleTokens.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace lvui {
enum class MaterialKind { surface, panel, window };
enum class MaterialDensity { glass25, dense75 };
enum class MaterialBackdrop { none, content, previous_layers };

// Accept SVG path data only, never XML. Validate complete command groups before
// handing an outline to a replaceable host rasterizer.
[[nodiscard]] inline bool valid_material_outline(std::string_view path)noexcept {
    if(path.empty())return true;
    if(path.size()>65536)return false;
    const auto* cursor=path.data();const auto* end=cursor+path.size();
    const auto space=[](char c){return c==' '||c=='\t'||c=='\n'||c=='\r';};
    const auto letter=[](char c){return (c>='A'&&c<='Z')||(c>='a'&&c<='z');};
    const auto arity=[](char c){switch(c){
        case 'M':case 'm':case 'L':case 'l':case 'T':case 't':return 2;
        case 'H':case 'h':case 'V':case 'v':return 1;
        case 'C':case 'c':return 6;
        case 'S':case 's':case 'Q':case 'q':return 4;
        case 'A':case 'a':return 7;
        default:return 0;
    }};
    bool seen_move=false,previous_number=false;char command=0;
    const auto separator=[&](){
        while(cursor!=end&&space(*cursor))++cursor;
        if(cursor!=end&&*cursor==','){
            if(!previous_number)return false;
            ++cursor;while(cursor!=end&&space(*cursor))++cursor;
            if(cursor==end||*cursor==','||letter(*cursor))return false;
            previous_number=false;
        }
        return true;
    };
    const auto number=[&]()->std::optional<double>{
        bool negative=false;if(cursor!=end&&(*cursor=='+'||*cursor=='-')){negative=*cursor=='-';++cursor;}
        const auto digit=[](char c){return c>='0'&&c<='9';};
        int digits=0,before_decimal=0,first_nonzero=-1,saved=0;long double significand=0;
        const auto append=[&](char c){
            if(first_nonzero<0&&c!='0')first_nonzero=digits;
            if(first_nonzero>=0&&saved<16){significand=significand*10+(c-'0');++saved;}
            ++digits;
        };
        while(cursor!=end&&digit(*cursor)){append(*cursor++);++before_decimal;}
        if(cursor!=end&&*cursor=='.'){++cursor;while(cursor!=end&&digit(*cursor))append(*cursor++);}
        if(digits==0)return std::nullopt;
        int exponent=0;
        if(cursor!=end&&(*cursor=='e'||*cursor=='E')){
            ++cursor;bool exponent_negative=false;
            if(cursor!=end&&(*cursor=='+'||*cursor=='-')){exponent_negative=*cursor=='-';++cursor;}
            if(cursor==end||!digit(*cursor))return std::nullopt;
            while(cursor!=end&&digit(*cursor)){exponent=(std::min)(100000,exponent*10+(*cursor-'0'));++cursor;}
            if(exponent_negative)exponent=-exponent;
        }
        if(first_nonzero<0)return 0.;
        const auto power=before_decimal-first_nonzero-saved+exponent;
        if(power>400)return std::nullopt;
        const auto scaled=power< -400?0.L:significand*std::pow(10.L,power);
        if(!std::isfinite(scaled)||scaled>1e9L)return std::nullopt;
        return static_cast<double>(negative?-scaled:scaled);
    };
    while(true){
        if(!separator())return false;
        if(cursor==end)return seen_move;
        if(letter(*cursor)){
            const auto next=*cursor++;
            if(next=='Z'||next=='z'){
                if(!seen_move)return false;command=0;previous_number=false;continue;
            }
            if(!arity(next)||(!seen_move&&next!='M'&&next!='m'))return false;
            command=next;previous_number=false;
        }else if(!command)return false;
        const auto count=arity(command);
        for(int i=0;i<count;++i){
            if(!separator()||cursor==end)return false;
            if((command=='A'||command=='a')&&(i==3||i==4)){
                if(*cursor!='0'&&*cursor!='1')return false;++cursor;
            }else{
                auto value=number();if(!value||((command=='A'||command=='a')&&i<2&&*value<0))return false;
            }
            previous_number=true;
        }
        if(command=='M'||command=='m'){seen_move=true;command=command=='M'?'L':'l';}
    }
}

// Optional overrides remain optional so changing density recomputes its defaults.
struct MaterialConfiguration final {
    MaterialKind kind=MaterialKind::surface;
    std::optional<MaterialDensity> density;
    std::optional<StyleColor> primary_color,color,border_color,backdrop_base_color;
    std::optional<double> tint_opacity,blur_radius,radius,inner_highlight_opacity,
        inner_highlight_offset_y,border_width,intense_opacity,faint_opacity,
        shadow_radius,shadow_offset_y,shadow_opacity,width,height;
    std::string outline_path;
    bool blur_enabled=true,motion_enabled=true,enabled=true;
    MaterialBackdrop backdrop=MaterialBackdrop::none;
    Motion motion{};
    friend bool operator==(const MaterialConfiguration&,const MaterialConfiguration&)=default;
};
struct ResolvedMaterial final {
    MaterialDensity density;
    MaterialBackdrop backdrop;
    StyleColor primary,tint,border,base;
    double blur_radius,radius,inner_highlight_opacity,inner_highlight_offset_y,border_width,
        intense_opacity,faint_opacity,shadow_radius,shadow_offset_y,shadow_opacity,width,height;
};
// Premultiplied contributions of the variable center and the fixed Figma
// edge colors after the ordered radial fields have been composited.
struct MaterialGradientWeights final { double primary=0,edge=0,alpha=0; };

// Design values and authoring validation are injected into the iiGUI material model.
struct MaterialModelPolicy final {
    using Configuration=MaterialConfiguration;using Resolved=ResolvedMaterial;using Kind=MaterialKind;using Density=MaterialDensity;
    using Backdrop=MaterialBackdrop;using Weights=MaterialGradientWeights;using Color=StyleColor;
    inline static constexpr auto fields=material_radial_defaults::fields;
    inline static constexpr auto first_position=material_radial_defaults::first_position,second_position=material_radial_defaults::second_position;
    inline static constexpr auto first_opacity=material_radial_defaults::first_opacity,second_opacity=material_radial_defaults::second_opacity;
    [[nodiscard]] static Color default_tint()noexcept{return figma_defaults::panel_window;}
    [[nodiscard]] static Color default_primary()noexcept{return figma_defaults::accent_blue_default;}
    [[nodiscard]] static bool valid_extra(const Configuration& value)noexcept {return valid_material_outline(value.outline_path)&&valid_motion(value.motion);}
};
} // namespace lvui
