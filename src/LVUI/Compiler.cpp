#include "Compiler.h"
#include "Language.h"
#include <iiSDKCore/Core/Text/Number.h>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <array>

namespace lvui {
namespace {
[[noreturn]] void error(SourcePosition at, std::string message) {
    throw Diagnostic{at.offset, at.line, at.column, std::move(message)};
}
bool identifier(std::string_view name) {
    constexpr std::string_view forbidden =
        " alignas alignof and and_eq asm auto bitand bitor bool break case catch char char8_t char16_t char32_t class "
        "compl concept const consteval constexpr constinit const_cast continue co_await co_return co_yield decltype "
        "default delete do double dynamic_cast else enum explicit export extern false float for friend goto if inline "
        "int long mutable namespace new noexcept not not_eq nullptr operator or or_eq private protected public register "
        "reinterpret_cast requires return short signed sizeof static static_assert static_cast struct switch template "
        "this thread_local throw true try typedef typeid typename union unsigned using virtual void volatile wchar_t while xor xor_eq ";
    if (name.empty() || name.front() == '_' || forbidden.find(" " + std::string(name) + " ") != forbidden.npos) return false;
    const auto alpha = [](char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_'; };
    return alpha(name.front()) && std::ranges::all_of(name, [&](char c) { return alpha(c) || (c >= '0' && c <= '9'); }) &&
           name.find("__") == name.npos;
}
std::string qualified(std::string_view name, SourcePosition at) {
    std::string result;
    while (!name.empty()) {
        const auto dot = name.find('.');
        auto piece = name.substr(0, dot);
        if (!identifier(piece)) error(at, "Invalid or reserved C++ identifier in backend type");
        result += "::"; result += piece;
        if (dot == name.npos) break;
        name.remove_prefix(dot + 1);
    }
    return result;
}
std::string literal(std::string_view value) {
    std::string out = "\"";
    for (unsigned char c : value) {
        if (c == '\\' || c == '"') { out += '\\'; out += char(c); }
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if (c >= 32 && c <= 126) out += char(c);
        else { out += '\\'; out += char('0' + (c >> 6)); out += char('0' + ((c >> 3) & 7)); out += char('0' + (c & 7)); }
    }
    return out + '"';
}
double number(const Value& value, SourcePosition at) {
    auto result = ii::core::parse_decimal(value.text);
    if (value.kind != ValueKind::number || !result)
        error(at, "Expected a finite Number");
    return *result;
}
std::string decimal(double value) {
    return ii::core::format_decimal(value);
}
class Generator final {
    const ViewDocument& document;
    const NativeCompileOptions& options;
    std::map<std::string, Value, std::less<>> constants;
    std::map<std::string, std::size_t, std::less<>> backends;
    std::map<std::string,std::string,std::less<>> reactive;
    std::map<std::string,std::string,std::less<>> variable_types;
    std::set<std::string, std::less<>> ids;
    std::map<std::string,std::string,std::less<>> widget_variables;
    std::map<std::string,std::string,std::less<>> toolbar_parents;
    struct ContextBinding final {std::string menu,target;SourcePosition position;};
    std::vector<ContextBinding> context_bindings;
    std::vector<std::string> initial_open;
    std::size_t factory_depth=0;
    std::string body;
    std::size_t next = 0;
    using Inputs = std::map<std::string, const Input*, std::less<>>;

    std::string property(const Value& value)const {
        if(value.kind!=ValueKind::reference || constants.contains(value.text))return {};
        if(auto found=reactive.find(value.text);found!=reactive.end())return found->second;
        const auto dot=value.text.find('.');if(dot==value.text.npos)return {};
        auto backend=backends.find(value.text.substr(0,dot));const auto member=value.text.substr(dot+1);
        if(backend==backends.end()||!identifier(member))return {};
        return "scope->backend_"+std::to_string(backend->second)+"."+member;
    }
    static std::string binding_owner(){
        return "            auto bindings = std::make_shared<ii::gui::BindingScope>();\n            if (auto owned_bindings = widgets.retain(bindings); !owned_bindings) return owned_bindings;\n";
    }

    Value resolve(const Value& value, SourcePosition at) const {
        if (value.kind != ValueKind::reference) return value;
        if (auto it = constants.find(value.text); it != constants.end()) return it->second;
        if (value.text.find('.') != value.text.npos) error(at, "Reactive C++ property bindings are not supported by native AOT yet");
        error(at, "Unknown value '" + value.text + "'");
    }
    Inputs inputs(const View& view, std::initializer_list<std::string_view> allowed, bool action = false) const {
        Inputs out;
        for (const auto& input : view.inputs) {
            auto name = input.name;
            if (name.empty() && action) name = "onClick";
            const bool common=(name=="visible"||name=="flex")&&view.component->spelling!="ApplicationWindow"&&view.component->spelling!="Window";
            if (std::ranges::find(allowed, name) == allowed.end() && !common) error(input.position, "Unknown input '" + name + "' for " + std::string(view.component->spelling));
            if (!out.emplace(name, &input).second) error(input.position, "Duplicate component input '" + name + "'");
        }
        if (!view.bindings.empty() || !view.imports.empty()) error(view.position, "Nested declarations are not supported by native AOT yet");
        return out;
    }
    std::string text(const Inputs& values, std::string_view name, std::string fallback = {}) const {
        auto it = values.find(name);
        if (it == values.end()) return fallback;
        auto value = resolve(it->second->value, it->second->position);
        if (value.kind != ValueKind::string) error(it->second->position, "Expected String for '" + std::string(name) + "'");
        return value.text;
    }
    double numeric(const Inputs& values, std::string_view name, double fallback, double low, double high) const {
        auto it = values.find(name);
        if (it == values.end()) return fallback;
        auto value = resolve(it->second->value, it->second->position);
        auto result = number(value, it->second->position);
        if (result < low || result > high) error(it->second->position, "Number outside supported range for '" + std::string(name) + "'");
        return result;
    }
    std::string boolean(const Inputs& values, std::string_view name, bool fallback) const {
        auto it = values.find(name);
        if (it == values.end()) return fallback ? "true" : "false";
        if(auto source=property(it->second->value);!source.empty()) {
            if(name!="enabled"&&name!="checked"&&name!="visible")error(it->second->position,"This Bool input has no native binding setter");
            return source+".get()";
        }
        auto value = resolve(it->second->value, it->second->position);
        if (value.kind != ValueKind::boolean) error(it->second->position, "Expected Bool for '" + std::string(name) + "'");
        return value.text;
    }
    std::string text_expression(const Inputs& values,std::string_view name,std::string fallback={})const {
        if(auto item=values.find(name);item!=values.end())if(auto source=property(item->second->value);!source.empty())return "ii::gui::binding_text("+source+".get())";
        return literal(text(values,name,std::move(fallback)));
    }
    std::string number_expression(const Inputs& values,std::string_view name,double fallback,double low,double high)const {
        if(auto item=values.find(name);item!=values.end())if(auto source=property(item->second->value);!source.empty())return source+".get()";
        return decimal(numeric(values,name,fallback,low,high));
    }
    void emit_bindings(const Inputs& values,const std::string& variable) {
        for(const auto& [name,input]:values){
            const auto source=property(input->value);if(source.empty())continue;
            std::string setter;
            if(name=="text")setter="text";else if(name=="enabled")setter="enabled";else if(name=="checked")setter="checked";
            else if(name=="visible")setter="visible";else if(name=="value"||name=="currentValue")setter="value";
            if(setter.empty())continue;
            body+="            if (auto binding = ii::gui::bind_"+setter+"(*bindings, *"+variable+", "+source+"); !binding) return binding;\n";
        }
        if(auto item=values.find("visible");item!=values.end()&&property(item->second->value).empty())
            body+="            if (auto visible = "+variable+"->set_visible("+boolean(values,"visible",true)+"); !visible) return visible;\n";
        if(values.contains("flex"))body+="            if (auto flex = "+variable+"->set_flex("+decimal(numeric(values,"flex",0,0,1024))+"); !flex) return flex;\n";
    }
    std::string scoped_member(const Inputs& values,std::string_view name) const {
        auto item=values.find(name);if(item==values.end())return "{}";
        const auto& input=*item->second;const auto& value=input.value;const auto dot=value.text.find('.');
        auto backend=backends.find(value.text.substr(0,dot));const auto member=dot==value.text.npos?std::string{}:value.text.substr(dot+1);
        if(value.kind!=ValueKind::reference||backend==backends.end()||!identifier(member))error(input.position,"Expected one public scoped C++ member for '"+std::string(name)+"'");
        return "scope->backend_"+std::to_string(backend->second)+"."+member;
    }
    std::string action(const Inputs& values, std::string_view name="onClick", std::string parameter={},std::string arguments="value") const {
        auto it = values.find(name);
        if (it == values.end()) return "{}";
        const auto& value = it->second->value;
        if (value.kind != ValueKind::reference) error(it->second->position, "Button requires a deferred method reference, not a call");
        const auto dot = value.text.find('.');
        const auto backend = backends.find(value.text.substr(0, dot));
        if (backend == backends.end()) error(it->second->position, "Unknown backend in button method reference");
        const auto method = dot == value.text.npos ? std::string_view{} : std::string_view(value.text).substr(dot + 1);
        if (!identifier(method)) error(it->second->position, "Expected one public C++ identifier as callback method");
        const auto args=parameter.empty()?std::string{}:arguments;
        return "[scope]" + (parameter.empty()?std::string{}:"("+parameter+")") + " { (void)scope->backend_" + std::to_string(backend->second) + "." + std::string(method) + "(" + args + "); }";
    }
    std::pair<std::string,double> text_role(const Inputs& values) const {
        const auto found=values.find("style");
        if (found==values.end()) return {"description",12};
        const auto& input=*found->second;
        constexpr std::pair<std::string_view,double> roles[]{
            {"title",26},{"title2",22},{"header",17},{"header2",15},{"body",13},{"description",12},{"caption",11},{"disabled",11}};
        for (const auto& [role,size]:roles)
            if (input.value.kind==ValueKind::enumeration && input.value.text=="."+std::string(role)) return {std::string(role),size};
        error(input.position,"Unknown LVRS text style");
    }
    std::string stepper_arrow(const Inputs& values) const {
        auto it=values.find("arrow");if(it==values.end()) return "up_down";
        const auto& v=it->second->value;
        if(v.kind==ValueKind::enumeration) {
            if(v.text==".upDown" || v.text==".UpDown") return "up_down";
            if(v.text==".up" || v.text==".Up") return "up";
            if(v.text==".down" || v.text==".Down") return "down";
        }
        error(it->second->position,"Unknown Stepper arrow");
    }
    std::string control_events(const Inputs& values) const {
        return "{.clicked = "+action(values)+", .pressed = "+action(values,"onPressed")+", .released = "+action(values,"onReleased")+", .canceled = "+action(values,"onCanceled")+"}";
    }
    std::string modal_content(const View& view,std::string_view factory_type="ModalContentFactory",bool isolated=true) {
        if(view.children.empty())return "{}";
        auto outer_body=std::move(body);auto outer_ids=std::move(ids);auto outer_variables=std::move(widget_variables);auto outer_bindings=std::move(context_bindings);
        body.clear();ids=isolated?std::set<std::string,std::less<>>{}:outer_ids;widget_variables.clear();context_bindings.clear();++factory_depth;
        for(const auto& child:view.children)emit(child,"content");
        for(const auto& binding:context_bindings){auto target=widget_variables.find(binding.target);if(target==widget_variables.end())error(binding.position,"Unknown context menu targetId in content scope");body+="            if (auto bound = "+target->second+"->set_context_menu(*"+binding.menu+"); !bound) return bound;\n";}
        auto children=std::move(body);--factory_depth;body=std::move(outer_body);if(isolated)ids=std::move(outer_ids);widget_variables=std::move(outer_variables);context_bindings=std::move(outer_bindings);
        return "lvrs::"+std::string(factory_type)+"{"+std::string(backends.empty()&&reactive.empty()?"[]":"[scope]")+"(ii::gui::WidgetBuilder& widgets, ii::gui::Widget content) -> ii::core::Result<void> {\n            lvrs::Widgets controls(widgets);\n"+binding_owner()+children+"            return {};\n        }}";
    }
    std::string widget_color(const Inputs& values,std::string_view name,std::string fallback) const {
        auto item=values.find(name);if(item==values.end())return fallback;const auto& input=*item->second;auto value=resolve(input.value,input.position);auto hex=std::string_view(value.text);if(hex.starts_with('#'))hex.remove_prefix(1);
        if(value.kind!=ValueKind::string||(hex.size()!=6&&hex.size()!=8))error(input.position,"Color requires a six/eight-digit RGB/RGBA HEX String");
        unsigned rgba[]{0,0,0,255};for(unsigned i=0;i<hex.size()/2;++i){auto parsed=std::from_chars(hex.data()+2*i,hex.data()+2*i+2,rgba[i],16);if(parsed.ec!=std::errc{}||parsed.ptr!=hex.data()+2*i+2)error(input.position,"Invalid color HEX");}
        return "ii::gui::WidgetColor{"+decimal(rgba[0]/255.)+", "+decimal(rgba[1]/255.)+", "+decimal(rgba[2]/255.)+", "+decimal(rgba[3]/255.)+"}";
    }
    std::string alias_key(const Inputs& values,std::string_view name,std::string_view alias)const{const auto found=values.find(alias);if(values.contains(name)&&found!=values.end())error(found->second->position,"Use a property or its alias, not both");return std::string(found!=values.end()?alias:name);}
    std::string button_method(const Inputs& values,std::string_view name)const{
        auto found=values.find(name);if(found==values.end())return "{}";const auto& input=*found->second;const auto& value=input.value;const auto dot=value.text.find('.');auto backend=backends.find(value.text.substr(0,dot));auto method=dot==value.text.npos?std::string{}:value.text.substr(dot+1);
        if(value.kind!=ValueKind::reference||backend==backends.end()||!identifier(method))error(input.position,"Button method requires one deferred scoped method reference");
        return "[scope](const lvrs::ButtonEvent& event) { []<class B>(B& backend, const lvrs::ButtonEvent& event) { if constexpr (requires { backend."+method+"(event); }) (void)backend."+method+"(event); else (void)backend."+method+"(); }(scope->backend_"+std::to_string(backend->second)+", event); }";
    }
    std::string link_value(const Inputs& values,std::string_view name,std::string fallback)const{
        auto found=values.find(name);if(found==values.end())return fallback;
        auto source=property(found->second->value);if(source.empty())error(found->second->position,"Link data input requires one scoped C++ member");return "ii::gui::binding_value("+source+")";
    }
    std::string link_text(const Inputs& values,std::string_view name,std::string fallback={})const{
        auto found=values.find(name);if(found!=values.end())if(auto source=property(found->second->value);!source.empty())return "ii::gui::binding_text(ii::gui::binding_value("+source+"))";return literal(text(values,name,std::move(fallback)));
    }
    std::string link_bool(const Inputs& values,std::string_view name,bool fallback)const{
        auto found=values.find(name);if(found!=values.end())if(auto source=property(found->second->value);!source.empty())return "ii::gui::binding_value("+source+")";return boolean(values,name,fallback);
    }
    std::string link_color(const Inputs& values,std::string_view name)const{
        auto found=values.find(name);if(found!=values.end())if(auto source=property(found->second->value);!source.empty())return "ii::gui::binding_value("+source+")";return widget_color(values,name,"std::nullopt");
    }
    std::string link_methods(const Inputs& values)const{
        auto found=values.find("methods");if(found==values.end())return "{}";const auto& input=*found->second;
        if(!property(input.value).empty())return link_value(values,"methods","{}");
        auto list=resolve(input.value,input.position);if(list.kind!=ValueKind::list||list.elements.size()>256)error(input.position,"Link methods require at most 256 deferred references");
        std::string result="{";for(const auto& value:list.elements){Input candidate{"method",value,input.position};if(result.size()>1)result+=", ";result+=button_method(Inputs{{"method",&candidate}},"method");}return result+"}";
    }
    void emit_link_bindings(const Inputs& values,const std::string& variable,const std::string& native){
        for(const auto& [name,input]:values){const auto source=property(input->value);if(source.empty())continue;
            std::string setter;
            if(name=="text"||name=="enabled"||name=="href"||name=="params"||name=="router"||name=="navigator"||name=="replace"||name=="underline"||name=="tone"||name=="methods")setter="set_"+name;
            else if(name=="to")setter="set_to";else if(name=="targetComponent")setter="set_target_component";else if(name=="linkColor")setter="set_link_color";else if(name=="hoverColor")setter="set_hover_color";else if(name=="pressedColor")setter="set_pressed_color";else if(name=="disabledColor")setter="set_disabled_color";else if(name=="visible")setter="root().set_visible";
            if(setter.empty())continue;
            body+="            if (auto binding = ii::gui::bind_member(*bindings, *"+variable+", "+source+", [link = *"+native+"](ii::gui::Widget, const auto& value) { return link."+setter+"(value); }); !binding) return binding;\n";
        }
        if(auto found=values.find("visible");found!=values.end()&&property(found->second->value).empty())body+="            if (auto visible = "+variable+"->set_visible("+boolean(values,"visible",true)+"); !visible) return visible;\n";
        if(values.contains("flex"))body+="            if (auto flex = "+variable+"->set_flex("+decimal(numeric(values,"flex",0,0,1024))+"); !flex) return flex;\n";
    }
    std::string surface_number(const Inputs& values,std::string_view name,std::string fallback)const{
        auto found=values.find(name);if(found==values.end())return fallback;
        if(auto source=property(found->second->value);!source.empty())return "ii::gui::binding_value("+source+")";
        return decimal(numeric(values,name,0,0,8192));
    }
    std::string surface_color(const Inputs& values,std::string_view name,std::string fallback)const{
        auto found=values.find(name);if(found!=values.end())if(auto source=property(found->second->value);!source.empty())return "ii::gui::binding_value("+source+")";
        return widget_color(values,name,std::move(fallback));
    }
    std::string surface_shape(const Inputs& values)const{
        auto found=values.find("shapeStyle");if(found==values.end())return "lvrs::AppCardShape::round_rect";
        if(!property(found->second->value).empty())return link_value(values,"shapeStyle","");
        auto value=resolve(found->second->value,found->second->position);
        if(value.kind==ValueKind::enumeration&&(value.text==".cylinder"||value.text==".shapeCylinder"))return "lvrs::AppCardShape::cylinder";
        if(value.kind==ValueKind::enumeration&&(value.text==".roundRect"||value.text==".shapeRoundRect"))return "lvrs::AppCardShape::round_rect";
        error(found->second->position,"Unknown AppCard shapeStyle");
    }
    void emit_surface_bindings(const Inputs& values,const std::string& variable,const std::string& native){
        const std::map<std::string,std::string,std::less<>> fields{{"title","title"},{"subtitle","subtitle"},{"enabled","enabled"},{"width","width"},{"height","height"},{"padding","padding"},{"spacing","spacing"},{"cornerRadius","corner_radius"},{"shapeStyle","shape"},{"backgroundColor","background"},{"titleColor","title_color"},{"subtitleColor","subtitle_color"},{"menuVisible","menu_visible"},{"compact","compact"},{"contentHorizontalPadding","content_horizontal_padding"},{"contentVerticalPadding","content_vertical_padding"},{"rowSpacing","row_spacing"},{"actionSpacing","action_spacing"},{"menuButtonPadding","menu_button_padding"},{"menuBackgroundColor","menu_background"},{"menuPressedBackgroundColor","menu_pressed_background"}};
        for(const auto& [name,input]:values){auto source=property(input->value);if(source.empty())continue;
            if(name=="visible")body+="            if (auto binding = ii::gui::bind_member(*bindings, *"+variable+", "+source+", [](ii::gui::Widget widget, const auto& value) { return widget.set_visible(value); }); !binding) return binding;\n";
            else if(auto field=fields.find(name);field!=fields.end())body+="            if (auto binding = ii::gui::bind_member(*bindings, *"+variable+", "+source+", [surface = *"+native+"](ii::gui::Widget, const auto& value) -> ii::core::Result<void> { auto options = surface.options(); if (!options) return std::unexpected(options.error()); options->"+field->second+" = "+(name=="title"||name=="subtitle"?"ii::gui::binding_text(value)":"value")+"; return surface.set_options(std::move(*options)); }); !binding) return binding;\n";
        }
        if(auto found=values.find("visible");found!=values.end()&&property(found->second->value).empty())body+="            if (auto visible = "+variable+"->set_visible("+boolean(values,"visible",true)+"); !visible) return visible;\n";
        if(values.contains("flex"))body+="            if (auto flex = "+variable+"->set_flex("+decimal(numeric(values,"flex",0,0,1024))+"); !flex) return flex;\n";
    }
    std::string toolbar_id(const Inputs& values,std::string_view name="buttonId")const{
        auto it=values.find(name);if(it==values.end())return "lvrs::ToolbarId{std::int64_t(-1)}";
        if(!property(it->second->value).empty())return link_value(values,name,"");
        auto value=resolve(it->second->value,it->second->position);
        if(value.kind==ValueKind::string)return "lvrs::ToolbarId{std::string("+literal(value.text)+")}";
        if(value.kind==ValueKind::number){auto n=numeric(values,name,0,-9007199254740991.,9007199254740991.);if(std::floor(n)==n)return "lvrs::ToolbarId{std::int64_t("+decimal(n)+")}";}
        error(it->second->position,"Toolbar identity requires a String, integer or scoped C++ ToolbarId");
    }
    std::string toolbar_tone(const Inputs& values)const{
        auto it=values.find("tone");if(it==values.end())return "ii::gui::ButtonTone::borderless";
        if(!property(it->second->value).empty())return link_value(values,"tone","");
        const std::map<std::string,std::string,std::less<>> tones{{".primary","primary"},{".default","normal"},{".borderless","borderless"},{".destructive","destructive"},{".disabled","disabled"}};
        auto selected=tones.find(it->second->value.text);if(it->second->value.kind!=ValueKind::enumeration||selected==tones.end())error(it->second->position,"Unknown toolbar button tone");return "ii::gui::ButtonTone::"+selected->second;
    }
    std::string toolbar_call(std::string_view kind,const std::string& args,const Inputs& v)const{
        const auto n=[&](std::string_view key,std::string value){return surface_number(v,key,std::move(value));};
        const auto b=[&](std::string_view key,bool value){return link_bool(v,key,value);};
        if(kind=="ToolbarButton")return "lvrs::ToolbarButton::create(widgets, "+args+", {.button_id = "+toolbar_id(v)+", .icon_name = "+link_text(v,"iconName")+", .accessibility_name = "+link_text(v,"accessibilityName")+", .text = "+link_text(v,"text")+", .icon = "+link_value(v,"icon","{}")+", .enabled = "+b("enabled",true)+", .visible = "+b("visible",true)+", .width = "+n("width","0")+", .height = "+n("height","22")+", .icon_size = "+n("iconSize","18")+", .left_padding = "+n("leftPadding","2")+", .right_padding = "+n("rightPadding","2")+", .corner_radius = "+n("cornerRadius","8")+", .tone = "+toolbar_tone(v)+"}, "+action(v)+")";
        if(kind=="ListToolbar")return "lvrs::ListToolbar::create(widgets, "+args+", {.icons = {"+link_text(v,"icon1")+", "+link_text(v,"icon2")+", "+link_text(v,"icon3")+"}, .icon_size = "+n("iconSize","18")+", .button_size = "+n("buttonSize","22")+", .horizontal_padding = "+n("horizontalPadding","8")+", .vertical_padding = "+n("verticalPadding","4")+", .spacing = "+n("spacing","4")+", .background = "+surface_color(v,"backgroundColor","ii::gui::WidgetColor{.0951643139,.0965511352,.0979379490,1}")+", .interactive = "+b("interactive",true)+", .enabled = "+b("enabled",true)+", .width = "+n("width","0")+", .height = "+n("height","0")+"}, {.icon_clicked = "+action(v,"onIconClicked","std::size_t index, std::string source","index, source")+"})";
        if(kind=="ListFooter")return "lvrs::ListFooter::create(widgets, "+args+", {.buttons = {"+("lvrs::footer_slot("+link_value(v,"button1","lvrs::ListFooterOptions{}.buttons[0]")+")")+", "+("lvrs::footer_slot("+link_value(v,"button2","lvrs::ListFooterOptions{}.buttons[1]")+")")+", "+("lvrs::footer_slot("+link_value(v,"button3","lvrs::ListFooterOptions{}.buttons[2]")+")")+"}, .horizontal_padding = "+n("horizontalPadding","2")+", .vertical_padding = "+n("verticalPadding","2")+", .spacing = "+n("spacing","0")+", .interactive = "+b("interactive",true)+", .enabled = "+b("enabled",true)+", .width = "+n("width","0")+", .height = "+n("height","0")+"}, {.button_clicked = "+action(v,"onButtonClicked","std::size_t index, const lvrs::ListFooterSlot& config","index, config")+"})";
        return "lvrs::HierarchyToolbar::create(widgets, "+args+", {.minimum_width = "+n("minimumToolbarWidth","200")+", .horizontal_padding = "+n("horizontalPadding","8")+", .vertical_padding = "+n("verticalPadding","2")+", .spacing = "+n("spacing","0")+", .slot_size = "+n("slotSize","22")+", .background = "+surface_color(v,"backgroundColor","ii::gui::WidgetColor{.0951643139,.0965511352,.0979379490,1}")+", .background_opacity = "+n("backgroundOpacity","0")+", .distribute_spacing = "+b("distributeSpacing",false)+", .enabled = "+b("enabled",true)+", .width = "+n("width","0")+", .height = "+n("height","0")+"}, "+("lvrs::toolbar_items("+link_value(v,"buttonItems","std::vector<lvrs::HierarchyToolbarEntry>{}")+")")+", {.active_changed = "+action(v,"onActiveChanged","const lvrs::ToolbarSelection& selection","selection")+", .button_triggered = "+action(v,"onButtonTriggered","const lvrs::ToolbarTrigger& context","context")+", .button_event_triggered = "+action(v,"onButtonEventTriggered","const lvrs::ToolbarEvent& event, const lvrs::ToolbarTrigger& context","event, context")+"})";
    }
    void emit_toolbar_bindings(const Inputs& values,const std::string& variable,const std::string& native,std::string_view kind){
        const std::map<std::string,std::string,std::less<>> fields{{"buttonId","button_id"},{"iconName","icon_name"},{"accessibilityName","accessibility_name"},{"text","text"},{"icon","icon"},{"enabled","enabled"},{"visible","visible"},{"width","width"},{"height","height"},{"iconSize","icon_size"},{"leftPadding","left_padding"},{"rightPadding","right_padding"},{"cornerRadius","corner_radius"},{"tone","tone"},{"buttonSize","button_size"},{"horizontalPadding","horizontal_padding"},{"verticalPadding","vertical_padding"},{"spacing","spacing"},{"backgroundColor","background"},{"interactive","interactive"},{"minimumToolbarWidth","minimum_width"},{"slotSize","slot_size"},{"backgroundOpacity","background_opacity"},{"distributeSpacing","distribute_spacing"},{"icon1","icons[0]"},{"icon2","icons[1]"},{"icon3","icons[2]"},{"button1","buttons[0]"},{"button2","buttons[1]"},{"button3","buttons[2]"}};
        for(const auto& [name,input]:values){auto source=property(input->value);if(source.empty())continue;std::string update;
            if(name=="visible"&&kind!="ToolbarButton")update="return widget.set_visible(value);";
            else if(name=="buttonItems")update="return component.set_items(lvrs::toolbar_items(value));";
            else if(name=="activeButtonId")update="return component.set_active_id(value);";
            else if(auto field=fields.find(name);field!=fields.end()){const auto text=name=="iconName"||name=="text"||name=="accessibilityName"||name=="icon1"||name=="icon2"||name=="icon3";update="auto options = component.options(); if (!options) return std::unexpected(options.error()); options->"+field->second+" = "+(text?"ii::gui::binding_text(value)":(name=="button1"||name=="button2"||name=="button3")?"lvrs::footer_slot(value)":"value")+"; return component.set_options(std::move(*options));";}
            if(!update.empty())body+="            if (auto binding = ii::gui::bind_member(*bindings, *"+variable+", "+source+", [component = *"+native+"](ii::gui::Widget widget, const auto& value) -> ii::core::Result<void> { (void)widget; "+update+" }); !binding) return binding;\n";
        }
        if(auto it=values.find("activeButtonId");it!=values.end()&&property(it->second->value).empty())body+="            if (auto active = "+native+"->set_active_id("+toolbar_id(values,"activeButtonId")+"); !active) return active;\n";
        if(auto it=values.find("visible");it!=values.end()&&property(it->second->value).empty())body+="            if (auto visible = "+variable+"->set_visible("+boolean(values,"visible",true)+"); !visible) return visible;\n";
        if(values.contains("flex"))body+="            if (auto flex = "+variable+"->set_flex("+decimal(numeric(values,"flex",0,0,1024))+"); !flex) return flex;\n";
    }
    std::string table_call(std::string_view kind,const std::string& args,const Inputs& v)const{
        const auto n=[&](std::string_view key,double fallback,double low=0.){return decimal(numeric(v,key,fallback,low,8192));};
        const auto b=[&](std::string_view key,bool fallback){return boolean(v,key,fallback);};
        const auto c=[&](std::string_view key,std::string fallback){return widget_color(v,key,std::move(fallback));};
        const std::string body="ii::gui::WidgetColor{1,1,1,.8}",divider="ii::gui::WidgetColor{.1555851549,.1570725590,.1585599482,1}";
        if(kind=="Table"){
            std::string selection="range";
            if(auto it=v.find("selectionMode");it!=v.end()){
                const auto value=resolve(it->second->value,it->second->position);
                if(value.kind!=ValueKind::enumeration)error(it->second->position,"Table selectionMode requires an enum");
                if(value.text==".NoSelection"||value.text==".none")selection="none";
                else if(value.text==".SingleCellSelection"||value.text==".singleCell")selection="single_cell";
                else if(value.text!=".RangeSelection"&&value.text!=".range")error(it->second->position,"Unknown Table selectionMode");
            }
            const auto gutter_y=b("structureControlsVisible",false)=="true"&&b("addColumnControlsVisible",true)=="true"?numeric(v,"structureGutterHeight",20,0,8192):0.;
            const auto gutter_x=b("structureControlsVisible",false)=="true"&&b("addRowControlsVisible",true)=="true"?numeric(v,"structureGutterWidth",20,0,8192):0.;
            if(numeric(v,"width",717,32,8192)-gutter_x<32)error(v.begin()->second->position,"Table structure gutter must leave a body width");
            if(numeric(v,"headerHeight",24,1,8192)+1>=numeric(v,"height",121,26,8192)-gutter_y)error(v.begin()->second->position,"Table headerHeight must leave a body viewport");
            return "controls.table("+args+", {.model = "+scoped_member(v,"model")+", .width = "+n("width",717,32)+", .height = "+n("height",121,26)+", .header_height = "+n("headerHeight",24,1)+", .content_spacing = "+n("contentSpacing",8)+", .border_width = "+n("borderWidth",1)+", .selection_enabled = "+b("selectionEnabled",true)+", .keyboard_navigation = "+b("keyboardNavigationEnabled",true)+", .sorting_enabled = "+b("sortingEnabled",false)+", .fit_columns_to_viewport = "+b("fitColumnsToViewport",true)+", .selection_mode = lvrs::TableSelectionMode::"+selection+", .background = "+c("backgroundColor","ii::gui::WidgetColor{30./255,30./255,30./255,1}")+", .divider = "+c("dividerColor",divider)+", .header_text = "+c("headerTextColor","ii::gui::WidgetColor{1,1,1,.6}")+", .cell_text = "+c("cellTextColor",body)+", .selection_color = "+c("selectionColor","std::nullopt")+", .current_color = "+c("currentCellBorderColor","std::nullopt")+", .resize_handles_visible = "+b("resizeHandlesVisible",true)+", .structure_controls_visible = "+b("structureControlsVisible",false)+", .add_row_controls_visible = "+b("addRowControlsVisible",true)+", .add_column_controls_visible = "+b("addColumnControlsVisible",true)+", .delete_context_menu_enabled = "+b("deleteContextMenuEnabled",true)+", .select_on_context_click = "+b("selectOnContextClick",true)+", .clipboard_enabled = "+b("clipboardEnabled",true)+", .column_resize_handle_width = "+n("columnResizeHandleWidth",6)+", .row_resize_handle_height = "+n("rowResizeHandleHeight",6)+", .structure_gutter_width = "+n("structureGutterWidth",20)+", .structure_gutter_height = "+n("structureGutterHeight",20)+", .resize_handle_color = "+c("resizeHandleColor","ii::gui::WidgetColor{0,0,0,0}")+", .resize_handle_hover_color = "+c("resizeHandleHoverColor","std::nullopt")+"}, {.edited = "+action(v,"onCellInputEdited","std::size_t row, std::size_t column, std::string value","row, column, value")+", .submitted = "+action(v,"onCellInputSubmitted","std::size_t row, std::size_t column, std::string value","row, column, value")+", .rejected = "+action(v,"onCellInputRejected","std::size_t row, std::size_t column, std::string value, std::string valueType","row, column, value, valueType")+", .selection_changed = "+action(v,"onSelectionChanged","std::optional<lvrs::TableRange> range","range")+", .activated = "+action(v,"onCellActivated","std::size_t row, std::size_t column","row, column")+", .sorted = "+action(v,"onRowsSorted","std::size_t column, lvrs::TableSortOrder order","column, order")+", .range_values_changed = "+action(v,"onRangeValuesChanged","lvrs::TableRange range","range")+", .column_resized = "+action(v,"onColumnResized","std::size_t column, double width","column, width")+", .row_resized = "+action(v,"onRowResized","std::size_t row, double height","row, height")+"})";
        }
        if(kind=="TableCellItem"){
            auto styles=v;if(auto it=v.find("textStyle");it!=v.end())styles["style"]=it->second;
            const auto role=v.contains("textStyle")?text_role(styles).first:std::string("body");
            return "controls.table_cell_item("+args+", {.text = "+literal(text(v,"text","Text"))+", .width = "+n("width",234,1)+", .cell_height = "+n("cellHeight",24,1)+", .content_spacing = "+n("contentSpacing",8)+", .divider_color = "+c("dividerColor","ii::gui::WidgetColor{.0839416310,.0839416310,.0863130987,1}")+", .text_color = "+c("textColor",body)+", .show_divider = "+b("showDivider",true)+", .clip_content = "+b("clipContent",true)+", .text_style = ii::gui::TextRole::"+role+", .inputable = "+b("inputable",false)+", .enabled = "+b("enabled",true)+", .selected = "+b("selected",false)+", .current = "+b("current",false)+", .selection_color = "+c("selectionColor","std::nullopt")+", .current_border_color = "+c("currentBorderColor","std::nullopt")+", .current_border_width = "+n("currentBorderWidth",1)+", .value_type = "+literal(text(v,"valueType","string"))+", .validator = "+scoped_member(v,"valueValidator")+"}, {.edited = "+action(v,"onInputEdited","std::string value")+", .submitted = "+action(v,"onInputSubmitted","std::string value")+", .rejected = "+action(v,"onInputRejected","std::string value, std::string valueType","value, valueType")+"})";
        }
        const auto strings=[&](std::string_view key,std::string type,bool nonempty){
            auto it=v.find(key);if(it==v.end())return std::string{};auto list=resolve(it->second->value,it->second->position);
            if(list.kind!=ValueKind::list||list.elements.size()>512||(nonempty&&list.elements.empty()))error(it->second->position,"Table items require a bounded String list");
            std::string result;for(const auto& item:list.elements){auto value=resolve(item,it->second->position);if(value.kind!=ValueKind::string)error(it->second->position,"Table items require Strings");if(!result.empty())result+=", ";result+=type+"{.text = "+literal(value.text)+"}";}return ", ."+std::string(key)+" = {"+result+"}";
        };
        if(kind=="TableRow")return "controls.table_row("+args+", {.width = "+n("width",717,1)+", .cell_width = "+n("cellWidth",234,1)+", .cell_height = "+n("cellHeight",24,1)+", .content_spacing = "+n("contentSpacing",8)+", .inputable = "+b("inputable",false)+", .enabled = "+b("enabled",true)+", .divider_color = "+c("dividerColor",divider)+", .text_color = "+c("textColor",body)+strings("cells","lvrs::TableCellOptions",true)+"}, {.edited = "+action(v,"onCellInputEdited","std::size_t index, std::string value","index, value")+", .submitted = "+action(v,"onCellInputSubmitted","std::size_t index, std::string value","index, value")+"})";
        std::string widths;if(auto it=v.find("columnWidths");it!=v.end()){
            auto list=resolve(it->second->value,it->second->position);if(list.kind!=ValueKind::list||list.elements.size()>512)error(it->second->position,"TableHeader columnWidths require at most 512 Numbers");
            for(const auto& item:list.elements){auto value=number(resolve(item,it->second->position),it->second->position);if(std::abs(value)>8192)error(it->second->position,"Column width outside supported range");if(!widths.empty())widths+=", ";widths+=decimal(value);}widths=", .column_widths = {"+widths+"}";
        }
        return "controls.table_header("+args+", {.layout = {.width = "+n("width",717,1)+", .row_height = "+n("rowHeight",24,1)+", .horizontal_padding = "+n("cellHorizontalPadding",8)+", .fallback_cell_width = "+n("fallbackCellWidth",0)+", .minimum_column_width = "+n("minColumnWidth",32,1)+widths+strings("columns","lvrs::TableHeaderColumn",false)+"}, .interactive = "+b("interactive",false)+", .enabled = "+b("enabled",true)+", .separator_height = "+n("separatorHeight",1)+", .text_color = "+c("textColor","ii::gui::WidgetColor{1,1,1,.6}")+", .separator_color = "+c("separatorColor",divider)+"}, {.column_clicked = "+action(v,"onColumnClicked","std::size_t index, const lvrs::TableHeaderColumn& column","index, column")+"})";
    }
    void emit(const View& view, const std::string& parent,const Inputs* menu_parent=nullptr) {
        const auto kind=view.component->spelling;
        const bool stack=kind=="VStack" || kind=="HStack" || kind=="ZStack";
        const bool icon_button=kind=="IconButton" || kind=="IconMenuButton";
        const bool dropdown=kind=="DropdownButton" || kind=="IconMenuButton" || kind=="LabelMenuButton";
        const bool button=kind=="PushButton" || kind=="LabelButton" || kind=="AbstractButton" || icon_button || dropdown || kind=="HelpButton" || kind=="ListItem";
        const bool toolbar=kind=="ToolbarButton"||kind=="HierarchyToolbar"||kind=="ListToolbar"||kind=="ListFooter";
        const bool list=kind=="List"||kind=="Hierarchy",card=kind=="AppCard",header=kind=="AppHeader",listener=kind=="EventListener",router=kind=="PageRouter",link=kind=="Link";
        if((toolbar||list||card||header||listener||router||link||kind=="ListItem")&&options.framework!="LVRS")error(view.position,"This LVRS component requires FRAMEWORK LVRS");
        const bool tabbar=kind=="TabBar",tab=kind=="Tab";
        const bool combo=kind=="ComboBox",stepper=kind=="Stepper",segmented=kind=="LabelSegmentedControl" || kind=="IconSegmentedControl";
        const bool menu=kind=="Menu" || kind=="ContextMenu",menu_item=kind=="MenuItem" || kind=="ContextMenuItem",divider=kind=="MenuDivider" || kind=="ContextMenuDivider";
        const bool color_surface=kind=="ColorPickerSurface" || kind=="ColorField";
        const bool color_picker=kind=="ColorPicker";
        const bool color_button=kind=="ColorPickerButton";
        const bool popover=kind=="Popover";
        const bool modal=kind=="Modal",sheet=kind=="Sheet";
        const bool table_cell=kind=="TableCellItem",table_row=kind=="TableRow",table_header=kind=="TableHeader",table_view=kind=="Table";
        if((table_cell||table_row||table_header||table_view)&&options.framework!="LVRS")error(view.position,"This LVRS component requires FRAMEWORK LVRS");
        const bool alert=kind=="Alert",alert_button=kind=="AlertButton";
        if((icon_button || dropdown || kind=="HelpButton" || tab || tabbar || combo || stepper || segmented || menu || menu_item || divider || color_surface || color_picker || color_button || popover || modal || sheet || alert || alert_button) && options.framework!="LVRS") error(view.position,"This LVRS component requires FRAMEWORK LVRS");
        const bool check=kind=="CheckBox" || kind=="RadioButton" || kind=="ToggleSwitch";
        Inputs values;
        if(stack) values=inputs(view,{"id","spacing","padding","alignment","width","height"});
        else if(link)values=inputs(view,{"id","text","href","to","params","router","navigator","targetComponent","replace","enabled","underline","tone","linkColor","hoverColor","pressedColor","disabledColor","horizontalPadding","verticalPadding","minimumHeight","cornerRadius","width","height","showFocusRing","accessibilityName","method","methods","onClick","onPressed","onReleased","onCanceled","onNavigated","onNavigationFailed"},true);
        else if(router){values=inputs(view,{"id","router","width","height"});if(!values.contains("router"))error(view.position,"PageRouter requires a scoped router member");}
        else if(kind=="ToolbarButton")values=inputs(view,{"id","buttonId","iconName","accessibilityName","text","icon","enabled","width","height","iconSize","leftPadding","rightPadding","cornerRadius","tone","onClick"},true);
        else if(kind=="ListToolbar")values=inputs(view,{"id","icon1","icon2","icon3","iconSize","buttonSize","horizontalPadding","verticalPadding","spacing","backgroundColor","interactive","enabled","width","height","onIconClicked"});
        else if(kind=="ListFooter")values=inputs(view,{"id","button1","button2","button3","horizontalPadding","verticalPadding","spacing","interactive","enabled","width","height","onButtonClicked"});
        else if(kind=="HierarchyToolbar")values=inputs(view,{"id","minimumToolbarWidth","horizontalPadding","verticalPadding","spacing","slotSize","backgroundColor","backgroundOpacity","distributeSpacing","enabled","width","height","buttonItems","activeButtonId","onActiveChanged","onButtonTriggered","onButtonEventTriggered"});
        else if(card)values=inputs(view,{"id","title","subtitle","enabled","width","height","padding","spacing","cornerRadius","shapeStyle","backgroundColor","titleColor","subtitleColor"});
        else if(header)values=inputs(view,{"id","title","subtitle","menuVisible","compact","enabled","contentHorizontalPadding","contentVerticalPadding","rowSpacing","actionSpacing","menuButtonPadding","width","height","backgroundColor","titleColor","subtitleColor","menuBackgroundColor","menuPressedBackgroundColor","onMenuClicked"});
        else if(listener)values=inputs(view,{"id","width","height","onInput","onResized"});
        else if(list)values=inputs(view,{"id","model","width","height","rowHeight","indent","backgroundColor","selectionColor","textColor","selectionRadius","fontSize","reorderable","editable","showChecks","onSelectionChanged","onItemActivated","onItemRenamed","onItemMoved","onItemChecked"});
        else if(table_view)values=inputs(view,{"id","model","width","height","headerHeight","contentSpacing","borderWidth","selectionEnabled","keyboardNavigationEnabled","sortingEnabled","fitColumnsToViewport","selectionMode","backgroundColor","dividerColor","headerTextColor","cellTextColor","selectionColor","currentCellBorderColor","onCellInputEdited","onCellInputSubmitted","onCellInputRejected","onSelectionChanged","onCellActivated","onRowsSorted","resizeHandlesVisible","structureControlsVisible","addRowControlsVisible","addColumnControlsVisible","deleteContextMenuEnabled","selectOnContextClick","clipboardEnabled","columnResizeHandleWidth","rowResizeHandleHeight","structureGutterWidth","structureGutterHeight","resizeHandleColor","resizeHandleHoverColor","onRangeValuesChanged","onColumnResized","onRowResized"});
        else if(table_cell)values=inputs(view,{"id","text","width","cellHeight","contentSpacing","dividerColor","textColor","showDivider","clipContent","textStyle","inputable","enabled","selected","current","selectionColor","currentBorderColor","currentBorderWidth","valueType","valueValidator","onInputEdited","onInputSubmitted","onInputRejected"});
        else if(table_row)values=inputs(view,{"id","cells","width","cellWidth","cellHeight","contentSpacing","inputable","enabled","dividerColor","textColor","onCellInputEdited","onCellInputSubmitted"});
        else if(table_header)values=inputs(view,{"id","columns","width","rowHeight","cellHorizontalPadding","columnWidths","fallbackCellWidth","minColumnWidth","textColor","separatorHeight","separatorColor","interactive","enabled","onColumnClicked"});
        else if(kind=="Spacer") values=inputs(view,{"id","minLength"});
        else if(kind=="Label") values=inputs(view,{"id","text","fontSize","fontWeight","lineHeight","color","selectable","style"});
        else if(button) values=inputs(view,{"id","text","enabled","tone","onClick","iconMode","iconName","iconSize","icon","height","horizontalPadding","cornerRadius","accessibilityName"},true);
        else if(tab) values=inputs(view,{"id","text","iconName","badge","enabled","selected","tabStyle","iconSize","minimumWidth","onClick"},true);
        else if(tabbar) values=inputs(view,{"id","spacing","minimumTabWidth","currentIndex","autoSelect","onCurrentIndexChanged"});
        else if(combo) values=inputs(view,{"id","text","enabled","tone","arrow","width","onClick","onPressed","onReleased","onCanceled"},true);
        else if(stepper) values=inputs(view,{"id","enabled","tone","arrow","cornerRadius","onStepped","onClick","onPressed","onReleased","onCanceled"},true);
        else if(segmented) values=inputs(view,{"id","horizontalPadding","verticalPadding","spacing","borderWidth","cornerRadius","shapeStyle","forceBorderlessTone"});
        else if(menu) values=inputs(view,{"id","items","targetId","compactItems","itemWidth","itemSpacing","leftPadding","rightPadding","topPadding","bottomPadding","edgeMargin","cornerRadius","menuOpacity","selectedIndex","autoCloseOnTrigger","dismissOnGlobalPress","dismissOnGlobalContextRequest","showIconSlot","onItemTriggered","onOpened","onClosed"});
        else if(menu_item) values=inputs(view,{"id","label","shortcut","key","keyPlaceholder","iconName","enabled","compact","state","selectionDirection","keyVisible","showIconSlot","showChevron","hasChildItems","expanded","itemWidth","itemHeight","iconSize","chevronSize","keepOpen","closeOnTrigger","onClick"},true);
        else if(divider) values=inputs(view,{"id","axis","thickness","crossPadding","linePadding","lineLength"});
        else if(check) values=inputs(view,{"id","text","enabled","checked","group","onToggled"});
        else if(kind=="InputField") values=inputs(view,{"id","text","placeholder","enabled","readOnly","width","height","onEdited","onSubmit","multiline","password"});
        else if(kind=="Slider") values=inputs(view,{"id","from","to","value","stepSize","width","enabled","thumbDiameter","trackHeight","rangeInset","height","onMoved"});
        else if(kind=="ProgressBar") values=inputs(view,{"id","minimumValue","maximumValue","startValue","currentValue","width","height"});
        else if(color_surface) values=inputs(view,{"id","model","field","width","height","enabled","accessibilityName","onColorEdited"});
        else if(color_picker) values=inputs(view,{"id","type","model","width","padding","enabled","showRecentColors","showActions","recentColors","applyText","cancelText","onColorEdited","onAccepted","onCanceled","onEyedropperRequested"});
        else if(color_button) values=inputs(view,{"id","model","currentColor","buttonSize","enabled","showHueRing","circular","borderOpacity","accessibilityName","onClick","onPressed","onReleased","onCanceled"},true);
        else if(popover) values=inputs(view,{"id","width","height","padding","margins","cornerRadius","surfaceColor","surfaceOpacity","modal","dim","focus","clip","closeOnEscape","closeOnPressOutside","accessibilityName","onOpened","onClosed"});
        else if(sheet) values=inputs(view,{"id","open","title","description","presentation","detent","cornerRadius","showHeader","showDescription","showCloseButton","showGrabber","scrollContent","preferredWidth","preferredHeight","desktopMargin","contentPadding","topSafeInset","bottomSafeInset","dismissOnBackground","dismissOnEscape","dismissOnDrag","motionEnabled","motion","animationDuration","surfaceColor","backdropColor","accessibilityName","onOpened","onClosed"});
        else if(modal) values=inputs(view,{"id","open","title","description","desc","message","iconName","iconSource","showIcon","iconSize","iconCornerRadius","iconBackgroundColor","iconBorderColor","buttonCount","primaryText","secondaryText","tertiaryText","primaryEnabled","secondaryEnabled","tertiaryEnabled","minWidth","maxWidth","preferredWidth","sidePadding","frameMinHeight","verticalOffset","frameCornerRadius","shapeStyle","frameColor","backdropColor","dismissOnBackground","motionEnabled","motion","accessibilityName","onPrimaryClicked","onSecondaryClicked","onTertiaryClicked","onCanceled","onOpened","onClosed"});
        else if(alert_button) values=inputs(view,{"id","text","enabled","tone","dialogStyle","height","horizontalPadding","cornerRadius","textColor","primaryColor","method","onClick","onPressed","onReleased","onCanceled"},true);
        else if(alert) values=inputs(view,{"id","open","title","message","description","buttonCount","primaryText","secondaryText","tertiaryText","button1Text","button2Text","button3Text","button1Method","button2Method","button3Method","primaryEnabled","secondaryEnabled","tertiaryEnabled","secondaryDestructive","dismissOnBackground","closeOnEscape","minWidth","maxWidth","preferredWidth","sidePadding","cardCornerRadius","shapeStyle","backdropColor","cardBackgroundColor","glassEnabled","glassBlurRadius","shadowEnabled","shadowColor","shadowBlurRadius","shadowOffsetX","shadowOffsetY","motionEnabled","motion","showIcon","appIconSource","imageSource","appIconSize","iconFrameSize","appIconBackgroundColor","appIconFrameColor","appIconInnerColor","primaryColor","accessibilityName","onPrimaryClicked","onSecondaryClicked","onTertiaryClicked","onDismissed","onOpened","onClosed"});
        else error(view.position,"Component '"+std::string(kind)+"' is not supported by native AOT yet");
        if(kind!="HierarchyToolbar" && !stack && !link && !card && !header && !tabbar && !segmented && !menu && !menu_item && !popover && !modal && !sheet && !view.children.empty()) error(view.position,"This C++ control cannot accept children");
        if(modal&&view.children.size()>1)error(view.position,"Modal accepts one content View; wrap children in a stack");
        if(popover){unsigned content=0;for(const auto& child:view.children)if(child.component->spelling!="Menu"&&child.component->spelling!="ContextMenu"&&child.component->spelling!="Popover")++content;if(content>1)error(view.position,"Popover accepts one content View; wrap children in a stack");}
        if(toolbar&&kind!="HierarchyToolbar"&&!view.children.empty())error(view.position,"This toolbar component has fixed native slots and accepts no child views");
        if(tabbar) for(const auto& child:view.children) if(child.component->spelling!="Tab") error(child.position,"TabBar accepts Tab children only");
        if(menu) for(const auto& child:view.children) if(child.component->spelling!="MenuItem" && child.component->spelling!="ContextMenuItem" && child.component->spelling!="MenuDivider" && child.component->spelling!="ContextMenuDivider" && child.component->spelling!="Menu" && child.component->spelling!="ContextMenu") error(child.position,"Menu accepts menu items, dividers and owned popups only");
        if(menu_item) for(const auto& child:view.children) if(child.component->spelling!="Menu" && child.component->spelling!="ContextMenu") error(child.position,"MenuItem children must be owned popups");
        if(next>=4096) error(view.position,"Native widget limit is 4096");
        const auto index=next++;
        const auto id=text(values,"id","@lvui/"+std::to_string(index));
        if(id.empty() || !ids.insert(id).second) error(view.position,"Duplicate widget id or empty id");
        const auto variable="node_"+std::to_string(index);
        widget_variables.emplace(id,variable);
        const auto api=options.framework=="LVRS"?"controls.":"widgets.";
        const auto prefix=std::string(api);
        const auto args=parent+", "+literal(id);
        std::string call;
        if(toolbar)call=toolbar_call(kind,args,values);
        else if(router){
            call=prefix+"page_router("+args+", "+scoped_member(values,"router")+", {"+decimal(numeric(values,"width",640,1,8192))+", "+decimal(numeric(values,"height",400,1,8192))+"})";
        } else
        if(link){
            const auto href=alias_key(values,"href","to");std::string tone="ii::gui::ButtonTone::normal";
            if(auto found=values.find("tone");found!=values.end()){
                if(!property(found->second->value).empty())tone=link_value(values,"tone","");
                else{const std::map<std::string,std::string,std::less<>> tones{{".primary","primary"},{".default","normal"},{".borderless","borderless"},{".destructive","destructive"},{".disabled","disabled"}};auto selected=tones.find(found->second->value.text);if(found->second->value.kind!=ValueKind::enumeration||selected==tones.end())error(found->second->position,"Unknown Link tone");tone="ii::gui::ButtonTone::"+selected->second;}
            }
            const auto optional_size=[&](std::string_view name){return values.contains(name)?decimal(numeric(values,name,0,0,8192)):"std::nullopt";};
            call="lvrs::Link::create(widgets, "+args+", {.text = "+link_text(values,"text")+", .href = "+link_text(values,href,"/")+", .params = "+link_value(values,"params","{}")+", .router = "+link_value(values,"router","{}")+", .navigator = "+link_value(values,"navigator","{}")+", .target_component = "+link_value(values,"targetComponent","{}")+", .replace = "+link_bool(values,"replace",false)+", .enabled = "+link_bool(values,"enabled",true)+", .underline = "+link_bool(values,"underline",false)+", .tone = "+tone+", .link_color = "+link_color(values,"linkColor")+", .hover_color = "+link_color(values,"hoverColor")+", .pressed_color = "+link_color(values,"pressedColor")+", .disabled_color = "+link_color(values,"disabledColor")+", .horizontal_padding = "+decimal(numeric(values,"horizontalPadding",14,0,8192))+", .vertical_padding = "+decimal(numeric(values,"verticalPadding",10,0,8192))+", .minimum_height = "+decimal(numeric(values,"minimumHeight",36,0,8192))+", .corner_radius = "+decimal(numeric(values,"cornerRadius",8,0,8192))+", .width = "+optional_size("width")+", .height = "+optional_size("height")+", .show_focus_ring = "+boolean(values,"showFocusRing",true)+", .accessibility_name = "+literal(text(values,"accessibilityName"))+"}, {.method = "+button_method(values,"method")+", .methods = "+link_methods(values)+", .clicked = "+action(values)+", .pressed = "+action(values,"onPressed")+", .released = "+action(values,"onReleased")+", .canceled = "+action(values,"onCanceled")+", .navigated = "+action(values,"onNavigated","bool accepted","accepted")+", .navigation_failed = "+action(values,"onNavigationFailed","const ii::core::Error& error","error")+"})";
        } else
        if(card)call="lvrs::AppCard::create(widgets, "+args+", {.width = "+surface_number(values,"width","0")+", .height = "+surface_number(values,"height","0")+", .padding = "+surface_number(values,"padding","18")+", .spacing = "+surface_number(values,"spacing","10")+", .corner_radius = "+surface_number(values,"cornerRadius","12")+", .background = "+surface_color(values,"backgroundColor","ii::gui::WidgetColor{.1139791608,.1166428775,.1166428775,1}")+", .title = "+link_text(values,"title")+", .subtitle = "+link_text(values,"subtitle")+", .shape = "+surface_shape(values)+", .enabled = "+link_bool(values,"enabled",true)+", .title_color = "+surface_color(values,"titleColor","ii::gui::WidgetColor{1,1,1,.9}")+", .subtitle_color = "+surface_color(values,"subtitleColor","ii::gui::WidgetColor{1,1,1,.8}")+"})";
        else if(header)call="lvrs::AppHeader::create(widgets, "+args+", {.title = "+link_text(values,"title")+", .subtitle = "+link_text(values,"subtitle")+", .menu_visible = "+link_bool(values,"menuVisible",false)+", .compact = "+link_bool(values,"compact",false)+", .enabled = "+link_bool(values,"enabled",true)+", .content_horizontal_padding = "+surface_number(values,"contentHorizontalPadding","std::nullopt")+", .content_vertical_padding = "+surface_number(values,"contentVerticalPadding","std::nullopt")+", .row_spacing = "+surface_number(values,"rowSpacing","std::nullopt")+", .action_spacing = "+surface_number(values,"actionSpacing","std::nullopt")+", .menu_button_padding = "+surface_number(values,"menuButtonPadding","std::nullopt")+", .width = "+surface_number(values,"width","0")+", .height = "+surface_number(values,"height","0")+", .background = "+surface_color(values,"backgroundColor","ii::gui::WidgetColor{.083941631,.083941631,.086313099,1}")+", .title_color = "+surface_color(values,"titleColor","ii::gui::WidgetColor{1,1,1,.9}")+", .subtitle_color = "+surface_color(values,"subtitleColor","ii::gui::WidgetColor{1,1,1,.8}")+", .menu_background = "+surface_color(values,"menuBackgroundColor","ii::gui::WidgetColor{.1139791608,.1166428775,.1166428775,1}")+", .menu_pressed_background = "+surface_color(values,"menuPressedBackgroundColor","ii::gui::WidgetColor{.1230252981,.1252803057,.1275353134,1}")+"}, {.menu_clicked = "+action(values,"onMenuClicked")+"})";
        else if(listener){
            std::string input="{}";if(values.contains("onInput"))input="[scope](const ii::gui::WidgetInput& value) -> ii::core::Result<bool> { return "+scoped_member(values,"onInput")+"(value); }";
            call=prefix+"event_listener("+args+", {"+decimal(numeric(values,"width",640,1,8192))+", "+decimal(numeric(values,"height",480,1,8192))+"}, "+input+", "+action(values,"onResized","ii::gui::Rect value")+")";
        }
        else if(list){
            call=prefix+(kind=="Hierarchy"?"hierarchy":"list")+"("+args+", {.model = "+scoped_member(values,"model")+", .width = "+decimal(numeric(values,"width",240,32,8192))+", .height = "+decimal(numeric(values,"height",400,24,8192))+", .row_height = "+decimal(numeric(values,"rowHeight",28,18,256))+", .indent = "+decimal(numeric(values,"indent",16,0,128))+", .hierarchy = "+std::string(kind=="Hierarchy"?"true":"false")+", .reorderable = "+boolean(values,"reorderable",true)+", .editable = "+boolean(values,"editable",true)+", .show_checks = "+boolean(values,"showChecks",false)+", .background = "+widget_color(values,"backgroundColor","ii::gui::WidgetColor{.1,.1,.1,1}")+", .selection_color = "+widget_color(values,"selectionColor","ii::gui::WidgetColor{.18,.31,.52,1}")+", .text_color = "+widget_color(values,"textColor","ii::gui::WidgetColor{.92,.92,.92,1}")+", .selection_radius = "+decimal(numeric(values,"selectionRadius",3,0,128))+", .font_size = "+decimal(numeric(values,"fontSize",13,1,256))+"}, {.selected = "+action(values,"onSelectionChanged","std::string value")+", .activated = "+action(values,"onItemActivated","std::string value")+", .renamed = "+action(values,"onItemRenamed","std::string key, std::string text","key, text")+", .moved = "+action(values,"onItemMoved","std::string key, std::string parent, std::string before","key, parent, before")+", .checked = "+action(values,"onItemChecked","std::string key, bool value","key, value")+"})";
        }
        else if(table_cell||table_row||table_header||table_view)call=table_call(kind,args,values);
        else if(alert_button){
            std::string tone="normal";if(auto item=values.find("tone");item!=values.end()){const auto& input=*item->second;const auto v=resolve(input.value,input.position);const std::map<std::string,std::string,std::less<>> tones{{".primary","primary"},{".default","normal"},{".borderless","borderless"},{".destructive","destructive"},{".disabled","disabled"}};auto found=tones.find(v.text);if(v.kind!=ValueKind::enumeration||found==tones.end())error(input.position,"Unknown AlertButton tone");tone=found->second;}
            std::string geometry;for(auto [name,field]:{std::pair{"height","height"},{"horizontalPadding","horizontal_padding"},{"cornerRadius","corner_radius"}})if(values.contains(name))geometry+=", ."+std::string(field)+" = "+decimal(numeric(values,name,0,std::string_view(name)=="height"?1:0,8192));
            call=prefix+"alert_button("+args+", {.text = "+literal(text(values,"text","Button"))+", .enabled = "+boolean(values,"enabled",true)+", .dialog_style = "+boolean(values,"dialogStyle",false)+", .tone = ii::gui::ButtonTone::"+tone+geometry+", .text_color = "+widget_color(values,"textColor","std::nullopt")+", .primary_color = "+widget_color(values,"primaryColor","std::nullopt")+"}, {.method = "+button_method(values,"method")+", .clicked = "+action(values)+", .pressed = "+action(values,"onPressed")+", .released = "+action(values,"onReleased")+", .canceled = "+action(values,"onCanceled")+"})";
        }
        else if(alert){
            const auto motion=scoped_member(values,"motion");
            const auto message_key=alias_key(values,"message","description"),first=alias_key(values,"primaryText","button1Text"),second=alias_key(values,"secondaryText","button2Text"),third=alias_key(values,"tertiaryText","button3Text"),image_key=alias_key(values,"appIconSource","imageSource");
            const auto minimum=numeric(values,"minWidth",280,0,8192),maximum=numeric(values,"maxWidth",500,1,8192),count=numeric(values,"buttonCount",0,-1e9,1e9);if(minimum>maximum)error(view.position,"Alert minWidth exceeds maxWidth");if(std::trunc(count)!=count)error(view.position,"Alert buttonCount requires an integer");
            std::string shape="round_rect";if(auto item=values.find("shapeStyle");item!=values.end()){auto value=resolve(item->second->value,item->second->position);if(value.kind==ValueKind::enumeration&&(value.text==".cylinder"||value.text==".shapeCylinder"))shape="cylinder";else if(value.kind!=ValueKind::enumeration||(value.text!=".roundRect"&&value.text!=".shapeRoundRect"))error(item->second->position,"Unknown Alert shapeStyle");}
            std::string image="{}",asset="std::nullopt",builtin="true";if(auto item=values.find(image_key);item!=values.end()){const auto& input=*item->second;const auto& value=input.value;if(value.kind==ValueKind::string){if(value.text.empty())builtin="false";else if(value.text=="alert-file-text.svg"||value.text=="qrc:/qt/qml/LVRS/resources/images/alert-file-text.svg")asset="lvrs::AlertIcon::file_text";else if(value.text=="alert-adjustments.svg"||value.text=="qrc:/qt/qml/LVRS/resources/images/alert-adjustments.svg")asset="lvrs::AlertIcon::adjustments";else error(input.position,"Native imageSource requires a scoped image or an embedded Alert asset");}else{const auto dot=value.text.find('.');auto backend=backends.find(value.text.substr(0,dot));auto member=dot==value.text.npos?std::string{}:value.text.substr(dot+1);if(value.kind!=ValueKind::reference||backend==backends.end()||!identifier(member))error(input.position,"Native imageSource requires one scoped image member");image="scope->backend_"+std::to_string(backend->second)+"."+member;builtin="false";}}
            if(boolean(values,"open",false)=="true"){if(factory_depth)error(view.position,"Initial open inside a content factory is not supported; open after activation");initial_open.push_back(id);}
            call=prefix+"alert("+args+", {.title = "+literal(text(values,"title","Alert Dialog"))+", .message = "+literal(text(values,message_key,"It can have 2 or 3 actions depending on your needs."))+", .button_count = "+decimal(count)+", .primary_text = "+literal(text(values,first,"Button"))+", .secondary_text = "+literal(text(values,second,"Button"))+", .tertiary_text = "+literal(text(values,third))+", .primary_enabled = "+boolean(values,"primaryEnabled",true)+", .secondary_enabled = "+boolean(values,"secondaryEnabled",true)+", .tertiary_enabled = "+boolean(values,"tertiaryEnabled",true)+", .secondary_destructive = "+(values.contains("secondaryDestructive")?boolean(values,"secondaryDestructive",false):"std::nullopt")+", .dismiss_on_background = "+boolean(values,"dismissOnBackground",false)+", .dismiss_on_escape = "+boolean(values,"closeOnEscape",false)+", .minimum_width = "+decimal(minimum)+", .maximum_width = "+decimal(maximum)+", .preferred_width = "+decimal(numeric(values,"preferredWidth",500,1,8192))+", .side_padding = "+decimal(numeric(values,"sidePadding",24,0,8192))+", .corner_radius = "+decimal(numeric(values,"cardCornerRadius",36,0,8192))+", .shape = lvrs::AlertShape::"+shape+", .backdrop_color = "+widget_color(values,"backdropColor","ii::gui::WidgetColor{0,0,0,89./255}")+", .card_color = "+widget_color(values,"cardBackgroundColor","ii::gui::WidgetColor{29./255,31./255,33./255,.72}")+", .glass_enabled = "+boolean(values,"glassEnabled",true)+", .glass_blur_radius = "+decimal(numeric(values,"glassBlurRadius",28,-8192,8192))+", .shadow_enabled = "+boolean(values,"shadowEnabled",true)+", .shadow = {.offset_x = "+decimal(numeric(values,"shadowOffsetX",0,-8192,8192))+", .offset_y = "+decimal(numeric(values,"shadowOffsetY",16,-8192,8192))+", .blur_radius = "+decimal(numeric(values,"shadowBlurRadius",32,0,64))+", .color = "+widget_color(values,"shadowColor","ii::gui::WidgetColor{0,0,0,.28}")+"}, .motion_enabled = "+boolean(values,"motionEnabled",true)+", .motion = "+motion+", .show_icon = "+boolean(values,"showIcon",true)+", .use_builtin_image = "+builtin+", .image_source = "+image+", .builtin_icon = "+asset+", .icon_size = "+(values.contains("appIconSize")?decimal(numeric(values,"appIconSize",64,1,8192)):"std::nullopt")+", .icon_frame_size = "+(values.contains("iconFrameSize")?decimal(numeric(values,"iconFrameSize",86,1,8192)):"std::nullopt")+", .icon_background = "+widget_color(values,"appIconBackgroundColor","std::nullopt")+", .icon_border = "+widget_color(values,"appIconFrameColor","std::nullopt")+", .primary_color = "+widget_color(values,"primaryColor","std::nullopt")+", .icon_inner = "+widget_color(values,"appIconInnerColor","ii::gui::WidgetColor{244./255,245./255,247./255,1}")+", .accessibility_name = "+literal(text(values,"accessibilityName","Alert"))+"}, {.button1_method = "+button_method(values,"button1Method")+", .button2_method = "+button_method(values,"button2Method")+", .button3_method = "+button_method(values,"button3Method")+", .primary_clicked = "+action(values,"onPrimaryClicked")+", .secondary_clicked = "+action(values,"onSecondaryClicked")+", .tertiary_clicked = "+action(values,"onTertiaryClicked")+", .dismissed = "+action(values,"onDismissed")+", .opened = "+action(values,"onOpened")+", .closed = "+action(values,"onClosed")+"})";
        }
        else if(sheet) {
            const auto choice=[&](std::string_view key,std::initializer_list<std::string_view> names,std::string fallback){auto found=values.find(key);if(found==values.end())return fallback;auto value=resolve(found->second->value,found->second->position);for(auto name:names){auto upper=std::string(name);upper[0]=char(upper[0]-'a'+'A');if(value.kind==ValueKind::enumeration&&(value.text=="."+std::string(name)||value.text=="."+upper))return std::string(name);}error(found->second->position,"Unknown Sheet "+std::string(key));};
            const auto duration=numeric(values,"animationDuration",420,0,86400000);if(duration!=std::trunc(duration))error(view.position,"Sheet animationDuration requires integral milliseconds");
            call=prefix+"sheet("+args+", {.title = "+literal(text(values,"title"))+", .description = "+literal(text(values,"description"))+", .presentation = lvrs::SheetPresentation::"+choice("presentation",{"automatic","mobile","desktop"},"automatic")+", .detent = lvrs::SheetDetent::"+choice("detent",{"fit","medium","large"},"fit")+", .corner_radius = "+(values.contains("cornerRadius")?decimal(numeric(values,"cornerRadius",16,0,8192)):"std::nullopt")+", .show_header = "+boolean(values,"showHeader",true)+", .show_description = "+boolean(values,"showDescription",true)+", .show_close_button = "+boolean(values,"showCloseButton",true)+", .show_grabber = "+(values.contains("showGrabber")?boolean(values,"showGrabber",false):"std::nullopt")+", .scroll_content = "+boolean(values,"scrollContent",true)+", .preferred_width = "+decimal(numeric(values,"preferredWidth",560,0,8192))+", .preferred_height = "+decimal(numeric(values,"preferredHeight",420,0,8192))+", .desktop_margin = "+decimal(numeric(values,"desktopMargin",24,0,8192))+", .content_padding = "+decimal(numeric(values,"contentPadding",20,0,8192))+", .top_safe_inset = "+decimal(numeric(values,"topSafeInset",0,0,8192))+", .bottom_safe_inset = "+decimal(numeric(values,"bottomSafeInset",0,0,8192))+", .dismiss_on_background = "+boolean(values,"dismissOnBackground",true)+", .dismiss_on_escape = "+boolean(values,"dismissOnEscape",true)+", .dismiss_on_drag = "+boolean(values,"dismissOnDrag",true)+", .motion_enabled = "+boolean(values,"motionEnabled",true)+", .motion = "+scoped_member(values,"motion")+", .animation_duration = std::chrono::milliseconds("+decimal(duration)+"), .surface_color = "+widget_color(values,"surfaceColor","ii::gui::WidgetColor{.1370945126,.1402995437,.1419020444,1}")+", .backdrop_color = "+widget_color(values,"backdropColor","ii::gui::WidgetColor{0,0,0,.48}")+", .accessibility_name = "+literal(text(values,"accessibilityName","Sheet"))+"}, {.opened = "+action(values,"onOpened")+", .closed = "+action(values,"onClosed")+"}, "+modal_content(view,"SheetContentFactory",false)+")";
            if(boolean(values,"open",false)=="true"){if(factory_depth)error(view.position,"Initial open inside a content factory is not supported; open after activation");initial_open.push_back(id);}
        }
        else if(modal) {
            const auto motion=scoped_member(values,"motion");
            if(values.contains("description")&&values.contains("desc"))error(view.position,"Use description or its desc alias, not both");
            const auto minimum=numeric(values,"minWidth",520,0,8192),maximum=numeric(values,"maxWidth",980,1,8192);if(minimum>maximum)error(view.position,"Modal minWidth exceeds maxWidth");
            std::string shape="round_rect";if(auto item=values.find("shapeStyle");item!=values.end()){auto value=resolve(item->second->value,item->second->position);if(value.kind==ValueKind::enumeration&&(value.text==".cylinder"||value.text==".shapeCylinder"))shape="cylinder";else if(value.kind!=ValueKind::enumeration||(value.text!=".roundRect"&&value.text!=".shapeRoundRect"))error(item->second->position,"Unknown Modal shapeStyle");}
            std::string image="{}";if(auto item=values.find("iconSource");item!=values.end()){const auto& input=*item->second;const auto& value=input.value;const auto dot=value.text.find('.');auto backend=backends.find(value.text.substr(0,dot));auto member=dot==value.text.npos?std::string{}:value.text.substr(dot+1);if(value.kind!=ValueKind::reference||backend==backends.end()||!identifier(member))error(input.position,"iconSource requires one scoped native image member");image="scope->backend_"+std::to_string(backend->second)+"."+member;}
            if(boolean(values,"open",false)=="true"){if(factory_depth)error(view.position,"Initial open inside a content factory is not supported; open after activation");initial_open.push_back(id);}
            call=prefix+"modal("+args+", {.title = "+literal(text(values,"title"))+", .description = "+literal(text(values,values.contains("desc")?"desc":"description"))+", .message = "+literal(text(values,"message"))+", .icon_name = "+literal(text(values,"iconName"))+", .icon_source = "+image+", .show_icon = "+boolean(values,"showIcon",true)+", .icon_size = "+decimal(numeric(values,"iconSize",72,1,8192))+", .icon_corner_radius = "+decimal(numeric(values,"iconCornerRadius",16,0,8192))+", .icon_background = "+widget_color(values,"iconBackgroundColor","ii::gui::WidgetColor{.1411764771,.1411764771,.1411764771,1}")+", .icon_border = "+widget_color(values,"iconBorderColor","ii::gui::WidgetColor{208./255,215./255,226./255,.18}")+", .button_count = "+decimal(std::trunc(numeric(values,"buttonCount",0,-1e9,1e9)))+", .primary_text = "+literal(text(values,"primaryText","OK"))+", .secondary_text = "+literal(text(values,"secondaryText"))+", .tertiary_text = "+literal(text(values,"tertiaryText"))+", .primary_enabled = "+boolean(values,"primaryEnabled",true)+", .secondary_enabled = "+boolean(values,"secondaryEnabled",true)+", .tertiary_enabled = "+boolean(values,"tertiaryEnabled",true)+", .minimum_width = "+decimal(minimum)+", .maximum_width = "+decimal(maximum)+", .preferred_width = "+decimal(numeric(values,"preferredWidth",760,1,8192))+", .side_padding = "+decimal(numeric(values,"sidePadding",24,0,8192))+", .minimum_height = "+decimal(numeric(values,"frameMinHeight",210,0,8192))+", .vertical_offset = "+decimal(numeric(values,"verticalOffset",-16,-32767,32767))+", .corner_radius = "+decimal(numeric(values,"frameCornerRadius",12,0,8192))+", .shape = lvrs::ModalShape::"+shape+", .frame_color = "+widget_color(values,"frameColor","ii::gui::WidgetColor{.1370945126,.1402995437,.1419020444,1}")+", .backdrop_color = "+widget_color(values,"backdropColor","ii::gui::WidgetColor{0,0,0,89./255}")+", .dismiss_on_background = "+boolean(values,"dismissOnBackground",true)+", .motion_enabled = "+boolean(values,"motionEnabled",true)+", .motion = "+motion+", .accessibility_name = "+literal(text(values,"accessibilityName","Modal"))+"}, {.primary_clicked = "+action(values,"onPrimaryClicked")+", .secondary_clicked = "+action(values,"onSecondaryClicked")+", .tertiary_clicked = "+action(values,"onTertiaryClicked")+", .canceled = "+action(values,"onCanceled")+", .opened = "+action(values,"onOpened")+", .closed = "+action(values,"onClosed")+"}, "+modal_content(view)+")";
        }
        else if(popover) {
            std::string surface;
            if(auto item=values.find("surfaceColor");item!=values.end()){
                auto value=resolve(item->second->value,item->second->position);auto hex=std::string_view(value.text);if(hex.starts_with('#'))hex.remove_prefix(1);
                if(value.kind!=ValueKind::string||hex.size()!=6)error(item->second->position,"Popover surfaceColor requires a six-digit RGB HEX String");
                unsigned rgb[3]{};for(unsigned c=0;c<3;++c){auto result=std::from_chars(hex.data()+2*c,hex.data()+2*c+2,rgb[c],16);if(result.ec!=std::errc{}||result.ptr!=hex.data()+2*c+2)error(item->second->position,"Invalid popover RGB HEX");}
                surface=", .surface_color = ii::gui::WidgetColor{"+decimal(rgb[0]/255.)+", "+decimal(rgb[1]/255.)+", "+decimal(rgb[2]/255.)+", 1}";
            }
            call=prefix+"popover("+args+", {.width = "+decimal(numeric(values,"width",0,0,8192))+", .height = "+decimal(numeric(values,"height",0,0,8192))+", .padding = "+decimal(numeric(values,"padding",12,0,8192))+", .margins = "+decimal(numeric(values,"margins",8,0,32767))+", .corner_radius = "+decimal(numeric(values,"cornerRadius",12,0,32767))+", .surface_opacity = "+decimal(numeric(values,"surfaceOpacity",.25,0,1))+surface+", .modal = "+boolean(values,"modal",false)+", .dim = "+boolean(values,"dim",false)+", .focus_on_open = "+boolean(values,"focus",true)+", .clip_content = "+boolean(values,"clip",false)+", .dismiss_on_escape = "+boolean(values,"closeOnEscape",true)+", .dismiss_on_outside_press = "+boolean(values,"closeOnPressOutside",true)+", .accessibility_name = "+literal(text(values,"accessibilityName","Popover"))+"}, {.opened = "+action(values,"onOpened")+", .closed = "+action(values,"onClosed")+"})";
        }
        else if(color_button) {
            std::string model="{}",current,size="medium";
            if(values.contains("model")&&values.contains("currentColor"))error(view.position,"ColorPickerButton accepts either model or currentColor");
            if(auto item=values.find("model");item!=values.end()) {
                const auto& input=*item->second;const auto& value=input.value;const auto dot=value.text.find('.');auto backend=backends.find(value.text.substr(0,dot));const auto member=dot==value.text.npos?std::string{}:value.text.substr(dot+1);
                if(value.kind!=ValueKind::reference||backend==backends.end()||!identifier(member))error(input.position,"Color model must be one public scoped C++ member");model="scope->backend_"+std::to_string(backend->second)+"."+member;
            }
            if(auto item=values.find("currentColor");item!=values.end()) {
                auto value=resolve(item->second->value,item->second->position);auto hex=std::string_view(value.text);if(hex.starts_with('#'))hex.remove_prefix(1);
                if(value.kind!=ValueKind::string||(hex.size()!=6&&hex.size()!=8))error(item->second->position,"currentColor requires a RRGGBB or RRGGBBAA String");
                unsigned rgba[4]{0,0,0,255};for(unsigned c=0;c<hex.size()/2;++c){auto p=std::from_chars(hex.data()+c*2,hex.data()+c*2+2,rgba[c],16);if(p.ec!=std::errc{}||p.ptr!=hex.data()+c*2+2)error(item->second->position,"Invalid currentColor HEX");}
                current=", .current_color = ii::gui::WidgetColor{"+decimal(rgba[0]/255.)+", "+decimal(rgba[1]/255.)+", "+decimal(rgba[2]/255.)+", "+decimal(rgba[3]/255.)+"}";
            }
            if(auto item=values.find("buttonSize");item!=values.end()) {
                auto value=resolve(item->second->value,item->second->position);
                if(value.kind==ValueKind::enumeration&&(value.text==".Small"||value.text==".small"))size="small";
                else if(value.kind==ValueKind::enumeration&&(value.text==".Medium"||value.text==".medium"))size="medium";
                else if(value.kind==ValueKind::enumeration&&(value.text==".Large"||value.text==".large"))size="large";
                else {const auto n=numeric(values,"buttonSize",28,22,36);if(n==22)size="small";else if(n==28)size="medium";else if(n==36)size="large";else error(item->second->position,"buttonSize requires Small/Medium/Large or 22/28/36");}
            }
            call=prefix+"color_picker_button("+args+", {.model = "+model+current+", .button_size = lvrs::ColorPickerButtonSize::"+size+", .enabled = "+boolean(values,"enabled",true)+", .show_hue_ring = "+boolean(values,"showHueRing",true)+", .circular = "+boolean(values,"circular",false)+", .border_opacity = "+decimal(numeric(values,"borderOpacity",1,0,1))+", .accessibility_name = "+literal(text(values,"accessibilityName","Choose color"))+"}, "+control_events(values)+")";
        }
        else if(color_picker) {
            std::string model="{}",type="wheel",recent;
            if(auto item=values.find("model");item!=values.end()) {
                const auto& input=*item->second;const auto& value=input.value;const auto dot=value.text.find('.');auto backend=backends.find(value.text.substr(0,dot));
                const auto member=dot==value.text.npos?std::string{}:value.text.substr(dot+1);
                if(value.kind!=ValueKind::reference || backend==backends.end() || !identifier(member))error(input.position,"Color model must be one public scoped C++ member");
                model="scope->backend_"+std::to_string(backend->second)+"."+member;
            }
            if(auto item=values.find("type");item!=values.end()) {
                const std::map<std::string,std::string,std::less<>> types{{".Wheel","wheel"},{".wheel","wheel"},{".HueSaturation","hue_saturation"},{".hueSaturation","hue_saturation"},{".SaturationBrightness","saturation_brightness"},{".saturationBrightness","saturation_brightness"},{".Grayscale","grayscale"},{".grayscale","grayscale"},{".RGB","rgb"},{".rgb","rgb"},{".CMYK","cmyk"},{".cmyk","cmyk"}};
                const auto entry=types.find(item->second->value.text);if(item->second->value.kind!=ValueKind::enumeration || entry==types.end())error(item->second->position,"Unknown color picker type");type=entry->second;
            }
            if(auto item=values.find("recentColors");item!=values.end()) {
                auto value=resolve(item->second->value,item->second->position);if(value.kind!=ValueKind::list)error(item->second->position,"Recent colors must be a literal HEX String list");
                recent=", .recent_colors = std::vector<ii::gui::WidgetColor>{";unsigned count=0;
                for(const auto& element:value.elements) {
                    if(count++==10)break;auto color=resolve(element,item->second->position);auto hex=std::string_view(color.text);if(hex.starts_with('#'))hex.remove_prefix(1);
                    if(color.kind!=ValueKind::string || (hex.size()!=6 && hex.size()!=8))error(item->second->position,"Recent colors require RRGGBB or RRGGBBAA Strings");
                    unsigned rgba[4]{0,0,0,255};for(unsigned c=0;c<hex.size()/2;++c){auto parsed=std::from_chars(hex.data()+c*2,hex.data()+c*2+2,rgba[c],16);if(parsed.ec!=std::errc{}||parsed.ptr!=hex.data()+c*2+2)error(item->second->position,"Invalid recent color HEX");}
                    if(count>1)recent+=", ";recent+="{"+decimal(rgba[0]/255.)+", "+decimal(rgba[1]/255.)+", "+decimal(rgba[2]/255.)+", "+decimal(rgba[3]/255.)+"}";
                }recent+="}";
            }
            const auto width=numeric(values,"width",320,320,1024),padding=numeric(values,"padding",12,0,64);if(width-2*padding<228)error(view.position,"ColorPicker padding must leave 228 content units");
            call=prefix+"color_picker("+args+", {.type = lvrs::ColorPickerType::"+type+", .model = "+model+", .width = "+decimal(width)+", .padding = "+decimal(padding)+", .enabled = "+boolean(values,"enabled",true)+", .show_recent_colors = "+boolean(values,"showRecentColors",true)+", .show_actions = "+boolean(values,"showActions",true)+recent+", .apply_text = "+literal(text(values,"applyText","Apply"))+", .cancel_text = "+literal(text(values,"cancelText","Cancel"))+"}, {.color_edited = "+action(values,"onColorEdited","ii::gui::WidgetColor value")+", .accepted = "+action(values,"onAccepted","ii::gui::WidgetColor value")+", .canceled = "+action(values,"onCanceled")+", .eyedropper_requested = "+action(values,"onEyedropperRequested")+"})";
        }
        else if(color_surface) {
            const auto model=values.find("model");if(model==values.end())error(view.position,"Color surface requires model: backend.member");
            const auto& input=*model->second;const auto& value=input.value;
            if(value.kind!=ValueKind::reference)error(input.position,"Color model must be a scoped C++ member reference");
            const auto dot=value.text.find('.');auto backend=backends.find(value.text.substr(0,dot));
            const auto member=dot==value.text.npos?std::string{}:value.text.substr(dot+1);
            if(backend==backends.end() || !identifier(member))error(input.position,"Unknown color model backend or invalid public member");
            std::string field="wheel";
            if(auto item=values.find("field");item!=values.end()) {
                const std::map<std::string,std::string,std::less<>> fields{{".Wheel","wheel"},{".wheel","wheel"},{".HueSaturation","hue_saturation"},{".hueSaturation","hue_saturation"},{".SaturationBrightness","saturation_brightness"},{".saturationBrightness","saturation_brightness"},{".Spectrum","spectrum"},{".spectrum","spectrum"},{".HueRail","hue_rail"},{".hueRail","hue_rail"},{".BrightnessRail","brightness_rail"},{".brightnessRail","brightness_rail"}};
                const auto entry=fields.find(item->second->value.text);if(item->second->value.kind!=ValueKind::enumeration || entry==fields.end())error(item->second->position,"Unknown color field");field=entry->second;
            }
            call=prefix+"color_surface("+args+", {.model = scope->backend_"+std::to_string(backend->second)+"."+member+", .field = ii::gui::ColorField::"+field+
                ", .width = "+decimal(numeric(values,"width",field=="hue_rail" || field=="brightness_rail"?18:232,12,1024))+", .height = "+decimal(numeric(values,"height",232,12,1024))+
                ", .enabled = "+boolean(values,"enabled",true)+", .accessibility_name = "+literal(text(values,"accessibilityName","Color field"))+"}, "+action(values,"onColorEdited","ii::gui::WidgetColor value")+")";
        }
        else if(stack){
            std::string alignment="center";if(auto input=values.find("alignment");input!=values.end()){auto value=resolve(input->second->value,input->second->position);if(value.kind!=ValueKind::enumeration||(value.text!=".center"&&value.text!=".leading"&&value.text!=".trailing"&&value.text!=".stretch"))error(input->second->position,"Invalid stack alignment");alignment=value.text.substr(1);}
            call=prefix+(kind=="VStack"?"vstack":kind=="HStack"?"hstack":"zstack")+"("+args+", {.spacing = "+decimal(numeric(values,"spacing",8,0,32767))+", .padding = "+decimal(numeric(values,"padding",0,0,32767))+", .alignment = ii::gui::StackAlignment::"+alignment+"})";
        }
        else if(kind=="Spacer") call=prefix+"spacer("+args+", "+decimal(numeric(values,"minLength",0,0,32767))+")";
        else if(kind=="Label") {
            const auto [role,size]=text_role(values);
            call=prefix+"label("+args+", {.text = "+text_expression(values,"text")+", .font_size = "+decimal(numeric(values,"fontSize",size,1,256))+
                ", .selectable = "+boolean(values,"selectable",true)+", .role = ii::gui::TextRole::"+role+", .color = "+widget_color(values,"color","std::nullopt")+", .line_height = "+decimal(numeric(values,"lineHeight",0,0,8192))+std::string(values.contains("fontWeight")?", .font_weight = "+decimal(numeric(values,"fontWeight",400,400,700)):"")+"})";
        } else if(button) {
            std::string tone=kind=="AbstractButton" || kind=="HelpButton"?"normal":"primary";
            if(auto it=values.find("tone");it!=values.end()) {
                const auto& v=it->second->value;
                if(v.kind!=ValueKind::enumeration) error(it->second->position,"Expected button tone enum");
                const std::map<std::string,std::string,std::less<>> tones{{".primary","primary"},{".default","normal"},{".borderless","borderless"},{".destructive","destructive"},{".disabled","disabled"}};
                auto entry=tones.find(v.text); if(entry==tones.end()) error(it->second->position,"Unknown LVRS button tone"); tone=entry->second;
            }
            const auto mode=boolean(values,"iconMode",icon_button),icon=text(values,"iconName");
            if(options.framework!="LVRS" && (mode=="true" || !icon.empty())) error(view.position,"Named icon assets require FRAMEWORK LVRS");
            const auto method=kind=="ListItem"?"list_item":kind=="HelpButton"?"help_button":kind=="IconMenuButton"?"icon_menu_button":dropdown?"dropdown_button":kind=="IconButton"?"icon_button":"button";
            std::string geometry;
            for(auto [name,field]:{std::pair{"height","height"},{"cornerRadius","corner_radius"},{"horizontalPadding","horizontal_padding"}})if(values.contains(name))geometry+=", ."+std::string(field)+" = "+decimal(numeric(values,name,0,std::string_view(name)=="height"?1:0,8192));
            const auto image=values.contains("icon")?", .icon = "+scoped_member(values,"icon"):"";
            call=prefix+method+"("+args+", {.text = "+text_expression(values,"text")+", .enabled = "+boolean(values,"enabled",true)+", .tone = ii::gui::ButtonTone::"+tone+
                image+", .icon_name = "+literal(icon)+", .accessibility_name = "+literal(text(values,"accessibilityName"))+", .icon_size = "+decimal(numeric(values,"iconSize",16,1,256))+", .icon_mode = "+mode+geometry+"}, "+action(values)+")";
        } else if(tab) {
            const auto icon=text(values,"iconName");
            if(icon.find_first_not_of(" \t\r\n")==icon.npos) error(view.position,"Tab iconName is required and must be nonempty");
            std::string style="underline";
            if(auto it=values.find("tabStyle");it!=values.end()) {
                const auto& v=it->second->value;
                if(v.kind!=ValueKind::enumeration || (v.text!=".underline" && v.text!=".Underline" && v.text!=".surface" && v.text!=".Surface")) error(it->second->position,"Unknown Tab style");
                if(v.text==".surface" || v.text==".Surface") style="surface";
            }
            call=prefix+"tab("+args+", {.text = "+text_expression(values,"text")+", .icon_name = "+literal(icon)+", .badge = "+literal(text(values,"badge"))+
                ", .enabled = "+boolean(values,"enabled",true)+", .selected = "+boolean(values,"selected",false)+", .style = ii::gui::TabStyle::"+style+
                ", .icon_size = "+decimal(numeric(values,"iconSize",18,1,256))+", .minimum_width = "+decimal(numeric(values,"minimumWidth",80,1,8192))+"}, "+action(values)+")";
        } else if(tabbar) {
            const auto current=numeric(values,"currentIndex",0,-1,4095);if(current!=std::trunc(current)) error(view.position,"TabBar currentIndex must be an integer");
            call=prefix+"tab_bar("+args+", {.spacing = "+decimal(numeric(values,"spacing",4,0,32767))+", .minimum_tab_width = "+decimal(numeric(values,"minimumTabWidth",120,1,8192))+
                ", .current_index = "+decimal(current)+", .auto_select = "+boolean(values,"autoSelect",true)+"}, "+action(values,"onCurrentIndexChanged","int value")+")";
        } else if(combo || stepper) {
            std::string tone="primary";
            if(auto it=values.find("tone");it!=values.end()) {
                const auto& v=it->second->value;
                const std::map<std::string,std::string,std::less<>> tones{{".primary","primary"},{".Primary","primary"},{".borderless","borderless"},{".Borderless","borderless"},
                    {".default","normal"},{".destructive","destructive"},{".disabled","disabled"}};
                auto entry=tones.find(v.text);
                if(v.kind!=ValueKind::enumeration || entry==tones.end() || (combo && entry->second!="primary" && entry->second!="borderless")) error(it->second->position,"Unknown tone for compound control");
                tone=entry->second;
            }
            const auto common=".tone = ii::gui::ButtonTone::"+tone+", .arrow = ii::gui::StepperArrow::"+stepper_arrow(values)+", .enabled = "+boolean(values,"enabled",true);
            if(combo) call=prefix+"combo_box("+args+", {.text = "+text_expression(values,"text","Label")+", "+common+", .width = "+decimal(numeric(values,"width",97,19,8192))+"}, "+control_events(values)+")";
            else call=prefix+"stepper("+args+", {"+common+", .corner_radius = "+decimal(numeric(values,"cornerRadius",4,0,32767))+"}, "+action(values,"onStepped","int value")+", "+control_events(values)+")";
        } else if(segmented) {
            std::string cylinder="false";
            if(auto it=values.find("shapeStyle");it!=values.end()) {
                const auto& v=it->second->value;
                if(v.kind!=ValueKind::enumeration || (v.text!=".roundRect" && v.text!=".shapeRoundRect" && v.text!=".cylinder" && v.text!=".shapeCylinder")) error(it->second->position,"Unknown segmented shapeStyle");
                if(v.text==".cylinder" || v.text==".shapeCylinder") cylinder="true";
            }
            const auto method=kind=="IconSegmentedControl"?"icon_segmented_control":"label_segmented_control";
            call=prefix+method+"("+args+", {.horizontal_padding = "+decimal(numeric(values,"horizontalPadding",4,0,32767))+
                ", .vertical_padding = "+decimal(numeric(values,"verticalPadding",kind=="IconSegmentedControl"?4:3.5,0,32767))+
                ", .spacing = "+decimal(numeric(values,"spacing",2,0,32767))+", .border_width = "+decimal(numeric(values,"borderWidth",2,0,32767))+
                ", .corner_radius = "+decimal(numeric(values,"cornerRadius",12,0,32767))+", .cylinder = "+cylinder+", .force_borderless_tone = "+boolean(values,"forceBorderlessTone",true)+"})";
        } else if(menu) {
            const auto selected=numeric(values,"selectedIndex",-1,-1,4095);if(selected!=std::trunc(selected)) error(view.position,"Menu selectedIndex must be an integer");
            const auto optional_number=[&](std::string_view name,std::string_view member) {return values.contains(name)?", ."+std::string(member)+" = "+decimal(numeric(values,name,0,0,32767)):std::string{};};
            const auto method=kind=="ContextMenu"?"context_menu":"menu";
            call=prefix+method+"("+args+", {"+(values.contains("compactItems")?".compact_items = "+boolean(values,"compactItems",false)+", ":std::string{})+
                ".item_width = "+decimal(numeric(values,"itemWidth",0,0,8192))+", .item_spacing = "+decimal(numeric(values,"itemSpacing",2,0,32767))+
                ", .left_padding = "+decimal(numeric(values,"leftPadding",8,0,32767))+", .right_padding = "+decimal(numeric(values,"rightPadding",8,0,32767))+
                optional_number("topPadding","top_padding")+optional_number("bottomPadding","bottom_padding")+
                ", .edge_margin = "+decimal(numeric(values,"edgeMargin",4,0,32767))+", .corner_radius = "+decimal(numeric(values,"cornerRadius",12,0,32767))+
                ", .opacity = "+decimal(numeric(values,"menuOpacity",.12,0,1))+", .selected_index = "+decimal(selected)+
                ", .auto_close_on_trigger = "+boolean(values,"autoCloseOnTrigger",true)+", .dismiss_on_outside_press = "+boolean(values,"dismissOnGlobalPress",true)+
                ", .dismiss_on_context_request = "+boolean(values,"dismissOnGlobalContextRequest",true)+"}, {.triggered = "+action(values,"onItemTriggered","int value")+
                ", .opened = "+action(values,"onOpened")+", .closed = "+action(values,"onClosed")+"})";
        } else if(menu_item) {
            if(values.contains("key") && values.contains("shortcut")) error(view.position,"Use key or shortcut, not both");
            if(values.contains("keepOpen") && values.contains("closeOnTrigger")) error(view.position,"Use keepOpen or closeOnTrigger, not both");
            std::string state="normal",direction="automatic";
            if(auto it=values.find("state");it!=values.end()) {
                const auto& v=it->second->value;
                if(v.kind!=ValueKind::enumeration || (v.text!=".default" && v.text!=".selected" && v.text!=".inactive")) error(it->second->position,"Unknown menu item state");
                if(v.text==".selected") state="selected";else if(v.text==".inactive") state="inactive";
            }
            if(auto it=values.find("selectionDirection");it!=values.end()) {
                const auto& v=it->second->value;constexpr std::string_view directions=" auto right left up down ";
                if(v.kind!=ValueKind::enumeration || directions.find(" "+v.text.substr(1)+" ")==directions.npos) error(it->second->position,"Unknown menu chevron direction");
                if(v.text!=".auto") direction=v.text.substr(1);
            }
            const auto method=kind=="ContextMenuItem"?"context_menu_item":"menu_item";
            const auto key_name=values.contains("key")?"key":"shortcut";
            std::string close;
            if(values.contains("closeOnTrigger")) close=", .close_on_trigger = "+boolean(values,"closeOnTrigger",true);
            else if(values.contains("keepOpen") && boolean(values,"keepOpen",false)=="true") close=", .close_on_trigger = false";
            call=prefix+method+"("+args+", {.label = "+literal(text(values,"label","Label"))+
                (values.contains(key_name)?", .shortcut = "+literal(text(values,key_name)):std::string{})+", .key_placeholder = "+literal(text(values,"keyPlaceholder"))+
                ", .icon_name = "+literal(text(values,"iconName","procedure"))+", .enabled = "+boolean(values,"enabled",true)+
                (values.contains("compact")?", .compact = "+boolean(values,"compact",false):std::string{})+
                ", .state = ii::gui::MenuItemState::"+state+", .direction = ii::gui::MenuDirection::"+direction+
                ", .key_visible = "+boolean(values,"keyVisible",true)+", .show_icon_slot = "+(values.contains("showIconSlot")?boolean(values,"showIconSlot",true):menu_parent?boolean(*menu_parent,"showIconSlot",true):"true")+
                ", .show_chevron = "+boolean(values,"showChevron",true)+", .has_child_items = "+boolean(values,"hasChildItems",true)+", .expanded = "+boolean(values,"expanded",false)+
                ", .item_width = "+decimal(numeric(values,"itemWidth",0,0,8192))+", .item_height = "+decimal(numeric(values,"itemHeight",0,0,8192))+
                ", .icon_size = "+decimal(numeric(values,"iconSize",18,1,256))+", .chevron_size = "+decimal(numeric(values,"chevronSize",16,1,256))+close+"}, "+action(values)+")";
        } else if(divider) {
            std::string axis="horizontal";
            if(auto it=values.find("axis");it!=values.end()) {
                auto v=resolve(it->second->value,it->second->position);
                axis=v.kind==ValueKind::enumeration?v.text.substr(1):v.kind==ValueKind::string?v.text:std::string{};
                if(axis!="horizontal" && axis!="vertical") error(it->second->position,"Unknown menu divider axis");
            }
            call=prefix+(kind=="ContextMenuDivider"?"context_menu_divider":"menu_divider")+"("+args+", {.vertical = "+(axis=="vertical"?"true":"false")+
                ", .thickness = "+decimal(numeric(values,"thickness",1,0,8192))+", .cross_padding = "+decimal(numeric(values,"crossPadding",1,0,32767))+
                (values.contains("linePadding")?", .line_padding = "+decimal(numeric(values,"linePadding",0,0,32767)):std::string{})+
                ", .line_length = "+decimal(numeric(values,"lineLength",0,0,8192))+"})";
        } else if(check) {
            const auto method=kind=="CheckBox"?"check_box":kind=="RadioButton"?"radio_button":"toggle_switch";
            call=prefix+method+"("+args+", {.text = "+text_expression(values,"text")+", .enabled = "+boolean(values,"enabled",true)+
                ", .checked = "+boolean(values,"checked",false)+", .group = "+literal(text(values,"group"))+"}, "+action(values,"onToggled","bool value")+")";
        } else if(kind=="InputField") {
            call=prefix+"input_field("+args+", {.text = "+text_expression(values,"text")+", .placeholder = "+literal(text(values,"placeholder"))+
                ", .enabled = "+boolean(values,"enabled",true)+", .read_only = "+boolean(values,"readOnly",false)+", .width = "+decimal(numeric(values,"width",206,1,8192))+
                (values.contains("height")?", .style = ii::gui::InputStyle{.height = "+decimal(numeric(values,"height",30,1,8192))+"}":"") +
                ", .multiline = "+boolean(values,"multiline",false)+", .password = "+boolean(values,"password",false)+"}, "+action(values,"onEdited","std::string value")+", "+action(values,"onSubmit")+")";
        } else if(kind=="Slider") {
            const auto minimum=numeric(values,"from",0,-1e12,1e12),maximum=numeric(values,"to",1,-1e12,1e12);
            if(minimum>=maximum) error(view.position,"Slider from must be smaller than to");
            call=prefix+"slider("+args+", {.minimum = "+decimal(minimum)+", .maximum = "+decimal(maximum)+", .value = "+number_expression(values,"value",.5,-1e12,1e12)+
                ", .step = "+decimal(numeric(values,"stepSize",0,0,32767))+", .width = "+decimal(numeric(values,"width",320,18,8192))+", .enabled = "+boolean(values,"enabled",true)+", .thumb_diameter = "+decimal(numeric(values,"thumbDiameter",18,1,256))+", .track_height = "+decimal(numeric(values,"trackHeight",6,1,256))+", .range_inset = "+decimal(numeric(values,"rangeInset",9,0,128))+", .height = "+decimal(numeric(values,"height",32,1,8192))+"}, "+action(values,"onMoved","double value")+")";
        } else {
            const auto minimum=numeric(values,"minimumValue",0,-1e12,1e12),maximum=numeric(values,"maximumValue",100,-1e12,1e12);
            if(minimum>maximum) error(view.position,"Progress minimum must not exceed maximum");
            call=prefix+"progress("+args+", {.minimum = "+decimal(minimum)+", .maximum = "+decimal(maximum)+", .start = "+decimal(numeric(values,"startValue",0,-1e12,1e12))+
                ", .value = "+number_expression(values,"currentValue",0,-1e12,1e12)+", .width = "+decimal(numeric(values,"width",100,1,8192))+", .height = "+decimal(numeric(values,"height",6,1,8192))+"})";
        }
        if(toolbar){const auto native="toolbar_"+variable;body+="            auto "+native+" = "+call+";\n            if (!"+native+") return std::unexpected("+native+".error());\n            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";emit_toolbar_bindings(values,variable,native,kind);
            if(kind=="HierarchyToolbar")toolbar_parents["*"+variable]=native;
            if(kind=="ToolbarButton")if(auto owner=toolbar_parents.find(parent);owner!=toolbar_parents.end())body+="            if (auto attached = "+owner->second+"->register_button(*"+native+", false); !attached) return attached;\n";
        }
        else if(link||card||header){const auto native=std::string(link?"link_":"surface_")+variable;body+="            auto "+native+" = "+call+";\n            if (!"+native+") return std::unexpected("+native+".error());\n            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";if(link)emit_link_bindings(values,variable,native);else emit_surface_bindings(values,variable,native);}
        else{body+="            auto "+variable+" = "+call+";\n            if (!"+variable+") return std::unexpected("+variable+".error());\n";emit_bindings(values,variable);}
        if(stack&&(values.contains("width")||values.contains("height")))body+="            if (auto extent = "+variable+"->set_extent({"+decimal(numeric(values,"width",0,0,8192))+", "+decimal(numeric(values,"height",0,0,8192))+"}); !extent) return extent;\n";
        if(menu && values.contains("targetId")) context_bindings.push_back({variable,text(values,"targetId"),values.at("targetId")->position});
        if(menu && values.contains("items")) {
            if(!view.children.empty()) error(view.position,"Use menu items or typed children, not both");
            const auto& input=*values.at("items");auto items=resolve(input.value,input.position);
            if(items.kind!=ValueKind::list) error(input.position,"Static menu items must be a String list");
            if(items.elements.size()>4096-next) error(input.position,"Native widget limit is 4096");
            for(const auto& item:items.elements) {
                const auto value=resolve(item,input.position);if(value.kind!=ValueKind::string) error(input.position,"Static menu items must contain Strings");
                const auto child=next++;const auto id="@lvui/"+std::to_string(child);if(!ids.insert(id).second) error(input.position,"Duplicate widget id");
                const auto name="node_"+std::to_string(child);const auto method=kind=="ContextMenu"?"context_menu_item":"menu_item";
                body+="            auto "+name+" = "+prefix+method+"(*"+variable+", "+literal(id)+", {.label = "+literal(value.text)+", .shortcut = \"\", .icon_name = \"\", .key_visible = false, .show_icon_slot = "+boolean(values,"showIconSlot",true)+", .has_child_items = false});\n            if (!"+name+") return std::unexpected("+name+".error());\n";
            }
        }
        if(!modal&&!sheet)for(const auto& child:view.children) emit(child,"*"+variable,menu?&values:nullptr);
        if(kind=="HierarchyToolbar")body+="            if (auto normalized = toolbar_"+variable+"->normalize(); !normalized) return normalized;\n";
    }

public:
    Generator(const ViewDocument& value, const NativeCompileOptions& config) : document(value), options(config) {}
    std::string run() {
        if (!identifier(options.name)) error({}, "Invalid or reserved C++ identifier for generated view name");
        if (options.framework!="iiGUI" && options.framework!="LVRS") error({}, "Unknown native component framework");
        std::string scope;
        for (const auto& backend : document.imports()) {
            const auto index = backends.size();
            backends.emplace(backend.name, index);
            scope += "        " + qualified(backend.type, backend.position) + " backend_" + std::to_string(index) + ";\n";
        }
        for (const auto& binding : document.bindings()) {
            if(!identifier(binding.name))error(binding.position,"Invalid C++ binding identifier");
            if(auto source=property(binding.initializer);!source.empty()) {
                if(binding.mutable_value)error(binding.position,"Reactive var initializer must own a scalar value");
                reactive.emplace(binding.name,std::move(source));continue;
            }
            auto value = resolve(binding.initializer, binding.position);
            if (value.kind != ValueKind::string && value.kind != ValueKind::number && value.kind != ValueKind::boolean)
                error(binding.position, "Native let initializer must be a scalar constant");
            if (!binding.type.empty()) {
                const auto& type = binding.type;
                const bool valid = (type == "String" && value.kind == ValueKind::string) ||
                    (type == "Bool" && value.kind == ValueKind::boolean) ||
                    ((type == "Double" || type == "Float" || type == "Int") && value.kind == ValueKind::number);
                if (!valid) error(binding.position, "Scalar declaration type does not match initializer or is unsupported");
                if (type == "Int") {
                    auto n = number(value, binding.position);
                    if (std::trunc(n) != n || std::abs(n) > 9007199254740991.0) error(binding.position, "Int constant must be an exactly representable integer");
                }
            }
            if(binding.mutable_value){
                const auto type=value.kind==ValueKind::string?"std::string":value.kind==ValueKind::boolean?"bool":binding.type=="Int"?"std::int64_t":"double";
                const auto initial=value.kind==ValueKind::string?literal(value.text):value.kind==ValueKind::boolean?value.text:decimal(number(value,binding.position));
                scope+="        ii::core::Observable<"+std::string(type)+"> value_"+binding.name+"{"+initial+"};\n";
                variable_types.emplace(binding.name,type);reactive.emplace(binding.name,"scope->value_"+binding.name);
            }else constants.emplace(binding.name, std::move(value));
        }
        const auto& root = document.root();
        if (root.component->spelling != "ApplicationWindow" && root.component->spelling != "Window") error(root.position, "Native AOT root must be ApplicationWindow or Window");
        if (root.children.size() != 1) error(root.position, "ApplicationWindow requires exactly one child View");
        const auto values = options.framework=="LVRS" ? inputs(root, {"title", "width", "height", "minimumWidth", "minimumHeight", "accent"}) : inputs(root, {"title", "width", "height", "minimumWidth", "minimumHeight"});
        std::string accent="blue";
        if(auto found=values.find("accent");found!=values.end()) {
            const auto& v=found->second->value; constexpr std::string_view choices=" blue purple rose red orange yellow green gray brown cyan teal white ";
            if(v.kind!=ValueKind::enumeration || v.text.size()<2 || choices.find(" "+v.text.substr(1)+" ")==choices.npos) error(found->second->position,"Unknown LVRS accent");
            accent=v.text.substr(1);
        }
        const auto width = numeric(values, "width", 640, 1, 32767), height = numeric(values, "height", 420, 1, 32767);
        const auto min_width = numeric(values, "minimumWidth", 320, 1, 32767), min_height = numeric(values, "minimumHeight", 220, 1, 32767);
        if (width < min_width || height < min_height) error(root.position, "Window size must be at least its minimum size");
        emit(root.children.front(), "native_parent");
        for(const auto& binding:context_bindings) {
            auto target=widget_variables.find(binding.target);if(target==widget_variables.end()) error(binding.position,"Unknown context menu targetId");
            body+="            if (auto bound = "+target->second+"->set_context_menu(*"+binding.menu+"); !bound) return bound;\n";
        }
        std::string result = "// Generated by lvui-compile. C++23 native widget construction; do not edit.\n#pragma once\n#include <iiGUI/NativeWidgets.h>\n#include <iiGUI/Bindings.h>\n#include <iiGUI/Host.h>\n#include <memory>\n";
        if(options.framework=="LVRS") result += "#include <LVRS/Widgets.h>\n";
        for (const auto& include : options.includes) {
            if (include.empty() || include.front() == '/' || include.find("..") != include.npos ||
                !std::ranges::all_of(include, [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                    (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.' || c == '/'; }))
                error({}, "Invalid relative C++ include path");
            result += "#include \"" + include + "\"\n";
        }
        result += "\nnamespace lvui_generated {\nstruct " + options.name + " final {\n    static ii::gui::HostOptions options() {\n        return {.title = " + literal(text(values, "title", "LVUI")) +
            ", .size = {" + decimal(width) + ", " + decimal(height) + "}, .minimum_size = {" + decimal(min_width) + ", " + decimal(min_height) + "}};\n    }\n";
        if (!scope.empty()) result += "    struct Scope final {\n" + scope + "    };\n";
        result += "    " + options.name + "() = default;\n    " + options.name + "(const " + options.name + "&) = delete;\n    " + options.name + "& operator=(const " + options.name + "&) = delete;\n";
        const auto scope_parameters=scope.empty()?std::string{}:", const std::function<void(Scope&)>& configure = {}, std::shared_ptr<Scope>* scope_owner = nullptr";
        const auto scope_arguments=scope.empty()?std::string{}:", configure, scope_owner";
        result += "private:\n    static ii::core::Result<void> construct(ii::gui::WidgetBuilder& widgets, ii::gui::Widget native_parent"+scope_parameters+") {\n";
        if (!scope.empty()) result += "            auto scope = std::make_shared<Scope>();\n            if (auto owned = widgets.retain(scope); !owned) return owned;\n            if (configure) configure(*scope);\n            if (scope_owner) *scope_owner = scope;\n";
        if(options.framework=="LVRS") result += "            lvrs::Widgets controls(widgets);\n";
        result += binding_owner();
        result += body;
        result += "            return {};\n    }\npublic:\n    static ii::core::Result<void> build(ii::gui::WidgetBuilder& widgets, ii::gui::Widget native_parent = {}"+scope_parameters+") {\n";
        if(!initial_open.empty())result+="        (void)widgets; (void)native_parent;"+std::string(scope.empty()?"":" (void)configure; (void)scope_owner;")+"\n        return ii::core::fail(ii::core::ErrorCode::unsupported, \"Embedded pages must open dialogs after activation\");\n";
        else result+="        return construct(widgets, native_parent"+scope_arguments+");\n";
        result+="    }\n    static ii::core::Result<ii::gui::WidgetMount> mount(ii::gui::Host& host"+scope_parameters+") {\n";
        result+=std::string(initial_open.empty()?"        return host.mount_widgets(":"        auto mounted = host.mount_widgets(")+std::string(scope.empty()?"[]":"[&configure, scope_owner]")+"(ii::gui::WidgetBuilder& widgets) -> ii::core::Result<void> {\n            return construct(widgets, {}"+scope_arguments+");\n        }, "+(options.framework=="LVRS"?"lvrs::Theme(lvrs::Accent::"+accent+").appearance()":"ii::gui::lvrs_appearance()")+");\n";
        if(!initial_open.empty()){result+="        if (!mounted) return mounted;\n";for(const auto& id:initial_open)result+="        { auto dialog = mounted->find("+literal(id)+"); if (!dialog) { (void)host.unmount_widgets(*mounted); return std::unexpected(dialog.error()); } if (auto opened = dialog->open_at(0,0); !opened) { (void)host.unmount_widgets(*mounted); return std::unexpected(opened.error()); } }\n";result+="        return mounted;\n";}
        result+="    }\n";
        const auto configuration=scope.empty() ? std::string{} : ", const std::function<void(Scope&)>& configure = {}";
        std::string accessors;
        for(const auto& [name,type]:variable_types) {
            accessors+="    ii::core::Result<void> set_"+name+"("+type+" value) {\n        auto current = native_widget_.snapshot();\n        if (!current) return std::unexpected(current.error());\n        if (!scope_) return ii::core::fail(ii::core::ErrorCode::closed, \"View scope has closed\");\n        try { scope_->value_"+name+".set(std::move(value)); return {}; }\n        catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, \"Observable update rejected\", std::current_exception()); }\n    }\n";
        }
        result += "    ii::core::Result<void> attach(ii::gui::Host& host" + configuration + ") {\n"
            "        auto current = native_widget_.snapshot();\n"
            "        if (current) return ii::core::fail(ii::core::ErrorCode::busy, \"Native C++ widget is already attached\");\n"
            "        if (current.error().code != ii::core::ErrorCode::closed) return std::unexpected(current.error());\n"
            + std::string(scope.empty() ? "" : "        std::shared_ptr<Scope> owner;\n") +
            "        auto created = mount(host" + std::string(scope.empty() ? "" : ", configure, &owner") + ");\n"
            "        if (!created) return std::unexpected(created.error());\n"
            "        native_widget_ = *created;\n" + std::string(scope.empty() ? "" : "        scope_ = std::move(owner);\n") +
            "        return {};\n    }\n"
            "    ii::core::Result<void> detach(ii::gui::Host& host) {\n"
            "        auto current = native_widget_.snapshot();\n"
            "        if (!current && current.error().code != ii::core::ErrorCode::closed) return std::unexpected(current.error());\n"
            "        if (current) { auto result = host.unmount_widgets(native_widget_); if (!result) return result; }\n"
            "        native_widget_ = {};\n" + std::string(scope.empty() ? "" : "        scope_.reset();\n") +
            "        return {};\n    }\n"
            "    ii::gui::WidgetMount widgets() const { return native_widget_; }\n"
            + accessors + "private:\n    ii::gui::WidgetMount native_widget_;\n" + std::string(scope.empty() ? "" : "    std::shared_ptr<Scope> scope_;\n") +
            "};\n} // namespace lvui_generated\n";
        return result;
    }
};
} // namespace
NativeCompileResult compile_native(std::string_view source, const NativeCompileOptions& options) {
    const auto first=source.find_first_not_of(" \t\r\n");
    auto document = first!=source.npos&&source[first]=='<'?parse_native_markup(source):parse_view(source);
    if (!document) return std::unexpected(std::move(document.error()));
    try { return Generator(*document, options).run(); }
    catch (Diagnostic& diagnostic) { return std::unexpected(std::move(diagnostic)); }
}
} // namespace lvui
