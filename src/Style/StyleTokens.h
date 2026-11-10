#pragma once
#include <expected>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace lvui {
// Authored, encoded sRGB. Keep alpha independent of the RGB channels.
struct StyleColor final {
    static constexpr bool encoded_srgb = true;
    double red = 0, green = 0, blue = 0, alpha = 1;
    friend bool operator==(const StyleColor &, const StyleColor &) = default;
};
// BEGIN GENERATED FIGMA COLOR DEFAULTS
namespace figma_defaults {
[[nodiscard]] constexpr StyleColor with_alpha(StyleColor color, double alpha) noexcept {
    color.alpha = alpha; return color;
}
inline constexpr StyleColor material_alert_action_border{0.3490196168422699, 0.3803921639919281, 0.40784314274787903, 1.0};
inline constexpr StyleColor material_alert_canvas{0.0694509819149971, 0.0744117721915245, 0.07937255501747131, 1.0};
inline constexpr StyleColor material_alert_surface{0.1438627392053604, 0.15378431975841522, 0.16370590031147003, 1.0};
inline constexpr StyleColor material_backdrop_base{0.05456862971186638, 0.08929412066936493, 0.15874511003494263, 1.0};
inline constexpr StyleColor material_glass_fill{0.1438627392053604, 0.15378431975841522, 0.16370590031147003, 0.7200000286102295};
inline constexpr StyleColor material_surface_dense_fill{0.09921568632125854, 0.09921568632125854, 0.09921568632125854, 0.75};
inline constexpr StyleColor material_surface_glass_fill{0.09921568632125854, 0.09921568632125854, 0.09921568632125854, 0.25};
inline constexpr StyleColor material_text_field_disabled{0.19681522250175476, 0.1986967921257019, 0.20057833194732666, 0.36000001430511475};
inline constexpr StyleColor material_text_field_fill{0.19681522250175476, 0.1986967921257019, 0.20057833194732666, 0.6399999856948853};
inline constexpr StyleColor material_text_field_inline{0.19681522250175476, 0.1986967921257019, 0.20057833194732666, 0.1599999964237213};
inline constexpr StyleColor accent_blue_default{0.1557759940624237, 0.5030806064605713, 0.8999999761581421, 1.0};
inline constexpr StyleColor accent_blue_disabled{0.1557759940624237, 0.5030806064605713, 0.8999999761581421, 0.30000001192092896};
inline constexpr StyleColor accent_blue_hover{0.13908571004867554, 0.449179083108902, 0.8035714030265808, 1.0};
inline constexpr StyleColor accent_blue_outline{0.13739700615406036, 0.2624266743659973, 0.40531763434410095, 1.0};
inline constexpr StyleColor accent_blue_overlay{0.1557759940624237, 0.5030806064605713, 0.8999999761581421, 0.250980406999588};
inline constexpr StyleColor accent_blue_pressed{0.1298133283853531, 0.4192337989807129, 0.75, 1.0};
inline constexpr StyleColor accent_blue_selected{0.1342381089925766, 0.2210642546415329, 0.3202941119670868, 1.0};
inline constexpr StyleColor accent_blue_subtle{0.1316535770893097, 0.18722230195999146, 0.25072941184043884, 1.0};
inline constexpr StyleColor accent_blue_text{0.3758823573589325, 0.6299999952316284, 0.8999999761581421, 1.0};
inline constexpr StyleColor accent_brown_default{0.23544545471668243, 0.1355230212211609, 0.09870952367782593, 1.0};
inline constexpr StyleColor accent_brown_disabled{0.23544545471668243, 0.1355230212211609, 0.09870952367782593, 0.30000001192092896};
inline constexpr StyleColor accent_brown_hover{0.21021915972232819, 0.121002696454525, 0.08813349902629852, 1.0};
inline constexpr StyleColor accent_brown_outline{0.16607801616191864, 0.13010594248771667, 0.11685307323932648, 1.0};
inline constexpr StyleColor accent_brown_overlay{0.23544545471668243, 0.1355230212211609, 0.09870952367782593, 0.250980406999588};
inline constexpr StyleColor accent_brown_pressed{0.1962045580148697, 0.11293584853410721, 0.08225793391466141, 1.0};
inline constexpr StyleColor accent_brown_selected{0.15415547788143158, 0.12917488813400269, 0.11997150629758835, 1.0};
inline constexpr StyleColor accent_brown_subtle{0.1444006860256195, 0.1284130960702896, 0.12252293527126312, 1.0};
inline constexpr StyleColor accent_brown_text{0.5358718037605286, 0.4811214208602905, 0.4609502851963043, 1.0};
inline constexpr StyleColor accent_cyan_default{0.1947030872106552, 0.758940577507019, 0.8999999761581421, 1.0};
inline constexpr StyleColor accent_cyan_disabled{0.1947030872106552, 0.758940577507019, 0.8999999761581421, 0.30000001192092896};
inline constexpr StyleColor accent_cyan_hover{0.17384204268455505, 0.6776254773139954, 0.8035714030265808, 1.0};
inline constexpr StyleColor accent_cyan_outline{0.1514107584953308, 0.3545362651348114, 0.40531763434410095, 1.0};
inline constexpr StyleColor accent_cyan_overlay{0.1947030872106552, 0.758940577507019, 0.8999999761581421, 0.250980406999588};
inline constexpr StyleColor accent_cyan_pressed{0.16225257515907288, 0.632450520992279, 0.75, 1.0};
inline constexpr StyleColor accent_cyan_selected{0.14396989345550537, 0.28502926230430603, 0.3202941119670868, 1.0};
inline constexpr StyleColor accent_cyan_subtle{0.13788191974163055, 0.22815990447998047, 0.25072941184043884, 1.0};
inline constexpr StyleColor accent_cyan_text{0.4062921702861786, 0.8012584447860718, 0.8999999761581421, 1.0};
inline constexpr StyleColor accent_green_default{0.0815294086933136, 0.8152941465377808, 0.18000000715255737, 1.0};
inline constexpr StyleColor accent_green_disabled{0.0815294086933136, 0.8152941465377808, 0.18000000715255737, 0.30000001192092896};
inline constexpr StyleColor accent_green_hover{0.0727941170334816, 0.7279411554336548, 0.1607142835855484, 1.0};
inline constexpr StyleColor accent_green_outline{0.11066824197769165, 0.37482354044914246, 0.14611764252185822, 1.0};
inline constexpr StyleColor accent_green_overlay{0.0815294086933136, 0.8152941465377808, 0.18000000715255737, 0.250980406999588};
inline constexpr StyleColor accent_green_pressed{0.06794117391109467, 0.679411768913269, 0.15000000596046448, 1.0};
inline constexpr StyleColor accent_green_selected{0.11567647755146027, 0.29911765456199646, 0.1402941197156906, 1.0};
inline constexpr StyleColor accent_green_subtle{0.11977411806583405, 0.23717647790908813, 0.13552941381931305, 1.0};
inline constexpr StyleColor accent_green_text{0.32707059383392334, 0.840705931186676, 0.3959999978542328, 1.0};
inline constexpr StyleColor accent_orange_default{0.8999999761581421, 0.4761454164981842, 0.2126682698726654, 1.0};
inline constexpr StyleColor accent_orange_disabled{0.8999999761581421, 0.4761454164981842, 0.2126682698726654, 0.30000001192092896};
inline constexpr StyleColor accent_orange_hover{0.8035714030265808, 0.42512986063957214, 0.18988238275051117, 1.0};
inline constexpr StyleColor accent_orange_outline{0.40531763434410095, 0.25273001194000244, 0.15787822008132935, 1.0};
inline constexpr StyleColor accent_orange_overlay{0.8999999761581421, 0.4761454164981842, 0.2126682698726654, 0.250980406999588};
inline constexpr StyleColor accent_orange_pressed{0.75, 0.3967878520488739, 0.1772235631942749, 1.0};
inline constexpr StyleColor accent_orange_selected{0.3202941119670868, 0.21433047950267792, 0.14846119284629822, 1.0};
inline constexpr StyleColor accent_orange_subtle{0.25072941184043884, 0.1829126924276352, 0.14075633883476257, 1.0};
inline constexpr StyleColor accent_orange_text{0.8999999761581421, 0.6033017635345459, 0.4188677966594696, 1.0};
inline constexpr StyleColor accent_purple_default{0.5172802805900574, 0.3259204030036926, 0.8999999761581421, 1.0};
inline constexpr StyleColor accent_purple_disabled{0.5172802805900574, 0.3259204030036926, 0.8999999761581421, 0.30000001192092896};
inline constexpr StyleColor accent_purple_hover{0.461857408285141, 0.2910003662109375, 0.8035714030265808, 1.0};
inline constexpr StyleColor accent_purple_outline{0.26753854751586914, 0.19864900410175323, 0.40531763434410095, 1.0};
inline constexpr StyleColor accent_purple_overlay{0.5172802805900574, 0.3259204030036926, 0.8999999761581421, 0.250980406999588};
inline constexpr StyleColor accent_purple_pressed{0.4310669004917145, 0.2716003358364105, 0.75, 1.0};
inline constexpr StyleColor accent_purple_selected{0.2246142029762268, 0.17677423357963562, 0.3202941119670868, 1.0};
inline constexpr StyleColor accent_purple_subtle{0.18949425220489502, 0.1588766872882843, 0.25072941184043884, 1.0};
inline constexpr StyleColor accent_purple_text{0.6320961713790894, 0.498144268989563, 0.8999999761581421, 1.0};
inline constexpr StyleColor accent_red_default{0.8999999761581421, 0.31362923979759216, 0.31362923979759216, 1.0};
inline constexpr StyleColor accent_red_disabled{0.8999999761581421, 0.31362923979759216, 0.31362923979759216, 0.30000001192092896};
inline constexpr StyleColor accent_red_hover{0.8035714030265808, 0.2800261080265045, 0.2800261080265045, 1.0};
inline constexpr StyleColor accent_red_outline{0.40531763434410095, 0.19422417879104614, 0.19422417879104614, 1.0};
inline constexpr StyleColor accent_red_overlay{0.8999999761581421, 0.31362923979759216, 0.31362923979759216, 0.250980406999588};
inline constexpr StyleColor accent_red_pressed{0.75, 0.26135769486427307, 0.26135769486427307, 1.0};
inline constexpr StyleColor accent_red_selected{0.3202941119670868, 0.1737014353275299, 0.1737014353275299, 1.0};
inline constexpr StyleColor accent_red_subtle{0.25072941184043884, 0.15691010653972626, 0.15691010653972626, 1.0};
inline constexpr StyleColor accent_red_text{0.8999999761581421, 0.4895404875278473, 0.4895404875278473, 1.0};
inline constexpr StyleColor accent_rose_default{0.8779597282409668, 0.30369314551353455, 0.4472598135471344, 1.0};
inline constexpr StyleColor accent_rose_disabled{0.8779597282409668, 0.30369314551353455, 0.4472598135471344, 0.30000001192092896};
inline constexpr StyleColor accent_rose_hover{0.7838926315307617, 0.27115461230278015, 0.39933910965919495, 1.0};
inline constexpr StyleColor accent_rose_outline{0.3973831534385681, 0.1906471848487854, 0.24233117699623108, 1.0};
inline constexpr StyleColor accent_rose_overlay{0.8779597282409668, 0.30369314551353455, 0.4472598135471344, 0.250980406999588};
inline constexpr StyleColor accent_rose_pressed{0.7316331267356873, 0.2530776262283325, 0.37271648645401, 1.0};
inline constexpr StyleColor accent_rose_selected{0.31478404998779297, 0.1712174117565155, 0.20710907876491547, 1.0};
inline constexpr StyleColor accent_rose_subtle{0.24720297753810883, 0.15532033145427704, 0.17829099297523499, 1.0};
inline constexpr StyleColor accent_rose_text{0.8845718502998352, 0.48258522152900696, 0.583081841468811, 1.0};
inline constexpr StyleColor accent_teal_default{0.07790151238441467, 0.7089037895202637, 0.7790151834487915, 1.0};
inline constexpr StyleColor accent_teal_disabled{0.07790151238441467, 0.7089037895202637, 0.7790151834487915, 0.30000001192092896};
inline constexpr StyleColor accent_teal_hover{0.06955492496490479, 0.6329497694969177, 0.6955492496490479, 1.0};
inline constexpr StyleColor accent_teal_outline{0.10936219245195389, 0.33652299642562866, 0.3617630898952484, 1.0};
inline constexpr StyleColor accent_teal_overlay{0.07790151238441467, 0.7089037895202637, 0.7790151834487915, 0.250980406999588};
inline constexpr StyleColor accent_teal_pressed{0.06491792947053909, 0.5907531380653381, 0.6491792798042297, 1.0};
inline constexpr StyleColor accent_teal_selected{0.11476950347423553, 0.2725200653076172, 0.29004791378974915, 1.0};
inline constexpr StyleColor accent_teal_subtle{0.1191936582326889, 0.2201540172100067, 0.23137183487415314, 1.0};
inline constexpr StyleColor accent_teal_text{0.32453107833862305, 0.7662326097488403, 0.8153105974197388, 1.0};
inline constexpr StyleColor accent_yellow_default{0.8999999761581421, 0.7625914812088013, 0.21295729279518127, 1.0};
inline constexpr StyleColor accent_yellow_disabled{0.8999999761581421, 0.7625914812088013, 0.21295729279518127, 0.30000001192092896};
inline constexpr StyleColor accent_yellow_hover{0.8035714030265808, 0.6808851957321167, 0.19014044106006622, 1.0};
inline constexpr StyleColor accent_yellow_outline{0.40531763434410095, 0.35585054755210876, 0.15798227488994598, 1.0};
inline constexpr StyleColor accent_yellow_overlay{0.8999999761581421, 0.7625914812088013, 0.21295729279518127, 0.250980406999588};
inline constexpr StyleColor accent_yellow_pressed{0.75, 0.6354928612709045, 0.17746439576148987, 1.0};
inline constexpr StyleColor accent_yellow_selected{0.3202941119670868, 0.2859419882297516, 0.14853344857692719, 1.0};
inline constexpr StyleColor accent_yellow_subtle{0.25072941184043884, 0.2287440448999405, 0.14080259203910828, 1.0};
inline constexpr StyleColor accent_yellow_text{0.8999999761581421, 0.8038139939308167, 0.4190700948238373, 1.0};
inline constexpr StyleColor icon_accent_blue_pressed_darker{0.11683199554681778, 0.3773104250431061, 0.675000011920929, 1.0};
inline constexpr StyleColor icon_accent_blue_selected_darker{0.12081430107355118, 0.19895783066749573, 0.2882646918296814, 1.0};
inline constexpr StyleColor icon_accent_blue_subtle_darker{0.11848821491003036, 0.16850008070468903, 0.22565646469593048, 1.0};
inline constexpr StyleColor icon_accent_cyan_pressed_darker{0.1460273116827011, 0.5692054629325867, 0.675000011920929, 1.0};
inline constexpr StyleColor icon_accent_green_outline_darker{0.09960141777992249, 0.33734118938446045, 0.13150587677955627, 1.0};
inline constexpr StyleColor icon_accent_green_pressed_darker{0.0611470565199852, 0.6114705801010132, 0.13500000536441803, 1.0};
inline constexpr StyleColor icon_accent_green_selected_darker{0.10410883277654648, 0.26920586824417114, 0.12626470625400543, 1.0};
inline constexpr StyleColor icon_accent_green_subtle_darker{0.10779670625925064, 0.2134588360786438, 0.12197647243738174, 1.0};
inline constexpr StyleColor icon_accent_orange_pressed_darker{0.675000011920929, 0.35710906982421875, 0.15950120985507965, 1.0};
inline constexpr StyleColor icon_accent_orange_selected_darker{0.2882646918296814, 0.19289742410182953, 0.13361506164073944, 1.0};
inline constexpr StyleColor icon_accent_orange_subtle_darker{0.22565646469593048, 0.16462141275405884, 0.12668070197105408, 1.0};
inline constexpr StyleColor icon_accent_orange_text_darker{0.8100000023841858, 0.5429716110229492, 0.3769810199737549, 1.0};
inline constexpr StyleColor icon_accent_purple_pressed_darker{0.3879602253437042, 0.24444030225276947, 0.675000011920929, 1.0};
inline constexpr StyleColor icon_accent_purple_selected_darker{0.2021527737379074, 0.15909680724143982, 0.2882646918296814, 1.0};
inline constexpr StyleColor icon_accent_purple_subtle_darker{0.17054483294487, 0.14298900961875916, 0.22565646469593048, 1.0};
inline constexpr StyleColor icon_accent_purple_text_darker{0.5688865780830383, 0.4483298361301422, 0.8100000023841858, 1.0};
inline constexpr StyleColor icon_accent_red_disabled_darker{0.8100000023841858, 0.2822663187980652, 0.2822663187980652, 0.30000001192092896};
inline constexpr StyleColor icon_accent_red_pressed_darker{0.675000011920929, 0.23522193729877472, 0.23522193729877472, 1.0};
inline constexpr StyleColor icon_accent_red_selected_darker{0.2882646918296814, 0.15633128583431244, 0.15633128583431244, 1.0};
inline constexpr StyleColor icon_accent_red_subtle_darker{0.22565646469593048, 0.1412190943956375, 0.1412190943956375, 1.0};
inline constexpr StyleColor icon_accent_red_text_darker{0.8100000023841858, 0.4405864179134369, 0.4405864179134369, 1.0};
inline constexpr StyleColor icon_accent_rose_pressed_darker{0.6584697961807251, 0.2277698665857315, 0.335444837808609, 1.0};
inline constexpr StyleColor icon_accent_rose_selected_darker{0.28330564498901367, 0.15409566462039948, 0.18639816343784332, 1.0};
inline constexpr StyleColor icon_accent_rose_subtle_darker{0.22248268127441406, 0.13978829979896545, 0.160461887717247, 1.0};
inline constexpr StyleColor icon_accent_teal_pressed_darker{0.058426134288311005, 0.5316778421401978, 0.5842613577842712, 1.0};
inline constexpr StyleColor icon_accent_yellow_pressed_darker{0.675000011920929, 0.5719435811042786, 0.15971796214580536, 1.0};
inline constexpr StyleColor icon_accent_yellow_selected_darker{0.2882646918296814, 0.2573477625846863, 0.13368010520935059, 1.0};
inline constexpr StyleColor icon_accent_yellow_subtle_darker{0.22565646469593048, 0.2058696299791336, 0.1267223209142685, 1.0};
inline constexpr StyleColor icon_accent_yellow_text_darker{0.8100000023841858, 0.7234326004981995, 0.3771630823612213, 1.0};
inline constexpr StyleColor icon_search{0.8078431487083435, 0.8156862854957581, 0.8392156958580017, 1.0};
inline constexpr StyleColor panel_canvas{0.06189476698637009, 0.06189476698637009, 0.06373938918113708, 1.0};
inline constexpr StyleColor panel_sub_surface{0.12038286030292511, 0.12213718146085739, 0.12389150261878967, 1.0};
inline constexpr StyleColor panel_surface_alt{0.15562701225280762, 0.15847958624362946, 0.1613321751356125, 1.0};
inline constexpr StyleColor panel_surface_control{0.18747973442077637, 0.192999929189682, 0.192999929189682, 1.0};
inline constexpr StyleColor panel_surface_ghost{0.0696646124124527, 0.06979759782552719, 0.07046254724264145, 1.0};
inline constexpr StyleColor panel_surface_input{0.19681522250175476, 0.1986967921257019, 0.20057833194732666, 1.0};
inline constexpr StyleColor panel_surface_input_hover{0.2218484729528427, 0.23012775182724, 0.23288752138614655, 1.0};
inline constexpr StyleColor panel_surface_interactive{0.17858824133872986, 0.17858824133872986, 0.17858824133872986, 1.0};
inline constexpr StyleColor panel_surface_panel{0.17342455685138702, 0.17747892439365387, 0.1795060932636261, 1.0};
inline constexpr StyleColor panel_surface_raised{0.24484941363334656, 0.25022611021995544, 0.2529144883155823, 1.0};
inline constexpr StyleColor panel_surface_solid{0.14418363571166992, 0.1475532352924347, 0.1475532352924347, 1.0};
inline constexpr StyleColor panel_window{0.09921568632125854, 0.09921568632125854, 0.09921568632125854, 1.0};
inline constexpr StyleColor panel_window_alt{0.1061861664056778, 0.1061861664056778, 0.1091860681772232, 1.0};
inline constexpr StyleColor text_body_color{1.0, 1.0, 1.0, 0.800000011920929};
inline constexpr StyleColor text_disabled_color{1.0, 1.0, 1.0, 0.30000001192092896};
inline constexpr StyleColor text_title_header_color{1.0, 1.0, 1.0, 0.8999999761581421};
} // namespace figma_defaults
// END GENERATED FIGMA COLOR DEFAULTS
using StyleTokenValue = std::variant<double, StyleColor, std::string, bool>;
enum class StyleTokenType { number, color, string, boolean };
struct StyleTokenAlias final {
    std::string id;
};
struct StyleTokenModeValue final {
    std::string mode_id;
    std::variant<StyleTokenValue, StyleTokenAlias> value;
};
struct StyleTokenMode final {
    std::string id, name;
};
struct StyleTokenCollection final {
    std::string id, key, name, default_mode_id;
    std::vector<StyleTokenMode> modes;
    bool hidden = false;
};
struct StyleToken final {
    std::string id, key, name, collection_id, css_name, description;
    StyleTokenType type;
    std::vector<StyleTokenModeValue> values;
    std::vector<std::string> scopes;
    bool hidden = false;
    std::string selector{}; // Color/Metric member; empty for non-style scalar tokens.
};
struct StyleTokenOptions final {
    // Collection name/id -> mode name/id. Unselected collections use their own
    // default.
    std::map<std::string, std::string, std::less<>> modes;
    // CSS name, Figma id/key, or Collection/Name -> typed construction override.
    std::map<std::string, StyleTokenValue, std::less<>> overrides;
    // App-specific Color/Metric members. Builtin changes belong in overrides.
    std::map<std::string, StyleTokenValue, std::less<>> members;
};
enum class StyleTokenErrorCode {
    unknown_token,
    unknown_mode,
    wrong_type,
    alias_cycle,
    invalid_value,
    invalid_syntax
};
struct StyleTokenError final {
    StyleTokenErrorCode code;
    std::string message;
};
template <class T> using StyleTokenResult = std::expected<T, StyleTokenError>;
class StyleTokenCatalog final {
  public:
    StyleTokenCatalog(std::vector<StyleTokenCollection>, std::vector<StyleToken>);
    [[nodiscard]] std::span<const StyleTokenCollection> collections() const noexcept { return collections_; }
    [[nodiscard]] std::span<const StyleToken> tokens() const noexcept { return tokens_; }
    [[nodiscard]] const StyleToken *find(std::string_view) const noexcept;
    [[nodiscard]] StyleTokenResult<void> validate(const StyleTokenOptions & = {}) const;
    [[nodiscard]] StyleTokenResult<StyleTokenValue> resolve(std::string_view,
                                                            const StyleTokenOptions & = {}) const;
    [[nodiscard]] StyleTokenResult<StyleTokenValue> select(std::string_view,
                                                           const StyleTokenOptions & = {}) const;
    [[nodiscard]] StyleTokenResult<double> number(std::string_view, const StyleTokenOptions & = {}) const;
    [[nodiscard]] StyleTokenResult<StyleColor> color(std::string_view, const StyleTokenOptions & = {}) const;
    [[nodiscard]] StyleTokenResult<std::string> string(std::string_view,
                                                       const StyleTokenOptions & = {}) const;
    [[nodiscard]] StyleTokenResult<bool> boolean(std::string_view, const StyleTokenOptions & = {}) const;
    // A non-var value returns nullopt. Unknown names may use a typed CSS
    // fallback.
    [[nodiscard]] StyleTokenResult<std::optional<StyleTokenValue>>
    css_value(std::string_view, const StyleTokenOptions & = {}) const;
    [[nodiscard]] StyleTokenResult<std::optional<StyleTokenValue>>
    style_value(std::string_view, StyleTokenType context, const StyleTokenOptions & = {}) const;

  private:
    std::vector<StyleTokenCollection> collections_;
    std::vector<StyleToken> tokens_;
    std::map<std::string, std::size_t, std::less<>> names_;
    StyleTokenResult<void> validate_options(const StyleTokenOptions &) const;
    StyleTokenResult<StyleTokenValue> resolve_impl(const StyleToken &, const StyleTokenOptions &,
                                                   std::vector<std::string_view> &) const;
    StyleTokenResult<std::optional<StyleTokenValue>> css_impl(std::string_view, const StyleTokenOptions &,
                                                              unsigned) const;
};
// Checked-in Figma snapshot, no LVRS/Qt/filesystem/network dependency.
[[nodiscard]] const StyleTokenCatalog &default_style_tokens();
[[nodiscard]] StyleTokenType style_token_type(const StyleTokenValue &) noexcept;
[[nodiscard]] bool valid_style_token_value(const StyleTokenValue &) noexcept;
[[nodiscard]] StyleColor composite_srgb(StyleColor foreground, StyleColor background);
[[nodiscard]] StyleTokenResult<StyleColor> parse_style_color(std::string_view);
} // namespace lvui
