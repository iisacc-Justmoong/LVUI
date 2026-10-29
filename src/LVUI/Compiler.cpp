#include "Compiler.h"
#include "Language.h"
#include "ConstantExpression.h"
#include "NativeLayoutValues.h"
#include "Material.h"
#include <iiSDKCore/Core/Text/Number.h>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <array>
#include <tuple>

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
    if ((value.kind != ValueKind::number && value.kind != ValueKind::metric) || !result)
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
    struct BackendAccess final { std::string object, revision, dependency{}; };
    std::map<std::string, BackendAccess, std::less<>> backend_access;
    std::map<std::string, std::string, std::less<>> backend_types;
    std::map<std::string, std::string, std::less<>> blueprint_types;
    std::map<std::string,std::string,std::less<>> reactive;
    std::map<std::string,std::string,std::less<>> variable_types;
    struct VariableSource final {std::string target;bool callable=false,inferred=false;};
    std::map<std::string,VariableSource,std::less<>> variable_sources;
    std::map<std::string, ValueKind, std::less<>> binding_kinds;
    struct ViewDefinition;
    using ViewValues = std::map<std::string, std::shared_ptr<const ViewDefinition>, std::less<>>;
    struct Environment final {
        std::map<std::string, Value, std::less<>> constants;
        std::map<std::string, std::size_t, std::less<>> backends;
        std::map<std::string, BackendAccess, std::less<>> access;
        std::map<std::string, std::string, std::less<>> blueprints, reactive, native, backend_types;
        std::map<std::string, ValueKind, std::less<>> kinds;
        ViewValues views;
        std::set<std::string, std::less<>> fixed;
        std::size_t depth;
    };
    struct ViewDefinition final {
        std::shared_ptr<const View> view;
        Environment environment;
        std::optional<std::vector<ComponentParameter>> parameters{};
        bool sequence=false;
    };
    ViewValues view_values;
    ViewValues captured_views;
    bool inspecting_views=false;
    std::set<std::string, std::less<>> fixed_values;
    struct BoundValue final { std::string initial, reactive; std::size_t depth; bool member_reference; std::set<std::string> reads{}; };
    struct BoundMethod final { BackendAccess access; std::string method; std::size_t depth; std::string factory{}; std::optional<CallableSignature> signature{}; std::string shared{},factory_initial{}; };
    std::map<std::string, BoundValue, std::less<>> bound_values;
    std::map<std::string, BoundMethod, std::less<>> bound_methods;
    struct CallbackMaterialization final {Value original;std::string shared;bool snapshot=false;};
    struct CallbackStorage final {std::string field;std::size_t depth;bool initialized=true;};
    std::map<std::string,CallbackMaterialization,std::less<>> callback_materializations;
    std::map<std::string,CallbackStorage,std::less<>> callback_storage;
    struct ScalarInput final { Value original; std::string type, field; };
    std::map<std::string, ScalarInput, std::less<>> scalar_inputs;
    mutable bool component_input_runtime = false;
    std::vector<Environment> slot_environments;
    std::size_t next_argument = 0;
    std::map<std::string, std::string, std::less<>> native_values;
    std::string native_initializers;
    std::string native_scope_parts, native_scope_base, scope_constructor, native_style_slots, active_segmented_style,active_tabbar_registration,active_menu_registration,active_menu_divider_registration,active_submenu_registration;
    std::size_t native_scope_next = 0;
    mutable bool native_runtime = false;
    mutable bool method_runtime=false;
    mutable bool inferred_runtime=false;
    mutable bool capturing_receiver=false;
    bool function_variable_runtime = false,object_input_runtime=false;
    struct AmbientReads final {std::size_t depth;std::set<std::string> expressions;};
    mutable std::vector<AmbientReads> ambient_reads;
    bool collection_runtime = false,widget_color_runtime=false,widget_layout_runtime=false,window_runtime=false,native_view_runtime=false,owned_progress_runtime=false,owned_image_runtime=false,owned_button_runtime=false,owned_alert_button_runtime=false,owned_alert_runtime=false,owned_sheet_runtime=false,owned_help_runtime=false,owned_dropdown_runtime=false,owned_tab_runtime=false,owned_tabbar_runtime=false,owned_menu_runtime=false,owned_menu_item_runtime=false,owned_popover_runtime=false,owned_modal_runtime=false,owned_color_button_runtime=false,owned_color_surface_runtime=false,owned_color_picker_runtime=false,owned_color_picker_default_runtime=false,owned_hierarchy_item_runtime=false,owned_hierarchy_list_runtime=false,owned_hierarchy_toolbar_runtime=false,owned_hierarchy_panel_runtime=false,owned_list_panel_runtime=false,owned_list_item_runtime=false,owned_list_item_composite_runtime=false,owned_list_toolbar_runtime=false,owned_list_footer_runtime=false,owned_toolbar_button_runtime=false,owned_table_runtime=false,owned_table_row_runtime=false,owned_table_cell_item_runtime=false,owned_table_header_runtime=false,owned_code_editor_runtime=false,owned_divider_runtime=false,owned_toggle_runtime=false,owned_checkbox_runtime=false,owned_radio_runtime=false,owned_material_runtime=false,owned_link_runtime=false,owned_router_runtime=false,owned_card_runtime=false,owned_media_card_runtime=false,owned_header_runtime=false,owned_listener_runtime=false,owned_avatar_runtime=false,owned_tooltip_runtime=false,owned_slider_runtime=false,owned_input_field_runtime=false,owned_stepper_runtime=false,owned_combo_box_runtime=false,owned_segmented_runtime=false;
    std::string nested_views;
    std::size_t next_scope = 0;
    bool needs_parent_scope = false;
    std::set<std::string, std::less<>> storage_names;
    std::size_t next_backend = 0;
    std::size_t lexical_depth = 0, view_expansion_depth = 0, next_view_instance = 0, view_expansions = 0;
    std::string id_prefix;
    std::set<std::string, std::less<>> ids;
    std::map<std::string,std::string,std::less<>> widget_variables;
    std::map<std::string,std::string,std::less<>> toolbar_parents;
    struct ContextBinding final {std::string menu,target;SourcePosition position;bool tooltip=false;};
    std::vector<ContextBinding> context_bindings;
    std::vector<std::string> initial_open;
    std::size_t factory_depth=0;
    std::string hierarchy_delegate_source;
    std::string body;
    std::size_t next = 0;
    using Inputs = std::map<std::string, const Input*, std::less<>>;

    std::string record_ambient_read(std::string expression)const{
        const auto word=[](char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_';};
        for(auto& capture:ambient_reads){
            if(lexical_depth<capture.depth)continue;
            std::string shifted;char quote=0;bool escaped=false,valid=true,found=false;
            for(std::size_t n=0;n<expression.size();){
                const auto c=expression[n];
                if(quote){shifted+=c;++n;if(escaped)escaped=false;else if(c=='\\')escaped=true;else if(c==quote)quote=0;}
                else if(c=='"'||c=='\''){quote=c;shifted+=c;++n;}
                else if(expression.compare(n,7,"scope->")==0&&(n==0||!word(expression[n-1]))){
                    auto tail=n+7;
                    for(auto depth=capture.depth;depth<lexical_depth;++depth){if(expression.compare(tail,8,"parent->")!=0){valid=false;break;}tail+=8;}
                    if(!valid)break;
                    shifted+="scope->";n=tail;found=true;
                }else{shifted+=c;++n;}
            }
            if(valid&&found)capture.expressions.insert(std::move(shifted));
        }
        return expression;
    }
    auto expression_reads(const Value& value,SourcePosition position)const{
        ambient_reads.push_back({lexical_depth,{}});
        auto expression=expression_code(value,position);
        auto reads=std::move(ambient_reads.back().expressions);ambient_reads.pop_back();
        return std::pair{std::move(expression),std::move(reads)};
    }

    Environment environment() const {
        return {constants, backends, backend_access, blueprint_types, reactive, native_values, backend_types, binding_kinds, view_values, fixed_values, lexical_depth};
    }
    void environment(const Environment& value) {
        if (lexical_depth < value.depth) error({}, "A View value escaped its lexical owner");
        constants = value.constants; backends = value.backends; backend_access = value.access;
        blueprint_types = value.blueprints; reactive = value.reactive; native_values = value.native;
        backend_types = value.backend_types;
        binding_kinds = value.kinds; view_values = value.views;
        fixed_values = value.fixed;
        for (auto depth = value.depth; depth < lexical_depth; ++depth) {
            for (auto& [key, access] : backend_access) { borrow_path(access.object); borrow_path(access.revision); borrow_path(access.dependency); }
            for (auto& [key, access] : reactive) borrow_path(access);
            for (auto& [key, access] : native_values) borrow_path(access);
        }
    }
    void collect_component_kinds(const View& value,std::vector<std::string_view>& kinds,std::size_t depth,std::size_t& work){
        if(depth>=64)error(value.position,"Maximum View value expansion depth is 64");
        if(++work>16384)error(value.position,"View composition inspection limit is 16384");
        const auto outer=environment();
        auto current=value;
        if(current.lexical_context){
            if(current.lexical_context>slot_environments.size())error(value.position,"Invalid supplied View context");
            environment(slot_environments[current.lexical_context-1]);current.lexical_context=0;
        }
        if(current.fragment){
            for(const auto& child:current.children)collect_component_kinds(child,kinds,depth+1,work);
        }else if(current.condition||current.repetition||current.component){
            if(kinds.size()>=4096)error(value.position,"Native widget limit is 4096");
            kinds.push_back(current.condition?std::string_view("if"):current.repetition?std::string_view("for"):current.component->spelling);
        }else{
            const auto found=view_values.find(current.custom_component);
            if(found==view_values.end())error(value.position,"Unknown lexical View value '"+current.custom_component+"'");
            const auto definition=found->second;
            std::erase_if(current.bindings,[&](const auto& binding){return compiler_callback(binding)||compiler_scalar_input(binding);});
            collect_component_kinds(instantiate_view(current,*definition),kinds,depth+1,work);
        }
        environment(outer);
    }
    std::vector<std::string_view> component_kinds(const std::vector<View>& values){
        std::vector<std::string_view> result;std::size_t work=0;
        const auto previous=std::exchange(inspecting_views,true);
        try{for(const auto& value:values)collect_component_kinds(value,result,0,work);}
        catch(...){inspecting_views=previous;throw;}
        inspecting_views=previous;
        return result;
    }
    void require_single_view(const View& value,std::string_view context){
        if(component_kinds({value}).size()!=1)error(value.position,std::string(context)+" requires exactly one concrete root View; wrap the sequence in a stack");
    }
    std::shared_ptr<const ViewDefinition> sequence_definition(const Value& value,SourcePosition at,
            std::optional<std::vector<ComponentParameter>> parameters={}){
        const auto parameter_type=[&](std::string_view name){
            if(parameters)for(const auto& p:*parameters)if(p.name==name)return p.type;
            return std::string{};
        };
        const auto known=[&](std::string_view name,bool sequence_only){
            if(auto found=view_values.find(name);found!=view_values.end())return !sequence_only||found->second->sequence;
            const auto type=parameter_type(name);return type=="[View]"||(!sequence_only&&type=="View");
        };
        if(value.kind==ValueKind::reference&&known(value.text,true)&&!parameters)return view_values.at(value.text);
        View result;result.position=at;
        if(value.kind==ValueKind::list){
            result.fragment=true;
            for(const auto& item:value.elements){
                if(item.kind==ValueKind::view&&item.view)result.children.push_back(*item.view);
                else if(item.kind==ValueKind::list)result.children.push_back(*sequence_definition(item,item.position,parameters)->view);
                else if(item.kind==ValueKind::reference&&known(item.text,false)){
                    View child;child.custom_component=item.text;child.position=item.position;result.children.push_back(std::move(child));
                }else error(item.position,"A View sequence element requires View content");
            }
        }else if(value.kind==ValueKind::view&&value.view&&!value.view->custom_component.empty()&&known(value.view->custom_component,true))result=*value.view;
        else error(at,"A [View] value requires a View sequence or a sequence constructor");
        return std::make_shared<ViewDefinition>(std::make_shared<View>(std::move(result)),environment(),std::move(parameters),true);
    }
    BoundMethod callback_target(const Value& value, SourcePosition position, std::size_t depth=0) const {
        if(depth>=64)error(position,"Maximum callback alias depth is 64");
        if(value.kind==ValueKind::reference)if(auto found=constants.find(value.text);found!=constants.end())
            return callback_target(found->second,position,depth+1);
        if (value.kind != ValueKind::reference && value.kind != ValueKind::internal_argument && value.kind != ValueKind::call &&
            value.kind!=ValueKind::member&&value.kind!=ValueKind::invocation)
            error(position, "Callback requires a C++ method reference or callable factory");
        if (auto found = bound_methods.find(value.text); value.kind == ValueKind::internal_argument && found != bound_methods.end()) {
            auto result = found->second;
            if (lexical_depth < result.depth) error(position, "A supplied callback escaped its owner");
            for (auto depth = result.depth; depth < lexical_depth; ++depth) {
                borrow_path(result.access.object); borrow_path(result.access.revision); borrow_path(result.access.dependency);
                borrow_path(result.factory);
                borrow_path(result.factory_initial);
            }
            result.depth = lexical_depth;
            return result;
        }
        if(value.kind==ValueKind::reference)if(auto found=native_values.find(value.text);found!=native_values.end()){
            inferred_runtime=method_runtime=native_runtime=true;
            const auto selected="lvui::native::inferred::function(context, "+found->second+")";
            const auto factory="[scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+selected+"; }";
            return {{},{},lexical_depth,"lvui::native::Methods::callable(context, "+factory+")",{}, {},"lvui::native::Methods::callable("+factory+")"};
        }
        const auto dot = value.text.find('.');
        const auto backend = backend_access.find(value.text.substr(0, dot));
        const auto method = dot == value.text.npos ? std::string{} : value.text.substr(dot + 1);
        BoundMethod target{{},method,lexical_depth};
        if(backend!=backend_access.end())target.access=backend->second;
        // Callable dependency reads belong to their own value, never to a
        // surrounding collection's intrinsic layout dependencies.
        auto captures=std::exchange(ambient_reads,{});
        try{
            if(value.kind==ValueKind::call||value.kind==ValueKind::invocation){
                if(!scoped_expression(value))error(position,"Unknown native callable factory receiver");
                auto previous=std::exchange(capturing_receiver,true);std::string expression;
                try{expression=expression_code(value,position);}
                catch(...){capturing_receiver=previous;throw;}
                capturing_receiver=previous;
                const auto factory="[scope]([[maybe_unused]] lvui::native::ReadContext& context) -> decltype(auto) { return ("+expression+"); }";
                target.factory="lvui::native::Methods::callable(context, "+factory+")";
                target.factory_initial="lvui::native::Methods::callable("+factory+")";
                method_runtime=native_runtime=true;
            }else{
                auto invoked=value.kind==ValueKind::reference?Value{ValueKind::call,value.text,{},value.position}:
                    Value{ValueKind::invocation,{}, {value},value.position};
                auto query=method_query(invoked);
                if(!query)error(position,"Unknown backend or receiver in callback method reference");
                if(!identifier(query->method))error(position,"Invalid public C++ method reference name");
                const auto* root=&query->receiver;
                for(std::size_t n=0;n<64;++n){
                    auto access=backend_access.find(root->text.substr(0,root->text.find('.')));
                    if(access!=backend_access.end()){target.access=access->second;break;}
                    if((root->kind!=ValueKind::member&&root->kind!=ValueKind::index&&root->kind!=ValueKind::invocation)||root->elements.empty())break;
                    root=&root->elements.front();
                }
                const auto direct=query->receiver.kind==ValueKind::reference?backend_access.find(query->receiver.text):backend_access.end();
                auto previous=std::exchange(capturing_receiver,true);std::string receiver;
                try{receiver=direct!=backend_access.end()?direct->second.object:expression_code(query->receiver,position);}
                catch(...){capturing_receiver=previous;throw;}
                capturing_receiver=previous;
                const auto call="std::forward<decltype(object)>(object)."+query->method+"(std::forward<decltype(arguments)>(arguments)...)";
                const auto selector="[](auto&& object, auto&&... arguments) -> decltype("+call+") { return "+call+"; }";
                const auto member="std::forward<decltype(object)>(object)."+query->method;
                const auto field="[](auto&& object) -> decltype(("+member+")) { return ("+member+"); }";
                std::string reads;
                if(!target.access.revision.empty())reads+="(void)context.read("+target.access.revision+"); ";
                if(!target.access.dependency.empty())reads+="(void)context.read("+target.access.dependency+"); ";
                const auto publication="[scope]([[maybe_unused]] lvui::native::ReadContext& context) { "+reads+"}";
                const auto factory="[scope]([[maybe_unused]] lvui::native::ReadContext& context) -> decltype(auto) { return ("+receiver+"); }";
                target.factory="lvui::native::Methods::reference(context, "+factory+", "+selector+", "+field+", "+publication+")";
                target.factory_initial="lvui::native::Methods::reference("+factory+", "+selector+", "+field+")";
                method_runtime=native_runtime=true;
            }
        }catch(...){ambient_reads=std::move(captures);throw;}
        ambient_reads=std::move(captures);
        return target;
    }
    Value supplied_argument(const Value& value, std::string_view name, SourcePosition position, std::size_t depth = 0) {
        if (depth >= 64) error(position, "Maximum supplied argument depth is 64");
        if(value.kind==ValueKind::internal_argument&&captured_views.contains(value.text))return value;
        std::shared_ptr<const ViewDefinition> visual;
        if(value.kind==ValueKind::reference){if(auto found=view_values.find(value.text);found!=view_values.end())visual=found->second;}
        else if(value.kind==ValueKind::view&&value.view){
            bool sequence=value.view->fragment;
            if(auto found=view_values.find(value.view->custom_component);found!=view_values.end())sequence=found->second->sequence;
            visual=std::make_shared<ViewDefinition>(value.view,environment(),std::nullopt,sequence);
        }else if(value.kind==ValueKind::list){
            const auto contains=[&](auto&& self,const Value& item)->bool{
                return item.kind==ValueKind::view||(item.kind==ValueKind::reference&&view_values.contains(item.text))||
                    (item.kind==ValueKind::list&&std::ranges::any_of(item.elements,[&](const auto& child){return self(self,child);}));
            };
            if(contains(contains,value))visual=sequence_definition(value,position);
        }
        if(visual){const auto key="_lvui_view_"+std::to_string(next_argument++);captured_views.emplace(key,std::move(visual));return {ValueKind::internal_argument,key,{},position};}
        if(inspecting_views)return value;
        if (name == "methods" && value.kind == ValueKind::list) {
            auto result = value;
            for (auto& child : result.elements) child = supplied_argument(child, "method", position, depth + 1);
            return result;
        }
        // A list may be an empty [View] argument to a named component. Its
        // receiving signature, rather than the input's spelling, owns the type.
        if ((name == "method" || name.starts_with("on")) && value.kind != ValueKind::list) {
            const auto key = "_lvui_supplied_" + std::to_string(next_argument++);
            bound_methods.emplace(key, callback_target(value, position));
            return {ValueKind::internal_argument, key, {}, value.position};
        }
        if (scoped_expression(value)) {
            const auto initial = expression_code(value, position, true);
            auto [formula,reads]=expression_reads(value,position);
            const auto key = "_lvui_supplied_" + std::to_string(next_argument++);
            const bool member_reference = value.kind == ValueKind::reference ||
                (value.kind == ValueKind::internal_argument && bound_values.at(value.text).member_reference);
            bound_values.emplace(key, BoundValue{initial, formula, lexical_depth, member_reference,std::move(reads)});
            return {ValueKind::internal_argument, key, {}, value.position};
        }
        if (value.kind == ValueKind::reference) if (auto found = constants.find(value.text); found != constants.end())
            return supplied_argument(found->second, name, position, depth + 1);
        auto result = value;
        for (auto& child : result.elements) child = supplied_argument(child, {}, position, depth + 1);
        return result;
    }
    View supplied_view(const View& invocation, const ViewDefinition& definition) {
        if (!invocation.bindings.empty() || !invocation.imports.empty())
            error(invocation.position, "Put supplied content declarations inside a containing View");
        auto result = *definition.view;
        if ((result.condition || result.repetition) && (!invocation.inputs.empty() || !invocation.children.empty()))
            error(invocation.position, "A conditional or repeated View value cannot override its branch inputs or children");
        std::set<std::string, std::less<>> names;
        for (const auto& input : invocation.inputs) {
            if (input.name.empty()) error(input.position, "A reusable View input must have a name");
            if (!names.insert(input.name).second) error(input.position, "Duplicate reusable View input '" + input.name + "'");
            Input supplied{input.name, supplied_argument(input.value, input.name, input.position), input.position};
            const auto found = std::ranges::find_if(result.inputs, [&](const Input& old) {
                return (old.name.empty() ? "onClick" : old.name) == input.name;
            });
            if (found == result.inputs.end()) result.inputs.push_back(std::move(supplied));
            else *found = std::move(supplied);
        }
        if (!invocation.children.empty()) {
            result.children = invocation.children;
            slot_environments.push_back(environment());
            for (auto& child : result.children) child.lexical_context = slot_environments.size();
        }
        return result;
    }

    struct BorrowedBackend final { BackendAccess access; std::string type; std::size_t index, depth; };
    struct ComponentArgument final {
        Value value{ValueKind::reference, {}, {}};
        std::optional<BorrowedBackend> backend;
        std::shared_ptr<const ViewDefinition> view;
    };
    std::string callback_type(std::string_view type,SourcePosition at)const{
        if(type=="Void"||type=="View")error(at,"Void and View are not native event argument types");
        if(type=="String")return "std::string";
        if(type=="Bool")return "bool";
        if(type=="Int")return "std::int64_t";
        if(type=="Float")return "float";
        if(type=="Double"||type=="Metric")return "double";
        if(type=="Color")return "ii::gui::WidgetColor";
        if(auto imported=blueprint_types.find(type);imported!=blueprint_types.end())return qualified(imported->second,at);
        error(at,"A callback argument requires a scalar or imported C++ type blueprint");
    }
    static std::string callback_signature(const CallableSignature& signature){
        std::string result=(signature.result=="Void"?"void":signature.result)+"(";bool first=true;
        for(const auto& type:signature.arguments){if(!first)result+=", ";first=false;result+=type;}
        return result+")";
    }
    CallableSignature normalized_signature(CallableSignature signature,SourcePosition at)const{
        if(signature.result!="Void")signature.result=callback_type(signature.result,at);
        for(auto& type:signature.arguments)type=callback_type(type,at);
        return signature;
    }
    void callable_declaration(const Binding& binding,std::string& scope){
        ComponentParameter parameter{binding.name,"Callable",{},binding.position,normalized_signature(*binding.callable,binding.position)};
        auto argument=component_argument(parameter,binding.initializer,binding.position);
        auto target=bound_methods.at(argument.value.text);
        std::string variable_type;
        std::optional<VariableSource> variable_source;
        const bool snapshot=binding.annotation==BindingAnnotation::static_value&&!target.shared.empty();
        if(binding.mutable_value){
            const auto signature=callback_signature(*target.signature);
            const auto field="function_var_"+storage_symbol(binding.name);
            const auto type="lvui::native::FunctionVariable<"+signature+">";
            scope+="        std::shared_ptr<"+type+"> "+field+";\n";
            std::string initializer;
            if(!target.shared.empty())initializer=type+"::from_value(scope, native_bindings, "+callback_path(target)+")";
            else{
                const bool method=target.factory.empty(),event=target.signature->result=="Void";
                const auto factory=method?
                    "[] { return []([[maybe_unused]] const auto& scope, auto&&... arguments) -> decltype(auto) { return "+target.access.object+"."+target.method+"(std::forward<decltype(arguments)>(arguments)...); }; }":
                    "[scope]() -> decltype(auto) { return "+target.factory_initial+"; }";
                const auto handler=method?
                    "[&](auto&&... native_arguments) -> decltype(auto) { return handler(scope, std::forward<decltype(native_arguments)>(native_arguments)...); }":std::string("handler");
                auto invoke="lvui::native::invoke_callback<"+signature+">("+handler+", std::forward<decltype(arguments)>(arguments)...)";
                if(event&&!target.access.revision.empty())invoke="lvui::native::invoke("+target.access.revision+", [&] { "+invoke+"; })";
                const auto adapter="[]([[maybe_unused]] const auto& scope, auto& handler, auto&&... arguments) -> "+(event?std::string("void"):target.signature->result)+" { "+(event?"(void)":"return ")+invoke+"; }";
                initializer=type+"::from_native(scope, native_bindings, "+factory+", "+adapter+")";
            }
            native_initializers+="            scope->"+field+" = "+initializer+";\n";
            const auto shared="_lvui_callback_"+std::to_string(next_argument++);
            callback_storage.emplace(shared,CallbackStorage{"scope->"+field+"->value()",lexical_depth});
            target.factory.clear();target.factory_initial.clear();target.shared=shared;
            const auto reference="_lvui_supplied_"+std::to_string(next_argument++);
            bound_methods.emplace(reference,std::move(target));argument.value={ValueKind::internal_argument,reference,{},binding.position};
            variable_type="std::shared_ptr<lvui::native::CallbackValue<"+signature+">>";
            variable_source=VariableSource{"(*scope_->"+field+")",true};
            function_variable_runtime=true;
        }else if(snapshot||!target.factory.empty()){
            if(binding.annotation==BindingAnnotation::static_value&&!snapshot){target.factory=target.factory_initial;bound_methods[argument.value.text]=target;}
            const auto shared="_lvui_callback_"+std::to_string(next_argument++);
            callback_materializations.emplace(shared,CallbackMaterialization{argument.value,shared,snapshot});
            callback_declaration({shared,{},argument.value,false,binding.position},scope);
            target.factory.clear();target.factory_initial.clear();target.shared=shared;
            const auto reference="_lvui_supplied_"+std::to_string(next_argument++);
            bound_methods.emplace(reference,std::move(target));argument.value={ValueKind::internal_argument,reference,{},binding.position};
        }
        const auto& name=binding.name;
        constants.erase(name);reactive.erase(name);native_values.erase(name);fixed_values.erase(name);backends.erase(name);backend_access.erase(name);
        backend_types.erase(name);blueprint_types.erase(name);view_values.erase(name);variable_types.erase(name);variable_sources.erase(name);
        if(variable_source){variable_types.emplace(name,std::move(variable_type));variable_sources.emplace(name,std::move(*variable_source));}
        constants[name]=std::move(argument.value);binding_kinds[name]=ValueKind::reference;native_runtime=true;
    }
    std::optional<BoundMethod> callable_alias(Value value)const{
        for(std::size_t depth=0;depth<64;++depth){
            if(value.kind==ValueKind::internal_argument&&bound_methods.contains(value.text))return callback_target(value,value.position);
            if(value.kind==ValueKind::reference)if(auto found=constants.find(value.text);found!=constants.end()){value=found->second;continue;}
            return {};
        }
        error(value.position,"Maximum callable alias depth is 64");
    }
    std::optional<BoundMethod> function_target(const Value& value)const{
        if(value.kind==ValueKind::call)return callable_alias({ValueKind::reference,value.text,{},value.position});
        if(value.kind==ValueKind::invocation&&!value.elements.empty())return callable_alias(value.elements.front());
        return {};
    }
    std::optional<std::string> inferred_call_source(const Value& value)const{
        const Value* callee=&value;
        if(value.kind==ValueKind::invocation&&!value.elements.empty())callee=&value.elements.front();
        else if(value.kind!=ValueKind::call)return {};
        if((callee->kind==ValueKind::reference||callee->kind==ValueKind::call)&&native_values.contains(callee->text))
            return native_values.at(callee->text);
        return {};
    }
    std::string function_expression(const Value& value,const BoundMethod& target,SourcePosition at,bool initial)const{
        if(!target.signature||target.signature->result=="Void")error(at,"A Void callable has no display value");
        const auto begin=value.kind==ValueKind::invocation?std::size_t(1):std::size_t(0);
        if(value.elements.size()-begin!=target.signature->arguments.size())error(at,"Callable argument count does not match its signature");
        std::string args;
        for(std::size_t index=begin;index<value.elements.size();++index){
            const auto& argument=value.elements[index];const auto& type=target.signature->arguments[index-begin];
            auto expression=argument.kind==ValueKind::reference&&backend_access.contains(argument.text)?backend_access.at(argument.text).object:expression_code(argument,at,initial);
            if(type=="std::int64_t")expression="lvui::native::integer("+expression+")";
            if(!args.empty())args+=", ";args+=expression;
        }
        auto result=method_invocation(target,args);
        if(!target.shared.empty()){
            auto field=callback_path(target);
            if(!initial)(void)record_ambient_read("lvui::native::track_function(context, "+field+")");
            result=std::string(initial?"lvui::native::call_function(":"lvui::native::read_function(context, ")+field+(args.empty()?std::string{}:", "+args)+")";
        }
        if(!initial&&!target.access.revision.empty()){
            const auto read=record_ambient_read("context.read("+target.access.revision+")");result="((void)"+read+", "+result+")";
        }
        if(!initial&&!target.access.dependency.empty()){
            const auto read=record_ambient_read("context.read("+target.access.dependency+")");result="((void)"+read+", "+result+")";
        }
        method_runtime=true;result="lvui::native::Methods::evaluate("+std::string(initial?"":"context, ")+"[&]() -> decltype(auto) { return "+result+"; })";
        return result;
    }
    bool compiler_callback(const Binding& binding)const{
        auto found=callback_materializations.find(binding.name);
        return binding.initializer.kind==ValueKind::internal_argument&&found!=callback_materializations.end()&&
               found->second.original.text==binding.initializer.text;
    }
    bool has_materialized_inputs(const View& view)const{
        return std::ranges::any_of(view.bindings,[&](const auto& binding){return compiler_callback(binding)||compiler_scalar_input(binding);});
    }
    bool compiler_scalar_input(const Binding& binding) const {
        const auto found = scalar_inputs.find(binding.name);
        return found != scalar_inputs.end() && binding.initializer.kind == ValueKind::internal_argument &&
               found->second.original.text == binding.initializer.text;
    }
    void scalar_input_declaration(const Binding& binding, std::string& scope) {
        const auto& input = scalar_inputs.at(binding.name);
        const auto formula = expression_code(input.original, binding.position);
        auto type=input.type;
        if(type.empty()){
            const auto initial=expression_code(input.original,binding.position,true);
            const auto prefix="ScopeStorage_"+std::to_string(native_scope_next++);
            const auto base=native_scope_base.empty()?"ScopeFactoryStorage":native_scope_base;
            native_scope_parts+="    struct "+prefix+" : "+base+" {\n"+scope+
                "        explicit "+prefix+"(const lvui::native::BackendFactories& factories) : "+base+"(factories)"+scope_constructor+" {}\n    };\n";
            scope.clear();scope_constructor.clear();native_scope_base=prefix;
            type="input_type_"+input.field;
            scope+="        using "+type+" = decltype([]("+prefix+"* scope) { return "+initial+"; }(std::declval<"+prefix+"*>()));\n";
        }
        scope += "        std::shared_ptr<lvui::native::ReadOnlyValue<" + type + ">> " + input.field + ";\n";
        native_initializers += "            { auto value = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return " + formula + "; });\n"
            "              if (!value) { (void)native_bindings->close(); return std::unexpected(value.error()); }\n"
            "              scope->" + input.field + " = *value; }\n";
        native_runtime = component_input_runtime = true;
    }
    Value materialize_scalar_input(const ComponentParameter& parameter, const Value& original, std::vector<Binding>& bindings) {
        const auto& captured = bound_values.at(original.text);
        if (lexical_depth < captured.depth) error(parameter.position, "A component input escaped its lexical owner");
        auto reads = captured.reads;
        std::set<std::string> shifted_reads;
        for (auto read : reads) {
            for (auto depth = captured.depth; depth < lexical_depth; ++depth) borrow_path(read);
            shifted_reads.insert(std::move(read));
        }
        const auto name = "_lvui_input_" + std::to_string(next_argument++);
        const auto field = "input_value_" + std::to_string(next_argument++);
        const auto type = parameter.type == "String" ? "std::string" : parameter.type == "Bool" ? "bool" :
            parameter.type == "Int" ? "std::int64_t" : parameter.type == "Color" ? "ii::gui::WidgetColor" :
            parameter.type=="Metric"||parameter.type=="Float"||parameter.type=="Double" ? "double" : "";
        scalar_inputs.emplace(name, ScalarInput{original, type, field});
        bindings.push_back({name, parameter.type, original, false, parameter.position});
        const auto storage = "lvui::native::component_input(scope, [](auto& owner) -> decltype((owner." + field + ")) { return (owner." + field + "); })";
        const auto reference = "_lvui_supplied_" + std::to_string(next_argument++);
        bound_values.emplace(reference, BoundValue{"lvui::native::initial(" + storage + ")", "context.read(" + storage + ")",
            lexical_depth, captured.member_reference, std::move(shifted_reads)});
        return {ValueKind::internal_argument, reference, {}, original.position};
    }
    std::string callback_path(const BoundMethod& target)const{
        auto found=callback_storage.find(target.shared);
        if(found==callback_storage.end()||lexical_depth<found->second.depth)error({},"A callable value escaped its activation");
        if (!found->second.initialized) {
            component_input_runtime = true;
            const auto field = found->second.field.substr(7); // reserved scope->field
            return "lvui::native::component_storage(scope, [](auto& owner) -> decltype((owner." + field + ")) { return (owner." + field + "); })";
        }
        auto field=found->second.field;for(auto depth=found->second.depth;depth<lexical_depth;++depth)borrow_path(field);
        return field;
    }
    std::string shared_action(const BoundMethod& target,const std::string& parameter,const std::string& args)const{
        return "lvui::native::callback(scope, native_bindings, []([[maybe_unused]] const auto& scope"+(parameter.empty()?std::string{}:", "+parameter)+") { auto native_callable = "+callback_path(target)+"; if (!native_callable) throw std::logic_error(\"Callable value not initialized\"); (*native_callable)("+args+"); })";
    }
    void callback_declaration(const Binding& binding,std::string& scope){
        const auto& materialization=callback_materializations.at(binding.name);
        const auto target=callback_target(materialization.original,binding.position);
        const auto signature=callback_signature(*target.signature);
        const auto reserved = callback_storage.find(materialization.shared);
        const auto field = reserved == callback_storage.end() ? "callable_value_" + std::to_string(callback_storage.size()) : reserved->second.field.substr(7);
        callback_storage.insert_or_assign(materialization.shared,CallbackStorage{"scope->"+field,lexical_depth});
        scope+="        std::shared_ptr<lvui::native::CallbackValue<"+signature+">> "+field+";\n";
        if(materialization.snapshot){
            native_initializers+="            scope->"+field+" = lvui::native::snapshot_callback_value(scope, native_bindings, "+callback_path(target)+");\n";
            native_runtime=true;
            return;
        }
        auto invoke="lvui::native::invoke_callback<"+signature+">(handler, std::forward<decltype(arguments)>(arguments)...)";
        const bool event=target.signature->result=="Void";
        if(event&&!target.access.revision.empty())invoke="lvui::native::invoke("+target.access.revision+", [&] { "+invoke+"; })";
        native_initializers+="            scope->"+field+" = lvui::native::make_callback_value<"+signature+">(scope, native_bindings, [scope]([[maybe_unused]] lvui::native::ReadContext& context) -> decltype(auto) { return "+target.factory+"; }, []([[maybe_unused]] const auto& scope, auto& handler, auto&&... arguments) -> "+(event?std::string("void"):target.signature->result)+" { "+(event?"(void)":"return ")+invoke+"; });\n";
        native_runtime=true;
    }
    std::string method_invocation(const BoundMethod& target,std::string args)const{
        if(!target.signature)return target.access.object+"."+target.method+"("+args+")";
        return "lvui::native::invoke_callback<"+callback_signature(*target.signature)+">([&](auto&&... arguments) -> decltype(auto) { return "+target.access.object+"."+target.method+"(std::forward<decltype(arguments)>(arguments)...); }"+(args.empty()?std::string{}:", "+args)+")";
    }
    ComponentArgument component_argument(const ComponentParameter& parameter, const Value& value, SourcePosition at) {
        const bool visual=parameter.type=="View"||parameter.type=="[View]";
        if(value.kind==ValueKind::internal_argument&&captured_views.contains(value.text)){
            const auto definition=captured_views.at(value.text);
            if(!visual||definition->sequence!=(parameter.type=="[View]"))error(at,"A captured View argument does not match its declared View type");
            return {.view=definition};
        }
        if(parameter.callable){
            auto target=callback_target(value,at);
            if(target.signature&&(target.signature->arguments!=parameter.callable->arguments||target.signature->result!=parameter.callable->result))
                error(at,"Callback parameter '"+parameter.name+"' does not match its declared signature");
            target.signature=parameter.callable;
            const auto key="_lvui_supplied_"+std::to_string(next_argument++);
            bound_methods.emplace(key,std::move(target));
            return {.value={ValueKind::internal_argument,key,{},at}};
        }
        if(parameter.type=="[View]")return {.view=sequence_definition(value,at)};
        if (parameter.type == "View") {
            if (value.kind == ValueKind::view && value.view)
            {
                if(auto found=view_values.find(value.view->custom_component);found!=view_values.end()&&found->second->sequence)
                    error(at,"A View parameter requires View content, not a [View] sequence");
                return {.view = std::make_shared<ViewDefinition>(value.view, environment())};
            }
            if (value.kind == ValueKind::reference) if (auto found = view_values.find(value.text); found != view_values.end())
            {
                if(found->second->sequence)error(at,"A View parameter requires View content, not a [View] sequence");
                return {.view = found->second};
            }
            error(at, "Component parameter '" + parameter.name + "' requires View content");
        }
        const bool scalar = parameter.type == "String" || parameter.type == "Bool" || parameter.type == "Int" ||
            parameter.type == "Float" || parameter.type == "Double" || parameter.type == "Metric" || parameter.type=="Color";
        if (!scalar) {
            if(value.kind==ValueKind::reference&&backend_access.contains(value.text)){
                auto access=backend_access.at(value.text);
                if (backend_types.at(value.text).empty())
                    access.object="lvui::native::typed_collection_object<"+qualified(parameter.type,at)+">("+access.object+")";
                else if (backend_types.at(value.text) != parameter.type)
                    error(at, "Backend parameter '" + parameter.name + "' does not match its declared C++ type");
                return {.backend = BorrowedBackend{std::move(access), parameter.type, backends.at(value.text), lexical_depth}};
            }
            const auto expected=qualified(parameter.type,at);
            const auto initial="lvui::native::object_input<"+expected+">("+expression_code(value,at,true)+")";
            ambient_reads.push_back({lexical_depth,{}});
            const auto formula="lvui::native::object_input<"+expected+">("+expression_code(value,at)+")";
            auto reads=std::move(ambient_reads.back().expressions);ambient_reads.pop_back();
            const auto key="_lvui_supplied_"+std::to_string(next_argument++);
            bound_values.emplace(key,BoundValue{initial,formula,lexical_depth,true,std::move(reads)});
            object_input_runtime=native_runtime=true;
            return {.value={ValueKind::internal_argument,key,{},at}};
        }
        if (scoped_expression(value)) {
            if (auto kind = binding_kinds.find(value.text); value.kind == ValueKind::reference && kind != binding_kinds.end()) {
                const bool numeric = kind->second == ValueKind::number || kind->second == ValueKind::metric;
                const bool compatible = (parameter.type == "String" && kind->second == ValueKind::string) ||
                    (parameter.type == "Bool" && kind->second == ValueKind::boolean) ||
                    (parameter.type == "Color" && (kind->second == ValueKind::color || kind->second == ValueKind::string)) ||
                    ((parameter.type == "Int" || parameter.type == "Float" || parameter.type == "Double" || parameter.type == "Metric") && numeric);
                if (!compatible) error(at, "Component parameter '" + parameter.name + "' does not match its reactive scalar type");
            }
            const Binding binding{parameter.name, parameter.type, value, false, at};
            const auto initial = typed_expression(binding, true);
            ambient_reads.push_back({lexical_depth,{}});
            const auto formula=typed_expression(binding);
            auto reads=std::move(ambient_reads.back().expressions);ambient_reads.pop_back();
            const auto key = "_lvui_supplied_" + std::to_string(next_argument++);
            bound_values.emplace(key, BoundValue{initial, formula, lexical_depth, false,std::move(reads)});
            return {.value = {ValueKind::internal_argument, key, {}, at}};
        }
        if(parameter.type=="Color")return {.value=style_token_literal(color_value(value,at))};
        auto result = parameter.type == "Metric" ? resolve_in(value, StyleTokenType::number, at) : resolve(value, at);
        const bool numeric = result.kind == ValueKind::number || result.kind == ValueKind::metric;
        const bool valid = (parameter.type == "String" && result.kind == ValueKind::string) ||
            (parameter.type == "Bool" && result.kind == ValueKind::boolean) ||
            ((parameter.type == "Int" || parameter.type == "Float" || parameter.type == "Double" || parameter.type == "Metric") && numeric);
        if (!valid) error(at, "Component parameter '" + parameter.name + "' does not match its scalar type");
        if (parameter.type == "Int") {
            const auto n = number(result, at);
            if (std::trunc(n) != n || std::abs(n) > 9007199254740991.0)
                error(at, "Int component argument must be an exactly representable integer");
        }
        if (numeric) result.kind = parameter.type == "Metric" ? ValueKind::metric : ValueKind::number;
        return {.value = std::move(result)};
    }
    View instantiate_view(const View& invocation, const ViewDefinition& definition) {
        if(definition.sequence&&!definition.parameters&&(!invocation.inputs.empty()||!invocation.children.empty()||!invocation.imports.empty()||!invocation.bindings.empty()))
            error(invocation.position,"A View sequence cannot override root inputs or child content");
        if (!definition.parameters) {
            auto result = supplied_view(invocation, definition);
            environment(definition.environment);
            return result;
        }
        if (!invocation.children.empty() || !invocation.bindings.empty() || !invocation.imports.empty())
            error(invocation.position, "A parameterized View receives content through its named View parameters");
        std::map<std::string, ComponentArgument, std::less<>> arguments;
        for (const auto& input : invocation.inputs) {
            if (input.name.empty()) error(input.position, "A component argument must have a name");
            const auto parameter = std::ranges::find_if(*definition.parameters, [&](const auto& p) { return p.name == input.name; });
            if (parameter == definition.parameters->end()) error(input.position, "Unknown component parameter '" + input.name + "'");
            if (arguments.contains(input.name)) error(input.position, "Duplicate component argument '" + input.name + "'");
            if(inspecting_views&&parameter->type!="View"&&parameter->type!="[View]")arguments.emplace(input.name,ComponentArgument{});
            else arguments.emplace(input.name, component_argument(*parameter, input.value, input.position));
        }
        environment(definition.environment);
        auto result=*definition.view;
        std::vector<Binding> input_bindings;
        for (const auto& parameter : *definition.parameters) {
            if (!arguments.contains(parameter.name)) {
                if (!parameter.default_value) error(invocation.position, "Missing required component parameter '" + parameter.name + "'");
                if(inspecting_views&&parameter.type!="View"&&parameter.type!="[View]")arguments.emplace(parameter.name,ComponentArgument{});
                else arguments.emplace(parameter.name, component_argument(parameter, *parameter.default_value, parameter.position));
            }
            auto argument = std::move(arguments.at(parameter.name));
            const auto& name = parameter.name;
            constants.erase(name); reactive.erase(name); native_values.erase(name); fixed_values.erase(name);
            backends.erase(name); backend_access.erase(name); backend_types.erase(name); blueprint_types.erase(name);
            binding_kinds.erase(name); view_values.erase(name);
            if(inspecting_views&&parameter.type!="View"&&parameter.type!="[View]")continue;
            if (argument.backend) {
                auto& borrowed = *argument.backend;
                if (lexical_depth < borrowed.depth) error(parameter.position, "A component backend escaped its lexical owner");
                for (auto depth = borrowed.depth; depth < lexical_depth; ++depth) {
                    borrow_path(borrowed.access.object); borrow_path(borrowed.access.revision); borrow_path(borrowed.access.dependency);
                }
                backends[name] = borrowed.index; backend_access[name] = std::move(borrowed.access); backend_types[name] = borrowed.type;
            } else if (argument.view) view_values[name] = std::move(argument.view);
            else {
                if(parameter.callable){
                    auto target=bound_methods.at(argument.value.text);
                    if(!target.factory.empty()&&target.shared.empty()){
                        const auto shared="_lvui_callback_"+std::to_string(next_argument++);
                        callback_materializations.emplace(shared,CallbackMaterialization{argument.value,shared});
                        callback_storage.emplace(shared, CallbackStorage{"scope->callable_value_" + std::to_string(callback_storage.size()), lexical_depth, false});
                        input_bindings.push_back({shared,{},argument.value,false,parameter.position});
                        target.factory.clear();target.shared=shared;
                        const auto reference="_lvui_supplied_"+std::to_string(next_argument++);
                        bound_methods.emplace(reference,std::move(target));
                        argument.value={ValueKind::internal_argument,reference,{},parameter.position};
                    }
                } else if (argument.value.kind == ValueKind::internal_argument && bound_values.contains(argument.value.text)) {
                    argument.value = materialize_scalar_input(parameter, argument.value, input_bindings);
                }
                constants[name] = std::move(argument.value);
                if(parameter.callable)binding_kinds[name]=ValueKind::reference;
                else if(parameter.type=="String")binding_kinds[name]=ValueKind::string;
                else if(parameter.type=="Bool")binding_kinds[name]=ValueKind::boolean;
                else if(parameter.type=="Color")binding_kinds[name]=ValueKind::color;
                else if(parameter.type=="Metric")binding_kinds[name]=ValueKind::metric;
                else if(parameter.type=="Int"||parameter.type=="Double"||parameter.type=="Float")binding_kinds[name]=ValueKind::number;
            }
        }
        result.bindings.insert(result.bindings.begin(),input_bindings.begin(),input_bindings.end());
        return result;
    }

    std::string property(const Value& value)const {
        if(value.kind!=ValueKind::reference || constants.contains(value.text))return {};
        if (auto found = native_values.find(value.text); found != native_values.end()) return found->second;
        if(auto found=reactive.find(value.text);found!=reactive.end())return found->second;
        const auto dot=value.text.find('.');if(dot==value.text.npos)return {};
        auto backend=backends.find(value.text.substr(0,dot));const auto member=value.text.substr(dot+1);
        if(backend==backends.end())return {};
        if (identifier(member)) return backend_access.at(backend->first).object + "." + member;
        if (member.find('.') == member.npos) return {};
        return member_path(value, value.position, true, false);
    }
    std::string member_path(const Value& value, SourcePosition at, bool initial, bool unwrap = true) const {
        const auto first = value.text.find('.');
        const auto name = value.text.substr(0, first);
        const auto backend = backend_access.find(name);
        if (first == value.text.npos || backend == backend_access.end()) error(at, "Unknown C++ member path");
        auto object = backend->second.object;
        auto remaining = std::string_view(value.text).substr(first + 1);
        std::size_t depth = 0;
        while (!remaining.empty()) {
            if (++depth > 64) error(at, "Maximum C++ member path depth is 64");
            const auto next = remaining.find('.');
            const auto name = remaining.substr(0, next);
            if (!identifier(name)) error(at, "Invalid or reserved C++ member path segment");
            const bool last = next == remaining.npos;
            const auto selected = capturing_receiver&&!initial?
                "lvui::native::Methods::receiver_member(context, [scope]([[maybe_unused]] lvui::native::ReadContext& context) -> decltype(auto) { return ("+object+"); }, "+literal(name)+", [](auto&& object) -> decltype(auto) { return (std::forward<decltype(object)>(object)."+std::string(name)+"); })":
                "lvui::native::named_member(" + std::string(initial ? "" : "context, ") + object + ", " + literal(name) + ", [](auto& object) -> decltype(auto) { return (object." + std::string(name) + "); })";
            if (initial) object = last && !unwrap ? selected : "lvui::native::initial(" + selected + ")";
            else object = backend->second.revision.empty()?"lvui::native::read_source(context, "+selected+")":
                "lvui::native::read(context, " + selected + ", " + backend->second.revision + ")";
            if (last){auto result=!initial&&!backend->second.dependency.empty()?"((void)context.read("+backend->second.dependency+"), "+object+")":object;return initial?result:record_ambient_read(std::move(result));}
            remaining.remove_prefix(next + 1);
        }
        error(at, "Empty C++ member path");
    }
    std::string binding_owner() const {
        std::string result;
        if (native_runtime) result += "            auto native_bindings = std::make_shared<lvui::native::Bindings>();\n            if (auto owned_native = widgets.retain(native_bindings); !owned_native) return owned_native;\n            if (auto retiring = widgets.on_retire([weak = std::weak_ptr(native_bindings)] { if (auto owner = weak.lock()) (void)owner->close(); }); !retiring) return retiring;\n";
        return result;
    }

    bool local_member(const Value& value)const{
        if(value.kind!=ValueKind::reference)return false;
        const auto dot=value.text.find('.');if(dot==value.text.npos)return false;
        const auto name=value.text.substr(0,dot);
        if(backends.contains(name))return false;
        if(native_values.contains(name)||reactive.contains(name))return true;
        auto found=constants.find(name);return found!=constants.end()&&found->second.kind==ValueKind::internal_argument&&bound_values.contains(found->second.text);
    }
    struct MethodQuery final {Value receiver;std::string method;};
    std::optional<MethodQuery> method_query(const Value& value)const{
        std::optional<MethodQuery> result;
        if(value.kind==ValueKind::call){
            const auto dot=value.text.rfind('.');
            if(dot!=value.text.npos)result=MethodQuery{Value{ValueKind::reference,value.text.substr(0,dot),{},value.position},value.text.substr(dot+1)};
        }else if(value.kind==ValueKind::invocation&&!value.elements.empty()){
            const auto& callee=value.elements.front();
            if(callee.kind==ValueKind::member&&!callee.elements.empty())result=MethodQuery{callee.elements.front(),callee.text};
        }
        if(!result)return {};
        const auto& receiver=result->receiver;
        if(!(receiver.kind==ValueKind::reference&&backend_access.contains(receiver.text))&&!scoped_expression(receiver))return {};
        return result;
    }
    std::string factory_call(const std::string& receiver,const std::string& method,const std::vector<std::string>& arguments)const{
        const auto call=receiver+"."+method;
        if(arguments.empty())return call+"()";
        std::string locals,probe,actual;
        for(std::size_t index=0;index<arguments.size();++index){
            const auto name="_lvui_factory_arg_"+std::to_string(index),slot=std::to_string(index);
            locals+="auto "+name+" = [scope]([[maybe_unused]] lvui::native::ReadContext& context) -> decltype(auto) { return ("+arguments[index]+"); }; ";
            if(!probe.empty()){probe+=", ";actual+=", ";}
            probe+="lvui::native::Methods::argument_value<selected["+slot+"]>(std::declval<lvui::native::ReadContext&>(), std::declval<decltype("+name+")&>())";
            actual+="lvui::native::Methods::argument_value<retained["+slot+"]>(context, "+name+")";
        }
        return "[&]() -> decltype(auto) { "+locals+"constexpr auto retained = lvui::native::Methods::arguments<"+std::to_string(arguments.size())+">([]<auto selected>() { return requires { std::declval<decltype(("+receiver+"))>()."+method+"("+probe+"); }; }); return "+call+"("+actual+"); }()";
    }
    std::string method_query_code(const Value& value,const MethodQuery& query,SourcePosition at,bool initial)const{
        if(!identifier(query.method))error(at,"Invalid public C++ method name");
        method_runtime=native_runtime=true;
        const auto direct=query.receiver.kind==ValueKind::reference?backend_access.find(query.receiver.text):backend_access.end();
        const auto receiver=direct!=backend_access.end()?direct->second.object:expression_code(query.receiver,at,initial);
        std::string arguments;
        std::vector<std::string> factory_arguments;
        const auto begin=value.kind==ValueKind::invocation?std::size_t(1):std::size_t(0);
        for(auto index=begin;index<value.elements.size();++index){auto argument=expression_code(value.elements[index],at,initial);arguments+=", "+argument;factory_arguments.push_back(std::move(argument));}
        auto result="lvui::native::Methods::query("+std::string(initial?"":"context, ")+receiver+
            ", [](auto&& object, auto&&... arguments) -> decltype(auto) { return std::forward<decltype(object)>(object)."+query.method+"(std::forward<decltype(arguments)>(arguments)...); }"+arguments+")";
        if(capturing_receiver&&!initial){
            result="lvui::native::Methods::receiver_member(context, [scope]([[maybe_unused]] lvui::native::ReadContext& context) -> decltype(auto) { return ("+receiver+"); }, [&](auto&& object) -> decltype(auto) { return "+factory_call("std::forward<decltype(object)>(object)",query.method,factory_arguments)+"; })";
        }
        if(!initial&&query.receiver.kind==ValueKind::reference){
            const auto dot=query.receiver.text.find('.');const auto root=backend_access.find(query.receiver.text.substr(0,dot));
            if(root!=backend_access.end())for(const auto& source:{root->second.revision,root->second.dependency})if(!source.empty()){
                const auto read=record_ambient_read("context.read("+source+")");result="((void)"+read+", "+result+")";
            }
        }
        return result;
    }
    bool scoped_expression(const Value& value) const {
        if(inferred_call_source(value))return true;
        if(method_query(value))return true;
        if(local_member(value))return true;
        if(function_target(value))return true;
        if (value.kind == ValueKind::internal_argument && bound_values.contains(value.text)) return true;
        if (value.kind == ValueKind::reference) if (auto found = constants.find(value.text);
            found != constants.end() && found->second.kind == ValueKind::internal_argument)
            return bound_values.contains(found->second.text);
        if (value.kind == ValueKind::reference && native_values.contains(value.text)) return true;
        if (!property(value).empty()) return true;
        if (value.kind == ValueKind::call && backends.contains(value.text.substr(0, value.text.find('.')))) return true;
        return std::ranges::any_of(value.elements, [&](const Value& child) { return scoped_expression(child); });
    }
    bool native_expression(const Value& value) const {
        if(inferred_call_source(value))return true;
        if(method_query(value))return true;
        return local_member(value) || bool(function_target(value)) || (value.kind == ValueKind::internal_argument && bound_values.contains(value.text)) || !property(value).empty() ||
               (value.kind == ValueKind::reference && scoped_expression(value)) ||
               (is_expression(value.kind) && scoped_expression(value)) ||
               (value.kind == ValueKind::call && scoped_expression(value));
    }
    std::string expression_code(const Value& value, SourcePosition at, bool initial = false) const {
        native_runtime = true;
        if(auto target=function_target(value))return function_expression(value,*target,at,initial);
        if(auto source=inferred_call_source(value)){
            inferred_runtime=true;
            std::string arguments;
            for(auto index=value.kind==ValueKind::invocation?std::size_t(1):std::size_t(0);index<value.elements.size();++index)
                arguments+=", "+expression_code(value.elements[index],at,initial);
            if(!initial)(void)record_ambient_read("lvui::native::inferred::track(context, "+*source+")");
            return "lvui::native::inferred::call("+std::string(initial?"":"context, ")+*source+arguments+")";
        }
        const auto recurse = [&](const Value& child) { return expression_code(child, at, initial); };
        const auto prefix = std::string("lvui::native::");
        if(value.kind==ValueKind::list){
            if(value.elements.empty())error(at,"An empty list requires a typed C++ collection");
            std::string result=prefix+"list(";
            for(const auto& item:value.elements){if(result.back()!='(')result+=", ";result+=recurse(item);}
            return result+")";
        }
        if (value.kind == ValueKind::reference) if (auto found = constants.find(value.text);
            found != constants.end() && found->second.kind == ValueKind::internal_argument)
            return expression_code(found->second, at, initial);
        if (value.kind == ValueKind::internal_argument) if (auto found = bound_values.find(value.text); found != bound_values.end()) {
            if (lexical_depth < found->second.depth) error(at, "A supplied argument escaped its owner");
            auto result = initial ? found->second.initial : found->second.reactive;
            for (auto depth = found->second.depth; depth < lexical_depth; ++depth) borrow_path(result);
            if(!initial)for(auto read:found->second.reads){for(auto depth=found->second.depth;depth<lexical_depth;++depth)borrow_path(read);(void)record_ambient_read(std::move(read));}
            return result;
        }
        if (value.kind == ValueKind::reference) {
            if(auto found=backend_access.find(value.text);found!=backend_access.end()){
                auto result=found->second.object;
                if(!initial)for(const auto& dependency:{found->second.revision,found->second.dependency})if(!dependency.empty())result="((void)"+record_ambient_read("context.read("+dependency+")")+", "+result+")";
                return result;
            }
            const auto first = value.text.find('.');
            if(local_member(value)){
                auto result=expression_code(Value{ValueKind::reference,value.text.substr(0,first),{},value.position},at,initial);
                auto rest=std::string_view(value.text).substr(first+1);std::size_t depth=0;
                while(!rest.empty()){
                    if(++depth>64)error(at,"Maximum native value member depth is 64");
                    const auto dot=rest.find('.');const auto field=rest.substr(0,dot);
                    if(!identifier(field))error(at,"Invalid public native value member");
                    result="lvui::native::named_member("+std::string(initial ? "" : "context, ")+result+", "+literal(field)+", [](const auto& value) -> decltype(auto) { return (value."+std::string(field)+"); })";
                    result=initial?"lvui::native::initial("+result+")":"lvui::native::read_source(context, "+result+")";
                    if(dot==rest.npos)return result;rest.remove_prefix(dot+1);
                }
                error(at,"Empty native value member path");
            }
            if (first != value.text.npos && value.text.find('.', first + 1) != value.text.npos && backends.contains(value.text.substr(0, first)))
                return member_path(value, at, initial);
        }
        if (auto found = native_values.find(value.text); value.kind == ValueKind::reference && found != native_values.end()) {
            if (fixed_values.contains(value.text)) return found->second;
            return initial ? prefix + "initial(" + found->second + ")" : record_ambient_read("context.read(" + found->second + ")");
        }
        if (auto source = property(value); !source.empty()) {
            if (initial) return prefix + "initial(" + source + ")";
            const auto dot = value.text.find('.');
            if (auto backend = backends.find(value.text.substr(0, dot)); backend != backends.end()) {
                const auto& access=backend_access.at(backend->first);
                auto result=access.revision.empty()?prefix+"read_source(context, "+source+")":prefix + "read(context, " + source + ", " + access.revision + ")";
                return record_ambient_read(access.dependency.empty()?result:"((void)context.read("+access.dependency+"), "+result+")");
            }
            return record_ambient_read("context.read(" + source + ")");
        }
        if (value.kind == ValueKind::unary) {
            const auto operand = recurse(value.elements.at(0));
            if (value.text == "!") return "(!" + prefix + "truth(" + operand + "))";
            if (value.text == "-") return prefix + "negate(" + operand + ")";
            return prefix + "number(" + operand + ")";
        }
        if (value.kind == ValueKind::binary) {
            const auto left = recurse(value.elements.at(0)), right = recurse(value.elements.at(1));
            if (value.text == "&&" || value.text == "||")
                return "(" + prefix + "truth(" + left + ") " + value.text + " " + prefix + "truth(" + right + "))";
            static const std::map<std::string, std::string, std::less<>> arithmetic{
                {"+", "add"}, {"-", "subtract"}, {"*", "multiply"}, {"/", "divide"}, {"%", "remainder"}};
            if (auto op = arithmetic.find(value.text); op != arithmetic.end()) return prefix + op->second + "(" + left + ", " + right + ")";
            if (value.text == "==" || value.text == "!=")
                return std::string(value.text == "!=" ? "(!" : "(") + prefix + "equal(" + left + ", " + right + "))";
            if (value.text == "<") return prefix + "less(" + left + ", " + right + ")";
            if (value.text == ">") return prefix + "less(" + right + ", " + left + ")";
            if (value.text == "<=") return "(!" + prefix + "less(" + right + ", " + left + "))";
            if (value.text == ">=") return "(!" + prefix + "less(" + left + ", " + right + "))";
            error(at, "Unknown native binary operator");
        }
        if(value.kind==ValueKind::conditional&&capturing_receiver&&!initial)
            return "(lvui::native::truth("+recurse(value.elements.at(0))+") ? "+recurse(value.elements.at(1))+" : "+recurse(value.elements.at(2))+")";
        if (value.kind == ValueKind::conditional)
            return prefix + "choose(" + prefix + "truth(" + recurse(value.elements.at(0)) + "), [&] { return " + recurse(value.elements.at(1)) + "; }, [&] { return " + recurse(value.elements.at(2)) + "; })";
        if(value.kind==ValueKind::index&&capturing_receiver&&!initial)
            return "lvui::native::Methods::receiver_index(context, [scope]([[maybe_unused]] lvui::native::ReadContext& context) -> decltype(auto) { return ("+recurse(value.elements.at(0))+"); }, [scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+recurse(value.elements.at(1))+"; })";
        if (value.kind == ValueKind::index) return prefix + "index(" + recurse(value.elements.at(0)) + ", " + recurse(value.elements.at(1)) + ")";
        if (value.kind == ValueKind::member) {
            if (!identifier(value.text)) error(at, "Invalid public C++ member name");
            if(capturing_receiver&&!initial){
                const auto selected="lvui::native::Methods::receiver_member(context, [scope]([[maybe_unused]] lvui::native::ReadContext& context) -> decltype(auto) { return ("+recurse(value.elements.at(0))+"); }, "+literal(value.text)+", [](auto&& object) -> decltype(auto) { return (std::forward<decltype(object)>(object)."+value.text+"); })";
                return "lvui::native::Methods::receiver_value(context, "+selected+")";
            }
            const auto selected="lvui::native::named_member("+std::string(initial ? "" : "context, ")+recurse(value.elements.at(0))+
                ", "+literal(value.text)+", [](const auto& value) -> decltype(auto) { return (value."+value.text+"); })";
            return initial ? "lvui::native::initial("+selected+")" : "lvui::native::read_source(context, "+selected+")";
        }
        if (value.kind == ValueKind::call) {
            if(value.text=="Color"){
                if(value.elements.size()==1)return color_expression(value.elements.front(),at,initial);
                if(value.elements.size()!=4)error(at,"Color requires one color or four numeric channels");
                std::string result="lvui::native::color<ii::gui::WidgetColor>(";
                for(const auto& channel:value.elements){if(result.back()!='(')result+=", ";result+=recurse(channel);}return result+")";
            }
            if(value.text=="Metric"){
                if(value.elements.size()!=1)error(at,"Metric requires one numeric value");
                return "lvui::native::number("+recurse(value.elements.front())+")";
            }
            if(value.text=="composite"){
                if(value.elements.size()!=2)error(at,"composite requires two Colors");
                return "lvui::native::composite_color<ii::gui::WidgetColor>("+color_expression(value.elements[0],at,initial)+", "+color_expression(value.elements[1],at,initial)+")";
            }
            const auto dot = value.text.find('.');
            if (const auto backend = backends.find(value.text.substr(0, dot)); backend != backends.end() && dot != value.text.npos && value.text.find('.',dot+1)==value.text.npos) {
                const auto method = value.text.substr(dot + 1);
                if (!identifier(method)) error(at, "Invalid public C++ method name");
                std::string arguments;
                std::vector<std::string> factory_arguments;
                for (const auto& argument : value.elements) {
                    if (!arguments.empty()) arguments += ", ";
                    auto expression=recurse(argument);arguments+=expression;factory_arguments.push_back(std::move(expression));
                }
                const auto& access = backend_access.at(backend->first);
                const auto call = capturing_receiver?factory_call(access.object,method,factory_arguments):access.object+"."+method+"("+arguments+")";
                method_runtime=true;
                if(initial)return "lvui::native::Methods::evaluate([&]() -> decltype(auto) { return "+call+"; })";
                const auto tracked="lvui::native::Methods::evaluate(context, [&]() -> decltype(auto) { return "+call+"; })";
                if(!access.revision.empty())(void)record_ambient_read("context.read("+access.revision+")");
                if(!access.dependency.empty())(void)record_ambient_read("context.read("+access.dependency+")");
                auto result=access.revision.empty()?tracked:"((void)context.read(" + access.revision + "), " + tracked + ")";
                return access.dependency.empty()?result:"((void)context.read("+access.dependency+"), "+result+")";
            }
        }
        if(auto query=method_query(value))return method_query_code(value,*query,at,initial);
        const auto constant = resolve(value, at);
        if (constant.kind == ValueKind::string) return "std::string(" + literal(constant.text) + ")";
        if (constant.kind == ValueKind::boolean) return constant.text;
        if(constant.kind==ValueKind::color)return color_literal(color_value(constant,at));
        if (constant.kind == ValueKind::number || constant.kind == ValueKind::metric) {
            auto result = decimal(number(constant, at));
            if (result.find_first_of(".eE") == result.npos) result += ".0";
            return result;
        }
        error(at, "This value cannot be used in a native scalar expression");
    }
    std::string typed_expression(const Binding& binding, bool initial = false) const {
        if(binding.type=="Color")return color_expression(binding.initializer,binding.position,initial);
        auto expression = expression_code(binding.initializer, binding.position, initial);
        if (binding.type == "Int") expression = "lvui::native::integer(" + expression + ")";
        else if (binding.type == "Float" || binding.type == "Double" || binding.type == "Metric") expression = "lvui::native::number(" + expression + ")";
        else if (binding.type == "Bool") expression = "lvui::native::truth(" + expression + ")";
        else if (binding.type == "String") expression = "std::string(" + expression + ")";
        else if (!binding.type.empty()) error(binding.position, "Unsupported native declaration type");
        return "lvui::native::value_copy(" + expression + ")";
    }
    std::string storage_symbol(const std::string& name) {
        if (storage_names.insert(name).second) return name;
        for (std::size_t suffix = 1;; ++suffix) {
            auto candidate = name + "_" + std::to_string(suffix);
            if (storage_names.insert(candidate).second) return candidate;
        }
    }
    std::string inferred_definition(const Value& value,SourcePosition at,bool initial)const{
        inferred_runtime=method_runtime=native_runtime=true;
        BackendAccess access;
        const auto* root=&value;
        for(std::size_t depth=0;depth<64;++depth){
            if(auto found=backend_access.find(root->text.substr(0,root->text.find('.')));found!=backend_access.end()){
                access=found->second;break;
            }
            if((root->kind!=ValueKind::member&&root->kind!=ValueKind::index&&root->kind!=ValueKind::invocation)||root->elements.empty())break;
            root=&root->elements.front();
        }
        const auto marker=access.revision.empty()?std::string("lvui::native::inferred::Marker{}"):
            "lvui::native::inferred::Marker{[]([[maybe_unused]] const auto& scope, auto action) -> decltype(auto) { return lvui::native::invoke("+access.revision+", std::move(action)); }}";
        // Evaluate every initializer in a private reader, including @Static and
        // var. It retains temporary receivers while keeping their dependencies
        // out of the caller's graph for a fixed selection.
        auto previous=std::exchange(capturing_receiver,true);
        std::string result;
        try{
            std::optional<MethodQuery> query;
            if(value.kind==ValueKind::member||(value.kind==ValueKind::reference&&value.text.find('.')!=value.text.npos)){
                auto invoked=value.kind==ValueKind::reference?Value{ValueKind::call,value.text,{},value.position}:
                    Value{ValueKind::invocation,{}, {value},value.position};
                query=method_query(invoked);
            }
            if(query){
                if(!identifier(query->method))error(at,"Invalid public inferred C++ member name");
                const auto direct=query->receiver.kind==ValueKind::reference?backend_access.find(query->receiver.text):backend_access.end();
                const auto receiver=direct!=backend_access.end()?direct->second.object:expression_code(query->receiver,at);
                const auto member="std::forward<decltype(object)>(object)."+query->method;
                const auto call=member+"(std::forward<decltype(arguments)>(arguments)...)";
                const auto field="[](auto&& object) -> decltype(("+member+")) { return ("+member+"); }";
                const auto invoke="[](auto&& object, auto&&... arguments) -> decltype("+call+") { return "+call+"; }";
                const auto pointer="[](auto&& object) -> decltype(&std::remove_cvref_t<decltype(object)>::"+query->method+") { return &std::remove_cvref_t<decltype(object)>::"+query->method+"; }";
                const auto factory="[scope]([[maybe_unused]] lvui::native::ReadContext& context) -> decltype(auto) { return ("+receiver+"); }";
                std::string reads;
                for(const auto& source:{access.revision,access.dependency})if(!source.empty())reads+="(void)context.read("+source+"); ";
                result="lvui::native::Methods::inferred_named_member("+std::string(initial?"":"context, ")+factory+", "+literal(query->method)+", "+field+", "+invoke+", "+pointer+", ";
                if(!initial)result+="[scope]([[maybe_unused]] lvui::native::ReadContext& context) { "+reads+"}, ";
                result+=marker+")";
            }else{
                std::string expression;
                if(auto alias=callable_alias(value);alias&&!alias->shared.empty()){
                    const auto path=callback_path(*alias);
                    expression="((void)lvui::native::track_function(context, "+path+"), "+path+")";
                }else expression=expression_code(value,at);
                result="lvui::native::Methods::inferred_result("+std::string(initial?"":"context, ")+
                    "[scope]([[maybe_unused]] lvui::native::ReadContext& context) -> decltype(auto) { return ("+expression+"); }, "+marker+")";
            }
        }catch(...){capturing_receiver=previous;throw;}
        capturing_receiver=previous;
        return result;
    }
    void native_declaration(const Binding& binding, std::string& scope) {
        if(binding.initializer.kind==ValueKind::list&&!binding.type.empty())error(binding.position,"A list declaration currently infers its native element type");
        std::optional<ValueKind> inferred_kind;
        if(binding.initializer.kind==ValueKind::list)inferred_kind=ValueKind::list;
        else if(binding.initializer.kind==ValueKind::reference)
            if(auto known=binding_kinds.find(binding.initializer.text);known!=binding_kinds.end())inferred_kind=known->second;
        if (const auto known = binding_kinds.find(binding.initializer.text);
            binding.initializer.kind == ValueKind::reference && known != binding_kinds.end() && !binding.type.empty()) {
            const auto kind = known->second;
            const bool valid = (binding.type == "String" && kind == ValueKind::string) ||
                               (binding.type == "Bool" && kind == ValueKind::boolean) ||
                               (binding.type == "Color" && (kind == ValueKind::color || kind == ValueKind::string)) ||
                               ((binding.type == "Metric" || binding.type == "Int" || binding.type == "Double" || binding.type == "Float") &&
                                (kind == ValueKind::number || kind == ValueKind::metric));
            if (!valid) error(binding.position, "Declaration type does not match reactive initializer");
        }
        const bool inferred=binding.type.empty();
        const auto initial = inferred?inferred_definition(binding.initializer,binding.position,true):typed_expression(binding,true);
        const auto formula = inferred?inferred_definition(binding.initializer,binding.position,false):typed_expression(binding);
        const auto name=storage_symbol(binding.name);
        const auto& key = binding.name;
        constants.erase(key); reactive.erase(key); native_values.erase(key); fixed_values.erase(key);
        view_values.erase(key);
        backends.erase(key); backend_access.erase(key); backend_types.erase(key); binding_kinds.erase(key);
        blueprint_types.erase(key);
        variable_types.erase(key); variable_sources.erase(key);
        // Infer against the already complete prefix, never an incomplete Scope.
        // Prefix storage adds no allocation, virtual dispatch or backend copy.
        const auto prefix = "ScopeStorage_" + std::to_string(native_scope_next++);
        const auto base=native_scope_base.empty()?"ScopeFactoryStorage":native_scope_base;
        native_scope_parts += "    struct " + prefix + " : " + base + " {\n" + scope +
            "        explicit "+prefix+"(const lvui::native::BackendFactories& factories) : "+base+"(factories)"+scope_constructor+" {}\n    };\n";
        scope.clear();
        scope_constructor.clear();
        native_scope_base = prefix;
        if(inferred){
            const auto mode="lvui::native::inferred::Mode::"+std::string(binding.mutable_value?"variable":binding.annotation==BindingAnnotation::static_value?"fixed":"readonly");
            const auto field="inferred_"+name;
            scope+="        using definition_"+name+" = decltype([]("+prefix+"* scope) { return "+initial+"; }(std::declval<"+prefix+"*>()));\n";
            scope+="        using slot_"+name+" = lvui::native::inferred::Slot<definition_"+name+", "+mode+">;\n";
            scope+="        using type_"+name+" = typename slot_"+name+"::value_type;\n        std::shared_ptr<slot_"+name+"> "+field+";\n";
            native_initializers+="            scope->"+field+" = Scope::slot_"+name+"::make(scope, native_bindings, [scope] { return "+initial+"; }, [scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+formula+"; });\n";
            if(binding.annotation==BindingAnnotation::static_value&&!binding.mutable_value){
                native_values.emplace(key,"scope->"+field+"->get()");fixed_values.insert(key);
            }else native_values.emplace(key,"(*scope->"+field+")");
            if(binding.mutable_value){
                variable_types.emplace(key,"Scope::type_"+name);
                variable_sources.emplace(key,VariableSource{"(*scope_->"+field+")",false,true});
            }
            if(inferred_kind)binding_kinds[key]=*inferred_kind;
            return;
        }
        scope += "        using type_" + name + " = decltype([](" + prefix + "* scope) { return " + initial + "; }(std::declval<" + prefix + "*>()));\n";
        if (binding.mutable_value) {
            scope += "        std::unique_ptr<ii::core::Observable<type_" + name + ">> state_" + name + ";\n";
            native_initializers += "            scope->state_" + name + " = std::make_unique<ii::core::Observable<typename Scope::type_" + name + ">>(" + initial + ");\n";
            native_values.emplace(key, "(*scope->state_" + name + ")");
            variable_types.emplace(key, "Scope::type_" + name);
            variable_sources.emplace(key, VariableSource{"(*scope_->state_" + name + ")"});
        } else if (binding.annotation == BindingAnnotation::static_value) {
            scope += "        std::optional<type_" + name + "> fixed_" + name + ";\n";
            native_initializers += "            scope->fixed_" + name + ".emplace(" + initial + ");\n";
            native_values.emplace(key, "(*scope->fixed_" + name + ")");
            fixed_values.insert(key);
        } else {
            scope += "        std::shared_ptr<lvui::native::ReadOnlyValue<type_" + name + ">> derived_" + name + ";\n";
            native_initializers += "            { auto value = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return " + formula + "; });\n              if (!value) { (void)native_bindings->close(); return std::unexpected(value.error()); }\n              scope->derived_" + name + " = *value; }\n";
            native_values.emplace(key, "(*scope->derived_" + name + ")");
        }
        if (!binding.type.empty()) binding_kinds[binding.name] = binding.type == "String" ? ValueKind::string : binding.type == "Bool" ? ValueKind::boolean : binding.type == "Color" ? ValueKind::color : binding.type == "Metric" ? ValueKind::metric : ValueKind::number;
        else if(inferred_kind)binding_kinds[binding.name]=*inferred_kind;
    }

    Value resolve(const Value& value, SourcePosition at) const {
        if (is_expression(value.kind)) {
            auto folded = fold_constant_expression(value, [this, at](const Value& leaf) -> ConstantExpressionResult {
                try { return resolve(leaf, at); }
                catch (Diagnostic& diagnostic) { return std::unexpected(std::move(diagnostic)); }
            }, at);
            if (!folded) throw std::move(folded.error());
            return std::move(*folded);
        }
        if (value.kind == ValueKind::style_member)
            return style_member(value.text, at);
        if (value.kind == ValueKind::call && (value.text == "Color" || value.text == "Metric")) {
            if (value.text == "Color" && value.elements.size() == 4) {
                double channels[4]{};
                for (std::size_t i = 0; i < 4; ++i)
                    channels[i] = number(resolve_in(value.elements[i], StyleTokenType::number, at), at);
                StyleColor color{channels[0], channels[1], channels[2], channels[3]};
                if (!valid_style_token_value(color))
                    error(at, "Color RGBA channels must be finite and within 0..1");
                return style_token_literal(color);
            }
            if (value.elements.size() != 1)
                error(at, value.text + "() requires one value");
            if (value.text == "Color")
                return style_token_literal(color_value(value.elements.front(), at));
            auto result = resolve_in(value.elements.front(), StyleTokenType::number, at);
            (void)number(result, at);
            result.kind = ValueKind::metric;
            return result;
        }
        if (value.kind == ValueKind::call && value.text == "token") {
            if (value.elements.size() != 1)
                error(at, "token() requires one String token name");
            auto name = resolve(value.elements.front(), at);
            if (name.kind != ValueKind::string)
                error(at, "token() requires one String token name");
            auto resolved = default_style_tokens().resolve(name.text, options.style_tokens);
            if (!resolved)
                error(at, resolved.error().message);
            return style_token_literal(*resolved);
        }
        if (value.kind == ValueKind::call && value.text == "composite") {
            if (value.elements.size() != 2)
                error(at, "composite() requires foreground and background Colors");
            return style_token_literal(
                composite_srgb(color_value(value.elements[0], at), color_value(value.elements[1], at)));
        }
        if (value.kind != ValueKind::reference) return value;
        if (auto it = constants.find(value.text); it != constants.end()) return it->second;
        const auto dot=value.text.find('.');
        if(dot!=value.text.npos)if(auto item=constants.find(value.text.substr(0,dot));item!=constants.end()&&item->second.kind==ValueKind::color){
            const std::map<std::string,std::size_t,std::less<>> fields{{"red",0},{"green",1},{"blue",2},{"alpha",3}};
            if(auto selected=fields.find(value.text.substr(dot+1));selected!=fields.end())return item->second.elements.at(selected->second);
        }
        if (value.text.find('.') != value.text.npos) error(at, "Reactive C++ property bindings are not supported by native AOT yet");
        error(at, "Unknown value '" + value.text + "'");
    }
    Value style_member(std::string_view name, SourcePosition at) const {
        auto selected = default_style_tokens().select(name, options.style_tokens);
        if (!selected)
            error(at, selected.error().message);
        auto value = style_token_literal(*selected);
        if (name.starts_with("Metric."))
            value.kind = ValueKind::metric;
        return value;
    }
    Value resolve_in(const Value &value, StyleTokenType context, SourcePosition at) const {
        if (is_expression(value.kind)) {
            auto folded = fold_constant_expression(value, [this, context, at](const Value& leaf) -> ConstantExpressionResult {
                try { return resolve_in(leaf, context, at); }
                catch (Diagnostic& diagnostic) { return std::unexpected(std::move(diagnostic)); }
            }, at);
            if (!folded) throw std::move(folded.error());
            return std::move(*folded);
        }
        if (value.kind == ValueKind::enumeration)
            return style_member(
                std::string(context == StyleTokenType::color ? "Color" : "Metric") + value.text, at);
        return resolve(value, at);
    }
    double token_number(std::string_view name, double low, double high) const {
        auto value = default_style_tokens().number(name, options.style_tokens);
        if (!value)
            error({}, value.error().message);
        if (*value < low || *value > high)
            error({}, "Default style token outside supported range: " + std::string(name));
        return *value;
    }
    StyleColor token_color(std::string_view name) const {
        auto value = default_style_tokens().color(name, options.style_tokens);
        if (!value)
            error({}, value.error().message);
        return *value;
    }
    StyleColor color_value(const Value &original, SourcePosition at) const {
        const auto value = resolve_in(original, StyleTokenType::color, at);
        if (value.kind == ValueKind::color && value.elements.size() == 4) {
            StyleColor c{number(value.elements[0], at), number(value.elements[1], at),
                         number(value.elements[2], at), number(value.elements[3], at)};
            if (!valid_style_token_value(c))
                error(at, "Invalid Color channels");
            return c;
        }
        if (value.kind == ValueKind::string) {
            auto parsed = parse_style_color(value.text);
            if (!parsed && (value.text.size() == 6 || value.text.size() == 8) && !value.text.starts_with('#'))
                parsed = parse_style_color("#" + value.text);
            if (parsed)
                return *parsed;
        }
        error(at, "Color requires a Color token or a hexadecimal RGB/RGBA String");
    }
    static std::string color_literal(StyleColor c) {
        return "ii::gui::WidgetColor{" + decimal(c.red) + ", " + decimal(c.green) + ", " + decimal(c.blue) +
               ", " + decimal(c.alpha) + "}";
    }
    std::string color_expression(const Value& value,SourcePosition at,bool initial=false)const{
        if(!native_expression(value))return color_literal(color_value(value,at));
        if(value.kind==ValueKind::conditional)return "lvui::native::color<ii::gui::WidgetColor>(lvui::native::choose(lvui::native::truth("+expression_code(value.elements[0],at,initial)+"), [&] { return "+color_expression(value.elements[1],at,initial)+"; }, [&] { return "+color_expression(value.elements[2],at,initial)+"; }))";
        return "lvui::native::color<ii::gui::WidgetColor>("+expression_code(value,at,initial)+")";
    }
    static std::string linear_color_literal(StyleColor c) {
        const auto linear = [](double v) {
            return v <= .04045 ? v / 12.92 : std::pow((v + .055) / 1.055, 2.4);
        };
        return "ii::gui::Color{" + decimal(linear(c.red)) + ", " + decimal(linear(c.green)) + ", " +
               decimal(linear(c.blue)) + ", " + decimal(c.alpha) + "}";
    }
    void emit_stack_style(const Inputs &values, const std::string &variable) {
        const auto background = values.find("backgroundColor"), border = values.find("borderColor");
        if (background == values.end() && border == values.end() && !values.contains("borderWidth") &&
            !values.contains("cornerRadius"))
            return;
        const bool dynamic=std::ranges::any_of(values,[&](const auto& item){return (item.first=="backgroundColor"||item.first=="borderColor"||item.first=="borderWidth"||item.first=="cornerRadius")&&native_expression(item.second->value);});
        if(dynamic){
            widget_color_runtime=true;
            const auto optional_color=[&](std::string_view name){auto found=values.find(name);return found==values.end()?std::string("std::optional<ii::gui::WidgetColor>{}"):"std::optional<ii::gui::WidgetColor>{"+color_expression(found->second->value,found->second->position)+"}";};
            const auto metric=[&](std::string_view name,double fallback){auto found=values.find(name);return found!=values.end()&&native_expression(found->second->value)?"lvui::native::range("+expression_code(found->second->value,found->second->position)+", 0, 8192)":decimal(numeric(values,name,fallback,0,8192));};
            body+="            { auto bound = lvui::native::bind_panel_colors(*native_bindings, *"+variable+", [scope]([[maybe_unused]] lvui::native::ReadContext& context) { return lvui::native::WidgetPanelColors{"+optional_color("backgroundColor")+", "+optional_color("borderColor")+", "+metric("borderWidth",border==values.end()?0:token_number("--lvrs-theme-metrics-stroke-1",0,8192))+", "+metric("cornerRadius",0)+"}; }); if (!bound) return std::unexpected(bound.error()); }\n";
            return;
        }
        const auto radius = decimal(numeric(values, "cornerRadius", 0, 0, 8192));
        const auto width = decimal(numeric(
            values, "borderWidth",
            border == values.end() ? 0 : token_number("--lvrs-theme-metrics-stroke-1", 0, 8192), 0, 8192));
        std::string panels;
        if (background != values.end()) {
            const auto c =
                linear_color_literal(color_value(background->second->value, background->second->position));
            panels +=
                "                drawing->panels.push_back({.bounds = {0, 0, "
                "static_cast<float>(state.size.width), static_cast<float>(state.size.height)}, .top = " +
                c + ", .bottom = " + c + ", .radius = " + radius + "});\n";
        }
        if (width != "0") {
            if (border == values.end())
                error(values.at("borderWidth")->position, "A stack borderWidth requires borderColor");
            const auto c = linear_color_literal(color_value(border->second->value, border->second->position));
            panels +=
                "                drawing->panels.push_back({.bounds = {0, 0, "
                "static_cast<float>(state.size.width), static_cast<float>(state.size.height)}, .top = " +
                c + ", .bottom = " + c + ", .radius = " + radius + ", .stroke_width = " + width + "});\n";
        }
        body += "            if (auto painted = " + variable +
                "->set_paint_handler([](const ii::gui::WidgetPaintState& state) -> "
                "ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> {\n                auto "
                "drawing = std::make_shared<ii::gui::WidgetDrawing>();\n" +
                panels +
                "                return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));\n "
                "           }); !painted) return painted;\n";
    }
    std::string appearance_code(const Inputs &values, const std::string &accent) const {
        std::string code =
            "    static ii::gui::WidgetAppearance appearance() {\n        auto result = " +
            (options.framework == "LVRS" ? "lvrs::Theme(lvrs::Accent::" + accent + ").appearance()"
                                         : "ii::gui::WidgetAppearance{}") +
            ";\n        result.styled = true;\n";
        const auto font =
            default_style_tokens().string("--lvrs-theme-typography-font-font-body", options.style_tokens);
        if (!font)
            error({}, font.error().message);
        const auto family = text(values, "fontFamily", *font);
        if (family.empty())
            error({}, "Default font family cannot be empty");
        code += "        result.window = " +
                widget_color(values, "backgroundColor",
                             color_literal(token_color("--lvrs-theme-colors-panel-window"))) +
                ";\n        result.font_family = " + literal(family) + ";\n";
        code += "        result.disabled_background = " +
                color_literal(token_color("--lvrs-theme-colors-panel-sub-surface")) + ";\n";
        const auto base = "--lvrs-theme-colors-accent-" + accent;
        if (default_style_tokens().find(base + "-default")) {
            code += "        result.primary = " +
                    color_literal(token_color(accent == "blue" ? "--lvrs-theme-colors-action-primary"
                                                               : base + "-default")) +
                    ";\n";
            code += "        result.hover = " + color_literal(token_color(base + "-hover")) +
                    ";\n        result.pressed = " + color_literal(token_color(base + "-pressed")) + ";\n";
        }
        code += "        result.text_alpha = {";
        bool first = true;
        for (auto role :
             {"title", "title2", "header", "header2", "body", "description", "caption", "disabled"}) {
            if (!first)
                code += ", ";
            first = false;
            code += decimal(text_color(role).alpha);
        }
        code += "};\n";
        code += "        result.button_height = " +
                decimal(token_number("--lvrs-theme-metrics-size-22", 1, 8192)) +
                ";\n        result.button_padding = " +
                decimal(token_number("--lvrs-theme-metrics-space-8", 0, 8192)) +
                ";\n        result.button_radius = " +
                decimal(token_number("--lvrs-theme-metrics-radius-8", 0, 8192)) +
                ";\n        return result;\n    }\n";
        return code;
    }
    std::string window_input_setup;
    std::vector<std::string> window_input_names;
    std::string materialize_window_input(std::string expression) {
        const auto name="window_value_"+std::to_string(window_input_names.size());
        window_input_names.push_back(name);
        window_input_setup+="            auto "+name+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+expression+"; }); if (!"+name+"_result) return std::unexpected("+name+"_result.error()); auto "+name+" = *"+name+"_result;\n";
        return "context.read(*"+name+")";
    }
    std::string window_properties_expression(const Inputs& values) {
        window_runtime=true;native_runtime=true;
        const auto checked_kind=[&](const Input& input,std::initializer_list<ValueKind> kinds){
            if(input.value.kind!=ValueKind::reference)return;
            if(auto kind=binding_kinds.find(input.value.text);kind!=binding_kinds.end()&&std::ranges::find(kinds,kind->second)==kinds.end())
                error(input.position,"Declared scalar type is incompatible with Window input '"+input.name+"'");
        };
        std::string geometry="ii::gui::WindowGeometry{";bool first=true;
        for(const auto [name,field]:{std::pair{"title","title"},{"width","width"},{"height","height"},{"minimumWidth","minimum_width"},{"minimumHeight","minimum_height"}}){
            auto found=values.find(name);if(found==values.end())continue;
            const auto& input=*found->second;const bool title=std::string_view(name)=="title";
            checked_kind(input,title?std::initializer_list<ValueKind>{ValueKind::string}:std::initializer_list<ValueKind>{ValueKind::number,ValueKind::metric});
            auto expression=native_expression(input.value)?
                (title?"std::string(lvui::native::string_view("+expression_code(input.value,input.position)+"))":"lvui::native::range("+expression_code(input.value,input.position)+", 1, 32767)"):
                (title?literal(text(values,name)):decimal(numeric(values,name,0,1,32767)));
            if(native_expression(input.value))expression=materialize_window_input(expression);
            if(!first)geometry+=", ";first=false;geometry+="."+std::string(field)+" = "+expression;
        }
        geometry+="}";
        bool appearance_dynamic=false;
        for(auto name:{"accent","backgroundColor","fontFamily"})if(auto input=values.find(name);input!=values.end())appearance_dynamic|=native_expression(input->second->value);
        std::string result="ii::gui::WindowProperties{.geometry = "+geometry;
        if(appearance_dynamic){
            result+=", .appearance = [&] { auto value = appearance(); ";
            if(auto input=values.find("accent");input!=values.end()&&native_expression(input->second->value)){
                checked_kind(*input->second,{ValueKind::string});
                result+="auto accent = "+materialize_window_input("lvui::native::window_accent<lvrs::Accent>("+expression_code(input->second->value,input->second->position)+")")+"; switch (accent) { ";
                for(auto name:{"blue","purple","rose","red","orange","yellow","green","gray","brown","cyan","teal","white"}){
                    const auto base="--lvrs-theme-colors-accent-"+std::string(name);
                    result+="case lvrs::Accent::"+std::string(name)+": { auto palette = lvrs::Theme(accent).appearance(); value.primary = palette.primary; value.hover = palette.hover; value.pressed = palette.pressed; ";
                    if(default_style_tokens().find(base+"-default"))result+="value.primary = "+color_literal(token_color(std::string_view(name)=="blue"?"--lvrs-theme-colors-action-primary":base+"-default"))+"; value.hover = "+color_literal(token_color(base+"-hover"))+"; value.pressed = "+color_literal(token_color(base+"-pressed"))+"; ";
                    result+="break; } ";
                }
                result+="} ";
            }
            if(auto input=values.find("backgroundColor");input!=values.end()&&native_expression(input->second->value)){
                checked_kind(*input->second,{ValueKind::color,ValueKind::string});
                result+="value.window = "+materialize_window_input(color_expression(input->second->value,input->second->position))+"; ";
            }
            if(auto input=values.find("fontFamily");input!=values.end()&&native_expression(input->second->value)){
                checked_kind(*input->second,{ValueKind::string});
                result+="value.font_family = "+materialize_window_input("std::string(lvui::native::string_view("+expression_code(input->second->value,input->second->position)+"))")+"; ";
            }
            result+="return value; }()";
        }
        return result+"}";
    }
    void emit_window_properties(const Inputs& values){
        if(!std::ranges::any_of(values,[&](const auto& input){return native_expression(input.second->value);}))return;
        const auto expression=window_properties_expression(values);
        std::string captures="scope";for(const auto& name:window_input_names)captures+=", "+name;
        body+="            { auto endpoint = widgets.window_properties(); if (endpoint) { auto target = *endpoint;\n"+window_input_setup+"            auto binding = native_bindings->bind(["+captures+"]([[maybe_unused]] lvui::native::ReadContext& context) { return "+expression+"; }, [target](const ii::gui::WindowProperties& value) { return target.update(value); }); if (!binding) return std::unexpected(binding.error()); } else if (endpoint.error().code != ii::core::ErrorCode::unsupported) return std::unexpected(endpoint.error()); }\n";
    }
    Inputs inputs(const View& view, std::initializer_list<std::string_view> allowed, bool action = false) const {
        Inputs out;
        for (const auto& input : view.inputs) {
            auto name = input.name;
            if (name.empty() && action) name = "onClick";
            const bool common=(name=="visible"||name=="flex"||name=="opacity")&&view.component->spelling!="ApplicationWindow"&&view.component->spelling!="Window";
            if (std::ranges::find(allowed, name) == allowed.end() && !common) error(input.position, "Unknown input '" + name + "' for " + std::string(view.component->spelling));
            if (!out.emplace(name, &input).second) error(input.position, "Duplicate component input '" + name + "'");
        }
        if (!view.bindings.empty() || !view.imports.empty()) error(view.position, "Nested declarations are not supported by native AOT yet");
        return out;
    }
    std::string text(const Inputs& values, std::string_view name, std::string fallback = {}) const {
        auto it = values.find(name);
        if (it == values.end()) return (name == "id" || name == "targetId" ? id_prefix : "") + fallback;
        auto value = resolve(it->second->value, it->second->position);
        if (value.kind != ValueKind::string) error(it->second->position, "Expected String for '" + std::string(name) + "'");
        return (name == "id" || name == "targetId" ? id_prefix : "") + value.text;
    }
    double numeric(const Inputs& values, std::string_view name, double fallback, double low, double high) const {
        auto it = values.find(name);
        if (it == values.end()) return fallback;
        auto value = resolve_in(it->second->value, StyleTokenType::number, it->second->position);
        auto result = number(value, it->second->position);
        if (result < low || result > high) error(it->second->position, "Number outside supported range for '" + std::string(name) + "'");
        return result;
    }
    std::string boolean(const Inputs& values, std::string_view name, bool fallback) const {
        auto it = values.find(name);
        if (it == values.end()) return fallback ? "true" : "false";
        if (native_expression(it->second->value))
            return "lvui::native::truth(" + expression_code(it->second->value, it->second->position, true) + ")";
        if(auto source=property(it->second->value);!source.empty()) {
            if(name!="enabled"&&name!="checked"&&name!="visible")error(it->second->position,"This Bool input has no native binding setter");
            return source+".get()";
        }
        auto value = resolve(it->second->value, it->second->position);
        if (value.kind != ValueKind::boolean) error(it->second->position, "Expected Bool for '" + std::string(name) + "'");
        return value.text;
    }
    std::string text_expression(const Inputs& values,std::string_view name,std::string fallback={})const {
        if (auto item = values.find(name); item != values.end() && native_expression(item->second->value))
            return "lvui::native::text(" + expression_code(item->second->value, item->second->position, true) + ")";
        if(auto item=values.find(name);item!=values.end())if(auto source=property(item->second->value);!source.empty())return "ii::gui::binding_text("+source+".get())";
        return literal(text(values,name,std::move(fallback)));
    }
    std::string number_expression(const Inputs& values,std::string_view name,double fallback,double low,double high)const {
        if (auto item = values.find(name); item != values.end() && native_expression(item->second->value))
            return "lvui::native::range(" + expression_code(item->second->value, item->second->position, true) + ", " + decimal(low) + ", " + decimal(high) + ")";
        if(auto item=values.find(name);item!=values.end())if(auto source=property(item->second->value);!source.empty())return source+".get()";
        return decimal(numeric(values,name,fallback,low,high));
    }
    void emit_bindings(const Inputs& values,const std::string& variable,bool label=false) {
        for(const auto& [name,input]:values){
            if (native_expression(input->value)) {
                std::string setter;
                if (name == "text") setter = "set_text(lvui::native::text(value))";
                else if (name == "enabled" || name == "checked") setter = "set_" + name + "(lvui::native::truth(value))";
                else if (name == "value" || name == "currentValue") setter = "set_value(lvui::native::number(value))";
                else if(label&&name=="color")setter="set_label_color(value)";
                else continue;
                const auto expression=label&&name=="color"?color_expression(input->value,input->position):expression_code(input->value,input->position);
                body += "            { auto target = *" + variable + "; auto binding = native_bindings->bind([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return " + expression + "; }, [target](const auto& value) { return target." + setter + "; });\n              if (!binding) return std::unexpected(binding.error()); }\n";
                continue;
            }
        }
    }
    void emit_common_layout_bindings(const Inputs& values,const std::string& variable,bool stack,bool owns_visibility=false) {
        for (const auto name : {"visible","flex","opacity"}) {
            const auto found=values.find(name);
            if (found==values.end() || (stack && std::string_view(name)=="flex") || (owns_visibility && std::string_view(name)=="visible")) continue;
            const auto& input=*found->second;
            const bool visible=std::string_view(name)=="visible";
            const bool opacity=std::string_view(name)=="opacity";
            if(opacity&&input.value.kind==ValueKind::reference){
                const auto kind=binding_kinds.find(input.value.text);
                if(kind!=binding_kinds.end()&&kind->second!=ValueKind::number&&kind->second!=ValueKind::metric)
                    error(input.position,"Opacity requires a numeric value within [0, 1]");
            }
            if (native_expression(input.value)) {
                const auto expression=expression_code(input.value,input.position);
                body+="            { auto target = *"+variable+"; auto binding = native_bindings->bind([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+
                    (visible?"lvui::native::truth(":"lvui::native::range(")+expression+(visible?")":opacity?", 0, 1)":", 0, 4096)")+"; }, [target](const auto& value) { return target.set_"+name+"(value); });\n              if (!binding) return std::unexpected(binding.error()); }\n";
            } else {
                const auto expression=visible ? boolean(values,name,true) : decimal(numeric(values,name,opacity?1:0,0,opacity?1:4096));
                body+="            if (auto updated = "+variable+"->set_"+name+"("+expression+"); !updated) return updated;\n";
            }
        }
    }
    std::string stack_layout_expression(const Inputs& values,std::string_view kind,bool initial=false) {
        widget_layout_runtime=true;
        const auto declared_kind=[&](const Input& input,std::initializer_list<ValueKind> allowed) {
            if (input.value.kind!=ValueKind::reference) return;
            const auto found=binding_kinds.find(input.value.text);
            if (found!=binding_kinds.end() && std::ranges::find(allowed,found->second)==allowed.end())
                error(input.position,"Declared scalar type is incompatible with layout input '"+input.name+"'");
        };
        const auto axis=std::string("lvui::native::StackAxis::")+(kind=="HStack"?"horizontal":kind=="VStack"?"vertical":"overlay");
        const auto numeric_input=[&](std::string_view name,double fallback,double low,double high) {
            const auto found=values.find(name);
            if (found!=values.end()) declared_kind(*found->second,{ValueKind::number,ValueKind::metric});
            if (found!=values.end() && native_expression(found->second->value))
                return "lvui::native::range("+expression_code(found->second->value,found->second->position,initial)+", "+decimal(low)+", "+decimal(high)+")";
            return decimal(numeric(values,name,fallback,low,high));
        };
        const auto boolean_input=[&](std::string_view name) {
            const auto found=values.find(name);
            if (found!=values.end()) declared_kind(*found->second,{ValueKind::boolean});
            return found!=values.end() && native_expression(found->second->value) ?
                "lvui::native::truth("+expression_code(found->second->value,found->second->position,initial)+")" : boolean(values,name,false);
        };
        const auto text_input=[&](std::string_view name) {
            const auto found=values.find(name);
            if (found!=values.end()) declared_kind(*found->second,{ValueKind::string});
            return found!=values.end() && native_expression(found->second->value) ?
                "lvui::native::string_view("+expression_code(found->second->value,found->second->position,initial)+")" : literal(text(values,name));
        };
        std::string alignment="lvui::native::LayoutAlignmentAxes{}";
        if (auto found=values.find("alignment");found!=values.end()) {
            const auto& input=*found->second;
            declared_kind(input,{ValueKind::string});
            std::string value;
            if (native_expression(input.value)) value=expression_code(input.value,input.position,initial);
            else {
                const auto known=resolve(input.value,input.position);
                const auto name=known.kind==ValueKind::enumeration ? known.text.substr(1) : known.text;
                const auto layout_axis=kind=="HStack"?native::StackAxis::horizontal:kind=="VStack"?native::StackAxis::vertical:native::StackAxis::overlay;
                if ((known.kind!=ValueKind::enumeration && known.kind!=ValueKind::string) || !native::named_stack_alignment(name,layout_axis))
                    error(input.position,"Invalid stack alignment");
                value=literal(name);
            }
            alignment="lvui::native::stack_alignment("+value+", "+axis+")";
        }
        std::string result="lvui::native::stack_layout({.spacing = "+numeric_input("spacing",-1,-32767,32767)+
            ", .default_spacing = "+numeric_input("defaultSpacing",token_number("--lvrs-theme-metrics-space-8",0,32767),0,32767)+
            ", .padding = "+numeric_input("padding",token_number("--lvrs-theme-metrics-space-0",0,32767),0,32767);
        for (auto [name,field] : {std::pair{"leftPadding","left_padding"},{"topPadding","top_padding"},{"rightPadding","right_padding"},{"bottomPadding","bottom_padding"}})
            if (values.contains(name)) result+=", ."+std::string(field)+" = "+numeric_input(name,0,0,8192);
        result+=", .alignment = "+alignment+", .alignment_name = "+text_input("alignmentName")+", .axis = "+axis+
            ", .width = "+numeric_input("width",0,0,8192)+", .height = "+numeric_input("height",0,0,8192)+
            ", .minimum_width = "+numeric_input("minimumWidth",0,0,8192)+", .minimum_height = "+numeric_input("minimumHeight",0,0,8192)+
            ", .flex = "+numeric_input("flex",0,0,4096)+", .intrinsic_width = "+boolean_input("intrinsicWidth")+
            ", .measure_hidden_children = "+boolean_input("measureHiddenChildren")+"})";
        return result;
    }
    std::string static_stack_layout(const Inputs& values,std::string_view kind,SourcePosition at) const {
        native::StackLayoutInput input;
        input.axis=kind=="HStack"?native::StackAxis::horizontal:kind=="VStack"?native::StackAxis::vertical:native::StackAxis::overlay;
        input.spacing=numeric(values,"spacing",-1,-32767,32767);
        input.default_spacing=numeric(values,"defaultSpacing",token_number("--lvrs-theme-metrics-space-8",0,32767),0,32767);
        input.padding=numeric(values,"padding",token_number("--lvrs-theme-metrics-space-0",0,32767),0,32767);
        for (auto [name,field] : {std::pair{"leftPadding",&native::StackLayoutInput::left_padding},{"topPadding",&native::StackLayoutInput::top_padding},
                                  {"rightPadding",&native::StackLayoutInput::right_padding},{"bottomPadding",&native::StackLayoutInput::bottom_padding}})
            if(values.contains(name))input.*field=numeric(values,name,0,0,8192);
        if(auto found=values.find("alignment");found!=values.end()){
            const auto value=resolve(found->second->value,found->second->position);
            if(value.kind!=ValueKind::enumeration&&value.kind!=ValueKind::string)error(found->second->position,"Invalid stack alignment");
            auto named=native::named_stack_alignment(value.kind==ValueKind::enumeration?value.text.substr(1):value.text,input.axis);
            if(!named)error(found->second->position,"Invalid stack alignment");
            input.alignment=*named;
        }
        const auto alignment_name=text(values,"alignmentName");input.alignment_name=alignment_name;
        input.width=numeric(values,"width",0,0,8192);input.height=numeric(values,"height",0,0,8192);
        input.minimum_width=numeric(values,"minimumWidth",0,0,8192);input.minimum_height=numeric(values,"minimumHeight",0,0,8192);
        input.flex=numeric(values,"flex",0,0,4096);
        input.intrinsic_width=boolean(values,"intrinsicWidth",false)=="true";
        input.measure_hidden_children=boolean(values,"measureHiddenChildren",false)=="true";
        native::StackLayoutValue layout;
        try{layout=native::stack_layout_value(input);}catch(const std::exception& exception){error(at,exception.what());}
        const auto alignment=[](native::LayoutAlignment value){
            constexpr std::string_view names[]={"center","leading","trailing","stretch"};
            return "ii::gui::StackAlignment::"+std::string(names[unsigned(value)]);
        };
        const bool same=layout.alignment.horizontal==layout.alignment.vertical;
        std::string result="ii::gui::StackLayoutOptions{.stack = {.spacing = "+decimal(layout.spacing)+", .padding = "+decimal(layout.padding)+
            ", .alignment = "+alignment(same?layout.alignment.horizontal:native::LayoutAlignment::center);
        if(layout.insets)result+=", .insets = ii::gui::Insets{"+decimal(layout.insets->left)+", "+decimal(layout.insets->top)+", "+decimal(layout.insets->right)+", "+decimal(layout.insets->bottom)+"}";
        result+=", .minimum_width = "+decimal(layout.minimum_width)+", .minimum_height = "+decimal(layout.minimum_height)+
            ", .intrinsic_width = "+(layout.intrinsic_width?"true":"false")+", .measure_hidden_children = "+(layout.measure_hidden_children?"true":"false");
        if(!same)result+=", .horizontal_alignment = "+alignment(layout.alignment.horizontal)+", .vertical_alignment = "+alignment(layout.alignment.vertical);
        return result+"}, .extent = {"+decimal(layout.width)+", "+decimal(layout.height)+"}, .flex = "+decimal(layout.flex)+"}";
    }
    void emit_stack_layout(const Inputs& values,const std::string& variable,std::string_view kind,SourcePosition at) {
        const auto dynamic=std::ranges::any_of(values,[&](const auto& entry) {
            constexpr std::string_view fields=" spacing defaultSpacing padding leftPadding topPadding rightPadding bottomPadding alignment alignmentName width height minimumWidth minimumHeight flex intrinsicWidth measureHiddenChildren ";
            return fields.find(" "+entry.first+" ")!=fields.npos && native_expression(entry.second->value);
        });
        if (dynamic) body+="            { auto target = *"+variable+"; auto binding = native_bindings->bind([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+stack_layout_expression(values,kind)+"; }, [target](const ii::gui::StackLayoutOptions& value) { return target.set_stack_layout(value); });\n              if (!binding) return std::unexpected(binding.error()); }\n";
        else body+="            if (auto layout = "+variable+"->set_stack_layout("+static_stack_layout(values,kind,at)+"); !layout) return layout;\n";
    }
    std::string scoped_member(const Inputs& values,std::string_view name) const {
        auto item=values.find(name);if(item==values.end())return "{}";
        const auto& input=*item->second;const auto& value=input.value;const auto dot=value.text.find('.');
        if(auto target=callable_alias(value)){
            native_runtime=true;const auto result=target->signature->result=="Void"?"void":target->signature->result;
            auto call=target->shared.empty()?method_invocation(*target,"std::forward<decltype(arguments)>(arguments)..."):
                "lvui::native::call_function("+callback_path(*target)+", std::forward<decltype(arguments)>(arguments)...)";
            return "lvui::native::scoped_function<"+result+">(scope, native_bindings, []([[maybe_unused]] const auto& scope, auto&&... arguments) -> "+result+" { return "+call+"; })";
        }
        if ((value.kind == ValueKind::reference && native_expression(value)) ||
            (value.kind == ValueKind::internal_argument && bound_values.contains(value.text) && bound_values.at(value.text).member_reference))
            return expression_code(value, input.position, true);
        auto backend=backends.find(value.text.substr(0,dot));const auto member=dot==value.text.npos?std::string{}:value.text.substr(dot+1);
        if(value.kind!=ValueKind::reference||backend==backends.end()||!identifier(member))error(input.position,"Expected one public scoped C++ member for '"+std::string(name)+"'");
        return backend_access.at(backend->first).object + "." + member;
    }
    std::string action(const Inputs& values, std::string_view name="onClick", std::string parameter={},std::string arguments="value") const {
        auto it = values.find(name);
        if (it == values.end()) return "{}";
        const auto& value = it->second->value;
        const auto target = callback_target(value, it->second->position);
        if(target.signature&&target.signature->result!="Void")error(it->second->position,"An event callback requires a Void result");
        const auto args=parameter.empty()?std::string{}:arguments;
        native_runtime = true;
        const auto& access = target.access;
        if(!target.shared.empty())return shared_action(target,parameter,args);
        if(!target.factory.empty())return returned_action(target,parameter,args);
        return "lvui::native::callback(scope, native_bindings, []([[maybe_unused]] const auto& scope" + (parameter.empty()?std::string{}:", "+parameter) + ") { (void)lvui::native::invoke(" + access.revision + ", [&]() -> decltype(auto) { return " + method_invocation(target,args) + "; }); })";
    }
    std::string returned_action(const BoundMethod& target,const std::string& parameter,const std::string& arguments,bool button=false,std::string_view event_type="lvrs::ButtonEvent")const{
        auto invocation=button?"[]<class H>(H& handler, [[maybe_unused]] const "+std::string(event_type)+"& event) { if constexpr (std::invocable<H&, const "+std::string(event_type)+"&>) (void)std::invoke(handler, event); else (void)std::invoke(handler); }(handler, event)":"std::invoke(handler"+(arguments.empty()?std::string{}:", "+arguments)+")";
        if(target.signature){
            const auto supplied=button&&target.signature->arguments.empty()?std::string{}:arguments;
            invocation="lvui::native::invoke_callback<"+callback_signature(*target.signature)+">(handler"+(supplied.empty()?std::string{}:", "+supplied)+")";
        }
        auto call=target.access.revision.empty()?"(void)"+invocation+";":"(void)lvui::native::invoke("+target.access.revision+", [&]() -> decltype(auto) { return "+invocation+"; });";
        return "lvui::native::returned_callback(scope, native_bindings, [scope]([[maybe_unused]] lvui::native::ReadContext& context) -> decltype(auto) { return "+target.factory+"; }, []([[maybe_unused]] const auto& scope, auto& handler"+(parameter.empty()?std::string{}:", "+parameter)+") { "+call+" })";
    }
    std::pair<std::string,double> text_role(const Inputs& values) const {
        const auto found=values.find("style");
        if (found != values.end() && found->second->value.kind != ValueKind::enumeration)
            error(found->second->position, "Unknown LVRS text style");
        const auto role =
            found == values.end() ? std::string("description") : found->second->value.text.substr(1);
        const auto metric = text_metric(role);
        if (metric.empty())
            error(found->second->position, "Unknown LVRS text style");
        return {role, token_number(metric, 1, 256)};
    }
    static std::string text_metric(std::string_view role) {
        constexpr std::pair<std::string_view, std::string_view> names[]{
            {"title", "title-text-title"},       {"title2", "subtitle-text-subtitle"},
            {"header", "header-text-header"},    {"header2", "subheader-text-subheader"},
            {"body", "body-text-body"},          {"description", "description-text-description"},
            {"caption", "caption-text-caption"}, {"disabled", "caption-text-caption"}};
        for (auto [name, metric] : names)
            if (name == role)
                return "--lvrs-theme-typography-" + std::string(metric);
        return {};
    }
    StyleColor text_color(std::string_view role) const {
        return token_color("--lvrs-theme-colors-text-" +
                           std::string(role == "body"          ? "body-color"
                                       : role == "description" ? "description-color"
                                       : role == "caption"     ? "caption-color"
                                       : role == "disabled"    ? "disabled-color"
                                                               : "title-header-color"));
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
        for(const auto& binding:context_bindings){auto target=widget_variables.find(binding.target);if(target==widget_variables.end())error(binding.position,binding.tooltip?"Unknown Tooltip targetId in content scope":"Unknown context menu targetId in content scope");body+="            if (auto bound = "+(binding.tooltip?binding.menu+"->set_target(*"+target->second+")":target->second+"->set_context_menu(*"+binding.menu+")")+"; !bound) return bound;\n";}
        auto children=std::move(body);--factory_depth;body=std::move(outer_body);if(isolated)ids=std::move(outer_ids);widget_variables=std::move(outer_variables);context_bindings=std::move(outer_bindings);
        if(children.empty())return "{}";
        const auto capture = native_runtime ? "[weak_scope = std::weak_ptr(scope), weak_activation = std::weak_ptr(native_bindings)]" : "[]";
        const auto admission = native_runtime ? "            auto scope = weak_scope.lock(); auto activation = weak_activation.lock();\n            if (!scope || !activation || !activation->available() || !scope->active()) return ii::core::fail(ii::core::ErrorCode::closed, \"Content owner retired\");\n" : "";
        const bool legacy=options.framework=="LVRS";
        const auto type=(legacy?std::string("lvrs::"):std::string("lvui::native::"))+std::string(factory_type);
        return type+"{"+capture+"(ii::gui::WidgetBuilder& widgets, ii::gui::Widget content) -> ii::core::Result<void> {\n" + admission +
            (legacy?"            lvrs::Widgets controls(widgets);\n":"")+binding_owner()+children+"            return {};\n        }}";
    }
    std::string widget_color(const Inputs& values,std::string_view name,std::string fallback) const {
        auto item = values.find(name);
        if (item == values.end())
            return fallback;
        return color_expression(item->second->value,item->second->position,true);
    }
    std::string alias_key(const Inputs& values,std::string_view name,std::string_view alias)const{const auto found=values.find(alias);if(values.contains(name)&&found!=values.end())error(found->second->position,"Use a property or its alias, not both");return std::string(found!=values.end()?alias:name);}
    std::string button_method(const Inputs& values,std::string_view name,std::string_view event_type="lvrs::ButtonEvent")const{
        auto found=values.find(name);if(found==values.end())return "{}";const auto& input=*found->second;
        const auto target = callback_target(input.value, input.position);
        if(target.signature&&target.signature->result!="Void")error(input.position,"A button method requires a Void result");
        const auto& method = target.method;
        native_runtime = true;
        const auto& access = target.access;
        const auto argument="[[maybe_unused]] const "+std::string(event_type)+"& event";
        if(!target.shared.empty())return shared_action(target,argument,target.signature->arguments.empty()?"":"event");
        if(!target.factory.empty())return returned_action(target,argument,"event",true,event_type);
        if(target.signature)return "lvui::native::callback(scope, native_bindings, []([[maybe_unused]] const auto& scope, "+argument+") { lvui::native::invoke("+access.revision+", [&] { "+method_invocation(target,target.signature->arguments.empty()?"":"event")+"; }); })";
        return "lvui::native::callback(scope, native_bindings, []([[maybe_unused]] const auto& scope, "+argument+") { lvui::native::invoke(" + access.revision + ", [&] { []<class B>(B& backend, "+argument+") { if constexpr (requires { backend."+method+"(event); }) (void)backend."+method+"(event); else (void)backend."+method+"(); }(" + access.object + ", event); }); })";
    }
    std::string link_value(const Inputs& values,std::string_view name,std::string fallback)const{
        auto found=values.find(name);if(found==values.end())return fallback;
        if (native_expression(found->second->value)) return expression_code(found->second->value, found->second->position, true);
        auto source=property(found->second->value);if(source.empty())error(found->second->position,"Link data input requires one scoped C++ member");return "ii::gui::binding_value("+source+")";
    }
    std::string link_text(const Inputs& values,std::string_view name,std::string fallback={})const{
        if (auto found = values.find(name); found != values.end() && native_expression(found->second->value))
            return "lvui::native::text(" + expression_code(found->second->value, found->second->position, true) + ")";
        auto found=values.find(name);if(found!=values.end())if(auto source=property(found->second->value);!source.empty())return "ii::gui::binding_text(ii::gui::binding_value("+source+"))";return literal(text(values,name,std::move(fallback)));
    }
    std::string link_bool(const Inputs& values,std::string_view name,bool fallback)const{
        if (auto found = values.find(name); found != values.end() && native_expression(found->second->value))
            return "lvui::native::truth(" + expression_code(found->second->value, found->second->position, true) + ")";
        auto found=values.find(name);if(found!=values.end())if(auto source=property(found->second->value);!source.empty())return "ii::gui::binding_value("+source+")";return boolean(values,name,fallback);
    }
    std::string link_color(const Inputs& values,std::string_view name)const{
        if (auto found = values.find(name); found != values.end() && native_expression(found->second->value))
            return color_expression(found->second->value, found->second->position, true);
        auto found=values.find(name);if(found!=values.end())if(auto source=property(found->second->value);!source.empty())return "ii::gui::binding_value("+source+")";return widget_color(values,name,"std::nullopt");
    }
    std::string link_methods(const Inputs& values,std::string_view event_type="lvrs::ButtonEvent")const{
        auto found=values.find("methods");if(found==values.end())return "{}";const auto& input=*found->second;
        if(native_expression(input.value))return link_value(values,"methods","{}");
        auto list=resolve(input.value,input.position);if(list.kind!=ValueKind::list||list.elements.size()>256)error(input.position,"Link methods require at most 256 deferred references");
        std::string result="{";for(const auto& value:list.elements){Input candidate{"method",value,input.position};if(result.size()>1)result+=", ";result+=button_method(Inputs{{"method",&candidate}},"method",event_type);}return result+"}";
    }
    std::string owned_link_options(const Inputs& values,bool initial)const {
        const auto expression=[&](const Input& input){return "lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input.value,input.position,initial)+")";};
        const auto text_value=[&](std::string_view name,std::string fallback={}){auto item=values.find(name);if(item==values.end())return literal(fallback);
            return native_expression(item->second->value)?"lvui::native::text("+expression(*item->second)+")":literal(text(values,name,fallback));};
        const auto flag=[&](std::string_view name,bool fallback){auto item=values.find(name);if(item==values.end())return std::string(fallback?"true":"false");
            return native_expression(item->second->value)?"lvui::native::truth("+expression(*item->second)+")":boolean(values,name,fallback);};
        const auto number=[&](std::string_view name,double fallback,double low,double high){auto item=values.find(name);if(item==values.end())return decimal(fallback);
            return native_expression(item->second->value)?"lvui::native::number("+expression(*item->second)+")":decimal(numeric(values,name,fallback,low,high));};
        const auto color_code=[](StyleColor c){return "lvui::StyleColor{"+decimal(c.red)+", "+decimal(c.green)+", "+decimal(c.blue)+", "+decimal(c.alpha)+"}";};
        const auto color=[&](std::string_view name,std::string_view token){auto item=values.find(name);return item==values.end()?color_code(token_color(token)):
            "lvui::native::link_color("+color_expression(item->second->value,item->second->position,initial)+")";};
        const auto href=alias_key(values,"href","to");
        std::string setup="value.text = "+text_value("text")+"; value.accessibility_name = "+text_value("accessibilityName")+
            "; value.href = "+text_value(href,"/")+"; value.enabled = "+flag("enabled",true)+"; value.replace = "+flag("replace",false)+
            "; value.underline = "+flag("underline",false)+"; value.show_focus_ring = "+flag("showFocusRing",true)+"; ";
        for(auto name:{"params","router","navigator","targetComponent"})if(auto item=values.find(name);item!=values.end()){
            if(!native_expression(item->second->value))error(item->second->position,"Link data input requires a scoped native value");
            const auto field=std::string_view(name)=="targetComponent"?"target":name;
            auto value=expression(*item->second);
            if(std::string_view(name)=="router"||std::string_view(name)=="navigator")value="lvui::native::navigation_target("+value+")";
            else if(std::string_view(name)=="targetComponent")value="lvui::native::navigation_page("+value+")";
            setup+="value."+std::string(field)+" = "+value+"; ";
        }
        std::string tone="lvui::ButtonTone::borderless";
        if(auto item=values.find("tone");item!=values.end()){
            if(native_expression(item->second->value))tone="lvui::native::link_tone("+expression(*item->second)+")";
            else {const auto v=resolve(item->second->value,item->second->position);
                const std::map<std::string,std::string,std::less<>> choices{{".primary","primary"},{".default","normal"},{".borderless","borderless"},{".destructive","destructive"},{".disabled","disabled"}};
                const auto found=choices.find(v.text);if(v.kind!=ValueKind::enumeration||found==choices.end())error(item->second->position,"Unknown independent Link tone");
                tone="lvui::ButtonTone::"+found->second;}
        }
        setup+="value.tone = "+tone+"; value.horizontal_padding = "+number("horizontalPadding",14,0,4096)+
            "; value.vertical_padding = "+number("verticalPadding",10,0,4096)+"; value.minimum_height = "+number("minimumHeight",36,0,8192)+
            "; value.corner_radius = "+number("cornerRadius",8,0,8192)+"; ";
        for(auto name:{"width","height"})if(values.contains(name))setup+="value."+std::string(name)+" = "+number(name,0,0,8192)+"; ";
        setup+="value.font_size = "+decimal(token_number("--lvrs-theme-typography-description-text-description",1,256))+
            "; value.line_height = "+decimal(token_number("--lvrs-theme-typography-description-text-description-line-height",0,8192))+
            "; value.font_weight = "+decimal(token_number("--lvrs-theme-typography-weight-semi-bold",400,700))+"; ";
        setup+="value.palette.link = "+color("linkColor","--lvrs-theme-colors-action-primary")+
            "; value.palette.hover = "+color("hoverColor","--lvrs-theme-colors-text-title-header-color")+
            "; value.palette.disabled = "+color("disabledColor","--lvrs-theme-colors-text-description-color")+
            "; value.palette.focus = "+color_code(token_color("--lvrs-theme-colors-action-primary"))+"; ";
        if(auto item=values.find("pressedColor");item!=values.end())setup+="value.palette.pressed = lvui::native::link_color("+color_expression(item->second->value,item->second->position,initial)+"); ";
        return "([&] { lvui::LinkConfiguration value; "+setup+"return value; }())";
    }
    void native_binding(const Input& input, const std::string& variable, const std::string& setter) {
        const auto expression = expression_code(input.value, input.position);
        body += "            { auto binding = native_bindings->bind([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return lvui::native::widget_value<ii::gui::WidgetColor>(" + expression + "); }, [widget = *" + variable + ", setter = " + setter + "](const auto& value) { return setter(widget, value); });\n              if (!binding) return std::unexpected(binding.error()); }\n";
    }
    void emit_link_bindings(const Inputs& values,const std::string& variable,const std::string& native){
        for(const auto& [name,input]:values){if(!native_expression(input->value))continue;
            std::string setter;
            if(name=="text"||name=="enabled"||name=="href"||name=="params"||name=="router"||name=="navigator"||name=="replace"||name=="underline"||name=="tone"||name=="methods")setter="set_"+name;
            else if(name=="to")setter="set_to";else if(name=="targetComponent")setter="set_target_component";else if(name=="linkColor")setter="set_link_color";else if(name=="hoverColor")setter="set_hover_color";else if(name=="pressedColor")setter="set_pressed_color";else if(name=="disabledColor")setter="set_disabled_color";
            if(setter.empty())continue;
            native_binding(*input, variable, "[link = *"+native+"](ii::gui::Widget, const auto& value) { return link."+setter+"(value); }");
        }
    }
    std::string surface_number(const Inputs& values,std::string_view name,std::string fallback)const{
        auto found=values.find(name);if(found==values.end())return fallback;
        if (native_expression(found->second->value)) return "lvui::native::number(" + expression_code(found->second->value, found->second->position, true) + ")";
        if(auto source=property(found->second->value);!source.empty())return "ii::gui::binding_value("+source+")";
        return decimal(numeric(values,name,0,0,8192));
    }
    std::string surface_color(const Inputs& values,std::string_view name,std::string fallback)const{
        if (auto found = values.find(name); found != values.end() && native_expression(found->second->value))
            return color_expression(found->second->value, found->second->position, true);
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
    std::string owned_card_options(const Inputs& values,bool initial)const{
        const std::map<std::string,std::string,std::less<>> fields{{"title","title"},{"subtitle","subtitle"},{"enabled","enabled"},
            {"width","width"},{"height","height"},{"padding","padding"},{"spacing","spacing"},{"cornerRadius","corner_radius"},
            {"shapeStyle","shape"},{"backgroundColor","background"},{"titleColor","title_color"},{"subtitleColor","subtitle_color"}};
        std::string setup;
        for(const auto& [name,input]:values){auto field=fields.find(name);if(field==fields.end())continue;
            const auto expression=[&]{return "lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input->value,input->position,initial)+")";};
            std::string value;
            if(name=="title"||name=="subtitle")value=native_expression(input->value)?"lvui::native::text("+expression()+")":literal(text(values,name));
            else if(name=="enabled")value=native_expression(input->value)?"lvui::native::truth("+expression()+")":boolean(values,name,true);
            else if(name=="backgroundColor"||name=="titleColor"||name=="subtitleColor")
                value="lvui::native::app_card_color("+color_expression(input->value,input->position,initial)+")";
            else if(name=="shapeStyle"){
                if(native_expression(input->value))value="lvui::native::app_card_shape("+expression()+")";
                else {auto item=resolve(input->value,input->position);
                    if(item.kind==ValueKind::enumeration&&(item.text==".cylinder"||item.text==".shapeCylinder"))value="lvui::AppCardShape::cylinder";
                    else if(item.kind==ValueKind::enumeration&&(item.text==".roundRect"||item.text==".shapeRoundRect"))value="lvui::AppCardShape::round_rect";
                    else error(input->position,"Unknown independent AppCard shapeStyle");}
            }else value=native_expression(input->value)?"lvui::native::number("+expression()+")":decimal(numeric(values,name,0,0,8192));
            setup+="value."+field->second+" = "+value+"; ";
        }
        return "([&] { lvui::AppCardConfiguration value; "+setup+"return value; }())";
    }
    std::string owned_header_options(const Inputs& values,bool initial)const{
        const std::map<std::string,std::string,std::less<>> fields{{"title","title"},{"subtitle","subtitle"},{"enabled","enabled"},
            {"width","width"},{"height","height"},{"menuVisible","menu_visible"},{"compact","compact"},
            {"contentHorizontalPadding","content_horizontal_padding"},{"contentVerticalPadding","content_vertical_padding"},
            {"rowSpacing","row_spacing"},{"actionSpacing","action_spacing"},{"menuButtonPadding","menu_button_padding"},
            {"backgroundColor","background"},{"titleColor","title_color"},{"subtitleColor","subtitle_color"},
            {"menuBackgroundColor","menu_background"},{"menuPressedBackgroundColor","menu_pressed_background"}};
        std::string setup;
        for(const auto& [name,input]:values){auto field=fields.find(name);if(field==fields.end())continue;
            const auto expression=[&]{return "lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input->value,input->position,initial)+")";};
            std::string value;
            if(name=="title"||name=="subtitle")value=native_expression(input->value)?"lvui::native::text("+expression()+")":literal(text(values,name));
            else if(name=="enabled"||name=="menuVisible"||name=="compact")value=native_expression(input->value)?"lvui::native::truth("+expression()+")":boolean(values,name,name=="enabled");
            else if(name=="backgroundColor"||name=="titleColor"||name=="subtitleColor"||name=="menuBackgroundColor"||name=="menuPressedBackgroundColor")
                value="lvui::native::app_header_color("+color_expression(input->value,input->position,initial)+")";
            else value=native_expression(input->value)?"lvui::native::number("+expression()+")":decimal(numeric(values,name,0,0,name=="menuButtonPadding"?4090:8192));
            setup+="value."+field->second+" = "+value+"; ";
        }
        return "([&] { lvui::AppHeaderConfiguration value; "+setup+"return value; }())";
    }
    std::string owned_listener_options(const Inputs& values,bool initial)const{
        const auto metric=[&](std::string_view name,double fallback){auto found=values.find(name);
            if(found!=values.end()&&native_expression(found->second->value))
                return "lvui::native::number(lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(found->second->value,found->second->position,initial)+"))";
            return decimal(numeric(values,name,fallback,1,8192));};
        return "lvui::EventListenerConfiguration{.width = "+metric("width",640)+", .height = "+metric("height",480)+"}";
    }
    std::string listener_input(const Inputs& values)const{
        auto found=values.find("onInput");if(found==values.end())return "{}";
        const auto target=callback_target(found->second->value,found->second->position);native_runtime=true;
        const auto parameter=std::string("const ii::gui::WidgetInput& value"),result=std::string("ii::core::Result<bool>");
        if(!target.shared.empty())return "lvui::native::scoped_function<"+result+">(scope, native_bindings, []([[maybe_unused]] const auto& scope, "+parameter+") -> "+result+" { auto handler = "+callback_path(target)+"; if (!handler) return ii::core::fail(ii::core::ErrorCode::closed, \"Input callback is unavailable\"); return (*handler)(value); })";
        if(!target.factory.empty()){
            const auto call=std::string("std::invoke(handler, value)");
            const auto invoked=target.access.revision.empty()?call:"lvui::native::invoke("+target.access.revision+", [&]() -> decltype(auto) { return "+call+"; })";
            return "lvui::native::returned_function<"+result+">(scope, native_bindings, [scope]([[maybe_unused]] lvui::native::ReadContext& context) -> decltype(auto) { return "+target.factory+"; }, []([[maybe_unused]] const auto& scope, auto& handler, "+parameter+") -> "+result+" { return "+invoked+"; })";
        }
        const auto call=method_invocation(target,"value");
        const auto invoked=target.access.revision.empty()?call:"lvui::native::invoke("+target.access.revision+", [&]() -> decltype(auto) { return "+call+"; })";
        return "lvui::native::scoped_function<"+result+">(scope, native_bindings, []([[maybe_unused]] const auto& scope, "+parameter+") -> "+result+" { return "+invoked+"; })";
    }
    void emit_surface_bindings(const Inputs& values,const std::string& variable,const std::string& native){
        const std::map<std::string,std::string,std::less<>> fields{{"title","title"},{"subtitle","subtitle"},{"enabled","enabled"},{"width","width"},{"height","height"},{"padding","padding"},{"spacing","spacing"},{"cornerRadius","corner_radius"},{"shapeStyle","shape"},{"backgroundColor","background"},{"titleColor","title_color"},{"subtitleColor","subtitle_color"},{"menuVisible","menu_visible"},{"compact","compact"},{"contentHorizontalPadding","content_horizontal_padding"},{"contentVerticalPadding","content_vertical_padding"},{"rowSpacing","row_spacing"},{"actionSpacing","action_spacing"},{"menuButtonPadding","menu_button_padding"},{"menuBackgroundColor","menu_background"},{"menuPressedBackgroundColor","menu_pressed_background"}};
        for(const auto& [name,input]:values){if(!native_expression(input->value))continue;
            if(auto field=fields.find(name);field!=fields.end())native_binding(*input, variable, "[surface = *"+native+"](ii::gui::Widget, const auto& value) -> ii::core::Result<void> { auto options = surface.options(); if (!options) return std::unexpected(options.error()); options->"+field->second+" = "+(name=="title"||name=="subtitle"?"ii::gui::binding_text(value)":"value")+"; return surface.set_options(std::move(*options)); }");
        }
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
        for(const auto& [name,input]:values){if(!native_expression(input->value))continue;std::string update;
            if(name=="visible"&&kind!="ToolbarButton")continue;
            if(name=="buttonItems")update="return component.set_items(lvrs::toolbar_items(value));";
            else if(name=="activeButtonId")update="return component.set_active_id(value);";
            else if(auto field=fields.find(name);field!=fields.end()){const auto text=name=="iconName"||name=="text"||name=="accessibilityName"||name=="icon1"||name=="icon2"||name=="icon3";update="auto options = component.options(); if (!options) return std::unexpected(options.error()); options->"+field->second+" = "+(text?"ii::gui::binding_text(value)":(name=="button1"||name=="button2"||name=="button3")?"lvrs::footer_slot(value)":"value")+"; return component.set_options(std::move(*options));";}
            if(!update.empty())native_binding(*input, variable, "[component = *"+native+"](ii::gui::Widget widget, const auto& value) -> ii::core::Result<void> { (void)widget; "+update+" }");
        }
        if(auto it=values.find("activeButtonId");it!=values.end()&&property(it->second->value).empty())body+="            if (auto active = "+native+"->set_active_id("+toolbar_id(values,"activeButtonId")+"); !active) return active;\n";
    }
    std::string media_enum(const Inputs& v,std::string_view name,std::string_view type,std::string fallback,std::initializer_list<std::pair<std::string_view,std::string_view>> choices)const{
        auto it=v.find(name);if(it==v.end())return "lvrs::"+std::string(type)+"::"+fallback;
        if(!property(it->second->value).empty())return link_value(v,name,"");
        const auto value=resolve(it->second->value,it->second->position);
        if(value.kind==ValueKind::enumeration)for(auto [key,target]:choices)if(value.text=="."+std::string(key))return "lvrs::"+std::string(type)+"::"+std::string(target);
        error(it->second->position,"Unknown native "+std::string(type)+" value");
    }
    std::string media_size(const Inputs& v,bool avatar)const{return media_enum(v,"size",avatar?"AvatarSize":"CardSize",avatar?"large":"medium",{{"Small","small"},{"Medium","medium"},{"Large","large"},{"small","small"},{"medium","medium"},{"large","large"}});}
    std::string media_call(std::string_view kind,const std::string& args,const Inputs& v)const{
        const auto n=[&](std::string_view key,std::string value){return surface_number(v,key,std::move(value));};
        const auto b=[&](std::string_view key,bool value){return link_bool(v,key,value);};
        if(kind=="Avatar")return "lvrs::Avatar::create(widgets, "+args+", {.size = "+media_size(v,true)+", .source = "+link_value(v,"source","{}")+", .background = "+surface_color(v,"backgroundColor","ii::gui::WidgetColor{.1935568452,.1978072077,.1999323964,1}")+", .width = "+n("width","0")+", .height = "+n("height","0")+", .enabled = "+b("enabled",true)+", .accessibility_name = "+link_text(v,"accessibilityName","User avatar")+"})";
        const auto title=alias_key(v,"title","filename"),icon=alias_key(v,"icon","iconSource");
        const auto optional_bool=[&](std::string_view key){return v.contains(key)?b(key,true):"std::nullopt";};
        const auto optional_text=[&](std::string_view key){return v.contains(key)?link_text(v,key):"std::nullopt";};
        return "lvrs::Card::create(widgets, "+args+", {.type = "+media_enum(v,"type","CardType","file",{{"File","file"},{"FilePreview","file_preview"},{"Folder","folder"},{"Project","project"},{"Device","device"},{"Model","model"},{"Member","member"},{"Link","link"}})+", .size = "+media_size(v,false)+", .detail = "+media_enum(v,"detail","CardDetail","brief",{{"Brief","brief"},{"Detailed","detailed"}})+", .display_state = "+media_enum(v,"displayState","CardDisplayState","automatic",{{"Automatic","automatic"},{"DefaultState","normal"},{"HoverState","hover"},{"SelectedState","selected"}})+", .selected = "+b("selected",false)+", .enabled = "+b("enabled",true)+", .show_description = "+b("showDescription",true)+", .show_status = "+b("showStatus",true)+", .selectable = "+optional_bool("selectable")+", .show_menu = "+optional_bool("showMenu")+", .show_action = "+optional_bool("showAction")+", .title = "+link_text(v,title)+", .description = "+link_text(v,"description")+", .details = "+link_text(v,"details")+", .metadata = "+link_text(v,"metadata")+", .status_text = "+link_text(v,"statusText")+", .summary = "+link_text(v,"summary")+", .footnote = "+link_text(v,"footnote")+", .domain = "+link_text(v,"domain")+", .preview_title = "+link_text(v,"previewTitle")+", .icon_name = "+link_text(v,"iconName")+", .menu_accessibility_name = "+link_text(v,"menuAccessibleName","More actions")+", .action_text = "+optional_text("actionText")+", .progress_label = "+optional_text("progressLabel")+", .progress_text = "+link_text(v,"progressText")+", .rows = "+link_value(v,"rows","{}")+", .progress = "+(v.contains("progress")&&!property(v.at("progress")->value).empty()?link_value(v,"progress",""):decimal(numeric(v,"progress",0,-1e9,1e9)))+", .progress_color = "+link_color(v,"progressColor")+", .status_color = "+link_color(v,"statusColor")+", .icon = "+link_value(v,icon,"{}")+", .preview_source = "+link_value(v,"previewSource","{}")+", .width = "+n("width","0")+", .height = "+n("height","0")+"}, {.clicked = "+action(v)+", .menu_requested = "+action(v,"onMenuRequested")+", .action_triggered = "+action(v,"onActionTriggered")+", .selected_changed = "+action(v,"onSelectedChanged","bool value")+"}, {.preview = lvrs::card_component("+link_value(v,"previewComponent","ii::gui::WidgetFactory{}")+"), .icon = lvrs::card_component("+link_value(v,"iconComponent","ii::gui::WidgetFactory{}")+")})";
    }
    std::string native_view_options(const Inputs& v,bool initial)const{
        const auto expression=[&](std::string_view name,std::string fallback){auto it=v.find(name);return it==v.end()?fallback:expression_code(it->second->value,it->second->position,initial);};
        const auto number=[&](std::string_view name){auto it=v.find(name);if(it==v.end())return std::string("0");return native_expression(it->second->value)?"lvui::native::range("+expression(name,"")+", 0, 8192)":decimal(numeric(v,name,0,0,8192));};
        const auto flag=[&](std::string_view name){auto it=v.find(name);if(it==v.end())return std::string("true");return native_expression(it->second->value)?"lvui::native::truth("+expression(name,"")+")":boolean(v,name,true);};
        if(auto it=v.find("source");it!=v.end()&&!native_expression(it->second->value))error(it->second->position,"NativeView.source requires a native C++ view source");
        return "ii::gui::NativeViewOptions{.source = "+expression("source","{}")+", .width = "+number("width")+", .height = "+number("height")+", .enabled = "+flag("enabled")+", .clip = "+flag("clip")+"}";
    }
    std::string image_options(const Inputs& v,bool initial)const{
        const auto expression=[&](std::string_view name,std::string fallback){auto item=v.find(name);if(item==v.end())return fallback;return "lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(item->second->value,item->second->position,initial)+")";};
        const auto number=[&](std::string_view name,std::string fallback){auto item=v.find(name);if(item==v.end())return fallback;if(native_expression(item->second->value))return "lvui::native::number("+expression(name,"")+")";return decimal(numeric(v,name,0,0,8192));};
        const auto flag=[&](std::string_view name,bool fallback){auto item=v.find(name);if(item==v.end())return std::string(fallback?"true":"false");if(native_expression(item->second->value))return "lvui::native::truth("+expression(name,"")+")";return boolean(v,name,fallback);};
        const auto color=[&](std::string_view name){auto item=v.find(name);return item==v.end()?std::string("std::nullopt"):color_expression(item->second->value,item->second->position,initial);};
        if(auto source=v.find("source");source!=v.end()&&!native_expression(source->second->value))error(source->second->position,"Image.source requires a native image from a C++ backend");
        std::string fit="std::nullopt";
        if(auto item=v.find("fit");item!=v.end()){
            if(native_expression(item->second->value))fit=expression("fit","");
            else{const auto value=resolve(item->second->value,item->second->position);
                if(value.text==".Contain")fit="ii::gui::ImageFit::contain";
                else if(value.text==".Cover")fit="ii::gui::ImageFit::cover";
                else if(value.text==".Stretch")fit="ii::gui::ImageFit::stretch";
                else error(item->second->position,"Unknown native Image.fit value");}
        }
        std::string name=literal("");if(auto item=v.find("accessibilityName");item!=v.end())name=native_expression(item->second->value)?"ii::gui::binding_text("+expression("accessibilityName","")+")":literal(text(v,"accessibilityName"));
        return "ii::gui::ImageOptions{.image = "+expression("source","{}")+", .width = "+number("width","32")+", .height = "+number("height","32")+", .accessibility_name = "+name+", .padding = "+number("padding","0")+", .corner_radius = "+number("cornerRadius","0")+", .border_width = "+number("borderWidth","0")+", .background = "+color("backgroundColor")+", .border = "+color("borderColor")+", .preserve_aspect = "+flag("preserveAspect",false)+", .fit = "+fit+", .circular = "+flag("circular",false)+", .allow_empty = "+flag("allowEmpty",true)+"}";
    }
    std::string image_call(const Inputs& v,const std::string& args)const{return "lvrs::Image::create(widgets, "+args+", "+image_options(v,true)+")";}
    std::string owned_named_icon_setup(const Inputs& values,bool initial,bool tab=false)const {
        const auto dynamic=[&](std::string_view name){auto item=values.find(name);return item!=values.end()&&native_expression(item->second->value);};
        if(values.contains("icon")&&values.contains("iconName"))error(values.at("iconName")->position,"Use icon or iconName, not both");
        if((values.contains("iconSource")||values.contains("iconRevision"))&&!values.contains("iconName"))
            error(values.at(values.contains("iconSource")?"iconSource":"iconRevision")->position,"An icon source or revision requires iconName");
        std::string setup;
        if(auto item=values.find("iconName");item!=values.end())
            setup+=std::string("value.")+(tab?"configuration.":"")+"icon_name = "+(dynamic("iconName")?"lvui::native::text("+expression_code(item->second->value,item->second->position,initial)+")":literal(text(values,"iconName")))+"; ";
        if(auto item=values.find("iconSource");item!=values.end()){
            if(!dynamic("iconSource"))error(item->second->position,"iconSource requires a native IconSource from a C++ backend");
            setup+="value.icon_source = lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(item->second->value,item->second->position,initial)+"); ";
        }
        if(auto item=values.find("iconRevision");item!=values.end()){
            std::string value;
            if(dynamic("iconRevision"))value="lvui::native::integer(lvui::native::range(lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(item->second->value,item->second->position,initial)+"), 0, 9007199254740991.0))";
            else{const auto revision=numeric(values,"iconRevision",0,0,9007199254740991.0);
                if(std::trunc(revision)!=revision)error(item->second->position,"iconRevision must be an integer");value=decimal(revision);}
            setup+="value.icon_revision = "+value+"; ";
        }
        return setup;
    }
    std::string owned_tab_options(const Inputs& values,bool initial,SourcePosition position)const {
        const auto dynamic=[&](std::string_view name){auto item=values.find(name);return item!=values.end()&&native_expression(item->second->value);};
        auto icon=values.find("iconName");
        if(icon==values.end())error(position,"Tab iconName is required and must be nonempty");
        if(!dynamic("iconName")){const auto name=text(values,"iconName");
            if(name.find_first_not_of(" \t\r\n")==name.npos)error(icon->second->position,"Tab iconName is required and must be nonempty");}
        std::string setup=owned_named_icon_setup(values,initial,true);
        for(const auto& [field,key]:std::map<std::string,std::string,std::less<>>{
                {"selected","--lvrs-theme-colors-text-title-header-color"},{"text","--lvrs-theme-colors-text-body-color"},
                {"disabled_text","--lvrs-theme-colors-text-disabled-color"},{"badge","--lvrs-theme-colors-text-description-color"},
                {"surface_selected","--lvrs-theme-colors-panel-surface-raised"},{"hover","--lvrs-theme-colors-panel-surface-alt"},
                {"pressed","--lvrs-theme-colors-panel-surface-raised"},{"accent","--lvrs-theme-colors-action-primary"}}){
            const auto c=token_color(key);setup+="value.configuration.palette."+field+" = lvui::StyleColor{"+decimal(c.red)+", "+decimal(c.green)+", "+decimal(c.blue)+", "+decimal(c.alpha)+"}; ";
        }
        setup+="value.configuration.font_size = "+decimal(token_number("--lvrs-theme-typography-body-text-body",1,256))+"; ";
        setup+="value.configuration.line_height = "+decimal(token_number("--lvrs-theme-typography-body-text-body-line-height",0,8192))+"; ";
        setup+="value.configuration.font_weight = "+decimal(token_number("--lvrs-theme-typography-weight-medium",400,700))+"; ";
        for(const auto& [name,field]:std::map<std::string,std::string,std::less<>>{{"text","text"},{"badge","badge"},{"accessibilityName","accessibility_name"}})
            if(auto item=values.find(name);item!=values.end())setup+="value.configuration."+field+" = "+(dynamic(name)?"lvui::native::text("+expression_code(item->second->value,item->second->position,initial)+")":literal(text(values,name)))+"; ";
        for(const auto& [name,field]:std::map<std::string,std::string,std::less<>>{{"enabled","enabled"},{"selected","selected"}})
            if(auto item=values.find(name);item!=values.end())setup+="value.configuration."+field+" = "+(dynamic(name)?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(values,name,name=="enabled"))+"; ";
        if(auto item=values.find("visible");item!=values.end())setup+="value.visible = "+
            (dynamic("visible")?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(values,"visible",true))+"; ";
        for(const auto& [name,field,lo,hi]:{std::tuple{"height","height",1.,8192.},std::tuple{"iconSize","icon_size",1.,256.},std::tuple{"minimumWidth","minimum_width",1.,8192.}})
            if(auto item=values.find(name);item!=values.end())setup+="value.configuration."+std::string(field)+" = "+(dynamic(name)?"lvui::native::number("+expression_code(item->second->value,item->second->position,initial)+")":decimal(numeric(values,name,0,lo,hi)))+"; ";
        if(auto item=values.find("tabStyle");item!=values.end()){
            std::string style;
            if(dynamic("tabStyle"))style="lvui::native::tab_style("+expression_code(item->second->value,item->second->position,initial)+")";
            else{const auto& v=item->second->value;
                if(v.kind!=ValueKind::enumeration||(v.text!=".underline"&&v.text!=".Underline"&&v.text!=".surface"&&v.text!=".Surface"))error(item->second->position,"Unknown Tab style");
                style="lvui::TabStyle::"+std::string(v.text==".surface"||v.text==".Surface"?"surface":"underline");}
            setup+="value.configuration.style = "+style+"; ";
        }
        return "([&] { lvui::native::TabInput value; "+setup+"return value; }())";
    }
    std::string owned_tabbar_options(const Inputs& values,bool initial)const {
        const auto dynamic=[&](std::string_view name){auto item=values.find(name);return item!=values.end()&&native_expression(item->second->value);};
        std::string setup;
        for(const auto& [name,field,low,high]:{std::tuple{"spacing","spacing",0.,32767.},std::tuple{"minimumTabWidth","minimum_tab_width",1.,8192.}})
            if(auto item=values.find(name);item!=values.end())setup+="value."+std::string(field)+" = "+
                (dynamic(name)?"lvui::native::number("+expression_code(item->second->value,item->second->position,initial)+")":decimal(numeric(values,name,0,low,high)))+"; ";
        if(auto item=values.find("currentIndex");item!=values.end()){
            std::string current;
            if(dynamic("currentIndex"))current="static_cast<int>(lvui::native::integer(lvui::native::range("+expression_code(item->second->value,item->second->position,initial)+", -1, 4095)))";
            else{const auto number=numeric(values,"currentIndex",0,-1,4095);if(std::trunc(number)!=number)error(item->second->position,"TabBar currentIndex must be an integer");current=std::to_string(int(number));}
            setup+="value.current_index = "+current+"; ";
        }
        if(auto item=values.find("autoSelect");item!=values.end())setup+="value.auto_select = "+
            (dynamic("autoSelect")?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(values,"autoSelect",true))+"; ";
        if(auto item=values.find("accessibilityName");item!=values.end())setup+="value.accessibility_name = "+
            (dynamic("accessibilityName")?"lvui::native::text("+expression_code(item->second->value,item->second->position,initial)+")":literal(text(values,"accessibilityName")))+"; ";
        return "([&] { lvui::TabBarConfiguration value; "+setup+"return value; }())";
    }
    std::string owned_color_button_options(const Inputs& values,bool initial,SourcePosition position)const {
        const auto dynamic=[&](std::string_view name){auto item=values.find(name);return item!=values.end()&&native_expression(item->second->value);};
        if(values.contains("model")&&values.contains("currentColor"))error(position,"ColorPickerButton accepts either model or currentColor");
        if(values.contains("modelRevision")&&!values.contains("model"))error(values.at("modelRevision")->position,"modelRevision requires a color source");
        std::string setup;
        for(const auto& [field,key]:{std::pair{"current_color","--lvrs-theme-colors-accent-purple-default"},
                std::pair{"palette.normal","--lvrs-theme-colors-panel-surface-alt"},std::pair{"palette.hover","--lvrs-theme-colors-panel-surface-raised"},
                std::pair{"palette.pressed","--lvrs-theme-colors-panel-window-alt"},std::pair{"palette.focus","--lvrs-theme-colors-action-primary"}}){
            const auto c=token_color(key);setup+="value.configuration."+std::string(field)+" = lvui::StyleColor{"+decimal(c.red)+", "+decimal(c.green)+", "+decimal(c.blue)+", "+decimal(c.alpha)+"}; ";
        }
        for(const auto& [name,field]:{std::pair{"currentColor","current_color"},std::pair{"backgroundColor","palette.normal"},
                std::pair{"hoverColor","palette.hover"},std::pair{"pressedColor","palette.pressed"},std::pair{"borderColor","palette.border"},std::pair{"focusColor","palette.focus"}})
            if(auto item=values.find(name);item!=values.end())setup+="value.configuration."+std::string(field)+" = lvui::native::color_button_color("+color_expression(item->second->value,item->second->position,initial)+"); ";
        if(auto item=values.find("buttonSize");item!=values.end()){
            std::string size;
            if(dynamic("buttonSize"))size="lvui::native::color_button_size(lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(item->second->value,item->second->position,initial)+"))";
            else{const auto value=resolve(item->second->value,item->second->position);
                if(value.kind==ValueKind::enumeration){if(value.text==".Small"||value.text==".small")size="22";
                    else if(value.text==".Medium"||value.text==".medium")size="28";
                    else if(value.text==".Large"||value.text==".large")size="36";
                    else error(item->second->position,"Unknown color button size");}
                else size=decimal(numeric(values,"buttonSize",28,8,256));}
            setup+="value.configuration.size = "+size+"; ";
        }
        for(const auto& [name,field,high]:{std::tuple{"padding","padding",128.},std::tuple{"cornerRadius","corner_radius",256.},std::tuple{"borderOpacity","border_opacity",1.}})
            if(auto item=values.find(name);item!=values.end())setup+="value.configuration."+std::string(field)+" = "+
                (dynamic(name)?"lvui::native::number("+expression_code(item->second->value,item->second->position,initial)+")":decimal(numeric(values,name,0,0,high)))+"; ";
        for(const auto& [name,field,fallback]:{std::tuple{"enabled","enabled",true},std::tuple{"showHueRing","show_hue_ring",true},std::tuple{"circular","circular",false}})
            if(auto item=values.find(name);item!=values.end())setup+="value.configuration."+std::string(field)+" = "+
                (dynamic(name)?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(values,name,fallback))+"; ";
        if(auto item=values.find("accessibilityName");item!=values.end())setup+="value.configuration.accessibility_name = "+
            (dynamic("accessibilityName")?"lvui::native::text("+expression_code(item->second->value,item->second->position,initial)+")":literal(text(values,"accessibilityName")))+"; ";
        if(auto item=values.find("model");item!=values.end()){
            if(!dynamic("model"))error(item->second->position,"model requires an LVUI ColorSource from a C++ backend");
            setup+="value.require_source = true; value.source = lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(item->second->value,item->second->position,initial)+"); ";
        }
        if(auto item=values.find("modelRevision");item!=values.end()){
            std::string revision;
            if(dynamic("modelRevision"))revision="lvui::native::integer(lvui::native::range("+expression_code(item->second->value,item->second->position,initial)+", 0, 9007199254740991.0))";
            else{const auto number=numeric(values,"modelRevision",0,0,9007199254740991.0);if(std::trunc(number)!=number)error(item->second->position,"modelRevision must be an integer");revision=decimal(number);}
            setup+="value.source_revision = "+revision+"; ";
        }
        return "([&] { lvui::native::ColorPickerButtonInput value; "+setup+"return value; }())";
    }
    std::string owned_color_surface_options(const Inputs& values,bool initial,SourcePosition position)const {
        const auto dynamic=[&](std::string_view name){auto item=values.find(name);return item!=values.end()&&native_expression(item->second->value);};
        const auto model=values.find("model");
        if(model==values.end())error(position,"Color surface requires an LVUI ColorSurfaceSource model");
        if(!dynamic("model"))error(model->second->position,"model requires an LVUI ColorSurfaceSource from a C++ backend");
        std::string setup="value.source = lvui::native::widget_value<ii::gui::WidgetColor>("+
            expression_code(model->second->value,model->second->position,initial)+"); ";
        if(auto item=values.find("field");item!=values.end()){
            std::string kind;
            if(dynamic("field"))kind="lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(item->second->value,item->second->position,initial)+")";
            else{
                const auto value=resolve(item->second->value,item->second->position);
                const std::map<std::string,std::string,std::less<>> fields{{".Wheel","wheel"},{".wheel","wheel"},{".HueSaturation","hue_saturation"},{".hueSaturation","hue_saturation"},{".SaturationBrightness","saturation_brightness"},{".saturationBrightness","saturation_brightness"},{".Spectrum","spectrum"},{".spectrum","spectrum"},{".HueRail","hue_rail"},{".hueRail","hue_rail"},{".BrightnessRail","brightness_rail"},{".brightnessRail","brightness_rail"}};
                const auto found=fields.find(value.text);if(value.kind!=ValueKind::enumeration||found==fields.end())error(item->second->position,"Unknown color field");
                kind="lvui::ColorSurfaceKind::"+found->second;
            }
            setup+="value.configuration.kind = "+kind+"; ";
        }
        setup+="value.configuration.width = (value.configuration.kind == lvui::ColorSurfaceKind::hue_rail || value.configuration.kind == lvui::ColorSurfaceKind::brightness_rail) ? 18 : 232; value.configuration.height = 232; ";
        for(const auto& [name,field,low,high]:{std::tuple{"width","width",12.,1024.},std::tuple{"height","height",12.,1024.},std::tuple{"markerDiameter","marker_diameter",2.,64.}})
            if(auto item=values.find(name);item!=values.end())setup+="value.configuration."+std::string(field)+" = "+
                (dynamic(name)?"lvui::native::number("+expression_code(item->second->value,item->second->position,initial)+")":decimal(numeric(values,name,0,low,high)))+"; ";
        if(auto item=values.find("enabled");item!=values.end())setup+="value.configuration.enabled = "+
            (dynamic("enabled")?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(values,"enabled",true))+"; ";
        if(auto item=values.find("accessibilityName");item!=values.end())setup+="value.configuration.accessibility_name = "+
            (dynamic("accessibilityName")?"lvui::native::text("+expression_code(item->second->value,item->second->position,initial)+")":literal(text(values,"accessibilityName")))+"; ";
        if(auto item=values.find("modelRevision");item!=values.end()){
            std::string revision;
            if(dynamic("modelRevision"))revision="lvui::native::integer(lvui::native::range("+expression_code(item->second->value,item->second->position,initial)+", 0, 9007199254740991.0))";
            else{const auto number=numeric(values,"modelRevision",0,0,9007199254740991.0);if(std::trunc(number)!=number)error(item->second->position,"modelRevision must be an integer");revision=decimal(number);}
            setup+="value.source_revision = "+revision+"; ";
        }
        return "([&] { lvui::native::ColorSurfaceInput value; "+setup+"return value; }())";
    }
    std::string owned_color_picker_options(const Inputs& values,bool initial,SourcePosition position,std::string_view fallback)const {
        const auto dynamic=[&](std::string_view name){auto item=values.find(name);return item!=values.end()&&native_expression(item->second->value);};
        std::string setup;
        if(auto item=values.find("model");item!=values.end()){
            if(!dynamic("model"))error(item->second->position,"Color picker model requires an LVUI ColorPickerSession from a C++ backend");
            setup+="value.session = lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(item->second->value,item->second->position,initial)+"); ";
        }else setup+="value.session = "+std::string(fallback)+"; ";
        if(auto item=values.find("type");item!=values.end()){
            std::string mode;
            if(dynamic("type"))mode="lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(item->second->value,item->second->position,initial)+")";
            else{
                const auto value=resolve(item->second->value,item->second->position);
                const std::map<std::string,std::string,std::less<>> types{{".Wheel","wheel"},{".wheel","wheel"},{".HueSaturation","hue_saturation"},{".hueSaturation","hue_saturation"},{".SaturationBrightness","saturation_brightness"},{".saturationBrightness","saturation_brightness"},{".Grayscale","grayscale"},{".grayscale","grayscale"},{".RGB","rgb"},{".rgb","rgb"},{".CMYK","cmyk"},{".cmyk","cmyk"}};
                auto found=types.find(value.text);if(value.kind!=ValueKind::enumeration||found==types.end())error(item->second->position,"Unknown color picker type");
                mode="lvui::ColorPickerMode::"+found->second;
            }
            setup+="value.configuration.mode = "+mode+"; ";
        }
        if(!dynamic("width")&&!dynamic("padding")&&numeric(values,"width",320,320,1024)-2*numeric(values,"padding",12,0,64)<228)
            error(position,"ColorPicker padding must leave 228 content units");
        for(const auto& [name,field,low,high]:{std::tuple{"width","width",320.,1024.},std::tuple{"padding","padding",0.,64.}})
            if(auto item=values.find(name);item!=values.end())setup+="value.configuration."+std::string(field)+" = "+
                (dynamic(name)?"lvui::native::number("+expression_code(item->second->value,item->second->position,initial)+")":decimal(numeric(values,name,0,low,high)))+"; ";
        for(const auto& [name,field,fallback_value]:{std::tuple{"enabled","enabled",true},std::tuple{"showRecentColors","show_recent_colors",true},
                std::tuple{"showActions","show_actions",true},std::tuple{"rememberOnAccept","remember_on_accept",true}})
            if(auto item=values.find(name);item!=values.end())setup+="value.configuration."+std::string(field)+" = "+
                (dynamic(name)?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(values,name,fallback_value))+"; ";
        for(const auto& [name,field]:{std::pair{"applyText","apply_text"},std::pair{"cancelText","cancel_text"},std::pair{"accessibilityName","accessibility_name"}})
            if(auto item=values.find(name);item!=values.end())setup+="value.configuration."+std::string(field)+" = "+
                (dynamic(name)?"lvui::native::text("+expression_code(item->second->value,item->second->position,initial)+")":literal(text(values,name)))+"; ";
        if(auto item=values.find("recentColors");item!=values.end()){
            if(dynamic("recentColors"))setup+="value.configuration.recent_colors = lvui::native::value_copy("+expression_code(item->second->value,item->second->position,initial)+"); ";
            else{
                auto colors=resolve(item->second->value,item->second->position);
                if(colors.kind!=ValueKind::list)error(item->second->position,"Recent colors must be a Color/HEX String list");
                setup+="value.configuration.recent_colors = {";std::size_t count=0;
                for(const auto& element:colors.elements){if(count==10)break;if(count++)setup+=", ";const auto color=color_value(element,item->second->position);
                    setup+="lvui::StyleColor{"+decimal(color.red)+", "+decimal(color.green)+", "+decimal(color.blue)+", "+decimal(color.alpha)+"}";}
                setup+="}; ";
            }
        }else setup+="value.preserve_recent_colors = true; ";
        if(auto item=values.find("modelRevision");item!=values.end()){
            std::string revision;
            if(dynamic("modelRevision"))revision="lvui::native::integer(lvui::native::range("+expression_code(item->second->value,item->second->position,initial)+", 0, 9007199254740991.0))";
            else{const auto number=numeric(values,"modelRevision",0,0,9007199254740991.0);if(std::trunc(number)!=number)error(item->second->position,"modelRevision must be an integer");revision=decimal(number);}
            setup+="value.source_revision = "+revision+"; ";
        }
        return "([&] { lvui::native::ColorPickerInput value; "+setup+"return value; }())";
    }
    std::string owned_modal_options(const Inputs& values,bool initial,SourcePosition position)const {
        const auto dynamic=[&](std::string_view name){auto item=values.find(name);return item!=values.end()&&native_expression(item->second->value);};
        if(values.contains("description")&&values.contains("desc"))error(position,"Use description or its desc alias, not both");
        if(auto item=values.find("motion");item!=values.end())error(item->second->position,"The independent Modal does not support a motion model yet");
        if(!dynamic("minWidth")&&!dynamic("maxWidth")&&numeric(values,"minWidth",520,0,8192)>numeric(values,"maxWidth",980,1,8192))
            error(position,"Modal minWidth exceeds maxWidth");
        std::string setup;
        for(const auto& [name,field]:{std::pair{"title","title"},std::pair{"description","description"},std::pair{"desc","description"},
                std::pair{"message","message"},std::pair{"iconName","icon_name"},std::pair{"accessibilityName","accessibility_name"},
                std::pair{"primaryText","action_text[0]"},std::pair{"secondaryText","action_text[1]"},std::pair{"tertiaryText","action_text[2]"}})
            if(auto item=values.find(name);item!=values.end())setup+="value.configuration."+std::string(field)+" = "+
                (dynamic(name)?"lvui::native::text("+expression_code(item->second->value,item->second->position,initial)+")":literal(text(values,name)))+"; ";
        for(const auto& [name,field,fallback]:{std::tuple{"showIcon","show_icon",true},std::tuple{"dismissOnBackground","dismiss_on_background",true},
                std::tuple{"dismissOnEscape","dismiss_on_escape",true},std::tuple{"motionEnabled","motion_enabled",true},
                std::tuple{"primaryEnabled","action_enabled[0]",true},std::tuple{"secondaryEnabled","action_enabled[1]",true},std::tuple{"tertiaryEnabled","action_enabled[2]",true}})
            if(auto item=values.find(name);item!=values.end())setup+="value.configuration."+std::string(field)+" = "+
                (dynamic(name)?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(values,name,fallback))+"; ";
        for(const auto& [name,field,low,high]:{std::tuple{"minWidth","minimum_width",0.,8192.},std::tuple{"maxWidth","maximum_width",1.,8192.},
                std::tuple{"preferredWidth","preferred_width",1.,8192.},std::tuple{"sidePadding","side_padding",0.,8192.},
                std::tuple{"frameMinHeight","minimum_height",0.,8192.},std::tuple{"verticalOffset","vertical_offset",-8192.,8192.},
                std::tuple{"contentPadding","content_padding",0.,8192.},std::tuple{"sectionSpacing","section_spacing",0.,8192.},
                std::tuple{"actionSpacing","action_spacing",0.,8192.},std::tuple{"frameCornerRadius","corner_radius",0.,8192.},
                std::tuple{"iconSize","icon_size",1.,256.},std::tuple{"iconCornerRadius","icon_corner_radius",0.,8192.},
                std::tuple{"enterDuration","enter_duration_ms",0.,86400000.},std::tuple{"exitDuration","exit_duration_ms",0.,86400000.},
                std::tuple{"motionOvershoot","overshoot",0.,4.},std::tuple{"initialScale","initial_scale",.5,1.}})
            if(auto item=values.find(name);item!=values.end())setup+="value.configuration."+std::string(field)+" = "+
                (dynamic(name)?"lvui::native::number("+expression_code(item->second->value,item->second->position,initial)+")":decimal(numeric(values,name,0,low,high)))+"; ";
        if(auto item=values.find("buttonCount");item!=values.end()){
            std::string count;
            if(dynamic("buttonCount"))count="static_cast<unsigned>(lvui::native::integer(lvui::native::range("+expression_code(item->second->value,item->second->position,initial)+", 0, 3)))";
            else{const auto number=numeric(values,"buttonCount",0,0,3);if(std::trunc(number)!=number)error(item->second->position,"buttonCount must be an integer");count=decimal(number);}
            setup+="value.configuration.action_count = "+count+"; ";
        }
        if(auto item=values.find("shapeStyle");item!=values.end()){
            std::string shape;
            if(dynamic("shapeStyle"))shape="lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(item->second->value,item->second->position,initial)+")";
            else{const auto value=resolve(item->second->value,item->second->position);
                if(value.kind!=ValueKind::enumeration)error(item->second->position,"Unknown Modal shapeStyle");
                if(value.text==".cylinder"||value.text==".shapeCylinder")shape="lvui::ModalShape::cylinder";
                else if(value.text==".roundRect"||value.text==".shapeRoundRect")shape="lvui::ModalShape::rounded";
                else error(item->second->position,"Unknown Modal shapeStyle");}
            setup+="value.configuration.shape = "+shape+"; ";
        }
        for(const auto& [name,field]:{std::pair{"frameColor","frame"},std::pair{"backdropColor","backdrop"},
                std::pair{"iconBackgroundColor","icon_background"},std::pair{"iconBorderColor","icon_border"}})
            if(auto item=values.find(name);item!=values.end())setup+="value.configuration.palette."+std::string(field)+" = lvui::native::modal_color("+
                color_expression(item->second->value,item->second->position,initial)+"); ";
        for(const auto& [name,field]:{std::pair{"iconSource","icon"},std::pair{"iconResolver","icon_source"}})
            if(auto item=values.find(name);item!=values.end()){
                if(!dynamic(name))error(item->second->position,"Modal icon source requires a scoped C++ member");
                setup+="value."+std::string(field)+" = lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(item->second->value,item->second->position,initial)+"); ";
            }
        if(auto item=values.find("iconRevision");item!=values.end()){
            std::string revision;
            if(dynamic("iconRevision"))revision="lvui::native::integer(lvui::native::range("+expression_code(item->second->value,item->second->position,initial)+", 0, 9007199254740991.0))";
            else{const auto number=numeric(values,"iconRevision",0,0,9007199254740991.0);if(std::trunc(number)!=number)error(item->second->position,"iconRevision must be an integer");revision=decimal(number);}
            setup+="value.icon_revision = "+revision+"; ";
        }
        return "([&] { lvui::native::ModalInput value; "+setup+"return value; }())";
    }
    std::string owned_sheet_options(const Inputs& values,bool initial)const {
        const auto dynamic=[&](std::string_view name){auto item=values.find(name);return item!=values.end()&&native_expression(item->second->value);};
        if(auto item=values.find("motion");item!=values.end())error(item->second->position,"The independent Sheet does not support a motion model yet");
        std::string setup;
        for(const auto& [name,field]:{std::pair{"title","title"},std::pair{"description","description"},std::pair{"accessibilityName","accessibility_name"}})
            if(auto item=values.find(name);item!=values.end())setup+="value.configuration."+std::string(field)+" = "+
                (dynamic(name)?"lvui::native::text("+expression_code(item->second->value,item->second->position,initial)+")":literal(text(values,name)))+"; ";
        for(const auto& [name,field]:{std::pair{"showHeader","show_header"},std::pair{"showDescription","show_description"},
                std::pair{"showCloseButton","show_close_button"},std::pair{"showGrabber","show_grabber"},std::pair{"scrollContent","scroll_content"},
                std::pair{"dismissOnBackground","dismiss_on_background"},std::pair{"dismissOnEscape","dismiss_on_escape"},
                std::pair{"dismissOnDrag","dismiss_on_drag"},std::pair{"motionEnabled","motion_enabled"}})
            if(auto item=values.find(name);item!=values.end())setup+="value.configuration."+std::string(field)+" = "+
                (dynamic(name)?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(values,name,false))+"; ";
        for(const auto& [name,field]:{std::pair{"preferredWidth","preferred_width"},std::pair{"preferredHeight","preferred_height"},
                std::pair{"desktopMargin","desktop_margin"},std::pair{"contentPadding","content_padding"},std::pair{"topSafeInset","top_safe_inset"},
                std::pair{"bottomSafeInset","bottom_safe_inset"},std::pair{"cornerRadius","corner_radius"}})
            if(auto item=values.find(name);item!=values.end())setup+="value.configuration."+std::string(field)+" = "+
                (dynamic(name)?"lvui::native::number("+expression_code(item->second->value,item->second->position,initial)+")":decimal(numeric(values,name,0,0,8192)))+"; ";
        if(auto item=values.find("animationDuration");item!=values.end()){
            std::string value;
            if(dynamic("animationDuration"))value="lvui::native::integer(lvui::native::range("+expression_code(item->second->value,item->second->position,initial)+", 0, 86400000))";
            else{const auto number=numeric(values,"animationDuration",420,0,86400000);if(std::trunc(number)!=number)error(item->second->position,"Sheet animationDuration requires integral milliseconds");value=decimal(number);}
            setup+="value.configuration.animation_duration_ms = "+value+"; ";
        }
        for(const auto& [name,type,field]:{std::tuple{"presentation","SheetPresentation","presentation"},std::tuple{"detent","SheetDetent","detent"}})
            if(auto item=values.find(name);item!=values.end()){
                std::string result;
                if(dynamic(name))result="lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(item->second->value,item->second->position,initial)+")";
                else{const auto value=resolve(item->second->value,item->second->position);std::string choice;
                    if(value.kind!=ValueKind::enumeration)error(item->second->position,"Unknown Sheet "+std::string(name));
                    const auto choices=std::string_view(name)=="presentation"?std::array<std::string_view,3>{"automatic","mobile","desktop"}:std::array<std::string_view,3>{"fit","medium","large"};
                    for(auto candidate:choices){
                        auto upper=std::string(candidate);upper[0]=char(upper[0]-'a'+'A');
                        if(value.text=="."+std::string(candidate)||value.text=="."+upper){choice=std::string(candidate);break;}}
                    if(choice.empty())error(item->second->position,"Unknown Sheet "+std::string(name));
                    result="lvui::"+std::string(type)+"::"+choice;}
                setup+="value.configuration."+std::string(field)+" = "+result+"; ";
            }
        for(const auto& [name,field]:{std::pair{"surfaceColor","surface"},std::pair{"backdropColor","backdrop"}})
            if(auto item=values.find(name);item!=values.end())setup+="value.configuration.palette."+std::string(field)+" = lvui::native::sheet_color("+
                color_expression(item->second->value,item->second->position,initial)+"); ";
        return "([&] { lvui::native::SheetInput value; "+setup+"return value; }())";
    }
    std::string owned_alert_options(const Inputs& values,bool initial,SourcePosition position)const {
        const auto dynamic=[&](std::string_view name){auto item=values.find(name);return item!=values.end()&&native_expression(item->second->value);};
        if(auto item=values.find("motion");item!=values.end())error(item->second->position,"The independent Alert does not support a motion model yet");
        if(!dynamic("minWidth")&&!dynamic("maxWidth")&&numeric(values,"minWidth",280,0,8192)>numeric(values,"maxWidth",500,1,8192))
            error(position,"Alert minWidth exceeds maxWidth");
        const auto message=alias_key(values,"message","description");
        const auto first=alias_key(values,"primaryText","button1Text"),second=alias_key(values,"secondaryText","button2Text"),third=alias_key(values,"tertiaryText","button3Text");
        const auto icon=alias_key(values,"iconSource","appIconSource");
        if(values.contains("imageSource")&&(values.contains("iconSource")||values.contains("appIconSource")))error(position,"Use one Alert image source");
        const auto image=values.contains("imageSource")?std::string("imageSource"):icon;
        std::string setup;
        for(const auto& [name,field]:{std::pair{std::string("title"),"title"},std::pair{message,"message"},
                std::pair{first,"action_text[0]"},std::pair{second,"action_text[1]"},std::pair{third,"action_text[2]"},
                std::pair{std::string("accessibilityName"),"accessibility_name"}})
            if(auto item=values.find(name);item!=values.end())setup+="value.configuration."+std::string(field)+" = "+
                (dynamic(name)?"lvui::native::text("+expression_code(item->second->value,item->second->position,initial)+")":literal(text(values,name)))+"; ";
        if(auto item=values.find("iconName");item!=values.end())setup+="value.icon_name = "+
            (dynamic("iconName")?"lvui::native::text("+expression_code(item->second->value,item->second->position,initial)+")":literal(text(values,"iconName")))+"; ";
        for(const auto& [name,field]:{std::pair{"showIcon","show_icon"},std::pair{"dismissOnBackground","dismiss_on_background"},
                std::pair{"closeOnEscape","dismiss_on_escape"},std::pair{"motionEnabled","motion_enabled"},
                std::pair{"glassEnabled","glass_enabled"},std::pair{"shadowEnabled","shadow_enabled"},
                std::pair{"secondaryDestructive","secondary_destructive"},std::pair{"primaryEnabled","action_enabled[0]"},
                std::pair{"secondaryEnabled","action_enabled[1]"},std::pair{"tertiaryEnabled","action_enabled[2]"}})
            if(auto item=values.find(name);item!=values.end())setup+="value.configuration."+std::string(field)+" = "+
                (dynamic(name)?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(values,name,false))+"; ";
        for(const auto& [name,field,low,high]:{std::tuple{"minWidth","minimum_width",0.,8192.},std::tuple{"maxWidth","maximum_width",1.,8192.},
                std::tuple{"preferredWidth","preferred_width",1.,8192.},std::tuple{"sidePadding","side_padding",0.,8192.},
                std::tuple{"cardCornerRadius","corner_radius",0.,8192.},std::tuple{"iconFrameSize","icon_extent",1.,256.},
                std::tuple{"glassBlurRadius","blur_radius",0.,64.},std::tuple{"enterDuration","enter_duration_ms",0.,86400000.},
                std::tuple{"exitDuration","exit_duration_ms",0.,86400000.},std::tuple{"motionOvershoot","overshoot",0.,4.},
                std::tuple{"initialScale","initial_scale",.5,1.}})
            if(auto item=values.find(name);item!=values.end())setup+="value.configuration."+std::string(field)+" = "+
                (dynamic(name)?"lvui::native::number("+expression_code(item->second->value,item->second->position,initial)+")":decimal(numeric(values,name,0,low,high)))+"; ";
        if(auto item=values.find("buttonCount");item!=values.end()){
            std::string count;
            if(dynamic("buttonCount"))count="static_cast<unsigned>(lvui::native::integer(lvui::native::range("+expression_code(item->second->value,item->second->position,initial)+", 0, 3)))";
            else{const auto number=numeric(values,"buttonCount",0,0,3);if(std::trunc(number)!=number)error(item->second->position,"buttonCount must be an integer");count=decimal(number);}
            setup+="value.configuration.action_count = "+count+"; ";
        }
        if(auto item=values.find("shapeStyle");item!=values.end()){
            std::string shape;
            if(dynamic("shapeStyle"))shape="lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(item->second->value,item->second->position,initial)+")";
            else{const auto value=resolve(item->second->value,item->second->position);
                if(value.kind!=ValueKind::enumeration)error(item->second->position,"Unknown Alert shapeStyle");
                if(value.text==".cylinder"||value.text==".shapeCylinder")shape="lvui::AlertShape::cylinder";
                else if(value.text==".roundRect"||value.text==".shapeRoundRect")shape="lvui::AlertShape::rounded";
                else error(item->second->position,"Unknown Alert shapeStyle");}
            setup+="value.configuration.shape = "+shape+"; ";
        }
        for(const auto& [name,field]:{std::pair{"cardBackgroundColor","palette.frame"},std::pair{"backdropColor","palette.backdrop"},std::pair{"primaryColor","primary_color"}})
            if(auto item=values.find(name);item!=values.end())setup+="value.configuration."+std::string(field)+" = lvui::native::alert_button_color("+
                color_expression(item->second->value,item->second->position,initial)+"); ";
        if(auto item=values.find(image);item!=values.end()){
            if(dynamic(image))setup+="value.icon = lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(item->second->value,item->second->position,initial)+"); ";
            else if(item->second->value.kind==ValueKind::string&&item->second->value.text.empty())setup+="value.use_default_icon = false; ";
            else error(item->second->position,"Alert image source requires a scoped raster image or an empty string");
        }
        if(auto item=values.find("iconResolver");item!=values.end()){
            if(!dynamic("iconResolver"))error(item->second->position,"Alert icon resolver requires a scoped C++ member");
            setup+="value.icon_source = lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(item->second->value,item->second->position,initial)+"); ";
        }
        if(auto item=values.find("iconRevision");item!=values.end()){
            std::string revision;
            if(dynamic("iconRevision"))revision="lvui::native::integer(lvui::native::range("+expression_code(item->second->value,item->second->position,initial)+", 0, 9007199254740991.0))";
            else{const auto number=numeric(values,"iconRevision",0,0,9007199254740991.0);if(std::trunc(number)!=number)error(item->second->position,"iconRevision must be an integer");revision=decimal(number);}
            setup+="value.icon_revision = "+revision+"; ";
        }
        return "([&] { lvui::native::AlertInput value; "+setup+"return value; }())";
    }
    std::string owned_alert_button_options(const Inputs& values,bool initial)const {
        const auto dynamic=[&](std::string_view name){auto item=values.find(name);return item!=values.end()&&native_expression(item->second->value);};
        std::string setup;
        for(const auto& [field,key]:{std::pair{"palette.primary","--lvrs-theme-colors-action-primary"},
                std::pair{"palette.normal","--lvrs-theme-colors-panel-surface-raised"},std::pair{"palette.danger","--lvrs-theme-colors-action-danger"},
                std::pair{"palette.text","--lvrs-theme-colors-text-body-color"},std::pair{"palette.disabled_text","--lvrs-theme-colors-text-disabled-color"},
                std::pair{"dialog_text","--lvrs-material-alert-text-primary"},std::pair{"dialog_border","--lvrs-material-alert-action-border"},
                std::pair{"disabled_background","--lvrs-theme-colors-panel-surface-control"},std::pair{"normal_pressed","--lvrs-theme-colors-panel-surface-input"}}){
            const auto color=token_color(key);setup+="value."+std::string(field)+" = lvui::StyleColor{"+decimal(color.red)+", "+decimal(color.green)+", "+decimal(color.blue)+", "+decimal(color.alpha)+"}; ";
        }
        for(const auto& [name,field]:{std::pair{"text","text"},std::pair{"accessibilityName","accessibility_name"}})
            if(auto item=values.find(name);item!=values.end())setup+="value."+std::string(field)+" = "+
                (dynamic(name)?"lvui::native::text("+expression_code(item->second->value,item->second->position,initial)+")":literal(text(values,name)))+"; ";
        for(const auto& [name,field,fallback]:{std::tuple{"enabled","enabled",true},std::tuple{"dialogStyle","dialog_style",false}})
            if(auto item=values.find(name);item!=values.end())setup+="value."+std::string(field)+" = "+
                (dynamic(name)?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(values,name,fallback))+"; ";
        if(auto item=values.find("tone");item!=values.end()){
            std::string tone;
            if(dynamic("tone"))tone="lvui::native::button_tone("+expression_code(item->second->value,item->second->position,initial)+")";
            else{const auto value=resolve(item->second->value,item->second->position);
                const std::map<std::string,std::string,std::less<>> tones{{".primary","primary"},{".Primary","primary"},{".default","normal"},{".Default","normal"},
                    {".normal","normal"},{".borderless","borderless"},{".Borderless","borderless"},{".destructive","destructive"},{".Destructive","destructive"},{".disabled","disabled"},{".Disabled","disabled"}};
                const auto found=tones.find(value.text);if(value.kind!=ValueKind::enumeration||found==tones.end())error(item->second->position,"Unknown AlertButton tone");
                tone="lvui::ButtonTone::"+found->second;
            }
            setup+="value.tone = "+tone+"; ";
        }
        for(const auto& [name,field,low,high]:{std::tuple{"width","width",1.,8192.},std::tuple{"height","height",1.,8192.},
                std::tuple{"horizontalPadding","horizontal_padding",0.,8192.},std::tuple{"cornerRadius","corner_radius",0.,8192.}})
            if(auto item=values.find(name);item!=values.end())setup+="value."+std::string(field)+" = "+
                (dynamic(name)?"lvui::native::number("+expression_code(item->second->value,item->second->position,initial)+")":decimal(numeric(values,name,0,low,high)))+"; ";
        for(const auto& [name,field]:{std::pair{"textColor","text_color"},std::pair{"primaryColor","primary_color"}})
            if(auto item=values.find(name);item!=values.end())setup+="value."+std::string(field)+" = lvui::native::alert_button_color("+
                color_expression(item->second->value,item->second->position,initial)+"); ";
        return "([&] { lvui::AlertButtonConfiguration value; "+setup+"return value; }())";
    }
    std::string owned_popover_options(const Inputs& values,bool initial)const {
        const auto dynamic=[&](std::string_view name){auto item=values.find(name);return item!=values.end()&&native_expression(item->second->value);};
        std::string setup;
        for(const auto& [name,field,high]:{std::tuple{"width","width",8192.},std::tuple{"height","height",8192.},
                std::tuple{"padding","padding",8192.},std::tuple{"margins","margins",8192.},std::tuple{"cornerRadius","corner_radius",8192.},
                std::tuple{"surfaceOpacity","surface_opacity",1.}})
            if(auto item=values.find(name);item!=values.end())setup+="value."+std::string(field)+" = "+
                (dynamic(name)?"lvui::native::number("+expression_code(item->second->value,item->second->position,initial)+")":decimal(numeric(values,name,0,0,high)))+"; ";
        for(const auto& [name,field,fallback]:{std::tuple{"modal","modal",false},std::tuple{"dim","dim",false},std::tuple{"focus","focus_on_open",true},
                std::tuple{"clip","clip_content",false},std::tuple{"closeOnEscape","dismiss_on_escape",true},
                std::tuple{"closeOnPressOutside","dismiss_on_outside_press",true},std::tuple{"closeOnContextRequest","dismiss_on_context_request",true}})
            if(auto item=values.find(name);item!=values.end())setup+="value."+std::string(field)+" = "+
                (dynamic(name)?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(values,name,fallback))+"; ";
        if(auto item=values.find("surfaceColor");item!=values.end())setup+="value.surface_color = lvui::native::popover_color("+
            color_expression(item->second->value,item->second->position,initial)+"); ";
        if(auto item=values.find("accessibilityName");item!=values.end())setup+="value.accessibility_name = "+
            (dynamic("accessibilityName")?"lvui::native::text("+expression_code(item->second->value,item->second->position,initial)+")":literal(text(values,"accessibilityName")))+"; ";
        return "([&] { lvui::PopoverConfiguration value; "+setup+"return value; }())";
    }
    std::string owned_menu_options(const Inputs& values,bool initial,bool context)const {
        const auto dynamic=[&](std::string_view name){auto item=values.find(name);return item!=values.end()&&native_expression(item->second->value);};
        std::string setup="value.configuration.compact_items = "+std::string(context?"true":"false")+
            "; value.configuration.item_width = "+std::string(context?"141":"145")+
            "; value.configuration.top_padding = "+std::string(context?"8":"4")+
            "; value.configuration.bottom_padding = "+std::string(context?"8":"4")+"; value.configuration.corner_radius = 12; ";
        for(const auto& [name,field,low,high]:{std::tuple{"itemWidth","item_width",1.,8192.},std::tuple{"itemSpacing","item_spacing",0.,8192.},
                std::tuple{"leftPadding","left_padding",0.,8192.},std::tuple{"rightPadding","right_padding",0.,8192.},
                std::tuple{"topPadding","top_padding",0.,8192.},std::tuple{"bottomPadding","bottom_padding",0.,8192.},
                std::tuple{"edgeMargin","edge_margin",0.,8192.},std::tuple{"cornerRadius","corner_radius",0.,8192.},
                std::tuple{"menuOpacity","opacity",0.,1.}})
            if(auto item=values.find(name);item!=values.end())setup+="value.configuration."+std::string(field)+" = "+
                (dynamic(name)?"lvui::native::number("+expression_code(item->second->value,item->second->position,initial)+")":decimal(numeric(values,name,0,low,high)))+"; ";
        for(const auto& [name,field,fallback]:{std::tuple{"compactItems","compact_items",context},std::tuple{"autoCloseOnTrigger","auto_close_on_trigger",true},
                std::tuple{"dismissOnGlobalPress","dismiss_outside",true},std::tuple{"dismissOnGlobalContextRequest","dismiss_context",true},
                std::tuple{"showIconSlot","show_icon_slot",true}})
            if(auto item=values.find(name);item!=values.end())setup+="value.configuration."+std::string(field)+" = "+
                (dynamic(name)?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(values,name,fallback))+"; ";
        if(auto item=values.find("selectedIndex");item!=values.end()){
            std::string index;
            if(dynamic("selectedIndex"))index="static_cast<int>(lvui::native::integer(lvui::native::range("+expression_code(item->second->value,item->second->position,initial)+", -1, 4095)))";
            else{const auto number=numeric(values,"selectedIndex",-1,-1,4095);if(std::trunc(number)!=number)error(item->second->position,"Menu selectedIndex must be an integer");index=std::to_string(int(number));}
            setup+="value.selected_index = "+index+"; ";
        }
        if(auto item=values.find("accessibilityName");item!=values.end())setup+="value.configuration.accessibility_name = "+
            (dynamic("accessibilityName")?"lvui::native::text("+expression_code(item->second->value,item->second->position,initial)+")":literal(text(values,"accessibilityName")))+"; ";
        for(const auto& [name,field]:{std::pair{"baseColor","base"},std::pair{"backgroundColor","background"},std::pair{"borderColor","border"}})
            if(auto item=values.find(name);item!=values.end())setup+="value.configuration."+std::string(field)+" = lvui::native::menu_color("+color_expression(item->second->value,item->second->position,initial)+"); ";
        return "([&] { lvui::native::MenuInput value; "+setup+"return value; }())";
    }
    std::string owned_menu_item_options(const Inputs& values,bool initial,bool compact_default,SourcePosition position)const {
        const auto dynamic=[&](std::string_view name){auto item=values.find(name);return item!=values.end()&&native_expression(item->second->value);};
        if(values.contains("key")&&values.contains("shortcut"))error(position,"Use key or shortcut, not both");
        if(values.contains("keepOpen")&&values.contains("closeOnTrigger"))error(position,"Use keepOpen or closeOnTrigger, not both");
        if(auto icon=values.find("icon");icon!=values.end()&&!dynamic("icon"))error(icon->second->position,"MenuItem icon requires a native image from a C++ backend");
        std::string setup="value.configuration.compact = "+std::string(compact_default?"true":"false")+"; ";
        setup+=owned_named_icon_setup(values,initial,true);
        if(auto icon=values.find("icon");icon!=values.end())setup+="value.configuration.icon_name.clear(); value.icon = lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(icon->second->value,icon->second->position,initial)+"); ";
        for(const auto& [field,key]:std::map<std::string,std::string,std::less<>>{
                {"text","--lvrs-theme-colors-text-body-color"},{"disabled_text","--lvrs-theme-colors-text-disabled-color"},
                {"detail","--lvrs-theme-colors-text-description-color"},{"selected","--lvrs-theme-colors-action-primary"},
                {"hover","--lvrs-theme-colors-panel-surface-alt"},{"pressed","--lvrs-theme-colors-accent-blue-subtle"},
                {"focus","--lvrs-theme-colors-action-primary"}}){
            const auto c=token_color(key);setup+="value.configuration.palette."+field+" = lvui::StyleColor{"+decimal(c.red)+", "+decimal(c.green)+", "+decimal(c.blue)+", "+decimal(c.alpha)+"}; ";
        }
        for(const auto& [name,field]:std::map<std::string,std::string,std::less<>>{{"label","label"},{"shortcut","shortcut"},{"key","shortcut"},
                {"keyPlaceholder","key_placeholder"},{"accessibilityName","accessibility_name"}})
            if(auto item=values.find(name);item!=values.end())setup+="value.configuration."+field+" = "+(dynamic(name)?"lvui::native::text("+expression_code(item->second->value,item->second->position,initial)+")":literal(text(values,name)))+"; ";
        for(const auto& [name,field,fallback]:{std::tuple{"enabled","enabled",true},std::tuple{"compact","compact",compact_default},
                std::tuple{"keyVisible","key_visible",true},std::tuple{"showIconSlot","show_icon_slot",true},std::tuple{"showChevron","show_chevron",true},
                std::tuple{"hasChildItems","has_children",true},std::tuple{"expanded","expanded",false}})
            if(auto item=values.find(name);item!=values.end())setup+="value.configuration."+std::string(field)+" = "+
                (dynamic(name)?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(values,name,fallback))+"; ";
        if(auto item=values.find("visible");item!=values.end())setup+="value.visible = "+
            (dynamic("visible")?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(values,"visible",true))+"; ";
        if(auto item=values.find("closeOnTrigger");item!=values.end())setup+="value.close_on_trigger = "+
            (dynamic("closeOnTrigger")?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(values,"closeOnTrigger",true))+"; ";
        if(auto item=values.find("keepOpen");item!=values.end())setup+="value.close_on_trigger = ("+
            (dynamic("keepOpen")?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(values,"keepOpen",false))+
            " ? std::optional<bool>{false} : std::nullopt); ";
        setup+="value.configuration.font_size = value.configuration.compact ? 12 : "+decimal(token_number("--lvrs-theme-typography-body-text-body",1,256))+"; ";
        setup+="value.configuration.line_height = value.configuration.font_size; value.configuration.font_weight = value.configuration.compact ? 400 : "+decimal(token_number("--lvrs-theme-typography-weight-medium",400,700))+"; ";
        for(const auto& [name,field,low,high]:{std::tuple{"itemWidth","width",1.,8192.},std::tuple{"itemHeight","height",1.,8192.},
                std::tuple{"iconSize","icon_size",1.,256.},std::tuple{"chevronSize","chevron_size",1.,256.},
                std::tuple{"horizontalPadding","horizontal_padding",0.,4096.},std::tuple{"spacing","spacing",0.,8192.},
                std::tuple{"fontSize","font_size",1.,256.},std::tuple{"lineHeight","line_height",0.,8192.}})
            if(auto item=values.find(name);item!=values.end())setup+="value.configuration."+std::string(field)+" = "+
                (dynamic(name)?"lvui::native::number("+expression_code(item->second->value,item->second->position,initial)+")":decimal(numeric(values,name,0,low,high)))+"; ";
        if(auto item=values.find("state");item!=values.end()){
            std::string state;
            if(dynamic("state"))state="lvui::native::menu_item_state("+expression_code(item->second->value,item->second->position,initial)+")";
            else{const std::map<std::string,std::string,std::less<>> choices{{".default","normal"},{".normal","normal"},{".selected","selected"},{".inactive","inactive"}};
                const auto found=choices.find(item->second->value.text);if(item->second->value.kind!=ValueKind::enumeration||found==choices.end())error(item->second->position,"Unknown menu item state");state="lvui::MenuEntryState::"+found->second;}
            setup+="value.configuration.state = "+state+"; ";
        }
        if(auto item=values.find("selectionDirection");item!=values.end()){
            std::string direction;
            if(dynamic("selectionDirection"))direction="lvui::native::menu_item_direction("+expression_code(item->second->value,item->second->position,initial)+")";
            else{const std::map<std::string,std::string,std::less<>> choices{{".auto","automatic"},{".right","right"},{".left","left"},{".up","up"},{".down","down"}};
                const auto found=choices.find(item->second->value.text);if(item->second->value.kind!=ValueKind::enumeration||found==choices.end())error(item->second->position,"Unknown menu chevron direction");direction="lvui::MenuDirection::"+found->second;}
            setup+="value.configuration.direction = "+direction+"; ";
        }
        return "([&] { lvui::native::MenuItemInput value; "+setup+"return value; }())";
    }
    std::string owned_dropdown_options(const Inputs& values,bool initial,bool icon_default)const {
        const auto dynamic=[&](std::string_view name){auto item=values.find(name);return item!=values.end()&&native_expression(item->second->value);};
        if(values.contains("leftPadding")&&values.contains("horizontalPadding"))error(values.at("leftPadding")->position,"Use leftPadding or horizontalPadding, not both");
        if(auto icon=values.find("icon");icon!=values.end()&&!dynamic("icon"))error(icon->second->position,"Dropdown icon requires a native image from a C++ backend");
        std::string setup;
        for(const auto& [field,key]:std::map<std::string,std::string,std::less<>>{
                {"primary","--lvrs-theme-colors-action-primary"},{"primary_hover","--lvrs-theme-colors-accent-blue-hover"},
                {"primary_pressed","--lvrs-theme-colors-accent-blue-pressed"},{"normal","--lvrs-theme-colors-panel-surface-raised"},
                {"normal_hover","--lvrs-theme-colors-panel-surface-alt"},{"normal_pressed","--lvrs-theme-colors-accent-blue-subtle"},
                {"danger","--lvrs-theme-colors-action-danger"},{"disabled_background","--lvrs-theme-colors-panel-sub-surface"},
                {"text","--lvrs-theme-colors-text-body-color"},{"disabled_text","--lvrs-theme-colors-text-disabled-color"}}){
            const auto color=token_color(key);setup+="value.configuration.palette."+field+" = lvui::StyleColor{"+decimal(color.red)+", "+decimal(color.green)+", "+decimal(color.blue)+", "+decimal(color.alpha)+"}; ";
        }
        setup+="value.configuration.font_size = "+decimal(token_number("--lvrs-theme-typography-body-text-body",1,256))+"; ";
        setup+="value.configuration.line_height = "+decimal(token_number("--lvrs-theme-typography-body-text-body-line-height",0,8192))+"; ";
        setup+="value.configuration.font_weight = "+decimal(token_number("--lvrs-theme-typography-weight-medium",400,700))+"; ";
        for(const auto& [name,field]:std::map<std::string,std::string,std::less<>>{{"text","text"},{"accessibilityName","accessibility_name"}})
            if(auto item=values.find(name);item!=values.end())setup+="value.configuration."+field+" = "+(dynamic(name)?"lvui::native::text("+expression_code(item->second->value,item->second->position,initial)+")":literal(text(values,name)))+"; ";
        for(const auto& [name,field,fallback]:{std::tuple{"enabled","enabled",true},std::tuple{"iconMode","icon_mode",icon_default}})
            if(auto item=values.find(name);item!=values.end())setup+="value.configuration."+std::string(field)+" = "+(dynamic(name)?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(values,name,fallback))+"; ";
            else if(std::string_view(name)=="iconMode")setup+="value.configuration.icon_mode = "+std::string(fallback?"true":"false")+"; ";
        setup+="value.configuration.left_padding = value.configuration.icon_mode ? 4 : 8; value.configuration.spacing = value.configuration.icon_mode ? -2 : 0; ";
        if(auto item=values.find("icon");item!=values.end())setup+="value.icon = lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(item->second->value,item->second->position,initial)+"); ";
        setup+=owned_named_icon_setup(values,initial);
        if(auto item=values.find("tone");item!=values.end()){
            std::string tone;
            if(dynamic("tone"))tone="lvui::native::dropdown_tone("+expression_code(item->second->value,item->second->position,initial)+")";
            else{const std::map<std::string,std::string,std::less<>> tones{{".primary","primary"},{".default","normal"},{".borderless","borderless"},{".destructive","destructive"},{".disabled","disabled"}};
                const auto selected=tones.find(item->second->value.text);if(item->second->value.kind!=ValueKind::enumeration||selected==tones.end())error(item->second->position,"Unknown Dropdown tone");tone="lvui::ButtonTone::"+selected->second;}
            setup+="value.configuration.tone = "+tone+"; ";
        }
        const auto number=[&](std::string_view name,std::string_view field,double low,double high){auto item=values.find(name);if(item==values.end())return;
            const auto value=dynamic(name)?"lvui::native::number(lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(item->second->value,item->second->position,initial)+"))":decimal(numeric(values,name,0,low,high));
            setup+="value.configuration."+std::string(field)+" = "+value+"; ";};
        number("width","width",1,8192);number("height","height",1,8192);number("leftPadding","left_padding",0,8192);
        number("rightPadding","right_padding",0,8192);number("horizontalPadding","left_padding",0,8192);number("spacing","spacing",-256,8192);
        number("cornerRadius","corner_radius",0,8192);number("iconSize","icon_size",1,256);number("indicatorSize","indicator_size",1,256);
        number("fontSize","font_size",1,256);number("lineHeight","line_height",0,8192);
        for(const auto& [name,field]:std::map<std::string,std::string,std::less<>>{{"primaryColor","primary"},{"hoverColor","primary_hover"},{"pressedColor","primary_pressed"},{"textColor","text"},{"disabledTextColor","disabled_text"}})
            if(auto item=values.find(name);item!=values.end())setup+="value.configuration.palette."+field+" = lvui::native::dropdown_color("+color_expression(item->second->value,item->second->position,initial)+"); ";
        return "([&] { lvui::native::DropdownInput value; "+setup+"return value; }())";
    }
    std::string owned_help_button_options(const Inputs& values,bool initial)const {
        const auto dynamic=[&](std::string_view name){auto item=values.find(name);return item!=values.end()&&native_expression(item->second->value);};
        std::string setup="value.configuration = lvui::help_button_configuration(); ";
        for(const auto& [field,key]:std::map<std::string,std::string,std::less<>>{
                {"primary","--lvrs-theme-colors-action-primary"},{"primary_hover","--lvrs-theme-colors-accent-blue-hover"},
                {"primary_pressed","--lvrs-theme-colors-accent-blue-pressed"},{"normal","--lvrs-theme-colors-panel-sub-surface"},
                {"normal_hover","--lvrs-theme-colors-panel-surface-alt"},{"normal_pressed","--lvrs-theme-colors-accent-blue-subtle"},
                {"danger","--lvrs-theme-colors-action-danger"},{"disabled_background","--lvrs-theme-colors-panel-sub-surface"},
                {"text","--lvrs-theme-colors-text-description-color"},{"disabled_text","--lvrs-theme-colors-text-disabled-color"}}){
            const auto color=token_color(key);setup+="value.configuration.palette."+field+" = lvui::StyleColor{"+decimal(color.red)+", "+decimal(color.green)+", "+decimal(color.blue)+", "+decimal(color.alpha)+"}; ";
        }
        setup+="value.configuration.font_size = "+decimal(token_number("--lvrs-theme-typography-description-text-description",1,256))+"; ";
        setup+="value.configuration.line_height = "+decimal(token_number("--lvrs-theme-typography-description-text-description-line-height",0,8192))+"; ";
        setup+="value.configuration.font_weight = "+decimal(token_number("--lvrs-theme-typography-weight-semi-bold",400,700))+"; ";
        for(const auto& [name,field]:std::map<std::string,std::string,std::less<>>{{"text","text"},{"accessibilityName","accessibility_name"}})
            if(auto item=values.find(name);item!=values.end())setup+="value.configuration."+field+" = "+(dynamic(name)?"lvui::native::text("+expression_code(item->second->value,item->second->position,initial)+")":literal(text(values,name)))+"; ";
        if(auto item=values.find("enabled");item!=values.end())setup+="value.configuration.enabled = "+(dynamic("enabled")?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(values,"enabled",true))+"; ";
        if(auto item=values.find("tone");item!=values.end()){
            std::string tone;
            if(dynamic("tone"))tone="lvui::native::button_tone("+expression_code(item->second->value,item->second->position,initial)+")";
            else{const std::map<std::string,std::string,std::less<>> tones{{".primary","primary"},{".default","normal"},{".borderless","borderless"},{".destructive","destructive"},{".disabled","disabled"}};
                const auto selected=tones.find(item->second->value.text);if(item->second->value.kind!=ValueKind::enumeration||selected==tones.end())error(item->second->position,"Unknown HelpButton tone");tone="lvui::ButtonTone::"+selected->second;}
            setup+="value.configuration.tone = "+tone+"; ";
        }
        const auto number=[&](std::string_view name,std::string_view field,double low,double high){auto item=values.find(name);if(item==values.end())return;
            const auto value=dynamic(name)?"lvui::native::number(lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(item->second->value,item->second->position,initial)+"))":decimal(numeric(values,name,0,low,high));
            setup+="value.configuration."+std::string(field)+" = "+value+"; ";};
        number("width","width",1,8192);number("height","height",1,8192);number("horizontalPadding","horizontal_padding",0,8192);
        number("cornerRadius","corner_radius",0,8192);number("fontSize","font_size",1,256);number("lineHeight","line_height",0,8192);
        number("textOffsetY","text_vertical_offset",-8192,8192);
        for(const auto& [name,field]:std::map<std::string,std::string,std::less<>>{{"backgroundColor","normal"},{"hoverColor","normal_hover"},{"pressedColor","normal_pressed"},{"textColor","text"},{"disabledTextColor","disabled_text"}})
            if(auto item=values.find(name);item!=values.end())setup+="value.configuration.palette."+field+" = lvui::native::button_color("+color_expression(item->second->value,item->second->position,initial)+"); ";
        return "([&] { lvui::native::ButtonInput value; "+setup+"return value; }())";
    }
    std::string owned_button_options(const Inputs& v,bool initial,bool icon_default,bool normal_default,SourcePosition position)const{
        const auto expression=[&](std::string_view name,std::string fallback){auto item=v.find(name);if(item==v.end())return fallback;return "lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(item->second->value,item->second->position,initial)+")";};
        const auto number=[&](std::string_view name,double fallback,double low,double high){auto item=v.find(name);if(item==v.end())return decimal(fallback);return native_expression(item->second->value)?"lvui::native::number("+expression(name,"")+")":decimal(numeric(v,name,fallback,low,high));};
        const auto flag=[&](std::string_view name,bool fallback){auto item=v.find(name);if(item==v.end())return std::string(fallback?"true":"false");return native_expression(item->second->value)?"lvui::native::truth("+expression(name,"")+")":boolean(v,name,fallback);};
        const auto text_value=[&](std::string_view name){auto item=v.find(name);if(item==v.end())return literal("");return native_expression(item->second->value)?"lvui::native::text("+expression(name,"")+")":literal(text(v,name));};
        if(auto icon=v.find("icon");icon!=v.end()&&!native_expression(icon->second->value))
            error(icon->second->position,"Button icon requires a native image from a C++ backend");
        const auto mode=flag("iconMode",icon_default);
        const auto name=v.find("iconName");
        const bool has_name=name!=v.end()&&(native_expression(name->second->value)||!text(v,"iconName").empty());
        if(mode=="true"&&!v.contains("icon")&&!has_name)error(position,"IconButton or iconMode requires a native icon or iconName");
        std::string tone=normal_default?"lvui::ButtonTone::normal":"lvui::ButtonTone::primary";
        if(auto item=v.find("tone");item!=v.end()){
            if(native_expression(item->second->value))tone="lvui::native::button_tone("+expression("tone","")+")";
            else {const std::map<std::string,std::string,std::less<>> tones{{".primary","primary"},{".default","normal"},{".borderless","borderless"},{".destructive","destructive"},{".disabled","disabled"}};
                const auto selected=tones.find(item->second->value.text);if(item->second->value.kind!=ValueKind::enumeration||selected==tones.end())error(item->second->position,"Unknown LVUI button tone");tone="lvui::ButtonTone::"+selected->second;}
        }
        const auto color_code=[](StyleColor c){return "lvui::StyleColor{"+decimal(c.red)+", "+decimal(c.green)+", "+decimal(c.blue)+", "+decimal(c.alpha)+"}";};
        const auto color=[&](std::string_view key){return color_code(token_color(key));};
        const auto palette="lvui::ButtonPalette{.primary = "+color("--lvrs-theme-colors-action-primary")+
            ", .primary_hover = "+color("--lvrs-theme-colors-accent-blue-hover")+
            ", .primary_pressed = "+color("--lvrs-theme-colors-accent-blue-pressed")+
            ", .normal = "+color("--lvrs-theme-colors-panel-surface-raised")+
            ", .normal_hover = "+color("--lvrs-theme-colors-panel-surface-alt")+
            ", .normal_pressed = "+color("--lvrs-theme-colors-accent-blue-subtle")+
            ", .danger = "+color("--lvrs-theme-colors-action-danger")+
            ", .disabled_background = "+color("--lvrs-theme-colors-panel-sub-surface")+
            ", .text = "+color("--lvrs-theme-colors-text-body-color")+
            ", .disabled_text = "+color("--lvrs-theme-colors-text-disabled-color")+"}";
        const auto padding=v.contains("horizontalPadding")?number("horizontalPadding",0,0,8192):
            "(icon_mode ? 2 : "+decimal(token_number("--lvrs-theme-metrics-space-8",0,8192))+")";
        const auto height=number("height",token_number("--lvrs-theme-metrics-size-22",1,8192),1,8192);
        const auto radius=number("cornerRadius",token_number("--lvrs-theme-metrics-radius-8",0,8192),0,8192);
        const auto size=number("iconSize",16,1,256);
        return "([&] { const bool icon_mode = "+mode+"; auto value = lvui::native::ButtonInput{.icon = "+expression("icon","{}")+
            ", .configuration = lvui::ButtonConfiguration{.text = "+text_value("text")+", .accessibility_name = "+text_value("accessibilityName")+
            ", .enabled = "+flag("enabled",true)+", .tone = "+tone+", .height = "+height+", .horizontal_padding = "+padding+
            ", .corner_radius = "+radius+", .icon_size = "+size+", .font_size = "+decimal(token_number("--lvrs-theme-typography-body-text-body",1,256))+
            ", .line_height = "+decimal(token_number("--lvrs-theme-typography-body-text-body-line-height",0,8192))+
            ", .font_weight = "+decimal(token_number("--lvrs-theme-typography-weight-medium",400,700))+
            ", .icon_mode = icon_mode, .palette = "+palette+"}}; "+owned_named_icon_setup(v,initial)+"return value; }())";
    }
    std::string owned_radio_options(const Inputs& v,bool initial)const{
        const auto expression=[&](std::string_view name){const auto& input=*v.find(name)->second;return "lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input.value,input.position,initial)+")";};
        const auto number=[&](std::string_view name,double fallback,double low,double high){auto item=v.find(name);if(item==v.end())return decimal(fallback);return native_expression(item->second->value)?"lvui::native::number("+expression(name)+")":decimal(numeric(v,name,fallback,low,high));};
        const auto flag=[&](std::string_view name,bool fallback){auto item=v.find(name);if(item==v.end())return std::string(fallback?"true":"false");return native_expression(item->second->value)?"lvui::native::truth("+expression(name)+")":boolean(v,name,fallback);};
        const auto text_value=[&](std::string_view name){auto item=v.find(name);if(item==v.end())return literal("");return native_expression(item->second->value)?"lvui::native::text("+expression(name)+")":literal(text(v,name));};
        if(v.contains("checked")&&v.contains("state"))error(v.at("state")->position,"Use checked or state, not both");
        if(v.contains("enabled")&&v.contains("available"))error(v.at("available")->position,"Use enabled or available, not both");
        const auto color_code=[](StyleColor c){return "lvui::StyleColor{"+decimal(c.red)+", "+decimal(c.green)+", "+decimal(c.blue)+", "+decimal(c.alpha)+"}";};
        const auto color=[&](std::string_view field,std::string_view key){auto item=v.find(field);return item==v.end()?color_code(token_color(key)):
            "lvui::native::radio_color("+color_expression(item->second->value,item->second->position,initial)+")";};
        const auto palette="lvui::RadioButtonPalette{.on = "+color("onColor","--lvrs-theme-colors-action-primary")+
            ", .off = "+color("offColor","--lvrs-theme-colors-text-title-header-color")+
            ", .disabled_on = "+color("onColorDisabled","--lvrs-theme-colors-panel-surface-raised")+
            ", .disabled_off = "+color("offColorDisabled","--lvrs-theme-colors-panel-surface-raised")+
            ", .dot = "+color("dotColor","--lvrs-theme-colors-text-title-header-color")+
            ", .disabled_dot = "+color("dotColorDisabled","--lvrs-theme-colors-text-caption-color")+
            ", .text = "+color("textColor","--lvrs-theme-colors-text-body-color")+
            ", .disabled_text = "+color("disabledTextColor","--lvrs-theme-colors-text-disabled-color")+
            ", .focus = "+color_code(token_color("--lvrs-theme-colors-action-primary"))+"}";
        std::string motion="lvui::Motion{.duration = std::chrono::milliseconds{180}, .curve = lvui::MotionCurve::out_back, .overshoot = 1.45}";
        if(auto item=v.find("motion");item!=v.end()){
            if(!native_expression(item->second->value))error(item->second->position,"RadioButton motion requires a scoped native value");
            motion="lvui::motion_value("+expression("motion")+")";
        }
        if(auto item=v.find("transitionDuration");item!=v.end()){
            if(!native_expression(item->second->value)){const auto duration=numeric(v,"transitionDuration",180,0,86400000);if(std::floor(duration)!=duration)error(item->second->position,"RadioButton transitionDuration must be whole milliseconds");}
            motion="lvui::native::radio_motion_duration("+motion+", "+number("transitionDuration",180,0,86400000)+")";
        }
        return "lvui::RadioButtonConfiguration{.text = "+text_value("text")+", .accessibility_name = "+text_value("accessibilityName")+
            ", .group = "+text_value("group")+", .enabled = "+flag(v.contains("enabled")?"enabled":"available",true)+
            ", .checked = "+flag(v.contains("checked")?"checked":"state",false)+", .motion_enabled = "+flag("motionEnabled",true)+
            ", .indicator_size = "+number("indicatorSize",18,1,8192)+", .dot_size = "+number("dotSize",8,0,8192)+
            ", .label_gap = "+number("labelGap",8,0,8192)+
            ", .font_size = "+decimal(token_number("--lvrs-theme-typography-body-text-body",1,256))+
            ", .line_height = "+decimal(token_number("--lvrs-theme-typography-body-text-body-line-height",0,8192))+
            ", .font_weight = "+decimal(token_number("--lvrs-theme-typography-weight-medium",400,700))+
            ", .motion = "+motion+", .palette = "+palette+"}";
    }
    std::string owned_checkbox_options(const Inputs& v,bool initial)const{
        const auto expression=[&](std::string_view name){const auto& input=*v.find(name)->second;return "lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input.value,input.position,initial)+")";};
        const auto number=[&](std::string_view name,double fallback,double low,double high){auto item=v.find(name);if(item==v.end())return decimal(fallback);return native_expression(item->second->value)?"lvui::native::number("+expression(name)+")":decimal(numeric(v,name,fallback,low,high));};
        const auto flag=[&](std::string_view name,bool fallback){auto item=v.find(name);if(item==v.end())return std::string(fallback?"true":"false");return native_expression(item->second->value)?"lvui::native::truth("+expression(name)+")":boolean(v,name,fallback);};
        const auto text_value=[&](std::string_view name){auto item=v.find(name);if(item==v.end())return literal("");return native_expression(item->second->value)?"lvui::native::text("+expression(name)+")":literal(text(v,name));};
        if(auto group=v.find("group");group!=v.end()&&(native_expression(group->second->value)||!text(v,"group").empty()))
            error(group->second->position,"CheckBox does not support radio groups");
        const auto color_code=[](StyleColor c){return "lvui::StyleColor{"+decimal(c.red)+", "+decimal(c.green)+", "+decimal(c.blue)+", "+decimal(c.alpha)+"}";};
        const auto color=[&](std::string_view field,std::string_view key){auto item=v.find(field);return item==v.end()?color_code(token_color(key)):
            "lvui::native::checkbox_color("+color_expression(item->second->value,item->second->position,initial)+")";};
        const auto transparent=std::string("lvui::StyleColor{0, 0, 0, 0}");
        const auto border_color=[&](std::string_view field){auto item=v.find(field);return item==v.end()?transparent:
            "lvui::native::checkbox_color("+color_expression(item->second->value,item->second->position,initial)+")";};
        const auto palette="lvui::CheckBoxPalette{.checked = "+color("checkedColor","--lvrs-theme-colors-action-primary")+
            ", .unchecked = "+color("uncheckedColor","--lvrs-theme-colors-text-body-color")+
            ", .disabled_checked = "+color("disabledCheckedColor","--lvrs-theme-colors-panel-surface-raised")+
            ", .disabled_unchecked = "+color("disabledUncheckedColor","--lvrs-theme-colors-panel-surface-raised")+
            ", .mark = "+color("checkColor","--lvrs-theme-colors-text-body-color")+
            ", .disabled_mark = "+color("checkMarkColorDisabled","--lvrs-theme-colors-text-disabled-color")+
            ", .checked_border = "+border_color("boxBorderColorCheckedEnabled")+
            ", .unchecked_border = "+color("boxBorderColorUncheckedEnabled","--lvrs-theme-colors-text-body-color")+
            ", .disabled_checked_border = "+color("boxBorderColorCheckedDisabled","--lvrs-theme-colors-panel-surface-raised")+
            ", .disabled_unchecked_border = "+border_color("boxBorderColorUncheckedDisabled")+
            ", .text = "+color("textColor","--lvrs-theme-colors-text-body-color")+
            ", .disabled_text = "+color("disabledTextColor","--lvrs-theme-colors-text-disabled-color")+
            ", .focus = "+color_code(token_color("--lvrs-theme-colors-action-primary"))+"}";
        const auto size=number("boxSize",17,1,8192);
        const auto scaled=[&](std::string_view name,double fallback,double factor){return v.contains(name)?number(name,fallback,0,8192):"("+size+" * "+decimal(factor/17)+")";};
        return "lvui::CheckBoxConfiguration{.text = "+text_value("text")+", .accessibility_name = "+text_value("accessibilityName")+
            ", .enabled = "+flag("enabled",true)+", .checked = "+flag("checked",false)+", .box_size = "+size+
            ", .frame_padding = "+number("framePadding",.5,0,8192)+", .box_radius = "+scaled("boxRadius",3.5,3.5)+
            ", .mark_stroke_width = "+scaled("checkMarkStrokeWidth",2,2)+", .label_gap = "+number("labelGap",6,0,8192)+
            ", .checked_border_width = "+number("boxBorderWidthCheckedEnabled",0,0,8192)+
            ", .unchecked_border_width = "+scaled("boxBorderWidthUncheckedEnabled",.5,.5)+
            ", .disabled_checked_border_width = "+scaled("boxBorderWidthCheckedDisabled",.5,.5)+
            ", .disabled_unchecked_border_width = "+number("boxBorderWidthUncheckedDisabled",0,0,8192)+
            ", .font_size = "+decimal(token_number("--lvrs-theme-typography-body-text-body",1,256))+
            ", .line_height = "+decimal(token_number("--lvrs-theme-typography-body-text-body-line-height",0,8192))+
            ", .font_weight = "+decimal(token_number("--lvrs-theme-typography-weight-medium",400,700))+
            ", .palette = "+palette+"}";
    }
    std::string owned_toggle_options(const Inputs& v,bool initial)const{
        const auto expression=[&](std::string_view name){const auto& input=*v.find(name)->second;return "lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input.value,input.position,initial)+")";};
        const auto number=[&](std::string_view name,double fallback,double low,double high){auto item=v.find(name);if(item==v.end())return decimal(fallback);return native_expression(item->second->value)?"lvui::native::number("+expression(name)+")":decimal(numeric(v,name,fallback,low,high));};
        const auto flag=[&](std::string_view name,bool fallback){auto item=v.find(name);if(item==v.end())return std::string(fallback?"true":"false");return native_expression(item->second->value)?"lvui::native::truth("+expression(name)+")":boolean(v,name,fallback);};
        const auto text_value=[&](std::string_view name){auto item=v.find(name);if(item==v.end())return literal("");return native_expression(item->second->value)?"lvui::native::text("+expression(name)+")":literal(text(v,name));};
        if(auto group=v.find("group");group!=v.end()&&(native_expression(group->second->value)||!text(v,"group").empty()))
            error(group->second->position,"ToggleSwitch does not support radio groups");
        const auto color_code=[](StyleColor c){return "lvui::StyleColor{"+decimal(c.red)+", "+decimal(c.green)+", "+decimal(c.blue)+", "+decimal(c.alpha)+"}";};
        const auto color=[&](std::string_view field,std::string_view key){auto item=v.find(field);return item==v.end()?color_code(token_color(key)):
            "lvui::native::toggle_color("+color_expression(item->second->value,item->second->position,initial)+")";};
        const auto palette="lvui::ToggleSwitchPalette{.on = "+color("onColor","--lvrs-theme-colors-action-primary")+
            ", .on_hover = "+color("onColorHover","--lvrs-theme-colors-accent-blue-hover")+
            ", .on_pressed = "+color("onColorPressed","--lvrs-theme-colors-accent-blue-pressed")+
            ", .off = "+color("offColor","--lvrs-theme-colors-panel-surface-raised")+
            ", .off_hover = "+color("offColorHover","--lvrs-theme-colors-panel-surface-alt")+
            ", .off_pressed = "+color("offColorPressed","--lvrs-theme-colors-panel-sub-surface")+
            ", .disabled = "+color("disabledTrackColor","--lvrs-theme-colors-panel-surface-raised")+
            ", .knob = "+color("knobFillColor","--lvrs-theme-colors-text-title-header-color")+
            ", .disabled_knob = "+color("disabledKnobColor","--lvrs-theme-colors-text-caption-color")+
            ", .text = "+color("textColor","--lvrs-theme-colors-text-body-color")+
            ", .disabled_text = "+color("disabledTextColor","--lvrs-theme-colors-text-disabled-color")+
            ", .focus = "+color_code(token_color("--lvrs-theme-colors-action-primary"))+"}";
        std::string motion="lvui::Motion{.duration = std::chrono::milliseconds{320}, .curve = lvui::MotionCurve::out_back, .overshoot = 1.45}";
        if(auto item=v.find("motion");item!=v.end()){
            if(!native_expression(item->second->value))error(item->second->position,"ToggleSwitch motion requires a scoped native value");
            motion="lvui::motion_value("+expression("motion")+")";
        }
        if(auto item=v.find("transitionDuration");item!=v.end()){
            if(!native_expression(item->second->value)){const auto duration=numeric(v,"transitionDuration",320,0,86400000);if(std::floor(duration)!=duration)error(item->second->position,"ToggleSwitch transitionDuration must be whole milliseconds");}
            motion="lvui::native::toggle_motion_duration("+motion+", "+number("transitionDuration",320,0,86400000)+")";
        }
        return "lvui::ToggleSwitchConfiguration{.text = "+text_value("text")+", .accessibility_name = "+text_value("accessibilityName")+
            ", .enabled = "+flag("enabled",true)+", .checked = "+flag("checked",false)+", .right_to_left = "+flag("rightToLeft",false)+
            ", .motion_enabled = "+flag("motionEnabled",true)+", .track_width = "+number("trackWidth",38,1,8192)+
            ", .track_height = "+number("trackHeight",22,1,8192)+", .track_padding = "+number("trackPadding",2,0,8192)+
            ", .knob_size = "+number("knobSize",18,1,8192)+", .track_radius = "+number("trackCornerRadius",20,0,8192)+
            ", .knob_radius = "+number("knobCornerRadius",9,0,8192)+", .label_gap = "+number("labelGap",8,0,8192)+
            ", .font_size = "+decimal(token_number("--lvrs-theme-typography-body-text-body",1,256))+
            ", .line_height = "+decimal(token_number("--lvrs-theme-typography-body-text-body-line-height",0,8192))+
            ", .font_weight = "+decimal(token_number("--lvrs-theme-typography-weight-medium",400,700))+
            ", .motion = "+motion+", .palette = "+palette+"}";
    }
    std::string owned_divider_options(const Inputs& v,bool initial,bool context)const{
        const auto expression=[&](std::string_view name){const auto& input=*v.find(name)->second;return "lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input.value,input.position,initial)+")";};
        const auto number=[&](std::string_view name,double fallback){auto item=v.find(name);if(item==v.end())return decimal(fallback);return native_expression(item->second->value)?"lvui::native::number("+expression(name)+")":decimal(numeric(v,name,fallback,0,8192));};
        std::string axis="lvui::DividerAxis::horizontal";
        if(auto item=v.find("axis");item!=v.end()){
            if(native_expression(item->second->value))axis="lvui::native::divider_axis("+expression("axis")+")";
            else {const auto value=resolve(item->second->value,item->second->position);auto name=value.text;
                if(value.kind==ValueKind::enumeration&&!name.empty())name.erase(0,1);
                else if(value.kind!=ValueKind::string)error(item->second->position,"Expected a divider axis");
                for(auto& c:name)if(c>='A'&&c<='Z')c=char(c-'A'+'a');
                if(name!="horizontal"&&name!="vertical")error(item->second->position,"Unknown divider axis");
                axis="lvui::DividerAxis::"+name;}
        }
        std::string color;
        if(auto item=v.find("dividerColor");item!=v.end())color="lvui::native::divider_color("+color_expression(item->second->value,item->second->position,initial)+")";
        else {const auto c=token_color(context?"--lvrs-theme-colors-text-disabled-color":"--lvrs-theme-colors-panel-surface-panel");
            color="lvui::StyleColor{"+decimal(c.red)+", "+decimal(c.green)+", "+decimal(c.blue)+", "+decimal(c.alpha)+"}";}
        return "lvui::DividerConfiguration{.axis = "+axis+", .length = "+number("lineLength",context?145:220)+
            ", .thickness = "+number("thickness",1)+", .cross_padding = "+number("crossPadding",1)+
            ", .line_padding = "+number("linePadding",context?4:0)+", .color = "+color+"}";
    }
    std::string owned_image_options(const Inputs& v,bool initial)const{
        const auto expression=[&](std::string_view name,std::string fallback){auto item=v.find(name);if(item==v.end())return fallback;return "lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(item->second->value,item->second->position,initial)+")";};
        const auto number=[&](std::string_view name,std::string fallback){auto item=v.find(name);if(item==v.end())return fallback;if(native_expression(item->second->value))return "lvui::native::number("+expression(name,"")+")";return decimal(numeric(v,name,0,0,8192));};
        const auto flag=[&](std::string_view name,bool fallback){auto item=v.find(name);if(item==v.end())return std::string(fallback?"true":"false");if(native_expression(item->second->value))return "lvui::native::truth("+expression(name,"")+")";return boolean(v,name,fallback);};
        const auto color=[&](std::string_view name){auto item=v.find(name);return item==v.end()?std::string("std::nullopt"):"lvui::native::image_color("+color_expression(item->second->value,item->second->position,initial)+")";};
        if(auto source=v.find("source");source!=v.end()&&!native_expression(source->second->value))error(source->second->position,"Image.source requires a native image from a C++ backend");
        std::string fit="std::nullopt";
        if(auto item=v.find("fit");item!=v.end()){
            if(native_expression(item->second->value))fit="lvui::native::image_fit("+expression("fit","")+")";
            else{const auto value=resolve(item->second->value,item->second->position);
                if(value.text==".Contain")fit="lvui::ImageFit::contain";
                else if(value.text==".Cover")fit="lvui::ImageFit::cover";
                else if(value.text==".Stretch")fit="lvui::ImageFit::stretch";
                else error(item->second->position,"Unknown native Image.fit value");}
        }
        std::string name=literal("");if(auto item=v.find("accessibilityName");item!=v.end())name=native_expression(item->second->value)?"ii::gui::binding_text("+expression("accessibilityName","")+")":literal(text(v,"accessibilityName"));
        return "lvui::native::ImageInput{.image = "+expression("source","{}")+", .configuration = lvui::ImageConfiguration{.width = "+number("width","32")+", .height = "+number("height","32")+", .padding = "+number("padding","0")+", .corner_radius = "+number("cornerRadius","0")+", .border_width = "+number("borderWidth","0")+", .background = "+color("backgroundColor")+", .border = "+color("borderColor")+", .fit = "+fit+", .preserve_aspect = "+flag("preserveAspect",false)+", .circular = "+flag("circular",false)+", .allow_empty = "+flag("allowEmpty",true)+", .accessibility_name = "+name+"}}";
    }
    std::string owned_avatar_options(const Inputs& values,bool initial)const{
        const std::map<std::string,std::string,std::less<>> fields{{"size","size"},{"width","width"},{"height","height"},
            {"backgroundColor","background"},{"enabled","enabled"},{"accessibilityName","accessibility_name"}};
        std::string setup;
        for(const auto& [name,input]:values){auto field=fields.find(name);if(field==fields.end())continue;
            const auto expression=[&]{return "lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input->value,input->position,initial)+")";};
            std::string value;
            if(name=="size"){
                if(native_expression(input->value))value="lvui::native::avatar_size("+expression()+")";
                else {const auto item=resolve(input->value,input->position);
                    if(item.kind==ValueKind::enumeration&&(item.text==".Small"||item.text==".small"))value="lvui::AvatarSize::small";
                    else if(item.kind==ValueKind::enumeration&&(item.text==".Medium"||item.text==".medium"))value="lvui::AvatarSize::medium";
                    else if(item.kind==ValueKind::enumeration&&(item.text==".Large"||item.text==".large"))value="lvui::AvatarSize::large";
                    else error(input->position,"Unknown independent Avatar size");}
            }else if(name=="backgroundColor")value="lvui::native::avatar_color("+color_expression(input->value,input->position,initial)+")";
            else if(name=="enabled")value=native_expression(input->value)?"lvui::native::truth("+expression()+")":boolean(values,name,true);
            else if(name=="accessibilityName")value=native_expression(input->value)?"lvui::native::text("+expression()+")":literal(text(values,name));
            else value=native_expression(input->value)?"lvui::native::number("+expression()+")":decimal(numeric(values,name,0,0,8192));
            setup+="value.configuration."+field->second+" = "+value+"; ";
        }
        if(auto source=values.find("source");source!=values.end()){
            if(!native_expression(source->second->value))error(source->second->position,"Avatar.source requires a native image from a C++ backend");
            setup+="value.image = lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(source->second->value,source->second->position,initial)+"); ";
        }
        return "([&] { lvui::native::AvatarInput value; "+setup+"return value; }())";
    }
    void emit_image_bindings(const Inputs& v,const std::string&,const std::string& native){
        bool dynamic=false;for(const auto& [name,input]:v)if(name!="visible"&&name!="opacity"&&name!="flex"&&native_expression(input->value))dynamic=true;
        if(!dynamic)return;
        body+="            { auto binding = native_bindings->bind([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+image_options(v,false)+"; }, [component = *"+native+"](const ii::gui::ImageOptions& options) { return component.set_options(options); });\n              if (!binding) return std::unexpected(binding.error()); }\n";
    }
    void emit_media_bindings(const Inputs& v,const std::string& variable,const std::string& native,bool avatar){
        const std::map<std::string,std::string,std::less<>> fields{{"size","size"},{"type","type"},{"detail","detail"},{"displayState","display_state"},{"selected","selected"},{"enabled","enabled"},{"showDescription","show_description"},{"showStatus","show_status"},{"selectable","selectable"},{"showMenu","show_menu"},{"showAction","show_action"},{"title","title"},{"filename","title"},{"description","description"},{"details","details"},{"metadata","metadata"},{"statusText","status_text"},{"summary","summary"},{"footnote","footnote"},{"domain","domain"},{"previewTitle","preview_title"},{"iconName","icon_name"},{"menuAccessibleName","menu_accessibility_name"},{"actionText","action_text"},{"progressLabel","progress_label"},{"progressText","progress_text"},{"rows","rows"},{"progress","progress"},{"progressColor","progress_color"},{"statusColor","status_color"},{"icon","icon"},{"iconSource","icon"},{"previewSource","preview_source"},{"source","source"},{"backgroundColor","background"},{"width","width"},{"height","height"},{"accessibilityName","accessibility_name"}};
        const std::set<std::string,std::less<>> texts{"title","filename","description","details","metadata","statusText","summary","footnote","domain","previewTitle","iconName","menuAccessibleName","actionText","progressLabel","progressText","accessibilityName"};
        for(const auto& [name,input]:v){if(!native_expression(input->value))continue;std::string update;
            if(!avatar&&(name=="previewComponent"||name=="iconComponent"))update="return component.set_"+std::string(name=="previewComponent"?"preview":"icon")+"_content(lvrs::card_component(value));";
            else if(auto it=fields.find(name);it!=fields.end())update="auto options = component.options(); if (!options) return std::unexpected(options.error()); options->"+it->second+" = "+(texts.contains(name)?"ii::gui::binding_text(value)":"value")+"; return component.set_options(std::move(*options));";
            if(!update.empty())native_binding(*input, variable, "[component = *"+native+"](ii::gui::Widget widget, const auto& value) -> ii::core::Result<void> { (void)widget; "+update+" }");
        }
    }
    const std::map<std::string,std::string,std::less<>> material_fields{{"density","density"},{"primaryColor","primary_color"},{"color","color"},{"borderColor","border_color"},{"backdropBaseColor","backdrop_base_color"},{"tintOpacity","tint_opacity"},{"blurRadius","blur_radius"},{"radius","radius"},{"innerHighlightOpacity","inner_highlight_opacity"},{"shadowRadius","shadow_radius"},{"shadowOffsetY","shadow_offset_y"},{"shadowOpacity","shadow_opacity"},{"borderWidth","border_width"},{"innerHighlightOffsetY","inner_highlight_offset_y"},{"intenseOpacity","intense_opacity"},{"faintOpacity","faint_opacity"},{"blurEnabled","blur_enabled"},{"motionEnabled","motion_enabled"},{"enabled","enabled"},{"backdrop","backdrop"},{"outlinePath","outline_path"},{"width","width"},{"height","height"},{"motion","motion"}};
    std::string owned_material_options(std::string_view kind,const Inputs& values,bool initial)const {
        const auto expression=[&](const Input& input){return "lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input.value,input.position,initial)+")";};
        const auto primary=token_color("--lvrs-theme-colors-action-primary");
        std::string setup="value.kind = lvui::MaterialKind::"+std::string(kind=="WindowMaterial"?"window":kind=="PanelMaterial"?"panel":"surface")+"; ";
        if(!values.contains("primaryColor"))setup+="value.primary_color = lvui::StyleColor{"+decimal(primary.red)+", "+decimal(primary.green)+", "+decimal(primary.blue)+", "+decimal(primary.alpha)+"}; ";
        for(const auto& [name,input]:values){
            auto field=material_fields.find(name);if(field==material_fields.end())continue;
            std::string value;
            if(name=="outlinePath"){
                if(native_expression(input->value))value="lvui::native::text("+expression(*input)+")";
                else {const auto path=text(values,name);if(!valid_material_outline(path))error(input->position,"Invalid independent material outlinePath");value=literal(path);}
            }else if(name=="density"||name=="backdrop"){
                if(native_expression(input->value))value="lvui::native::material_"+std::string(name=="density"?"density":"backdrop")+"("+expression(*input)+")";
                else {const auto item=resolve(input->value,input->position);
                    if(name=="density"){
                        if(item.text==".Dense75")value="lvui::MaterialDensity::dense75";
                        else if(item.text==".Glass25")value="lvui::MaterialDensity::glass25";
                        else error(input->position,"Unknown independent material density");
                    }else{
                        if(item.text==".None")value="lvui::MaterialBackdrop::none";
                        else if(item.text==".Content")value="lvui::MaterialBackdrop::content";
                        else if(item.text==".PreviousLayers")value="lvui::MaterialBackdrop::previous_layers";
                        else error(input->position,"Unknown independent material backdrop");
                    }
                }
            }else if(name=="primaryColor"||name=="color"||name=="borderColor"||name=="backdropBaseColor")
                value="lvui::native::material_color("+color_expression(input->value,input->position,initial)+")";
            else if(name=="motion"){
                if(!native_expression(input->value))error(input->position,"Material motion requires a scoped native value");
                value="lvui::motion_value("+expression(*input)+")";
            }else if(name=="blurEnabled"||name=="motionEnabled"||name=="enabled")
                value=native_expression(input->value)?"lvui::native::truth("+expression(*input)+")":boolean(values,name,true);
            else {
                const bool opacity=name=="tintOpacity"||name=="innerHighlightOpacity"||name=="shadowOpacity"||name=="intenseOpacity"||name=="faintOpacity";
                const bool offset=name=="shadowOffsetY"||name=="innerHighlightOffsetY";
                const double upper=opacity?1:(name=="blurRadius"||name=="shadowRadius"?64:8192);
                value=native_expression(input->value)?"lvui::native::number("+expression(*input)+")":decimal(numeric(values,name,0,offset?-8192:0,upper));
            }
            setup+="value."+field->second+" = "+value+"; ";
        }
        return "([&] { lvui::MaterialConfiguration value; "+setup+"return value; }())";
    }
    std::string material_call(std::string_view kind,const std::string& args,const Inputs& v)const{
        std::string setup;
        for(const auto& [name,input]:v){auto field=material_fields.find(name);if(field==material_fields.end())continue;std::string value;
            if(name=="density")value=media_enum(v,name,"MaterialDensity","glass25",{{"Dense75","dense75"},{"Glass25","glass25"}});
            else if(name=="backdrop")value=media_enum(v,name,"MaterialBackdrop","none",{{"None","none"},{"Content","content"},{"PreviousLayers","previous_layers"}});
            else if(name=="outlinePath")value=link_text(v,name);
            else if(name=="motion")value=link_value(v,name,"{}");
            else if(name=="primaryColor"||name=="color"||name=="borderColor"||name=="backdropBaseColor")value=surface_color(v,name,"{}");
            else if(name=="blurEnabled"||name=="motionEnabled"||name=="enabled")value=link_bool(v,name,true);
            else if(!property(input->value).empty())value=link_value(v,name,"");
            else{const bool opacity=name=="tintOpacity"||name=="innerHighlightOpacity"||name=="shadowOpacity"||name=="intenseOpacity"||name=="faintOpacity";const bool offset=name=="shadowOffsetY"||name=="innerHighlightOffsetY";value=decimal(numeric(v,name,0,offset?-8192:0,opacity?1:8192));}
            setup+="value."+field->second+" = "+value+"; ";
        }
        return "([&]() { lvrs::MaterialOptions value; "+setup+"return lvrs::"+std::string(kind)+"::create(widgets, "+args+", std::move(value)); }())";
    }
    void emit_material_bindings(const Inputs& v,const std::string& variable,const std::string& native){
        for(const auto& [name,input]:v){if(!native_expression(input->value))continue;std::string update;
            if(auto field=material_fields.find(name);field!=material_fields.end())update="auto options = material.options(); if (!options) return std::unexpected(options.error()); options->"+field->second+" = "+(name=="outlinePath"?"ii::gui::binding_text(value)":"value")+"; return material.set_options(std::move(*options));";
            if(!update.empty())native_binding(*input, variable, "[material = *"+native+"](ii::gui::Widget widget, const auto& value) -> ii::core::Result<void> { (void)widget; "+update+" }");
        }
    }
    std::string owned_code_editor_appearance(const Inputs& values,bool initial) const {
        const std::map<std::string,std::string,std::less<>> numbers{{"width","width"},{"height","height"},{"editorHeight","height"},
            {"insetHorizontal","horizontal_padding"},{"insetVertical","vertical_padding"},{"cornerRadius","corner_radius"},
            {"fontPixelSize","font_size"},{"textLineHeight","line_height"},{"tabStopDistance","tab_stop"}};
        const std::map<std::string,std::string,std::less<>> colors{{"textColor","text"},{"backgroundColor","background"},
            {"backgroundColorHover","hover"},{"selectionColor","selection"},{"focusColor","focus"},{"caretColor","caret"}};
        std::string result="([&] { lvui::native::CodeEditorAppearance value; ";
        for(const auto& [name,input]:values){
            const bool dynamic=native_expression(input->value);std::string field,expression;
            if(auto found=numbers.find(name);found!=numbers.end()){
                field=found->second;
                const double low=name=="width"||name=="height"||name=="editorHeight"||name=="fontPixelSize"||name=="tabStopDistance"?1.:
                    name=="textLineHeight"?12.:0.;
                const double high=name=="height"||name=="editorHeight"?4096.:name=="fontPixelSize"?256.:
                    name=="textLineHeight"||name=="insetHorizontal"||name=="insetVertical"?1024.:8192.;
                expression=dynamic?"lvui::native::number("+expression_code(input->value,input->position,initial)+")":decimal(numeric(values,name,0,low,high));
            }else if(auto found=colors.find(name);found!=colors.end()){
                field="palette."+found->second;expression="lvui::native::code_editor_color("+color_expression(input->value,input->position,initial)+")";
            }else if(name=="fontFamily"||name=="accessibilityName"){
                field=name=="fontFamily"?"font_family":"accessibility_name";
                expression=dynamic?"lvui::native::text("+expression_code(input->value,input->position,initial)+")":literal(text(values,name));
            }else if(name=="enabled"){
                field="enabled";expression=dynamic?"lvui::native::truth("+expression_code(input->value,input->position,initial)+")":boolean(values,name,true);
            }else continue;
            result+="value."+field+" = "+expression+"; ";
        }
        return result+"return value; }())";
    }
    const std::map<std::string,std::string,std::less<>> code_editor_text_fields{{"text","text"},{"placeholderText","placeholder_text"},{"snippetTitle","snippet_title"},{"snippetLanguage","snippet_language"},{"fontFamily","font_family"},{"accessibilityName","accessibility_name"}};
    const std::map<std::string,std::string,std::less<>> code_editor_number_fields{{"width","width"},{"height","editor_height"},{"editorHeight","editor_height"},{"fieldMinHeight","field_min_height"},{"headerHeight","header_height"},{"headerSpacing","header_spacing"},{"insetHorizontal","inset_horizontal"},{"insetVertical","inset_vertical"},{"cornerRadius","corner_radius"},{"fontPixelSize","font_pixel_size"},{"fontWeight","font_weight"},{"textLineHeight","text_line_height"},{"centeredTextHeight","centered_text_height"},{"tabStopDistance","tab_stop_distance"},{"textMargin","text_margin"},{"placeholderOpacity","placeholder_opacity"}};
    const std::map<std::string,std::string,std::less<>> code_editor_bool_fields{{"enabled","enabled"},{"readOnly","read_only"},{"showSnippetHeader","show_snippet_header"},{"showScrollBar","show_scroll_bar"},{"autoFocusOnPress","auto_focus_on_press"},{"selectByMouse","select_by_mouse"},{"selectByKeyboard","select_by_keyboard"},{"persistentSelection","persistent_selection"},{"overwriteMode","overwrite_mode"}};
    const std::map<std::string,std::string,std::less<>> code_editor_color_fields{{"textColor","text_color"},{"textColorDisabled","text_color_disabled"},{"placeholderColor","placeholder_color"},{"placeholderColorDisabled","placeholder_color_disabled"},{"selectedTextColor","selected_text_color"},{"headerTextColor","header_text_color"},{"backgroundColor","background_color"},{"backgroundColorHover","background_color_hover"},{"backgroundColorFocused","background_color_focused"},{"backgroundColorDisabled","background_color_disabled"},{"selectionColor","selection_color"}};
    std::string code_editor_call(const std::string& args,const Inputs& v)const{
        if(v.contains("height")&&v.contains("editorHeight"))error(v.at("height")->position,"Use height or editorHeight for CodeEditor");
        if(v.contains("model")&&v.contains("text"))error(v.at("text")->position,"Use a scoped CodeEditor model or initial text");
        std::string setup;
        for(const auto& [name,field]:code_editor_text_fields)if(v.contains(name))setup+="value."+field+" = "+link_text(v,name)+"; ";
        for(const auto& [name,field]:code_editor_bool_fields)if(v.contains(name))setup+="value."+field+" = "+link_bool(v,name,false)+"; ";
        for(const auto& [name,field]:code_editor_number_fields)if(v.contains(name))setup+="value."+field+" = "+surface_number(v,name,"0")+"; ";
        for(const auto& [name,field]:code_editor_color_fields)if(v.contains(name))setup+="value."+field+" = "+surface_color(v,name,"{}")+"; ";
        if(v.contains("model"))setup+="value.model = "+scoped_member(v,"model")+"; ";
        return "([&]() { lvrs::CodeEditorOptions value; "+setup+"return lvrs::CodeEditor::create(widgets, "+args+", std::move(value), {.document_edited = "+action(v,"onDocumentEdited","const lvrs::TextDocumentChange& change","change")+", .text_edited = "+action(v,"onTextEdited","std::string value","value")+", .submitted = "+action(v,"onSubmitted","std::string value","value")+", .selection_changed = "+action(v,"onSelectionChanged","ii::gui::TextRange range","range")+"}); }())";
    }
    void emit_code_editor_bindings(const Inputs& v,const std::string& variable,const std::string& native){
        for(const auto& [name,input]:v){if(!native_expression(input->value))continue;std::string update;
            if(name=="text")update="auto changed = editor.set_text(ii::gui::binding_text(value)); if (!changed) return std::unexpected(changed.error()); return {};";
            else if(name=="cursorPosition")update="auto changed = editor.set_cursor_position(value); if (!changed) return std::unexpected(changed.error()); return {};";
            else {
                std::string field,expression="value";
                if(auto found=code_editor_text_fields.find(name);found!=code_editor_text_fields.end()){field=found->second;expression="ii::gui::binding_text(value)";}
                else if(auto found=code_editor_number_fields.find(name);found!=code_editor_number_fields.end())field=found->second;
                else if(auto found=code_editor_bool_fields.find(name);found!=code_editor_bool_fields.end())field=found->second;
                else if(auto found=code_editor_color_fields.find(name);found!=code_editor_color_fields.end())field=found->second;
                if(!field.empty())update="auto options = editor.options(); if (!options) return std::unexpected(options.error()); options->"+field+" = "+expression+"; return editor.set_options(std::move(*options));";
            }
            if(!update.empty())native_binding(*input, variable, "[editor = *"+native+"](ii::gui::Widget widget, const auto& value) -> ii::core::Result<void> { (void)widget; "+update+" }");
        }
        if(v.contains("cursorPosition")&&property(v.at("cursorPosition")->value).empty())body+="            if (auto cursor = "+native+"->set_cursor_position("+decimal(numeric(v,"cursorPosition",0,0,1e12))+"); !cursor) return std::unexpected(cursor.error());\n";
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
            for (const auto &item : list.elements) {
                auto value = number(resolve_in(item, StyleTokenType::number, it->second->position),
                                    it->second->position);
                if (std::abs(value) > 8192)
                    error(it->second->position, "Column width outside supported range");
                if (!widths.empty())
                    widths += ", ";
                widths += decimal(value);
            }
            widths = ", .column_widths = {" + widths + "}";
        }
        return "controls.table_header("+args+", {.layout = {.width = "+n("width",717,1)+", .row_height = "+n("rowHeight",24,1)+", .horizontal_padding = "+n("cellHorizontalPadding",8)+", .fallback_cell_width = "+n("fallbackCellWidth",0)+", .minimum_column_width = "+n("minColumnWidth",32,1)+widths+strings("columns","lvrs::TableHeaderColumn",false)+"}, .interactive = "+b("interactive",false)+", .enabled = "+b("enabled",true)+", .separator_height = "+n("separatorHeight",1)+", .text_color = "+c("textColor","ii::gui::WidgetColor{1,1,1,.6}")+", .separator_color = "+c("separatorColor",divider)+"}, {.column_clicked = "+action(v,"onColumnClicked","std::size_t index, const lvrs::TableHeaderColumn& column","index, column")+"})";
    }
    const std::map<std::string,std::string,std::less<>> tooltip_numbers{{"maximumWidth","maximum_width"},{"maximumHeight","maximum_height"},{"contentPadding","content_padding"},{"edgeMargin","edge_margin"},{"cornerRadius","corner_radius"},{"tailLength","tail_length"},{"tailWidth","tail_width"},{"surfaceOpacity","surface_opacity"}};
    const std::map<std::string,std::string,std::less<>> tooltip_booleans{{"automatic","automatic"},{"scrollContent","scroll_content"},{"motionEnabled","motion_enabled"}};
    const std::map<std::string,std::string,std::less<>> tooltip_durations{{"delay","delay"},{"timeout","timeout"},{"hideDelay","hide_delay"},{"animationDuration","animation_duration"}};
    const std::map<std::string,std::string,std::less<>> tooltip_colors{{"surfaceColor","surface_color"},{"borderColor","border_color"},{"primaryColor","primary_color"}};
    std::string owned_hierarchy_item_options(const Inputs& values,bool initial)const {
        const auto label=alias_key(values,"text","label"),active=alias_key(values,"activatable","selectable");
        const auto width=alias_key(values,"width","itemWidth"),height=alias_key(values,"height","rowHeight");
        const std::map<std::string,std::string,std::less<>> strings{{label,"label"},{"itemKey","key"},{"iconName","icon_name"},{"accessibilityName","accessibility_name"}};
        const std::map<std::string,std::string,std::less<>> flags{{"enabled","enabled"},{active,"activatable"},{"selected","selected"},{"expanded","expanded"},
            {"showChevron","show_chevron"},{"hasChildItems","has_children"},{"dragPreviewActive","drag_preview"}};
        const std::map<std::string,std::string,std::less<>> numbers{{width,"width"},{height,"height"},{"indentStep","indent_step"},{"iconSize","icon_size"},
            {"chevronSize","chevron_size"},{"baseLeftPadding","left_padding"},{"rowRightPadding","right_padding"},{"leadingSpacing","leading_spacing"},
            {"cornerRadius","corner_radius"},{"countWidth","count_width"}};
        const std::map<std::string,std::string,std::less<>> colors{{"textColorNormal","text"},{"textColorDisabled","disabled_text"},
            {"rowBackgroundColorIdle","idle"},{"rowBackgroundColorHover","hover"},{"rowBackgroundColorPressed","pressed"},
            {"rowBackgroundColorActive","active"},{"rowBackgroundColorInactive","inactive"},{"rowBackgroundColorDrag","drag"},{"focusColor","focus"}};
        std::string result="([&] { lvui::native::HierarchyItemInput value; ";
        for(const auto& [name,input]:values){
            const bool dynamic=native_expression(input->value);std::string field,expression;
            if(auto found=strings.find(name);found!=strings.end()){
                field="configuration."+found->second;
                expression=dynamic?"lvui::native::text("+expression_code(input->value,input->position,initial)+")":literal(text(values,name));
            }else if(auto found=flags.find(name);found!=flags.end()){
                field="configuration."+found->second;
                expression=dynamic?"lvui::native::truth("+expression_code(input->value,input->position,initial)+")":boolean(values,name,false);
            }else if(auto found=numbers.find(name);found!=numbers.end()){
                field="configuration."+found->second;
                const double high=found->second=="icon_size"||found->second=="chevron_size"?256:8192;
                const double low=found->second=="width"||found->second=="height"?1:0;
                expression=dynamic?"lvui::native::number("+expression_code(input->value,input->position,initial)+")":decimal(numeric(values,name,0,low,high));
            }else if(auto found=colors.find(name);found!=colors.end()){
                field="configuration.palette."+found->second;
                expression="lvui::native::hierarchy_item_color("+color_expression(input->value,input->position,initial)+")";
            }else if(name=="itemId"||name=="count"||name=="indentLevel"||name=="iconRevision"){
                field=name=="itemId"?"configuration.id":name=="count"?"configuration.count":name=="indentLevel"?"configuration.depth":"icon_revision";
                const double low=name=="indentLevel"||name=="iconRevision"?0:name=="count"?-1:-9007199254740991.;
                const double high=name=="indentLevel"?65536:9007199254740991.;
                if(dynamic)expression="lvui::native::integer(lvui::native::range("+expression_code(input->value,input->position,initial)+", "+decimal(low)+", "+decimal(high)+"))";
                else{const auto n=numeric(values,name,0,low,high);if(std::trunc(n)!=n)error(input->position,"Hierarchy integer input must be integral");expression="static_cast<std::int64_t>("+decimal(n)+")";}
                if(name=="indentLevel")expression="static_cast<std::size_t>("+expression+")";
            }else if(name=="iconSource"){
                if(!dynamic)error(input->position,"Hierarchy iconSource requires a native IconSource");
                field="icon_source";expression="lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input->value,input->position,initial)+")";
            }else continue;
            result+="value."+field+" = "+expression+"; ";
        }
        return result+"return value; }())";
    }
    std::string owned_table_options(const Inputs& values,bool initial,std::string_view fallback,SourcePosition position)const {
        const auto name=alias_key(values,"controller","model");
        const auto dynamic=[&](std::string_view key){auto item=values.find(key);return item!=values.end()&&native_expression(item->second->value);};
        std::string result="([&] { lvui::native::TableInput value; ";
        if(auto item=values.find("editable");item!=values.end()&&!values.contains("editor") &&
            (native_expression(item->second->value)||boolean(values,"editable",false)=="true"))
            error(item->second->position,"Editable tables require an explicit TableEditor");
        if(auto item=values.find("sortingEnabled");item!=values.end()&&!values.contains("orderEditor") &&
            (native_expression(item->second->value)||boolean(values,"sortingEnabled",false)=="true"))
            error(item->second->position,"Sortable tables require an explicit TableOrderEditor");
        for(auto name:{"columnResizeEnabled","rowResizeEnabled"})if(auto item=values.find(name);item!=values.end()&&!values.contains("structureEditor") &&
            (native_expression(item->second->value)||boolean(values,name,false)=="true"))
            error(item->second->position,"Resizable tables require an explicit TableStructureEditor");
        if(auto item=values.find(name);item!=values.end()){
            if(!dynamic(name))error(item->second->position,"Table model requires an LVUI TableController from a C++ backend");
            result+="value.controller = lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(item->second->value,item->second->position,initial)+"); ";
        }else result+="value.controller = "+std::string(fallback)+"; ";
        if(!dynamic("height")&&!dynamic("headerHeight")&&numeric(values,"headerHeight",24,1,8191)>=numeric(values,"height",300,19,8192))
            error(position,"Table headerHeight must be smaller than height");
        if(!dynamic("minColumnWidth")&&!dynamic("fallbackCellWidth")&&numeric(values,"fallbackCellWidth",120,32,8192)<numeric(values,"minColumnWidth",32,32,8192))
            error(position,"Table fallbackCellWidth is smaller than minColumnWidth");
        const std::map<std::string,std::string,std::less<>> numbers{{"width","width"},{"height","height"},{"headerHeight","header_height"},
            {"cellPadding","cell_padding"},{"fallbackCellWidth","fallback_column_width"},{"minColumnWidth","minimum_column_width"},{"fontSize","font_size"}};
        const std::map<std::string,std::string,std::less<>> flags{{"enabled","enabled"},{"selectionEnabled","selection_enabled"},
            {"keyboardNavigationEnabled","keyboard_navigation"},{"fitColumnsToViewport","fit_flexible_columns"},{"showIndicator","show_indicator"}};
        const std::map<std::string,std::string,std::less<>> colors{{"backgroundColor","background"},{"dividerColor","divider"},
            {"headerTextColor","header_text"},{"cellTextColor","cell_text"},{"selectionColor","selection"},{"currentCellBorderColor","current"}};
        for(const auto& [key,input]:values){
            const bool dynamic_value=native_expression(input->value);std::string field,expression;
            if(auto found=numbers.find(key);found!=numbers.end()){
                field="appearance."+found->second;
                const double low=key=="width"||key=="fallbackCellWidth"||key=="minColumnWidth"?32.:key=="height"?19.:key=="headerHeight"||key=="fontSize"?1.:0.;
                const double high=key=="fontSize"?256.:key=="headerHeight"?8191.:8192.;
                expression=dynamic_value?"lvui::native::number("+expression_code(input->value,input->position,initial)+")":decimal(numeric(values,key,0,low,high));
            }else if(auto found=flags.find(key);found!=flags.end()){
                field="appearance."+found->second;
                expression=dynamic_value?"lvui::native::truth("+expression_code(input->value,input->position,initial)+")":boolean(values,key,true);
            }else if(auto found=colors.find(key);found!=colors.end()){
                field="appearance.palette."+found->second;
                expression="lvui::native::table_color("+color_expression(input->value,input->position,initial)+")";
            }else if(key=="editor"){
                if(!dynamic_value)error(input->position,"Table editor requires a native TableEditor");
                field="editor";expression="lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input->value,input->position,initial)+")";
            }else if(key=="orderEditor"||key=="sorter"||key=="structureEditor"){
                if(!dynamic_value)error(input->position,"Table editor dependencies require native C++ handles");
                field=key=="orderEditor"?"order_editor":key=="structureEditor"?"structure_editor":"sorter";
                expression="lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input->value,input->position,initial)+")";
            }else if(key=="editable"){
                field="editable";expression=dynamic_value?"lvui::native::truth("+expression_code(input->value,input->position,initial)+")":boolean(values,key,false);
            }else if(key=="clipboardEnabled"){
                field="clipboard_enabled";expression=dynamic_value?"lvui::native::truth("+expression_code(input->value,input->position,initial)+")":boolean(values,key,true);
            }else if(key=="sortingEnabled"){
                field="sorting_enabled";expression=dynamic_value?"lvui::native::truth("+expression_code(input->value,input->position,initial)+")":boolean(values,key,false);
            }else if(key=="columnResizeEnabled"||key=="rowResizeEnabled"){
                field=key=="columnResizeEnabled"?"column_resize_enabled":"row_resize_enabled";
                expression=dynamic_value?"lvui::native::truth("+expression_code(input->value,input->position,initial)+")":boolean(values,key,false);
            }else if(key=="columnResizeHandleWidth"||key=="rowResizeHandleHeight"){
                field=key=="columnResizeHandleWidth"?"column_resize_handle":"row_resize_handle";
                expression=dynamic_value?"lvui::native::number("+expression_code(input->value,input->position,initial)+")":decimal(numeric(values,key,6,1,256));
            }else if(key=="accessibilityName"){
                field="appearance.accessibility_name";
                expression=dynamic_value?"lvui::native::text("+expression_code(input->value,input->position,initial)+")":literal(text(values,key));
            }else if(key=="modelRevision"){
                field="source_revision";
                if(dynamic_value)expression="lvui::native::integer(lvui::native::range("+expression_code(input->value,input->position,initial)+", 0, 9007199254740991.0))";
                else{const auto n=numeric(values,key,0,0,9007199254740991.);if(std::trunc(n)!=n)error(input->position,"modelRevision must be integral");expression="static_cast<std::int64_t>("+decimal(n)+")";}
            }else continue;
            result+="value."+field+" = "+expression+"; ";
        }
        return result+"return value; }())";
    }
    std::string owned_table_row_options(const Inputs& values,bool initial,SourcePosition position)const {
        if(values.contains("height")&&values.contains("cellHeight"))error(position,"Use height or cellHeight, not both");
        const std::map<std::string,std::string,std::less<>> numbers{{"width","width"},{"height","cell_height"},{"cellHeight","cell_height"},
            {"cellWidth","cell_width"},{"contentSpacing","content_spacing"},{"fontSize","font_size"}};
        const std::map<std::string,std::string,std::less<>> colors{{"backgroundColor","background"},{"dividerColor","divider"},{"textColor","cell_text"},
            {"selectionColor","selection"},{"currentBorderColor","current"}};
        std::string result="([&] { lvui::native::TableRowInput value; ";
        for(const auto& [key,input]:values){
            const bool dynamic=native_expression(input->value);std::string field,expression;
            if(auto found=numbers.find(key);found!=numbers.end()){
                field="configuration."+found->second;const auto low=key=="contentSpacing"?0.:1.;
                expression=dynamic?"lvui::native::number("+expression_code(input->value,input->position,initial)+")":decimal(numeric(values,key,0,low,key=="fontSize"?256.:8192.));
            }else if(auto found=colors.find(key);found!=colors.end()){
                field="configuration.palette."+found->second;expression="lvui::native::table_row_color("+color_expression(input->value,input->position,initial)+")";
            }else if(key=="enabled"||key=="visible"||key=="inputable"||key=="showDivider"){
                field="configuration."+std::string(key=="showDivider"?"show_divider":key);
                expression=dynamic?"lvui::native::truth("+expression_code(input->value,input->position,initial)+")":boolean(values,key,key!="inputable");
            }else if(key=="offset"){
                field="offset";expression=dynamic?"lvui::native::number("+expression_code(input->value,input->position,initial)+")":decimal(numeric(values,key,0,0,1e15));
            }else if(key=="cells"){
                field="configuration.cells";
                if(dynamic)expression="lvui::native::table_row_cells(lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input->value,input->position,initial)+"))";
                else{auto list=resolve(input->value,input->position);if(list.kind!=ValueKind::list||list.elements.size()>1024)error(input->position,"TableRow cells require at most 1024 Strings");
                    std::string items;for(const auto& item:list.elements){const auto value=resolve(item,input->position);if(value.kind!=ValueKind::string)error(input->position,"TableRow cells require Strings");
                        if(!items.empty())items+=", ";items+=literal(value.text);}
                    expression="lvui::native::table_row_cells(std::vector<std::string>{"+items+"})";}
            }else continue;
            result+="value."+field+" = "+expression+"; ";
        }
        return result+"return value; }())";
    }
    std::string owned_table_cell_item_options(const Inputs& values,bool initial,SourcePosition position)const {
        if(values.contains("height")&&values.contains("cellHeight"))error(position,"Use height or cellHeight, not both");
        if(values.contains("textStyle")&&values.contains("fontSize"))error(position,"Use textStyle or fontSize, not both");
        const std::map<std::string,std::string,std::less<>> numbers{{"width","width"},{"height","height"},{"cellHeight","height"},
            {"contentSpacing","padding"},{"fontSize","font_size"},{"currentBorderWidth","current_border_width"}};
        const std::map<std::string,std::string,std::less<>> colors{{"backgroundColor","background"},{"dividerColor","divider"},{"textColor","cell_text"},
            {"selectionColor","selection"},{"currentBorderColor","current"}};
        std::string result="([&] { lvui::native::TableCellItemInput value; ";
        for(const auto& [key,input]:values){
            const bool dynamic=native_expression(input->value);std::string field,expression;
            if(auto found=numbers.find(key);found!=numbers.end()){
                field="configuration."+found->second;const auto low=key=="width"||key=="height"||key=="cellHeight"||key=="fontSize"?1.:0.;
                expression=dynamic?"lvui::native::number("+expression_code(input->value,input->position,initial)+")":decimal(numeric(values,key,0,low,key=="fontSize"?256.:8192.));
            }else if(auto found=colors.find(key);found!=colors.end()){
                field="configuration.palette."+found->second;expression="lvui::native::table_cell_item_color("+color_expression(input->value,input->position,initial)+")";
            }else if(key=="text"||key=="valueType"||key=="accessibilityName"){
                field="configuration."+std::string(key=="text"?"text":key=="valueType"?"value_type":"accessibility_name");
                expression=dynamic?"lvui::native::text("+expression_code(input->value,input->position,initial)+")":literal(text(values,key));
            }else if(key=="enabled"||key=="visible"||key=="inputable"||key=="selected"||key=="current"||key=="showDivider"||key=="clipContent"){
                field="configuration."+std::string(key=="showDivider"?"show_divider":key=="clipContent"?"clip_content":key);
                expression=dynamic?"lvui::native::truth("+expression_code(input->value,input->position,initial)+")":boolean(values,key,key=="enabled"||key=="visible"||key=="showDivider"||key=="clipContent");
            }else if(key=="valueValidator"){
                if(!dynamic)error(input->position,"Table cell validator requires a native C++ validator");field="validator";
                expression="lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input->value,input->position,initial)+")";
            }else if(key=="textStyle"){
                if(dynamic)error(input->position,"Table cell textStyle must be a static style; use fontSize for a reactive value");
                auto styles=values;styles["style"]=input;field="configuration.font_size";expression=decimal(text_role(styles).second);
            }else continue;
            result+="value."+field+" = "+expression+"; ";
        }
        return result+"return value; }())";
    }
    std::string owned_table_header_options(const Inputs& values,bool initial)const {
        const std::map<std::string,std::string,std::less<>> numbers{{"width","width"},{"rowHeight","row_height"},{"cellHorizontalPadding","horizontal_padding"},
            {"separatorHeight","separator_height"},{"minColumnWidth","minimum_column_width"},{"fallbackCellWidth","fallback_column_width"}};
        const std::map<std::string,std::string,std::less<>> colors{{"textColor","header_text"},{"backgroundColor","background"},{"separatorColor","divider"},{"focusColor","current"}};
        std::string result="([&] { lvui::native::TableHeaderInput value; ",widths;
        for(const auto& [key,input]:values){
            const bool dynamic=native_expression(input->value);std::string field,expression;
            if(auto found=numbers.find(key);found!=numbers.end()){
                field="configuration."+found->second;const auto low=key=="width"||key=="rowHeight"?1.:key=="minColumnWidth"?32.:0.;
                expression=dynamic?"lvui::native::number("+expression_code(input->value,input->position,initial)+")":decimal(numeric(values,key,0,low,8192));
            }else if(auto found=colors.find(key);found!=colors.end()){
                field="configuration.palette."+found->second;expression="lvui::native::table_header_color("+color_expression(input->value,input->position,initial)+")";
            }else if(key=="interactive"||key=="enabled"||key=="visible"||key=="fitColumnsToViewport"){
                field="configuration."+std::string(key=="fitColumnsToViewport"?"fit_flexible_columns":key);
                expression=dynamic?"lvui::native::truth("+expression_code(input->value,input->position,initial)+")":boolean(values,key,key!="interactive");
            }else if(key=="offset"){
                field="offset";expression=dynamic?"lvui::native::number("+expression_code(input->value,input->position,initial)+")":decimal(numeric(values,key,0,0,1e15));
            }else if(key=="columns"){
                field="configuration.columns";
                if(dynamic)expression="lvui::native::table_header_columns(lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input->value,input->position,initial)+"))";
                else{auto list=resolve(input->value,input->position);if(list.kind!=ValueKind::list||list.elements.size()>1024)error(input->position,"TableHeader columns require at most 1024 Strings");
                    std::string items;for(const auto& item:list.elements){const auto value=resolve(item,input->position);if(value.kind!=ValueKind::string)error(input->position,"TableHeader columns require Strings");
                        if(!items.empty())items+=", ";items+=literal(value.text);}
                    expression="lvui::native::table_header_columns(std::vector<std::string>{"+items+"})";}
            }else if(key=="columnWidths"){
                if(dynamic)widths="lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input->value,input->position,initial)+")";
                else{auto list=resolve(input->value,input->position);if(list.kind!=ValueKind::list||list.elements.size()>1024)error(input->position,"TableHeader columnWidths require at most 1024 Numbers");
                    std::string items;for(const auto& item:list.elements){const auto n=number(resolve_in(item,StyleTokenType::number,input->position),input->position);
                        if(n<0||n>8192||(n>0&&n<32))error(input->position,"Invalid TableHeader column width");if(!items.empty())items+=", ";items+=decimal(n);}
                    widths="std::vector<double>{"+items+"}";}
                continue;
            }else continue;
            result+="value."+field+" = "+expression+"; ";
        }
        if(values.contains("fallbackCellWidth")&&!values.contains("fitColumnsToViewport"))result+="value.configuration.fit_flexible_columns = value.configuration.fallback_column_width == 0; ";
        if(!widths.empty())result+="lvui::native::table_header_widths(value.configuration.columns, "+widths+"); ";
        return result+"return value; }())";
    }
    std::string owned_media_card_options(const Inputs& values,bool initial,SourcePosition position)const {
        if(values.contains("title")&&values.contains("filename"))error(position,"Use title or filename, not both");
        if(values.contains("icon")&&(values.contains("iconName")||values.contains("iconSource")))error(position,"Use an icon image or an icon source, not both");
        const std::map<std::string,std::string,std::less<>> numbers{{"width","width"},{"height","height"},{"padding","padding"},{"cornerRadius","corner_radius"},{"progress","progress"}};
        const std::map<std::string,std::string,std::less<>> colors{{"backgroundColor","surface"},{"hoverColor","hover"},{"selectedColor","selected"},
            {"borderColor","border"},{"selectedBorderColor","selected_border"},{"textColor","text"},{"secondaryColor","secondary"},{"disabledTextColor","disabled_text"},
            {"progressColor","progress"},{"progressTrackColor","progress_track"},{"statusColor","status"},{"focusColor","focus"}};
        const std::map<std::string,std::string,std::less<>> strings{{"title","title"},{"filename","title"},{"description","description"},{"details","details"},
            {"metadata","metadata"},{"statusText","status_text"},{"summary","summary"},{"footnote","footnote"},{"actionText","action_text"},{"domain","domain"},
            {"previewTitle","preview_title"},{"progressLabel","progress_label"},{"progressText","progress_text"},{"menuAccessibleName","menu_accessibility_name"},{"iconName","icon_name"}};
        const std::map<std::string,std::string,std::less<>> flags{{"selected","selected"},{"selectable","selectable"},{"enabled","enabled"},{"visible","visible"},
            {"showDescription","show_description"},{"showStatus","show_status"},{"showMenu","show_menu"},{"showAction","show_action"}};
        std::string result="([&] { lvui::native::CardInput value; ";
        for(const auto& [key,input]:values){
            const bool dynamic=native_expression(input->value);std::string field,expression;
            if(auto found=numbers.find(key);found!=numbers.end()){
                field="configuration."+found->second;expression=dynamic?"lvui::native::number("+expression_code(input->value,input->position,initial)+")":decimal(numeric(values,key,0,0,key=="progress"?1.:8192.));
            }else if(auto found=colors.find(key);found!=colors.end()){
                field="configuration.palette."+found->second;expression="lvui::native::card_color("+color_expression(input->value,input->position,initial)+")";
            }else if(auto found=strings.find(key);found!=strings.end()){
                field="configuration."+found->second;expression=dynamic?"lvui::native::text("+expression_code(input->value,input->position,initial)+")":literal(text(values,key));
            }else if(auto found=flags.find(key);found!=flags.end()){
                field="configuration."+found->second;expression=dynamic?"lvui::native::truth("+expression_code(input->value,input->position,initial)+")":boolean(values,key,key!="selected");
            }else if(key=="type"||key=="size"||key=="detail"||key=="displayState"){
                field="configuration."+std::string(key=="displayState"?"display_state":key);
                if(dynamic)expression="lvui::native::"+std::string(key=="displayState"?"card_display_state":"card_"+key)+"("+expression_code(input->value,input->position,initial)+")";
                else {const auto item=resolve(input->value,input->position);
                    const std::map<std::string,std::string,std::less<>> types{{".File","file"},{".FilePreview","file_preview"},{".Folder","folder"},{".Project","project"},{".Device","device"},{".Model","model"},{".Member","member"},{".Link","link"}};
                    const std::map<std::string,std::string,std::less<>> sizes{{".Small","small"},{".Medium","medium"},{".Large","large"},{".small","small"},{".medium","medium"},{".large","large"}};
                    const std::map<std::string,std::string,std::less<>> details{{".Brief","brief"},{".Detailed","detailed"},{".brief","brief"},{".detailed","detailed"}};
                    const std::map<std::string,std::string,std::less<>> states{{".Automatic","automatic"},{".DefaultState","normal"},{".HoverState","hover"},{".SelectedState","selected"}};
                    const auto& choices=key=="type"?types:key=="size"?sizes:key=="detail"?details:states;auto found=choices.find(item.text);
                    if(item.kind!=ValueKind::enumeration||found==choices.end())error(input->position,"Unknown independent Card "+key);
                    expression="lvui::"+std::string(key=="type"?"CardType":key=="size"?"CardSize":key=="detail"?"CardDetail":"CardDisplayState")+"::"+found->second;}
            }else if(key=="rows"||key=="icon"||key=="iconSource"||key=="previewSource"){
                if(!dynamic)error(input->position,"Card images, icon sources and rows require native C++ values");
                field=key=="rows"?"configuration.rows":key=="icon"?"icon":key=="iconSource"?"icon_source":"preview";
                expression="lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input->value,input->position,initial)+")";
            }else if(key=="iconRevision"){
                field="icon_revision";
                if(dynamic)expression="lvui::native::integer(lvui::native::range("+expression_code(input->value,input->position,initial)+", 0, 9007199254740991.0))";
                else{const auto n=numeric(values,key,0,0,9007199254740991.);if(std::trunc(n)!=n)error(input->position,"iconRevision must be integral");expression="static_cast<std::int64_t>("+decimal(n)+")";}
            }else continue;
            result+="value."+field+" = "+expression+"; ";
        }
        return result+"return value; }())";
    }
    std::string owned_list_item_options(const Inputs& values,bool initial,SourcePosition position,bool composite=false)const {
        if(values.contains("icon")&&values.contains("iconName"))error(position,"Use icon or iconName, not both");
        if(values.contains("type")&&values.contains("size"))error(position,"Use type or size, not both");
        if(values.contains("label")&&values.contains("text"))error(position,"Use label or text, not both");
        if(values.contains("description")&&values.contains("detail"))error(position,"Use description or detail, not both");
        const std::map<std::string,std::string,std::less<>> numbers{{"width","width"},{"height","height"},{"leftPadding","left_padding"},
            {"rightPadding","right_padding"},{"iconSize","icon_size"},{"spacing","spacing"},{"cornerRadius","corner_radius"},{"chevronSize","chevron_size"}};
        const std::map<std::string,std::string,std::less<>> colors{{"textColor","text"},{"descriptionColor","description"},{"disabledTextColor","disabled_text"},
            {"backgroundColor","idle"},{"hoverColor","hover"},{"pressedColor","pressed"},{"selectedColor","selected"},{"focusColor","focus"}};
        std::string result=std::string("([&] { lvui::native::")+(composite?"ListItemCompositeInput":"ListItemInput")+" value; ";
        for(const auto& [key,input]:values){
            const bool dynamic=native_expression(input->value);std::string field,expression;
            if(auto found=numbers.find(key);found!=numbers.end()){
                field="configuration."+found->second;const auto high=key=="iconSize"||key=="chevronSize"?256.:8192.;
                expression=dynamic?"lvui::native::number("+expression_code(input->value,input->position,initial)+")":decimal(numeric(values,key,0,0,high));
            }else if(auto found=colors.find(key);found!=colors.end()){
                field="configuration.palette."+found->second;expression="lvui::native::list_item_color("+color_expression(input->value,input->position,initial)+")";
            }else if(key=="label"||key=="text"||key=="description"||key=="detail"||key=="iconName"||key=="accessibilityName"){
                field="configuration."+std::string(key=="label"||key=="text"?"label":key=="description"||key=="detail"?"description":key=="iconName"?"icon_name":"accessibility_name");
                expression=dynamic?"lvui::native::text("+expression_code(input->value,input->position,initial)+")":literal(text(values,key));
            }else if(key=="selected"||key=="enabled"||key=="visible"||key=="interactive"||key=="selectable"||key=="showDescription"||key=="showChevron"){
                field="configuration."+std::string(key=="showDescription"?"show_description":key=="showChevron"?"show_chevron":key);
                expression=dynamic?"lvui::native::truth("+expression_code(input->value,input->position,initial)+")":boolean(values,key,key!="selected"&&key!="showChevron");
            }else if(key=="type"||key=="size"){
                field="configuration.variant";
                if(dynamic)expression="lvui::native::list_item_variant("+expression_code(input->value,input->position,initial)+")";
                else {const auto variant=resolve(input->value,input->position);const std::map<std::string,std::string,std::less<>> variants{{".mini","mini"},{".Mini","mini"},{".detail","detail"},{".Detail","detail"},{".navigation","navigation"},{".Navigation","navigation"},{".toggle","toggle"},{".Toggle","toggle"},{".checkable","checkable"},{".Checkable","checkable"},{".action","action"},{".Action","action"},{".actionGroup","action_group"},{".ActionGroup","action_group"},{".stepper","stepper"},{".Stepper","stepper"},{".select","select"},{".Select","select"}};
                    auto found=variants.find(variant.text);if(variant.kind!=ValueKind::enumeration||found==variants.end())error(input->position,"Unsupported independent ListItem type");expression="lvui::ListItemVariant::"+found->second;}
            }else if(key=="checked"||key=="toggleEnabled"||key=="selectionEnabled"||key=="showToggle"||key=="showSelection"){
                field=key=="checked"?"checked":key=="toggleEnabled"?"toggle_enabled":key=="selectionEnabled"?"selection_enabled":key=="showToggle"?"show_toggle":"show_selection";
                expression=dynamic?"lvui::native::truth("+expression_code(input->value,input->position,initial)+")":boolean(values,key,key!="checked");
            }else if(key=="showPrimaryAction"||key=="showSecondaryAction"||key=="showMoreMenu"){
                field=key=="showPrimaryAction"?"show_primary_action":key=="showSecondaryAction"?"show_secondary_action":"show_more_menu";
                expression=dynamic?"lvui::native::truth("+expression_code(input->value,input->position,initial)+")":boolean(values,key,true);
            }else if(key=="primaryAction"||key=="secondaryAction"||key=="moreMenu"){
                field=key=="primaryAction"?"primary_action":key=="secondaryAction"?"secondary_action":"more_menu";
                if(dynamic)expression="lvui::native::list_footer_slot(lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input->value,input->position,initial)+"))";
                else{const auto item=resolve(input->value,input->position);if(item.kind!=ValueKind::string)error(input->position,"List item action requires a String or native ListFooterSlot");
                    expression="lvui::ListFooterSlot{.text = "+literal(item.text)+"}";}
            }else if(key=="slotPadding"||key=="slotSpacing"||key=="actionWidth"){
                field=key=="slotPadding"?"slot_padding":key=="slotSpacing"?"slot_spacing":"action_width";
                expression=dynamic?"lvui::native::number("+expression_code(input->value,input->position,initial)+")":decimal(numeric(values,key,0,key=="actionWidth"?1.:0.,8192));
            }else if(key=="quantity"||key=="minimumQuantity"||key=="maximumQuantity"||key=="stepSize"||key=="quantityWidth"||key=="quantitySpacing"){
                field=key=="quantity"?"quantity":key=="minimumQuantity"?"minimum_quantity":key=="maximumQuantity"?"maximum_quantity":key=="stepSize"?"step_size":key=="quantityWidth"?"quantity_width":"quantity_spacing";
                if(dynamic)expression="lvui::native::number("+expression_code(input->value,input->position,initial)+")";
                else{const auto bound=std::numeric_limits<double>::max();const bool layout=key=="quantityWidth"||key=="quantitySpacing";const auto n=numeric(values,key,0,layout?0.:-bound,layout?8192.:bound);
                    if(key=="stepSize"&&n<=0)error(input->position,"stepSize must be positive");expression=decimal(n);}
            }else if(key=="stepperEnabled"||key=="showStepper"){
                field=key=="stepperEnabled"?"stepper_enabled":"show_stepper";
                expression=dynamic?"lvui::native::truth("+expression_code(input->value,input->position,initial)+")":boolean(values,key,true);
            }else if(key=="selector"){
                field="selector";
                if(dynamic)expression="lvui::native::list_item_selector(lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input->value,input->position,initial)+"))";
                else{const auto list=resolve(input->value,input->position);if(list.kind!=ValueKind::list||list.elements.size()>400)error(input->position,"List selector requires at most 400 Strings");
                    std::string items;for(const auto& element:list.elements){const auto item=resolve(element,input->position);if(item.kind!=ValueKind::string)error(input->position,"List selector options must be Strings");
                        if(!items.empty())items+=", ";items+=literal(item.text);}
                    expression="lvui::native::list_item_selector(std::vector<std::string>{"+items+"})";}
            }else if(key=="selectorIndex"){
                field="selector_index";
                if(dynamic)expression="lvui::native::integer(lvui::native::range("+expression_code(input->value,input->position,initial)+", 0, 399))";
                else{const auto n=numeric(values,key,0,0,399);if(std::trunc(n)!=n)error(input->position,"selectorIndex must be integral");expression=decimal(n);}
            }else if(key=="selectorWidth"){
                field="selector_width";expression=dynamic?"lvui::native::number("+expression_code(input->value,input->position,initial)+")":decimal(numeric(values,key,97,19,8176));
            }else if(key=="selectorEnabled"||key=="showSelector"){
                field=key=="selectorEnabled"?"selector_enabled":"show_selector";
                expression=dynamic?"lvui::native::truth("+expression_code(input->value,input->position,initial)+")":boolean(values,key,true);
            }else if(key=="icon"||key=="iconSource"){
                if(!dynamic)error(input->position,"List item icon inputs require native C++ handles");
                field=key=="icon"?"icon":"icon_source";expression="lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input->value,input->position,initial)+")";
            }else if(key=="iconRevision"){
                field="icon_revision";
                if(dynamic)expression="lvui::native::integer(lvui::native::range("+expression_code(input->value,input->position,initial)+", 0, 9007199254740991.0))";
                else {const auto n=numeric(values,key,0,0,9007199254740991.);if(std::trunc(n)!=n)error(input->position,"iconRevision must be integral");expression="static_cast<std::int64_t>("+decimal(n)+")";}
            }else continue;
            if(composite&&(field.starts_with("configuration.")||field=="icon"||field=="icon_source"||field=="icon_revision"))field="row."+field;
            result+="value."+field+" = "+expression+"; ";
        }
        return result+"return value; }())";
    }
    std::string owned_toolbar_button_options(const Inputs& values,bool initial,SourcePosition position)const {
        if(values.contains("icon")&&values.contains("iconName"))error(position,"Use icon or iconName, not both");
        const std::map<std::string,std::string,std::less<>> numbers{{"width","width"},{"height","height"},{"iconSize","icon_size"},
            {"leftPadding","left_padding"},{"rightPadding","right_padding"},{"spacing","spacing"},{"cornerRadius","corner_radius"},{"fontSize","font_size"}};
        const std::map<std::string,std::string,std::less<>> colors{{"textColor","text"},{"disabledTextColor","disabled_text"},
            {"hoverColor","hover"},{"pressedColor","pressed"},{"primaryColor","primary"},{"dangerColor","danger"},{"focusColor","focus"}};
        std::string result="([&] { lvui::native::ToolbarButtonInput value; ";
        for(const auto& [key,input]:values){
            const bool dynamic=native_expression(input->value);std::string field,expression;
            if(auto found=numbers.find(key);found!=numbers.end()){
                field="configuration."+found->second;
                const double low=key=="height"||key=="iconSize"||key=="fontSize"?1.:0.;
                const double high=key=="iconSize"||key=="fontSize"?256.:8192.;
                expression=dynamic?"lvui::native::number("+expression_code(input->value,input->position,initial)+")":decimal(numeric(values,key,0,low,high));
            }else if(auto found=colors.find(key);found!=colors.end()){
                field="configuration.palette."+found->second;
                expression="lvui::native::toolbar_button_color("+color_expression(input->value,input->position,initial)+")";
            }else if(key=="text"||key=="accessibilityName"||key=="iconName"){
                field=key=="iconName"?"icon_name":"configuration."+std::string(key=="text"?"text":"accessibility_name");
                expression=dynamic?"lvui::native::text("+expression_code(input->value,input->position,initial)+")":literal(text(values,key));
            }else if(key=="enabled"||key=="visible"){
                field="configuration."+key;expression=dynamic?"lvui::native::truth("+expression_code(input->value,input->position,initial)+")":boolean(values,key,true);
            }else if(key=="buttonId"){
                field="configuration.id";
                if(dynamic)expression="lvui::native::toolbar_button_id("+expression_code(input->value,input->position,initial)+")";
                else {const auto id=resolve(input->value,input->position);
                    if(id.kind==ValueKind::string){if(id.text.empty())error(input->position,"Toolbar button identity cannot be empty");expression="lvui::ToolbarButtonId{std::string("+literal(id.text)+")}";}
                    else if(id.kind==ValueKind::number){const auto n=numeric(values,key,0,-9007199254740991.,9007199254740991.);if(std::trunc(n)!=n)error(input->position,"Toolbar button identity must be integral");expression="lvui::ToolbarButtonId{std::int64_t("+decimal(n)+")}";}
                    else error(input->position,"Toolbar button identity requires a String or integer");}
            }else if(key=="tone"){
                field="configuration.tone";
                if(dynamic)expression="lvui::native::toolbar_button_tone("+expression_code(input->value,input->position,initial)+")";
                else {const auto tone=resolve(input->value,input->position);const std::map<std::string,std::string,std::less<>> tones{{".primary","primary"},{".default","normal"},{".borderless","borderless"},{".destructive","destructive"},{".disabled","disabled"}};
                    auto found=tones.find(tone.text);if(tone.kind!=ValueKind::enumeration||found==tones.end())error(input->position,"Unknown toolbar button tone");expression="lvui::ButtonTone::"+found->second;}
            }else if(key=="icon"||key=="iconSource"){
                if(!dynamic)error(input->position,"Toolbar button icon inputs require native C++ handles");
                field=key=="icon"?"icon":"icon_source";expression="lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input->value,input->position,initial)+")";
            }else if(key=="iconRevision"){
                field="icon_revision";
                if(dynamic)expression="lvui::native::integer(lvui::native::range("+expression_code(input->value,input->position,initial)+", 0, 9007199254740991.0))";
                else {const auto n=numeric(values,key,0,0,9007199254740991.);if(std::trunc(n)!=n)error(input->position,"iconRevision must be integral");expression="static_cast<std::int64_t>("+decimal(n)+")";}
            }else continue;
            result+="value."+field+" = "+expression+"; ";
        }
        return result+"return value; }())";
    }
    std::string owned_hierarchy_toolbar_options(const Inputs& values,bool initial)const {
        const std::map<std::string,std::string,std::less<>> numbers{{"minimumToolbarWidth","minimum_width"},{"width","width"},{"height","height"},
            {"horizontalPadding","horizontal_padding"},{"verticalPadding","vertical_padding"},{"spacing","spacing"},{"slotSize","slot_size"},
            {"iconSize","icon_size"},{"cornerRadius","corner_radius"},{"fontSize","font_size"},{"backgroundOpacity","background_opacity"}};
        const std::map<std::string,std::string,std::less<>> colors{{"textColor","text"},{"disabledTextColor","disabled_text"},
            {"hoverColor","hover"},{"pressedColor","pressed"},{"primaryColor","primary"},{"dangerColor","danger"},{"focusColor","focus"}};
        std::string result="([&] { lvui::native::HierarchyToolbarInput value; ";
        for(const auto& [name,input]:values){
            const bool dynamic=native_expression(input->value);std::string field,expression;
            if(name=="buttonItems"){
                if(!dynamic)error(input->position,"Hierarchy toolbar items require native HierarchyToolbarItem values");
                field="items";expression="lvui::native::hierarchy_toolbar_items("+expression_code(input->value,input->position,initial)+")";
            }else if(name=="activeButtonId"){
                field="active_id";
                if(dynamic)expression="lvui::native::hierarchy_toolbar_active("+expression_code(input->value,input->position,initial)+")";
                else {const auto id=resolve(input->value,input->position);
                    if(id.kind==ValueKind::string){if(id.text.empty())error(input->position,"Toolbar active identity cannot be empty");expression="lvui::ToolbarButtonId{std::string("+literal(id.text)+")}";}
                    else if(id.kind==ValueKind::number){const auto n=numeric(values,name,0,-9007199254740991.,9007199254740991.);if(std::trunc(n)!=n)error(input->position,"Toolbar active identity must be integral");expression="lvui::ToolbarButtonId{std::int64_t("+decimal(n)+")}";}
                    else error(input->position,"Toolbar active identity requires a String or integer");}
            }else if(auto found=numbers.find(name);found!=numbers.end()){
                field="appearance."+found->second;
                const double low=name=="minimumToolbarWidth"||name=="slotSize"||name=="iconSize"||name=="fontSize"?1.:0.;
                const double high=name=="backgroundOpacity"?1.:name=="iconSize"||name=="fontSize"?256.:8192.;
                expression=dynamic?"lvui::native::number("+expression_code(input->value,input->position,initial)+")":decimal(numeric(values,name,0,low,high));
            }else if(name=="backgroundColor"){
                field="appearance.background";expression="lvui::native::hierarchy_toolbar_color("+color_expression(input->value,input->position,initial)+")";
            }else if(auto found=colors.find(name);found!=colors.end()){
                field="appearance.palette."+found->second;expression="lvui::native::hierarchy_toolbar_color("+color_expression(input->value,input->position,initial)+")";
            }else if(name=="enabled"||name=="distributeSpacing"){
                field=name=="enabled"?"appearance.enabled":"appearance.distribute_spacing";
                expression=dynamic?"lvui::native::truth("+expression_code(input->value,input->position,initial)+")":boolean(values,name,name=="enabled");
            }else if(name=="iconRevision"){
                field="icon_revision";
                if(dynamic)expression="lvui::native::integer(lvui::native::range("+expression_code(input->value,input->position,initial)+", 0, 9007199254740991.0))";
                else{const auto n=numeric(values,name,0,0,9007199254740991.);if(std::trunc(n)!=n)error(input->position,"iconRevision must be integral");expression="static_cast<std::int64_t>("+decimal(n)+")";}
            }else if(name=="iconSource"){
                if(!dynamic)error(input->position,"Hierarchy toolbar iconSource requires a native IconSource");
                field="icon_source";expression="lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input->value,input->position,initial)+")";
            }else continue;
            result+="value."+field+" = "+expression+"; ";
        }
        return result+"return value; }())";
    }
    std::string owned_list_footer_options(const Inputs& values,bool initial)const {
        const std::map<std::string,std::string,std::less<>> numbers{{"width","width"},{"height","height"},
            {"horizontalPadding","horizontal_padding"},{"verticalPadding","vertical_padding"},{"spacing","spacing"},
            {"iconSize","icon_size"},{"buttonHeight","button_height"},{"fontSize","font_size"},{"cornerRadius","corner_radius"}};
        const std::map<std::string,std::string,std::less<>> colors{{"textColor","text"},{"disabledTextColor","disabled_text"},
            {"hoverColor","hover"},{"pressedColor","pressed"},{"primaryColor","primary"},{"dangerColor","danger"},{"focusColor","focus"}};
        std::string result="([&] { lvui::native::ListFooterInput value; ";
        for(const auto& [name,input]:values){
            const bool dynamic=native_expression(input->value);std::string field,expression;
            if(name=="button1"||name=="button2"||name=="button3"){
                if(!dynamic)error(input->position,"List footer buttons require native ListFooterSlot values");
                field="configuration.slots["+std::to_string(name.back()-'1')+"]";
                expression="lvui::native::list_footer_slot("+expression_code(input->value,input->position,initial)+")";
            }else if(auto found=numbers.find(name);found!=numbers.end()){
                field="configuration."+found->second;const double high=name=="iconSize"||name=="fontSize"?256.:8192.;
                const double low=name=="iconSize"||name=="fontSize"||name=="buttonHeight"?1.:0.;
                expression=dynamic?"lvui::native::number("+expression_code(input->value,input->position,initial)+")":decimal(numeric(values,name,0,low,high));
            }else if(auto found=colors.find(name);found!=colors.end()){
                field="configuration.palette."+found->second;
                expression="lvui::native::list_footer_color("+color_expression(input->value,input->position,initial)+")";
            }else if(name=="enabled"||name=="interactive"){
                field="configuration."+name;
                expression=dynamic?"lvui::native::truth("+expression_code(input->value,input->position,initial)+")":boolean(values,name,true);
            }else if(name=="iconRevision"){
                field="icon_revision";
                if(dynamic)expression="lvui::native::integer(lvui::native::range("+expression_code(input->value,input->position,initial)+", 0, 9007199254740991.0))";
                else{const auto n=numeric(values,name,0,0,9007199254740991.);if(std::trunc(n)!=n)error(input->position,"iconRevision must be integral");expression="static_cast<std::int64_t>("+decimal(n)+")";}
            }else if(name=="iconSource"){
                if(!dynamic)error(input->position,"List footer iconSource requires a native IconSource");
                field="icon_source";expression="lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input->value,input->position,initial)+")";
            }else continue;
            result+="value."+field+" = "+expression+"; ";
        }
        return result+"return value; }())";
    }
    std::string owned_list_toolbar_options(const Inputs& values,bool initial)const {
        const std::map<std::string,std::string,std::less<>> numbers{{"width","width"},{"height","height"},{"iconSize","icon_size"},
            {"buttonSize","button_size"},{"horizontalPadding","horizontal_padding"},{"verticalPadding","vertical_padding"},
            {"spacing","spacing"},{"cornerRadius","corner_radius"}};
        const std::map<std::string,std::string,std::less<>> colors{{"backgroundColor","background"},{"hoverColor","hover"},
            {"pressedColor","pressed"},{"focusColor","focus"}};
        std::string result="([&] { lvui::native::ListToolbarInput value; ";
        for(const auto& [name,input]:values){
            const bool dynamic=native_expression(input->value);std::string field,expression;
            if(name=="icon1"||name=="icon2"||name=="icon3"){
                field="configuration.icons["+std::to_string(name.back()-'1')+"]";
                expression=dynamic?"lvui::native::text("+expression_code(input->value,input->position,initial)+")":literal(text(values,name));
            }else if(auto found=numbers.find(name);found!=numbers.end()){
                field="configuration."+found->second;const double high=name=="iconSize"?256.:8192.;
                const double low=name=="iconSize"||name=="buttonSize"?1.:0.;
                expression=dynamic?"lvui::native::number("+expression_code(input->value,input->position,initial)+")":decimal(numeric(values,name,0,low,high));
            }else if(auto found=colors.find(name);found!=colors.end()){
                field="configuration.palette."+found->second;
                expression="lvui::native::list_toolbar_color("+color_expression(input->value,input->position,initial)+")";
            }else if(name=="enabled"||name=="interactive"){
                field="configuration."+name;
                expression=dynamic?"lvui::native::truth("+expression_code(input->value,input->position,initial)+")":boolean(values,name,true);
            }else if(name=="iconRevision"){
                field="icon_revision";
                if(dynamic)expression="lvui::native::integer(lvui::native::range("+expression_code(input->value,input->position,initial)+", 0, 9007199254740991.0))";
                else{const auto n=numeric(values,name,0,0,9007199254740991.);if(std::trunc(n)!=n)error(input->position,"iconRevision must be integral");expression="static_cast<std::int64_t>("+decimal(n)+")";}
            }else if(name=="iconSource"){
                if(!dynamic)error(input->position,"List toolbar iconSource requires a native IconSource");
                field="icon_source";expression="lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input->value,input->position,initial)+")";
            }else continue;
            result+="value."+field+" = "+expression+"; ";
        }
        return result+"return value; }())";
    }
    std::string owned_hierarchy_list_options(const Inputs& values,bool initial,std::string_view fallback)const {
        const auto row_height=alias_key(values,"generatedRowHeight","rowHeight");
        const auto indent=alias_key(values,"generatedIndentStep","indentStep");
        const auto icon=alias_key(values,"generatedIconSize","iconSize");
        const auto chevron=alias_key(values,"generatedChevronSize","chevronSize");
        std::string result="([&] { lvui::native::HierarchyListInput value; ";
        if(auto item=values.find("editable");item!=values.end()&&!values.contains("editor") &&
            (native_expression(item->second->value)||boolean(values,"editable",false)=="true"))
            error(item->second->position,"Editable hierarchy lists require an explicit HierarchyEditor");
        if(auto item=values.find("controller");item!=values.end()){
            if(!native_expression(item->second->value))error(item->second->position,"Hierarchy controller requires an LVUI HierarchyController from a C++ backend");
            result+="value.controller = lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(item->second->value,item->second->position,initial)+"); ";
        }else result+="value.controller = "+std::string(fallback)+"; ";
        const std::map<std::string,std::string,std::less<>> numbers{{"width","width"},{"height","height"},{row_height,"row_height"},
            {indent,"row_appearance.indent_step"},{icon,"row_appearance.icon_size"},{chevron,"row_appearance.chevron_size"},
            {"baseLeftPadding","row_appearance.left_padding"},{"rowRightPadding","row_appearance.right_padding"},
            {"leadingSpacing","row_appearance.leading_spacing"},{"cornerRadius","row_appearance.corner_radius"},{"countWidth","row_appearance.count_width"}};
        const std::map<std::string,std::string,std::less<>> colors{{"textColorNormal","text"},{"textColorDisabled","disabled_text"},
            {"rowBackgroundColorIdle","idle"},{"rowBackgroundColorHover","hover"},{"rowBackgroundColorPressed","pressed"},
            {"rowBackgroundColorActive","active"},{"rowBackgroundColorInactive","inactive"},{"rowBackgroundColorDrag","drag"},{"focusColor","focus"}};
        for(const auto& [name,input]:values){
            const bool dynamic=native_expression(input->value);std::string field,expression;
            if(auto found=numbers.find(name);found!=numbers.end()){
                field=found->second;const auto high=name==icon||name==chevron||name==row_height?256.:8192.;
                const auto low=name==row_height?18.:name=="width"||name=="height"?1.:0.;
                expression=dynamic?"lvui::native::number("+expression_code(input->value,input->position,initial)+")":decimal(numeric(values,name,0,low,high));
            }else if(auto found=colors.find(name);found!=colors.end()){
                field="row_appearance.palette."+found->second;
                expression="lvui::native::hierarchy_item_color("+color_expression(input->value,input->position,initial)+")";
            }else if(name=="enabled"||name=="editable"||name=="showIndicator"||name=="keyboardNavigationEnabled"){
                field=name=="enabled"?"enabled":name=="editable"?"editable":name=="showIndicator"?"show_indicator":"keyboard_navigation";
                expression=dynamic?"lvui::native::truth("+expression_code(input->value,input->position,initial)+")":boolean(values,name,name!="editable");
            }else if(name=="modelRevision"||name=="iconRevision"||name=="overscan"){
                field=name=="modelRevision"?"source_revision":name=="iconRevision"?"icon_revision":"overscan";
                const double high=name=="overscan"?256.:9007199254740991.;
                if(dynamic)expression="lvui::native::integer(lvui::native::range("+expression_code(input->value,input->position,initial)+", 0, "+decimal(high)+"))";
                else{const auto n=numeric(values,name,0,0,high);if(std::trunc(n)!=n)error(input->position,"Hierarchy integer input must be integral");expression="static_cast<std::int64_t>("+decimal(n)+")";}
                if(name=="overscan")expression="static_cast<std::size_t>("+expression+")";
            }else if(name=="iconSource"||name=="editor"){
                if(!dynamic)error(input->position,name=="editor"?"Hierarchy editor requires a native HierarchyEditor":"Hierarchy iconSource requires a native IconSource");
                field=name=="editor"?"editor":"icon_source";expression="lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input->value,input->position,initial)+")";
            }else continue;
            result+="value."+field+" = "+expression+"; ";
        }
        return result+"return value; }())";
    }
    std::string owned_hierarchy_panel_options(const Inputs& values,bool initial,std::string_view fallback)const {
        Inputs list,toolbar,footer;
        const std::set<std::string,std::less<>> list_fields{"controller","editor","editable","modelRevision","rowHeight","overscan",
            "showIndicator","keyboardNavigationEnabled","indentStep","iconSize","chevronSize","baseLeftPadding","rowRightPadding",
            "leadingSpacing","cornerRadius","countWidth","iconSource","iconRevision","textColorNormal","textColorDisabled",
            "rowBackgroundColorIdle","rowBackgroundColorHover","rowBackgroundColorPressed","rowBackgroundColorActive",
            "rowBackgroundColorInactive","rowBackgroundColorDrag","focusColor"};
        for(const auto& [name,input]:values){
            if(list_fields.contains(name))list.emplace(name,input);
            if(name=="toolbarItems")toolbar.emplace("buttonItems",input);
            else if(name=="activeToolbarButtonId")toolbar.emplace("activeButtonId",input);
            else if(name=="iconSource"||name=="iconRevision")toolbar.emplace(name,input);
            if(name=="footerButton1"||name=="footerButton2"||name=="footerButton3")footer.emplace("button"+name.substr(name.size()-1),input);
            else if(name=="footerInteractive")footer.emplace("interactive",input);
            else if(name=="iconSource"||name=="iconRevision")footer.emplace(name,input);
        }
        std::string result="([&] { lvui::native::HierarchyPanelInput value; value.list = "+owned_hierarchy_list_options(list,initial,fallback)+
            "; value.toolbar = "+owned_hierarchy_toolbar_options(toolbar,initial)+"; value.footer = "+owned_list_footer_options(footer,initial)+"; ";
        for(const auto& [name,input]:values){
            const bool dynamic=native_expression(input->value);std::string field,expression;
            if(name=="width"||name=="height"||name=="toolbarHeight"||name=="footerHeight"){
                field=name=="toolbarHeight"?"toolbar_height":name=="footerHeight"?"footer_height":name;
                const double low=name=="width"||name=="height"?1.:0.,high=name=="width"||name=="height"?8192.:1024.;
                expression=dynamic?"lvui::native::number("+expression_code(input->value,input->position,initial)+")":decimal(numeric(values,name,0,low,high));
            }else if(name=="toolbarVisible"||name=="footerVisible"||name=="enabled"){
                field=name=="toolbarVisible"?"toolbar_visible":name=="footerVisible"?"footer_visible":"enabled";
                expression=dynamic?"lvui::native::truth("+expression_code(input->value,input->position,initial)+")":boolean(values,name,name!="footerVisible");
            }else if(name=="backgroundColor"){
                field="background";expression="lvui::native::hierarchy_panel_color("+color_expression(input->value,input->position,initial)+")";
            }else continue;
            result+="value.appearance."+field+" = "+expression+"; ";
        }
        return result+"return value; }())";
    }
    std::string owned_list_panel_options(const Inputs& values,bool initial,std::string_view fallback)const {
        Inputs toolbar,footer;
        for(const auto& [name,input]:values){
            if(name=="toolbarIcon1"||name=="toolbarIcon2"||name=="toolbarIcon3")toolbar.emplace("icon"+name.substr(name.size()-1),input);
            else if(name=="toolbarIconSize")toolbar.emplace("iconSize",input);
            else if(name=="toolbarInteractive")toolbar.emplace("interactive",input);
            else if(name=="iconSource"||name=="iconRevision")toolbar.emplace(name,input);
            if(name=="footerButton1"||name=="footerButton2"||name=="footerButton3")footer.emplace("button"+name.substr(name.size()-1),input);
            else if(name=="footerInteractive")footer.emplace("interactive",input);
            else if(name=="iconSource"||name=="iconRevision")footer.emplace(name,input);
        }
        std::string result="([&] { lvui::native::ListPanelInput value; value.toolbar = "+owned_list_toolbar_options(toolbar,initial)+
            "; value.footer = "+owned_list_footer_options(footer,initial)+"; ";
        if(auto item=values.find("controller");item!=values.end()){
            if(!native_expression(item->second->value))error(item->second->position,"List controller requires an LVUI ListController from a C++ backend");
            result+="value.list.controller = lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(item->second->value,item->second->position,initial)+"); ";
        }else result+="value.list.controller = "+std::string(fallback)+"; ";
        const std::map<std::string,std::string,std::less<>> list_numbers{{"rowHeight","row_height"},{"leftPadding","left_padding"},
            {"rightPadding","right_padding"},{"iconSize","icon_size"},{"spacing","spacing"},{"cornerRadius","corner_radius"}};
        const std::map<std::string,std::string,std::less<>> list_colors{{"textColor","text"},{"descriptionColor","description"},
            {"disabledTextColor","disabled_text"},{"rowBackgroundColor","idle"},{"hoverColor","hover"},{"pressedColor","pressed"},
            {"selectedRowColor","selected"},{"focusColor","focus"}};
        for(const auto& [name,input]:values){
            const bool dynamic=native_expression(input->value);std::string field,expression;
            if(auto found=list_numbers.find(name);found!=list_numbers.end()){
                field="list."+found->second;const auto low=name=="rowHeight"?18.:0.,high=name=="rowHeight"||name=="iconSize"?256.:8192.;
                expression=dynamic?"lvui::native::number("+expression_code(input->value,input->position,initial)+")":decimal(numeric(values,name,0,low,high));
            }else if(auto found=list_colors.find(name);found!=list_colors.end()){
                field="list.palette."+found->second;expression="lvui::native::list_view_color("+color_expression(input->value,input->position,initial)+")";
            }else if(name=="modelRevision"||name=="iconRevision"||name=="overscan"){
                field=name=="modelRevision"?"list.source_revision":name=="iconRevision"?"list.icon_revision":"list.overscan";
                const auto high=name=="overscan"?128.:9007199254740991.;
                if(dynamic)expression="lvui::native::integer(lvui::native::range("+expression_code(input->value,input->position,initial)+", 0, "+decimal(high)+"))";
                else{const auto n=numeric(values,name,0,0,high);if(std::trunc(n)!=n)error(input->position,"List integer input must be integral");expression="static_cast<std::int64_t>("+decimal(n)+")";}
                if(name=="overscan")expression="static_cast<std::size_t>("+expression+")";
            }else if(name=="iconSource"){
                if(!dynamic)error(input->position,"List iconSource requires a native IconSource");field="list.icon_source";
                expression="lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input->value,input->position,initial)+")";
            }else if(name=="showIndicator"||name=="keyboardNavigationEnabled"){
                field=name=="showIndicator"?"list.show_indicator":"list.keyboard_navigation";
                expression=dynamic?"lvui::native::truth("+expression_code(input->value,input->position,initial)+")":boolean(values,name,true);
            }else if(name=="width"||name=="height"||name=="toolbarHeight"||name=="footerHeight"){
                field="appearance."+std::string(name=="toolbarHeight"?"toolbar_height":name=="footerHeight"?"footer_height":name);
                const auto low=name=="width"||name=="height"?1.:0.,high=name=="width"||name=="height"?8192.:1024.;
                expression=dynamic?"lvui::native::number("+expression_code(input->value,input->position,initial)+")":decimal(numeric(values,name,0,low,high));
            }else if(name=="toolbarVisible"||name=="footerVisible"||name=="enabled"||name=="interactive"){
                field="appearance."+std::string(name=="toolbarVisible"?"toolbar_visible":name=="footerVisible"?"footer_visible":name);
                expression=dynamic?"lvui::native::truth("+expression_code(input->value,input->position,initial)+")":boolean(values,name,name!="toolbarVisible");
            }else if(name=="backgroundColor"){
                field="appearance.background";expression="lvui::native::list_panel_color("+color_expression(input->value,input->position,initial)+")";
            }else continue;
            result+="value."+field+" = "+expression+"; ";
        }
        return result+"return value; }())";
    }
    std::string hierarchy_item_options(const Inputs& values,bool initial,bool generated=false,std::string inherited={}){
        const auto label=alias_key(values,"text","label"),active=alias_key(values,"activatable","selectable");
        std::map<std::string,std::string,std::less<>> strings{{label,"row.label"},{"itemKey","row.key"},{"iconName","row.icon_name"},{"iconGlyph","row.icon_glyph"}};
        std::map<std::string,std::string,std::less<>> flags{{"enabled","row.enabled"},{active,"row.activatable"},{"selected","row.selected"},{"expanded","row.expanded"},{"showChevron","row.show_chevron"},{"hasChildItems","row.has_child_items"},{"dragAllowed","row.draggable"},{"dragPreviewActive","drag_preview"}};
        std::map<std::string,std::string,std::less<>> numbers{{"indentStep","indent_step"},{"iconSize","icon_size"},{"chevronSize","chevron_size"},{"baseLeftPadding","left_padding"},{"rowRightPadding","right_padding"},{"leadingSpacing","spacing"},{"cornerRadius","corner_radius"},{"countWidth","count_width"},{"dragPreviewOpacity","drag_opacity"}};
        if(generated){numbers[alias_key(values,"generatedRowHeight","rowHeight")]="row_height";numbers[alias_key(values,"generatedIndentStep","indentStep")]="indent_step";numbers[alias_key(values,"generatedIconSize","iconSize")]="icon_size";numbers[alias_key(values,"generatedChevronSize","chevronSize")]="chevron_size";numbers["generatedItemWidth"]="width";}
        else{numbers[alias_key(values,"height","rowHeight")]="row_height";numbers[alias_key(values,"width","itemWidth")]="width";}
        const std::map<std::string,std::string,std::less<>> colors{{"textColorNormal","text"},{"textColorDisabled","disabled_text"},{"rowBackgroundColorIdle","idle"},{"rowBackgroundColorHover","hover"},{"rowBackgroundColorPressed","pressed"},{"rowBackgroundColorActive","active"},{"rowBackgroundColorInactive","inactive"},{"rowBackgroundColorDrag","drag"}};
        std::string result="([&] { ";
        if(inherited.empty())result+="lvrs::HierarchyItemOptions value; ";
        else result+="auto inherited = "+inherited+".options(); if (!inherited) { if (inherited.error().cause) std::rethrow_exception(inherited.error().cause); throw std::runtime_error(inherited.error().message); } auto value = std::move(*inherited); ";
        for(const auto& [name,input]:values){if(name=="countView"||name=="itemDelegate")continue;const bool dynamic=native_expression(input->value);std::string field,expression;
            if(auto found=strings.find(name);found!=strings.end()){field=found->second;expression=dynamic?"lvui::native::text("+expression_code(input->value,input->position,initial)+")":literal(text(values,name));}
            else if(auto found=flags.find(name);found!=flags.end()){field=found->second;expression=dynamic?"lvui::native::truth("+expression_code(input->value,input->position,initial)+")":boolean(values,name,false);}
            else if(auto found=numbers.find(name);found!=numbers.end()){
                field=found->second;const auto high=field=="drag_opacity"?1:field=="icon_size"||field=="chevron_size"?256:8192;const auto low=field=="row_height"||field=="width"?.000001:0;
                expression=dynamic?"lvui::native::range("+expression_code(input->value,input->position,initial)+", "+decimal(low)+", "+decimal(high)+")":decimal(numeric(values,name,0,low,high));
            }else if(auto found=colors.find(name);found!=colors.end()){field=found->second;expression=dynamic?color_expression(input->value,input->position,initial):widget_color(values,name,"{}");}
            else if(name=="itemId"||name=="count"||name=="indentLevel"){
                field=name=="itemId"?"row.item_id":name=="count"?"row.count":"row.depth";
                if(dynamic)expression="lvui::native::integer("+expression_code(input->value,input->position,initial)+")";
                else{const auto n=numeric(values,name,0,name=="indentLevel"?0:-9007199254740991.,name=="indentLevel"?256:9007199254740991.);if(std::trunc(n)!=n)error(input->position,"Hierarchy integer input must be integral");expression="static_cast<std::int64_t>("+decimal(n)+")";}
                if(name=="indentLevel")expression="static_cast<std::uint32_t>(lvui::native::range("+expression+", 0, 256))";
            }else if(name=="controller"&&!generated){field="controller";if(!dynamic)error(input->position,"Hierarchy controller requires a native C++ handle");expression=expression_code(input->value,input->position,initial);}
            else continue;
            result+="value."+field+" = "+expression+"; ";
        }
        return result+"return value; }())";
    }
    std::string hierarchy_list_options(const Inputs& values,bool initial){
        const auto source=alias_key(values,"model","treeModel");if(values.contains(source)&&values.contains("controller"))error(values.at("controller")->position,"Use HierarchyList model or controller, not both");
        std::string result="([&] { lvrs::HierarchyListOptions value; value.item = "+hierarchy_item_options(values,initial,true)+"; ";
        for(auto name:{source,std::string("controller")})if(auto found=values.find(name);found!=values.end()){if(!native_expression(found->second->value))error(found->second->position,"Hierarchy source requires a native C++ handle");result+="value."+std::string(name=="controller"?"controller":"model")+" = "+expression_code(found->second->value,found->second->position,initial)+"; ";}
        const std::map<std::string,std::string,std::less<>> flags{{"enabled","enabled"},{"editable","editable"},{"keyboardNavigationEnabled","keyboard_navigation"},{"autoExpandAncestorsOnActivate","auto_expand_ancestors"}};
        for(const auto& [name,field]:flags)if(auto input=values.find(name);input!=values.end())result+="value."+field+" = "+(native_expression(input->second->value)?"lvui::native::truth("+expression_code(input->second->value,input->second->position,initial)+")":boolean(values,name,false))+"; ";
        for(auto name:{"width","height"})if(auto input=values.find(name);input!=values.end())result+="value."+std::string(name)+" = "+(native_expression(input->second->value)?"lvui::native::range("+expression_code(input->second->value,input->second->position,initial)+", 0.000001, 8192)":decimal(numeric(values,name,0,.000001,8192)))+"; ";
        if(values.contains("generatedRowHeight")||values.contains("rowHeight"))result+="value.row_extent = value.item.row_height; ";
        return result+"return value; }())";
    }
    std::string progress_options(const Inputs& values,bool initial){
        const auto maximum=alias_key(values,"maximumValue","endValue");
        const std::map<std::string,std::string,std::less<>> fields{
            {"minimumValue","values.minimum"},{maximum,"values.maximum"},{"startValue","values.start"},{"currentValue","values.current"},
            {"width","width"},{"height","height"},{"largeHeight","large_height"},{"regularHeight","regular_height"},{"cornerRadius","corner_radius"},
            {"displayedStart","displayed_start"},{"displayedProgress","displayed_progress"},
            {"minimumValueStateKey","state_keys.minimum"},{"maximumValueStateKey","state_keys.maximum"},{"startValueStateKey","state_keys.start"},{"currentValueStateKey","state_keys.current"},
            {"trackColor","track_color"},{"fillColor","fill_color"},{"motionEnabled","motion_enabled"},{"stateModel","state_model"},{"motion","motion"},{"size","size"},{"shapeStyle","shape"}};
        std::string result="([&] { lvrs::ProgressBarOptions value; ";
        for(const auto& [name,input]:values){
            auto field=fields.find(name);if(field==fields.end())continue;
            const bool dynamic=native_expression(input->value);
            std::string expression;
            if(name=="stateModel"){
                const auto source=input->value.kind==ValueKind::reference?backend_access.find(input->value.text):backend_access.end();
                if(source!=backend_access.end())expression=source->second.object;
                else if(dynamic)expression=expression_code(input->value,input->position,initial);
                else error(input->position,"ProgressBar stateModel requires a scoped StateSource or shared/weak source handle");
                expression="lvrs::progress_source(scope, "+expression+")";
            }else if(name=="motion"){
                if(!dynamic)error(input->position,"ProgressBar motion requires a native shared Motion handle");
                expression=expression_code(input->value,input->position,initial);
            }else if(name=="size"||name=="shapeStyle"){
                const bool size=name=="size";
                if(dynamic)expression=std::string("lvrs::progress_")+(size?"size(":"shape(")+expression_code(input->value,input->position,initial)+")";
                else{
                    const auto value=resolve(input->value,input->position);int index=-1;
                    if(value.kind==ValueKind::enumeration){
                        if(value.text==(size?".large":".shapeRoundRect"))index=0;
                        if(value.text==(size?".regular":".shapeCylinder"))index=1;
                    }else if(value.kind==ValueKind::number&&(value.text=="0"||value.text=="1"))index=value.text=="0"?0:1;
                    if(index<0)error(input->position,std::string("Unknown ProgressBar ")+name+" value");
                    expression=size?(index?"lvrs::ProgressSize::regular":"lvrs::ProgressSize::large"):(index?"lvrs::ProgressShape::cylinder":"lvrs::ProgressShape::round_rect");
                }
            }else if(name=="motionEnabled")expression=dynamic?"lvui::native::truth("+expression_code(input->value,input->position,initial)+")":boolean(values,name,true);
            else if(name=="trackColor"||name=="fillColor")expression=dynamic?color_expression(input->value,input->position,initial):widget_color(values,name,"{}");
            else if(name.ends_with("StateKey"))expression=dynamic?"lvui::native::text("+expression_code(input->value,input->position,initial)+")":literal(text(values,name));
            else{
                const bool metric=name=="width"||name=="height"||name=="largeHeight"||name=="regularHeight"||name=="cornerRadius";
                expression=dynamic?std::string(metric?"lvui::native::range(":"lvui::native::number(")+expression_code(input->value,input->position,initial)+(metric?", 0, 8192)":")"):
                    decimal(numeric(values,name,0,metric?0:-std::numeric_limits<double>::max(),metric?8192:std::numeric_limits<double>::max()));
            }
            result+="value."+field->second+" = "+expression+"; ";
        }
        return result+"return value; }())";
    }
    std::string owned_progress_options(const Inputs& values) {
        const auto maximum=alias_key(values,"maximumValue","endValue");
        const std::map<std::string,std::string,std::less<>> fields{
            {"minimumValue","values.minimum"},{maximum,"values.maximum"},{"startValue","values.start"},
            {"currentValue","values.current"},{"width","style.width"},{"height","style.height"},
            {"largeHeight","style.large_height"},{"regularHeight","style.regular_height"},
            {"cornerRadius","style.corner_radius"},{"displayedStart","style.displayed_start"},
            {"displayedProgress","style.displayed_progress"},{"trackColor","style.track"},
            {"fillColor","style.fill"},{"size","style.size"},{"shapeStyle","style.shape"},
            {"motionEnabled","motion_enabled"},{"motion","motion"}};
        const auto color_code=[](StyleColor c) {
            return "lvui::StyleColor{"+decimal(c.red)+", "+decimal(c.green)+", "+decimal(c.blue)+", "+decimal(c.alpha)+"}";
        };
        std::string result="([&] { lvui::ProgressConfiguration value; value.style.fill = "+
                           color_code(token_color("--lvrs-theme-colors-action-primary"))+"; ";
        for(const auto& [name,input]:values){
            auto field=fields.find(name); if(field==fields.end())continue;
            const bool dynamic=native_expression(input->value);
            std::string expression;
            if(name=="motion") {
                const bool imported=input->value.kind==ValueKind::reference && backend_access.contains(input->value.text);
                if(!dynamic&&!imported)error(input->position,"ProgressBar motion requires a native LVUI Motion value");
                expression="lvui::motion_value("+expression_code(input->value,input->position)+")";
            } else if(name=="motionEnabled") {
                expression=dynamic?"lvui::native::truth("+expression_code(input->value,input->position)+")":boolean(values,name,true);
            } else if(name=="size"||name=="shapeStyle") {
                const bool size=name=="size";
                if(dynamic) expression=std::string("lvui::progress_")+(size?"size(":"shape(")+expression_code(input->value,input->position)+")";
                else {
                    const auto v=resolve(input->value,input->position);
                    int selected=-1;
                    if(v.kind==ValueKind::enumeration) {
                        if(v.text==(size?".large":".shapeRoundRect"))selected=0;
                        if(v.text==(size?".regular":".shapeCylinder"))selected=1;
                    } else if(v.kind==ValueKind::number&&(v.text=="0"||v.text=="1"))selected=v.text=="0"?0:1;
                    if(selected<0)error(input->position,"Unknown ProgressBar "+name+" value");
                    expression=size?(selected?"lvui::ProgressSize::regular":"lvui::ProgressSize::large"):
                        (selected?"lvui::ProgressShape::cylinder":"lvui::ProgressShape::round_rect");
                }
            } else if(name=="trackColor"||name=="fillColor") {
                expression=dynamic?"lvui::native::color<lvui::StyleColor>("+color_expression(input->value,input->position)+")":
                                   color_code(color_value(input->value,input->position));
            } else {
                const bool metric=name=="width"||name=="height"||name=="largeHeight"||name=="regularHeight"||name=="cornerRadius";
                const auto low=metric?(name=="width"||name=="height"?1.:0.):-1e12;
                const auto high=metric?8192.:1e12;
                expression=dynamic?"lvui::native::range("+expression_code(input->value,input->position)+", "+decimal(low)+", "+decimal(high)+")":
                                   decimal(numeric(values,name,0,low,high));
            }
            result+="value."+field->second+" = "+expression+"; ";
        }
        if(auto source=values.find("stateModel");source!=values.end()) {
            const auto& supplied=source->second->value;
            const bool imported=supplied.kind==ValueKind::reference && backend_access.contains(supplied.text);
            if(!imported && !native_expression(supplied))
                error(source->second->position,"ProgressBar stateModel requires a native LVUI state snapshot or source");
            result+="lvui::ProgressStateKeys keys; ";
            for(const auto& [name,field]:std::map<std::string,std::string,std::less<>>{
                    {"minimumValueStateKey","minimum"},{"maximumValueStateKey","maximum"},
                    {"startValueStateKey","start"},{"currentValueStateKey","current"}}) {
                if(auto input=values.find(name);input!=values.end()) {
                    const auto expression=native_expression(input->second->value)
                        ? "lvui::native::text("+expression_code(input->second->value,input->second->position)+")"
                        : literal(text(values,name));
                    result+="keys."+field+" = "+expression+"; ";
                }
            }
            result+="value.values = lvui::progress_values(value.values, keys, "+
                    expression_code(source->second->value,source->second->position)+"); ";
        } else {
            for(auto name:{"minimumValueStateKey","maximumValueStateKey","startValueStateKey","currentValueStateKey"})
                if(auto key=values.find(name);key!=values.end())
                    error(key->second->position,"ProgressBar state keys require stateModel");
        }
        return result+"return value; }())";
    }
    void emit_progress_bindings(const Inputs& values,const std::string& native){
        if(!std::ranges::any_of(values,[&](const auto& pair){return native_expression(pair.second->value);}))return;
        body+="            { auto binding = native_bindings->bind([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+progress_options(values,false)+"; }, [component = *"+native+"](const lvrs::ProgressBarOptions& value) { return component.set_options(value); });\n              if (!binding) return std::unexpected(binding.error()); }\n";
    }
    std::string owned_segmented_options(const Inputs& values,bool initial,bool icon)const {
        const auto dynamic=[&](std::string_view name){auto item=values.find(name);return item!=values.end()&&native_expression(item->second->value);};
        std::string setup=icon?"value.vertical_padding = 4; ":"";
        for(const auto& [field,key]:std::map<std::string,std::string,std::less<>>{{"background","--lvrs-theme-colors-panel-surface-panel"},{"border","--lvrs-theme-colors-panel-surface-raised"}}){
            const auto color=token_color(key);setup+="value."+field+" = lvui::StyleColor{"+decimal(color.red)+", "+decimal(color.green)+", "+decimal(color.blue)+", "+decimal(color.alpha)+"}; ";
        }
        for(const auto& [name,field]:std::map<std::string,std::string,std::less<>>{{"enabled","enabled"},{"forceBorderlessTone","force_borderless_tone"}})
            if(auto item=values.find(name);item!=values.end())setup+="value."+field+" = "+(dynamic(name)?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(values,name,true))+"; ";
        if(auto item=values.find("shapeStyle");item!=values.end()){
            std::string shape;
            if(dynamic("shapeStyle"))shape="lvui::native::segmented_shape("+expression_code(item->second->value,item->second->position,initial)+")";
            else{const auto& supplied=item->second->value;
                if(supplied.kind!=ValueKind::enumeration)error(item->second->position,"Unknown segmented shapeStyle");
                if(supplied.text==".roundRect"||supplied.text==".shapeRoundRect")shape="lvui::SegmentedShape::round_rect";
                else if(supplied.text==".cylinder"||supplied.text==".shapeCylinder")shape="lvui::SegmentedShape::cylinder";
                else error(item->second->position,"Unknown segmented shapeStyle");}
            setup+="value.shape = "+shape+"; ";
        }
        const auto number=[&](std::string_view name,std::string_view field,double high){auto item=values.find(name);if(item==values.end())return;
            const auto value=dynamic(name)?"lvui::native::number(lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(item->second->value,item->second->position,initial)+"))":decimal(numeric(values,name,0,0,high));
            setup+="value."+std::string(field)+" = "+value+"; ";};
        number("horizontalPadding","horizontal_padding",8192);number("verticalPadding","vertical_padding",8192);
        number("spacing","spacing",32767);number("borderWidth","border_width",8192);number("cornerRadius","corner_radius",8192);
        for(const auto& [name,field]:std::map<std::string,std::string,std::less<>>{{"backgroundColor","background"},{"borderColor","border"}})
            if(auto item=values.find(name);item!=values.end())setup+="value."+field+" = lvui::native::segmented_color("+color_expression(item->second->value,item->second->position,initial)+"); ";
        return "([&] { lvui::SegmentedControlConfiguration value; "+setup+"return value; }())";
    }
    std::string owned_combo_box_options(const Inputs& values,bool initial)const {
        const auto dynamic=[&](std::string_view name){auto item=values.find(name);return item!=values.end()&&native_expression(item->second->value);};
        std::string setup;
        for(const auto& [field,key]:std::map<std::string,std::string,std::less<>>{
                {"surface","--lvrs-theme-colors-panel-surface-input"},{"hover","--lvrs-theme-colors-panel-surface-input-hover"},
                {"pressed","--lvrs-theme-colors-panel-surface-raised"},{"disabled","--lvrs-theme-colors-panel-sub-surface"},
                {"focus","--lvrs-theme-colors-action-primary"}}){
            const auto color=token_color(key);setup+="value.palette."+field+" = lvui::StyleColor{"+decimal(color.red)+", "+decimal(color.green)+", "+decimal(color.blue)+", "+decimal(color.alpha)+"}; ";
        }
        for(const auto& [name,field]:std::map<std::string,std::string,std::less<>>{{"text","text"},{"accessibilityName","accessibility_name"}})
            if(auto item=values.find(name);item!=values.end())setup+="value."+field+" = "+(dynamic(name)?"lvui::native::text("+expression_code(item->second->value,item->second->position,initial)+")":literal(text(values,name)))+"; ";
        if(auto item=values.find("enabled");item!=values.end())setup+="value.enabled = "+(dynamic("enabled")?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(values,"enabled",true))+"; ";
        if(auto item=values.find("arrow");item!=values.end())setup+="value.arrow = "+(dynamic("arrow")?"lvui::native::combo_box_arrow("+expression_code(item->second->value,item->second->position,initial)+")":"lvui::StepperArrow::"+stepper_arrow(values))+"; ";
        if(auto item=values.find("tone");item!=values.end()){
            std::string tone;
            if(dynamic("tone"))tone="lvui::native::combo_box_tone("+expression_code(item->second->value,item->second->position,initial)+")";
            else{const std::map<std::string,std::string,std::less<>> tones{{".primary","primary"},{".Primary","primary"},{".borderless","borderless"},{".Borderless","borderless"}};
                const auto selected=tones.find(item->second->value.text);if(item->second->value.kind!=ValueKind::enumeration||selected==tones.end())error(item->second->position,"Unknown ComboBox tone");tone="lvui::ButtonTone::"+selected->second;}
            setup+="value.tone = "+tone+"; ";
        }
        const auto number=[&](std::string_view name,std::string_view field,double low,double high){auto item=values.find(name);if(item==values.end())return;
            const auto value=dynamic(name)?"lvui::native::number(lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(item->second->value,item->second->position,initial)+"))":decimal(numeric(values,name,0,low,high));
            setup+="value."+std::string(field)+" = "+value+"; ";};
        number("width","width",19,8192);number("height","height",1,8192);number("leftPadding","left_padding",0,8192);
        number("rightPadding","right_padding",0,8192);number("indicatorSize","indicator_size",1,256);number("cornerRadius","corner_radius",0,8192);
        number("fontSize","font_size",1,256);number("lineHeight","line_height",1,8192);
        for(const auto& [name,field]:std::map<std::string,std::string,std::less<>>{{"backgroundColor","surface"},{"hoverColor","hover"},{"pressedColor","pressed"},{"disabledColor","disabled"},{"textColor","text"},{"disabledTextColor","disabled_text"},{"focusColor","focus"}})
            if(auto item=values.find(name);item!=values.end())setup+="value.palette."+field+" = lvui::native::combo_box_color("+color_expression(item->second->value,item->second->position,initial)+"); ";
        return "([&] { lvui::ComboBoxConfiguration value; "+setup+"return value; }())";
    }
    std::string owned_stepper_options(const Inputs& values,bool initial)const {
        const auto dynamic=[&](std::string_view name){auto item=values.find(name);return item!=values.end()&&native_expression(item->second->value);};
        std::string setup;
        for(const auto& [field,key]:std::map<std::string,std::string,std::less<>>{{"primary","--lvrs-theme-colors-action-primary"},{"primary_hover","--lvrs-theme-colors-accent-blue-hover"},{"primary_pressed","--lvrs-theme-colors-accent-blue-pressed"},{"normal","--lvrs-theme-colors-panel-surface-raised"},{"normal_hover","--lvrs-theme-colors-panel-surface-alt"},{"normal_pressed","--lvrs-theme-colors-accent-blue-subtle"},{"danger","--lvrs-theme-colors-action-danger"},{"disabled_background","--lvrs-theme-colors-panel-sub-surface"}}){
            const auto color=token_color(key);setup+="value.palette."+field+" = lvui::StyleColor{"+decimal(color.red)+", "+decimal(color.green)+", "+decimal(color.blue)+", "+decimal(color.alpha)+"}; ";
        }
        if(auto item=values.find("arrow");item!=values.end())setup+="value.arrow = "+(dynamic("arrow")?"lvui::native::stepper_arrow("+expression_code(item->second->value,item->second->position,initial)+")":"lvui::StepperArrow::"+stepper_arrow(values))+"; ";
        if(auto item=values.find("tone");item!=values.end()){
            std::string tone;
            if(dynamic("tone"))tone="lvui::native::stepper_tone("+expression_code(item->second->value,item->second->position,initial)+")";
            else{const std::map<std::string,std::string,std::less<>> tones{{".primary","primary"},{".Primary","primary"},{".borderless","borderless"},{".Borderless","borderless"},{".default","normal"},{".destructive","destructive"},{".disabled","disabled"}};
                const auto selected=tones.find(item->second->value.text);if(item->second->value.kind!=ValueKind::enumeration||selected==tones.end())error(item->second->position,"Unknown Stepper tone");tone="lvui::ButtonTone::"+selected->second;}
            setup+="value.tone = "+tone+"; ";
        }
        if(auto item=values.find("enabled");item!=values.end())setup+="value.enabled = "+(dynamic("enabled")?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(values,"enabled",true))+"; ";
        if(auto item=values.find("accessibilityName");item!=values.end())setup+="value.accessibility_name = "+(dynamic("accessibilityName")?"lvui::native::text("+expression_code(item->second->value,item->second->position,initial)+")":literal(text(values,"accessibilityName")))+"; ";
        const auto number=[&](std::string_view name,std::string_view field,double low,double high){auto item=values.find(name);if(item==values.end())return;
            const auto value=dynamic(name)?"lvui::native::number(lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(item->second->value,item->second->position,initial)+"))":decimal(numeric(values,name,0,low,high));
            setup+="value."+std::string(field)+" = "+value+"; ";};
        number("width","width",1,8192);number("height","height",1,8192);number("cornerRadius","corner_radius",0,8192);
        if(auto item=values.find("primaryColor");item!=values.end())setup+="value.palette.primary = lvui::native::stepper_color("+color_expression(item->second->value,item->second->position,initial)+"); ";
        for(const auto& [name,field]:std::map<std::string,std::string,std::less<>>{{"hoverColor","hover_color"},{"pressedColor","pressed_color"},{"disabledColor","disabled_color"},{"textColor","text_color"},{"disabledTextColor","disabled_text_color"}})
            if(auto item=values.find(name);item!=values.end())setup+="value."+field+" = lvui::native::stepper_color("+color_expression(item->second->value,item->second->position,initial)+"); ";
        return "([&] { lvui::StepperConfiguration value; "+setup+"return value; }())";
    }
    std::string owned_input_field_options(const Inputs& values,bool initial)const {
        const auto dynamic=[&](std::string_view name){auto item=values.find(name);return item!=values.end()&&native_expression(item->second->value);};
        const auto primary=token_color("--lvrs-theme-colors-action-primary");
        std::string setup="value.palette.focus = lvui::StyleColor{"+decimal(primary.red)+", "+decimal(primary.green)+", "+decimal(primary.blue)+", "+decimal(primary.alpha)+"}; value.palette.selection = value.palette.focus; value.palette.selection.alpha *= .38; ";
        for(const auto& [name,field]:std::map<std::string,std::string,std::less<>>{{"text","text"},{"placeholder","placeholder"},{"accessibilityName","accessibility_name"},{"fontFamily","font_family"}})
            if(auto item=values.find(name);item!=values.end())setup+="value."+field+" = "+(dynamic(name)?"lvui::native::text("+expression_code(item->second->value,item->second->position,initial)+")":literal(text(values,name)))+"; ";
        for(const auto& [name,field]:std::map<std::string,std::string,std::less<>>{{"enabled","enabled"},{"readOnly","read_only"},{"multiline","multiline"},{"password","password"}})
            if(auto item=values.find(name);item!=values.end())setup+="value."+field+" = "+(dynamic(name)?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(values,name,name=="enabled"))+"; ";
        const auto number=[&](std::string_view name,std::string_view field,double low,double high){
            auto item=values.find(name);if(item==values.end())return;
            const auto value=dynamic(name)?"lvui::native::number(lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(item->second->value,item->second->position,initial)+"))":decimal(numeric(values,name,0,low,high));
            setup+="value."+std::string(field)+" = "+value+"; ";
        };
        number("width","width",1,8192);number("height","height",1,8192);
        number("horizontalPadding","horizontal_padding",0,8192);number("verticalPadding","vertical_padding",0,8192);
        number("cornerRadius","corner_radius",0,8192);number("fontSize","font_size",1,256);number("lineHeight","line_height",0,1024);
        for(const auto& [name,field]:std::map<std::string,std::string,std::less<>>{{"backgroundColor","background"},{"hoverColor","hover"},{"textColor","text"},{"placeholderColor","placeholder"},{"selectionColor","selection"},{"caretColor","caret"},{"focusColor","focus"}})
            if(auto item=values.find(name);item!=values.end())setup+="value.palette."+field+" = lvui::native::input_field_color("+color_expression(item->second->value,item->second->position,initial)+"); ";
        return "([&] { lvui::InputFieldConfiguration value; "+setup+"return value; }())";
    }
    std::string owned_slider_options(const Inputs& values,bool initial,SourcePosition position)const {
        const auto dynamic=[&](std::string_view name){auto item=values.find(name);return item!=values.end()&&native_expression(item->second->value);};
        if(!dynamic("from")&&!dynamic("to")&&numeric(values,"from",0,-1e12,1e12)>=numeric(values,"to",1,-1e12,1e12))
            error(position,"Slider from must be smaller than to");
        if(!dynamic("width")&&!dynamic("height")&&!dynamic("thumbDiameter")&&!dynamic("trackHeight")){
            const auto width=numeric(values,"width",320,18,8192),height=numeric(values,"height",32,1,8192);
            const auto thumb=numeric(values,"thumbDiameter",18,1,256),track=numeric(values,"trackHeight",6,1,256);
            if(width<thumb||height<thumb||height<track)error(position,"Slider dimensions must contain the thumb and track");
        }
        const auto primary=token_color("--lvrs-theme-colors-action-primary");
        std::string setup="value.palette.fill = lvui::StyleColor{"+decimal(primary.red)+", "+decimal(primary.green)+", "+decimal(primary.blue)+", "+decimal(primary.alpha)+"}; value.palette.focus = value.palette.fill; ";
        const auto assign_number=[&](std::string_view name,std::string_view field,double low,double high){
            auto item=values.find(name);if(item==values.end())return;
            const auto expression=dynamic(name)?"lvui::native::number(lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(item->second->value,item->second->position,initial)+"))":decimal(numeric(values,name,0,low,high));
            setup+="value."+std::string(field)+" = "+expression+"; ";
        };
        for(const auto& [name,field]:std::map<std::string,std::string,std::less<>>{{"from","minimum"},{"to","maximum"},{"value","value"}})
            assign_number(name,field,-1e12,1e12);
        assign_number("stepSize","step",0,32767);assign_number("width","width",18,8192);
        assign_number("height","height",1,8192);assign_number("thumbDiameter","thumb_diameter",1,256);
        assign_number("trackHeight","track_height",1,256);assign_number("rangeInset","range_inset",0,128);
        if(auto item=values.find("enabled");item!=values.end())setup+="value.enabled = "+(dynamic("enabled")?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(values,"enabled",true))+"; ";
        if(auto item=values.find("accessibilityName");item!=values.end())setup+="value.accessibility_name = "+(dynamic("accessibilityName")?"lvui::native::text("+expression_code(item->second->value,item->second->position,initial)+")":literal(text(values,"accessibilityName")))+"; ";
        for(const auto& [name,field]:std::map<std::string,std::string,std::less<>>{{"trackColor","track"},{"fillColor","fill"},{"thumbColor","thumb"},{"focusColor","focus"}})
            if(auto item=values.find(name);item!=values.end())setup+="value.palette."+field+" = lvui::native::slider_color("+color_expression(item->second->value,item->second->position,initial)+"); ";
        return "([&] { lvui::SliderConfiguration value; "+setup+"return value; }())";
    }
    void validate_owned_tooltip(const Inputs& values,const View& view)const {
        if(values.contains("text")&&!view.children.empty())error(view.position,"Use Tooltip text or inline content, not both");
        for(const auto name:{"target","availableRect","scrollContent","motionEnabled","motion","animationDuration"})
            if(auto item=values.find(name);item!=values.end())error(item->second->position,"This Tooltip input is not implemented by the independent runtime yet");
        for(const auto& [name,input]:values){
            if(name=="text"||name=="onOpened"||name=="onClosed"||name=="visible"||name=="opacity"||name=="flex")continue;
            if(native_expression(input->value))error(input->position,"This Tooltip input does not support reactive updates yet");
        }
    }
    std::string owned_tooltip_configuration(const Inputs& values)const {
        std::string setup;
        for(const auto& [name,field]:tooltip_numbers){
            if(name=="surfaceOpacity"||!values.contains(name))continue;
            setup+="value."+field+" = "+decimal(numeric(values,name,0,name=="maximumWidth"||name=="maximumHeight"?1:0,8192))+"; ";
        }
        if(values.contains("automatic"))setup+="value.automatic = "+boolean(values,"automatic",true)+"; ";
        for(const auto& [name,field]:tooltip_durations){
            if(name=="animationDuration"||!values.contains(name))continue;
            const auto duration=numeric(values,name,0,0,86400000);
            if(std::trunc(duration)!=duration)error(values.at(name)->position,"Tooltip duration must be whole milliseconds");
            setup+="value."+field+" = std::chrono::milliseconds{"+std::to_string(static_cast<std::int64_t>(duration))+"}; ";
        }
        if(auto item=values.find("preferredPlacement");item!=values.end()){
            const auto choice=resolve(item->second->value,item->second->position);
            const std::map<std::string,std::string,std::less<>> choices{{".Automatic","automatic"},{".Above","above"},{".Below","below"},{".LeftSide","left"},{".RightSide","right"}};
            auto found=choices.find(choice.text);if(choice.kind!=ValueKind::enumeration||found==choices.end())error(item->second->position,"Unknown independent Tooltip placement");
            setup+="value.preferred = lvui::TooltipPlacement::"+found->second+"; ";
        }
        return "([&] { lvui::TooltipConfiguration value; "+setup+"return value; }())";
    }
    std::string owned_tooltip_style(const Inputs& values)const {
        std::string setup;
        if(values.contains("contentPadding"))setup+="value.content_padding = "+decimal(numeric(values,"contentPadding",12,0,8192))+"; ";
        if(values.contains("surfaceOpacity"))setup+="value.material.tint_opacity = "+decimal(numeric(values,"surfaceOpacity",1,0,1))+"; ";
        if(values.contains("accessibilityName"))setup+="value.accessibility_name = "+literal(text(values,"accessibilityName"))+"; ";
        for(const auto [name,field]:{std::pair{"surfaceColor","color"},{"borderColor","border_color"},{"primaryColor","primary_color"}})
            if(auto item=values.find(name);item!=values.end())setup+="value.material."+std::string(field)+" = lvui::native::material_color("+color_expression(item->second->value,item->second->position,true)+"); ";
        if(auto item=values.find("backdrop");item!=values.end()){
            const auto choice=resolve(item->second->value,item->second->position);
            const std::map<std::string,std::string,std::less<>> choices{{".None","none"},{".Content","content"},{".PreviousLayers","previous_layers"}};
            auto found=choices.find(choice.text);if(choice.kind!=ValueKind::enumeration||found==choices.end())error(item->second->position,"Unknown independent Tooltip backdrop");
            setup+="value.material.backdrop = lvui::MaterialBackdrop::"+found->second+"; ";
        }
        return "([&] { lvui::native::TooltipPresentationStyle value; "+setup+"return value; }())";
    }
    std::string owned_tooltip_target(const Inputs& values)const {
        if(!values.contains("anchorX")&&!values.contains("anchorY"))return "lvui::native::TooltipTargetOptions{}";
        return "lvui::native::TooltipTargetOptions{.anchor = lvui::TooltipPoint{"+
            decimal(numeric(values,"anchorX",0,-8192,8192))+", "+decimal(numeric(values,"anchorY",0,-8192,8192))+"}}";
    }
    std::string tooltip_call(const std::string& args,const Inputs& v,const View& view){
        if(v.contains("target")&&v.contains("targetId"))error(view.position,"Use Tooltip target or targetId");
        std::string setup;
        for(auto name:{"text","accessibilityName"})if(v.contains(name))setup+="value."+std::string(name==std::string_view("text")?"text":"accessibility_name")+" = "+link_text(v,name)+"; ";
        for(const auto& [name,field]:tooltip_numbers)if(v.contains(name))setup+="value."+field+" = "+surface_number(v,name,"0")+"; ";
        for(const auto& [name,field]:tooltip_booleans)if(v.contains(name))setup+="value."+field+" = "+link_bool(v,name,false)+"; ";
        for(const auto& [name,field]:tooltip_colors)if(v.contains(name))setup+="value."+field+" = "+surface_color(v,name,"{}")+"; ";
        for(const auto& [name,field]:tooltip_durations)if(v.contains(name)){
            auto expression=property(v.at(name)->value).empty()?decimal(numeric(v,name,0,0,86400000)):link_value(v,name,"0");
            setup+="{ auto duration = lvrs::tooltip_milliseconds("+expression+"); if (!duration) return std::unexpected(duration.error()); value."+field+" = *duration; } ";
        }
        for(auto [name,field]:{std::pair{"motion","motion"},{"target","target"},{"availableRect","available_rect"}})if(v.contains(name))setup+="value."+std::string(field)+" = "+link_value(v,name,"{}")+"; ";
        if(v.contains("anchorX")||v.contains("anchorY"))setup+="value.anchor_point = lvrs::TooltipPoint{"+surface_number(v,"anchorX","0")+", "+surface_number(v,"anchorY","0")+"}; ";
        if(v.contains("preferredPlacement"))setup+="value.preferred_placement = "+(property(v.at("preferredPlacement")->value).empty()?media_enum(v,"preferredPlacement","TooltipPlacement","automatic",{{"Automatic","automatic"},{"Above","above"},{"Below","below"},{"LeftSide","left_side"},{"RightSide","right_side"}}):link_value(v,"preferredPlacement","{}"))+"; ";
        if(v.contains("backdrop"))setup+="value.backdrop = "+media_enum(v,"backdrop","MaterialBackdrop","content",{{"None","none"},{"Content","content"},{"PreviousLayers","previous_layers"}})+"; ";
        return "([&]() -> ii::core::Result<lvrs::Tooltip> { lvrs::TooltipOptions value; "+setup+"return lvrs::Tooltip::create(widgets, "+args+", std::move(value), {.opened = "+action(v,"onOpened")+", .closed = "+action(v,"onClosed")+"}, "+modal_content(view,"TooltipContentFactory",false)+"); }())";
    }
    void emit_tooltip_bindings(const Inputs& v,const std::string& variable,const std::string& native){
        for(const auto& [name,input]:v){if(!native_expression(input->value))continue;std::string update;
            if(name=="text"||name=="accessibilityName")update="options->"+std::string(name=="text"?"text":"accessibility_name")+" = ii::gui::binding_text(value);";
            else if(auto field=tooltip_numbers.find(name);field!=tooltip_numbers.end())update="options->"+field->second+" = value;";
            else if(auto field=tooltip_booleans.find(name);field!=tooltip_booleans.end())update="options->"+field->second+" = value;";
            else if(auto field=tooltip_colors.find(name);field!=tooltip_colors.end())update="options->"+field->second+" = value;";
            else if(auto field=tooltip_durations.find(name);field!=tooltip_durations.end())update="auto duration = lvrs::tooltip_milliseconds(value); if (!duration) return std::unexpected(duration.error()); options->"+field->second+" = *duration;";
            else if(name=="anchorX"||name=="anchorY")update="if (!options->anchor_point) options->anchor_point = lvrs::TooltipPoint{}; options->anchor_point->"+std::string(name=="anchorX"?"x":"y")+" = value;";
            else if(name=="target"||name=="motion"||name=="availableRect"||name=="preferredPlacement"||name=="backdrop"){
                const auto field=name=="availableRect"?"available_rect":name=="preferredPlacement"?"preferred_placement":name.c_str();update="options->"+std::string(field)+" = value;";
            }
            if(!update.empty())native_binding(*input, variable, "[tooltip = *"+native+"](ii::gui::Widget widget, const auto& value) -> ii::core::Result<void> { (void)widget; auto options = tooltip.options(); if (!options) return std::unexpected(options.error()); "+update+" return tooltip.set_options(std::move(*options)); }");
        }
    }
    std::string scope_declaration(const std::string& scope, bool nested = false) const {
        const auto base=native_scope_base.empty()?"ScopeFactoryStorage":native_scope_base;
        std::string revisions;
        for(std::size_t index=0;index<next_backend;++index)revisions+="            if (this->revision_"+std::to_string(index)+".get() == UINT64_MAX) throw std::length_error(\"Backend revision exhausted\"); this->revision_"+std::to_string(index)+".set(this->revision_"+std::to_string(index)+".get()+1);\n";
        const auto publisher=native_runtime?"        void publish_native_changes() { auto graph = native_lifetime.lock(); if (!graph || !active()) return; lvui::native::batch(*graph, [&] {\n"+revisions+"        }); }\n":"";
        return "    struct ScopeFactoryStorage {\n        lvui::native::BackendFactories backend_factories;\n        lvui::native::BackendActivation backend_activation;\n"+
               std::string(options.framework=="LVRS"?"":"        std::shared_ptr<lvui::RadioGroups> radio_groups;\n        std::shared_ptr<lvui::NavigationContext> navigation_context;\n")+
               "        explicit ScopeFactoryStorage(const lvui::native::BackendFactories& factories) : backend_factories(factories), backend_activation("+std::string(next_backend?"factories.activate()":"lvui::native::BackendActivation{}")+") {}\n    };\n"+
               native_scope_parts + "    struct Scope final : " + base +
               " {\n" + scope + native_style_slots + "        explicit Scope(const lvui::native::BackendFactories& factories = {}) : "+base+"(factories)"+scope_constructor+" {}\n"+publisher+
               (native_initializers.empty() ? "" : "        bool values_initialized = false;\n") +
               (native_runtime ? "        std::weak_ptr<lvui::native::Bindings> native_lifetime;\n        bool active() const noexcept { auto owner = native_lifetime.lock(); if (!owner || !owner->available()) return false; " + std::string(nested ? "if (!this->parent) return false; if constexpr (requires { this->parent->active(); }) return this->parent->active(); " : "") + "return true; }\n" : "") + "    };\n";
    }
    std::string value_initialization() const {
        if (native_initializers.empty()) return {};
        return "    static ii::core::Result<void> initialize_values(const std::shared_ptr<Scope>& scope, const std::shared_ptr<lvui::native::Bindings>& native_bindings) {\n        if (!scope || !native_bindings) return ii::core::fail(ii::core::ErrorCode::invalid_argument, \"Activation requires its scope and bindings\");\n        if (scope->values_initialized) return ii::core::fail(ii::core::ErrorCode::busy, \"Activation values are already initialized\");\n        scope->native_lifetime = native_bindings;\n        scope->values_initialized = true;\n        try {\n" + native_initializers + "            return {};\n        } catch (...) { (void)native_bindings->close(); return ii::core::fail(ii::core::ErrorCode::failed, \"Activation value initialization failed\", std::current_exception()); }\n    }\n";
    }
    static void borrow_path(std::string& expression) {
        std::string result;
        char quote = 0;
        bool escaped = false;
        const auto word = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                                              (c >= '0' && c <= '9') || c == '_'; };
        for (std::size_t position = 0; position < expression.size();) {
            const auto c = expression[position];
            if (quote) {
                result += c; ++position;
                if (escaped) escaped = false;
                else if (c == '\\') escaped = true;
                else if (c == quote) quote = 0;
            } else if (c == '"' || c == '\'') {
                quote = c; result += c; ++position;
            } else if (expression.compare(position, 7, "scope->") == 0 &&
                       (position == 0 || !word(expression[position - 1]))) {
                result += "scope->parent->"; position += 7;
            } else { result += c; ++position; }
        }
        expression = std::move(result);
    }
    void repetition_symbols(const Repetition& repetition,bool key_expression){
        const auto install=[&](const std::string& name,std::string initial,std::string formula,bool numeric){
            constants.erase(name);reactive.erase(name);native_values.erase(name);fixed_values.erase(name);view_values.erase(name);
            backends.erase(name);backend_access.erase(name);backend_types.erase(name);blueprint_types.erase(name);binding_kinds.erase(name);
            const auto capture="_lvui_supplied_"+std::to_string(next_argument++);
            bound_values.emplace(capture,BoundValue{std::move(initial),std::move(formula),lexical_depth,!numeric});
            constants[name]=Value{ValueKind::internal_argument,capture,{}};
            if(numeric)binding_kinds[name]=ValueKind::number;
        };
        const auto initial=key_expression?"row_value":"lvui::native::initial(scope->row->state).value";
        const auto formula=key_expression?"row_value":"context.read(scope->row->state).value";
        install(repetition.item,initial,formula,false);
        backends[repetition.item]=0;
        backend_access[repetition.item]={"lvui::native::collection_object("+std::string(initial)+")",
            key_expression?"":"scope->row->revision",key_expression?"":"scope->row->state"};
        backend_types[repetition.item]=""; // The C++ range determines the actual item type.
        if(!repetition.index.empty())install(repetition.index,key_expression?"row_index":"lvui::native::initial(scope->row->state).index",
            key_expression?"row_index":"context.read(scope->row->state).index",true);
    }
    struct NativeFactoryInput final {std::string name,type,cpp_type;};
    std::string emit_scoped(const View& view, const std::string& parent, const Inputs* menu_parent,const Repetition* repetition=nullptr,const NativeFactoryInput* factory_input=nullptr) {
        const auto name = std::string(repetition?"RepeatedView_":"LocalView_") + std::to_string(next_scope++);
        native_runtime = true;
        needs_parent_scope = true;
        const auto outer_delegate_source=hierarchy_delegate_source;
        const auto outer_segmented_style=active_segmented_style;
        if(!active_segmented_style.empty())borrow_path(active_segmented_style);
        if(factory_input)hierarchy_delegate_source=factory_input->type=="lvrs.HierarchyDelegateContext"?"scope->factory_input":"";
        else if(!hierarchy_delegate_source.empty())borrow_path(hierarchy_delegate_source);
        auto outer_constants = constants;
        auto outer_reactive = reactive;
        auto outer_native_values = native_values;
        auto outer_fixed_values = fixed_values;
        auto outer_backends = backends;
        auto outer_access = backend_access;
        auto outer_backend_types = backend_types;
        auto outer_blueprints = blueprint_types;
        auto outer_views = view_values;
        ++lexical_depth;
        auto outer_kinds = binding_kinds;
        auto outer_types = std::exchange(variable_types, {});
        auto outer_sources = std::exchange(variable_sources, {});
        auto outer_initializers = std::exchange(native_initializers, {});
        auto outer_parts = std::exchange(native_scope_parts, {});
        auto outer_style_slots = std::exchange(native_style_slots, {});
        auto outer_base = std::exchange(native_scope_base, {});
        auto outer_constructor = std::exchange(scope_constructor, {});
        const auto outer_scope_next = std::exchange(native_scope_next, 0);
        auto outer_body = std::exchange(body, {});
        auto outer_variables = std::exchange(widget_variables, {});
        auto outer_context = std::exchange(context_bindings, {});
        auto outer_storage = std::exchange(storage_names, {});
        const auto outer_backend_next = std::exchange(next_backend, 0);
        for (auto& [key, access] : backend_access) { borrow_path(access.object); borrow_path(access.revision); borrow_path(access.dependency); }
        for (auto& [key, access] : reactive) borrow_path(access);
        for (auto& [key, access] : native_values) borrow_path(access);
        std::string payload = "        std::shared_ptr<ParentScope> parent;\n", types;
        if(repetition){payload+="        std::shared_ptr<RowType> row;\n";repetition_symbols(*repetition,false);}
        if(factory_input){
            payload+="        "+factory_input->cpp_type+" factory_input;\n";
            backends[factory_input->name]=0;backend_access[factory_input->name]={"scope->factory_input","",""};backend_types[factory_input->name]=factory_input->type;
        }
        declarations(view.imports, view.bindings, payload, types);
        auto child = view;
        child.imports.clear(); child.bindings.clear();
        if(repetition)require_single_view(child,"A repeated row");
        emit(child, "native_parent", menu_parent);
        for (const auto& binding : context_bindings) {
            auto target = widget_variables.find(binding.target);
            if (target == widget_variables.end()) error(binding.position, "Unknown targetId in the local component scope");
            body += "            if (auto bound = " + (binding.tooltip ? binding.menu + "->set_target(*" + target->second + ")" : target->second + "->set_context_menu(*" + binding.menu + ")") + "; !bound) return bound;\n";
        }
        nested_views += "    template<class ParentScope"+std::string(repetition?", class RowType":"")+"> struct " + name + " final {\n" + types + scope_declaration(payload, true) + value_initialization() +
            "        static ii::core::Result<void> build(ii::gui::WidgetBuilder& widgets, ii::gui::Widget native_parent, std::shared_ptr<ParentScope> parent"+std::string(repetition?", std::shared_ptr<RowType> row":"")+(factory_input?", "+factory_input->cpp_type+" factory_input":"")+") {\n            auto scope = std::make_shared<Scope>(parent->backend_factories);\n            scope->parent = std::move(parent);\n"+
            std::string(options.framework=="LVRS"?"":"            scope->radio_groups = scope->parent->radio_groups;\n            scope->navigation_context = scope->parent->navigation_context;\n")+
            std::string(repetition?"            scope->row = std::move(row);\n":"")+(factory_input?"            scope->factory_input = std::move(factory_input);\n":"")+"            if (auto owned = widgets.retain(scope); !owned) return owned;\n" +
            (options.framework == "LVRS" ? "            lvrs::Widgets controls(widgets);\n" : "") + binding_owner() + "            scope->native_lifetime = native_bindings;\n            if (auto activated = lvui::native::attach_backend_activation(scope, widgets); !activated) return activated;\n" +
            (native_initializers.empty() ? "" : "            if (auto initialized = initialize_values(scope, native_bindings); !initialized) return initialized;\n") + body + "            return {};\n        }\n    };\n";
        constants = std::move(outer_constants); reactive = std::move(outer_reactive);
        native_values = std::move(outer_native_values); backends = std::move(outer_backends);
        fixed_values = std::move(outer_fixed_values);
        backend_access = std::move(outer_access); binding_kinds = std::move(outer_kinds);
        backend_types = std::move(outer_backend_types);
        blueprint_types = std::move(outer_blueprints);
        view_values = std::move(outer_views);
        --lexical_depth;
        variable_types = std::move(outer_types); variable_sources = std::move(outer_sources);
        native_initializers = std::move(outer_initializers); native_scope_parts = std::move(outer_parts);
        native_style_slots=std::move(outer_style_slots);active_segmented_style=outer_segmented_style;
        native_scope_base = std::move(outer_base); native_scope_next = outer_scope_next;
        scope_constructor = std::move(outer_constructor);
        widget_variables = std::move(outer_variables); context_bindings = std::move(outer_context);
        storage_names = std::move(outer_storage); next_backend = outer_backend_next;
        body = std::move(outer_body);
        hierarchy_delegate_source=outer_delegate_source;
        if(!repetition&&!factory_input)body += "            if (auto local = " + name + "<Scope>::build(widgets, " + parent + ", scope); !local) return local;\n";
        return name;
    }
    std::string scoped_factory_view(const Input& argument,std::string_view label,const NativeFactoryInput& input,std::string_view root_kind={}){
        const auto& value=argument.value;std::shared_ptr<const ViewDefinition> definition;
        if(value.kind==ValueKind::internal_argument&&captured_views.contains(value.text))definition=captured_views.at(value.text);
        else if(value.kind==ValueKind::reference&&view_values.contains(value.text))definition=view_values.at(value.text);
        else if(value.kind==ValueKind::view&&value.view)definition=std::make_shared<ViewDefinition>(value.view,environment());
        else error(argument.position,std::string(label)+" requires a View or a typed View construction");
        if(definition->sequence)error(argument.position,std::string(label)+" requires one root View");
        auto outer=environment();const auto alias="_lvui_count_view_"+std::to_string(next_view_instance++);view_values[alias]=definition;
        View child;child.position=argument.position;child.custom_component=alias;
        if(definition->parameters&&!definition->parameters->empty()){
            const auto& parameters=*definition->parameters;
            if(parameters.front().type!=input.type)error(argument.position,std::string(label)+" context parameter requires imported "+input.type);
            for(std::size_t i=1;i<parameters.size();++i)if(!parameters[i].default_value)error(parameters[i].position,"Additional native View factory parameters require defaults");
            child.inputs.push_back({parameters.front().name,{ValueKind::reference,input.name,{},argument.position},argument.position});
        }
        const auto kinds=component_kinds({child});if(kinds.size()!=1||(!root_kind.empty()&&kinds.front()!=root_kind))error(argument.position,std::string(label)+" requires a "+std::string(root_kind.empty()?"single":root_kind)+" root View");
        auto outer_ids=std::exchange(ids,{});
        const auto nested=emit_scoped(child,"{}",nullptr,nullptr,&input);ids=std::move(outer_ids);environment(outer);
        native_runtime=needs_parent_scope=true;
        return nested;
    }
    std::string hierarchy_count_factory(const Inputs& values){
        const auto found=values.find("countView");if(found==values.end())return "{}";
        const auto nested=scoped_factory_view(*found->second,"Hierarchy countView",{"_lvui_hierarchy_item","lvrs.HierarchyItem","lvrs::HierarchyItem"});
        return "lvrs::HierarchyCountFactory{[weak_scope = std::weak_ptr(scope), weak_activation = std::weak_ptr(native_bindings)](ii::gui::WidgetBuilder& widgets, ii::gui::Widget content, const lvrs::HierarchyItem& item) -> ii::core::Result<void> {\n            auto scope = weak_scope.lock(); auto activation = weak_activation.lock();\n            if (!scope || !activation || !activation->available() || !scope->active()) return ii::core::fail(ii::core::ErrorCode::closed, \"Hierarchy countView owner retired\");\n            return "+nested+"<Scope>::build(widgets, content, scope, item);\n        }}";
    }
    std::string hierarchy_delegate_factory(const Inputs& values){
        const auto found=values.find("itemDelegate");if(found==values.end())return "{}";
        const auto nested=scoped_factory_view(*found->second,"Hierarchy itemDelegate",{"_lvui_hierarchy_delegate","lvrs.HierarchyDelegateContext","lvrs::HierarchyDelegateContext"},"HierarchyItem");
        return "lvrs::HierarchyDelegateFactory{[weak_scope = std::weak_ptr(scope), weak_activation = std::weak_ptr(native_bindings)](ii::gui::WidgetBuilder& widgets, ii::gui::Widget parent, std::string_view id, lvrs::HierarchyDelegateContext context) -> ii::core::Result<lvrs::HierarchyItem> {\n            (void)id; auto scope = weak_scope.lock(); auto activation = weak_activation.lock();\n            if (!scope || !activation || !activation->available() || !scope->active()) return ii::core::fail(ii::core::ErrorCode::closed, \"Hierarchy itemDelegate owner retired\");\n            auto built = "+nested+"<Scope>::build(widgets, parent, scope, context); if (!built) return std::unexpected(built.error()); return context.item();\n        }}";
    }
    std::string collection_source(const Value& source,SourcePosition at,bool initial=false)const{
        return expression_code(source,at,initial);
    }
    void emit_repetition(const View& view,const std::string& parent){
        native_runtime=collection_runtime=needs_parent_scope=true;
        const auto& repetition=*view.repetition;
        if(!identifier(repetition.item)||(!repetition.index.empty()&&!identifier(repetition.index)))error(view.position,"Invalid repeated item or index identifier");
        if(view.children.size()!=1)error(view.position,"A repeated View requires exactly one root View");
        Inputs values;
        for(const auto& input:repetition.options){
            constexpr std::string_view supported[]={"id","first","count","spacing","horizontal","itemHeight","itemWidth","columns","overscan","width","height","flex","measureHeight","measureRevision","navigation","navigationFilter","selectionMode","selectOnMove","selectOnClick","onCurrentChanged","onSelectionChanged","onActivate"};
            if(std::ranges::find(supported,input.name)==std::end(supported))error(input.position,"Unknown repetition option '"+input.name+"'");
            if(!values.emplace(input.name,&input).second)error(input.position,"Duplicate repetition option");
        }
        const bool automatic=values.contains("itemHeight");
        if(automatic&&(values.contains("first")||values.contains("count")))error(view.position,"Automatic virtualization cannot combine with a manual first/count range");
        if(!automatic)for(auto name:{"itemWidth","columns","overscan","width","height","flex","measureHeight","measureRevision","navigation","navigationFilter","selectionMode","selectOnMove","selectOnClick","onCurrentChanged","onSelectionChanged","onActivate"})
            if(values.contains(name))error(view.position,"Virtual viewport options require itemHeight");
        const bool navigated=values.contains("navigation");
        if(!navigated)for(auto name:{"navigationFilter","selectionMode","selectOnMove","selectOnClick","onCurrentChanged","onSelectionChanged","onActivate"})
            if(values.contains(name))error(view.position,"Collection interaction options require a navigation backend");
        const auto offset=[&](std::string_view name,std::string fallback){
            const auto found=values.find(name);if(found==values.end())return fallback;
            const auto& input=*found->second;
            if(scoped_expression(input.value))return "static_cast<std::size_t>(lvui::native::integer(lvui::native::range("+expression_code(input.value,input.position)+", 0, 65536)))";
            const auto n=number(resolve(input.value,input.position),input.position);
            if(n<0||n>65536||std::trunc(n)!=n)error(input.position,"Repetition range requires integers within 0..65536");
            return decimal(n);
        };
        const auto first=offset("first","0"),count=offset("count","std::numeric_limits<std::size_t>::max()");
        const auto spacing=values.contains("spacing")&&scoped_expression(values.at("spacing")->value)?
            "lvui::native::number("+expression_code(values.at("spacing")->value,values.at("spacing")->position)+")":decimal(numeric(values,"spacing",0,0,32767));
        const auto horizontal=values.contains("horizontal")&&scoped_expression(values.at("horizontal")->value)?
            "lvui::native::truth("+expression_code(values.at("horizontal")->value,values.at("horizontal")->position)+")":boolean(values,"horizontal",false);
        const auto source=collection_source(repetition.source,view.position),initial_source=collection_source(repetition.source,view.position,true);
        const auto outer=environment();
        repetition_symbols(repetition,true);
        const auto key=expression_code(repetition.key,view.position);
        environment(outer);
        auto outer_ids=std::exchange(ids,{});
        ++factory_depth;
        ambient_reads.push_back({lexical_depth,{}});
        const auto row_view=emit_scoped(view.children[0],"{}",nullptr,&repetition);
        const auto row_ambient=std::move(ambient_reads.back().expressions);ambient_reads.pop_back();
        --factory_depth;
        ids=std::move(outer_ids);
        if(next>=4096)error(view.position,"Native widget limit is 4096");
        const auto index=next++;
        const auto variable="node_"+std::to_string(index);
        const auto id=text(values,"id","@lvui/repeat/"+std::to_string(index));
        if(id.empty()||!ids.insert(id).second)error(view.position,"Duplicate or empty repeated View id");
        widget_variables.emplace(id,variable);
        if(automatic){
            if(next>=4096)error(view.position,"Native widget limit is 4096");
            ++next;
            const auto metric=[&](std::string_view name,double fallback,double low,double high){
                const auto found=values.find(name);
                if(found!=values.end()&&scoped_expression(found->second->value))
                    return "lvui::native::number(lvui::native::range("+expression_code(found->second->value,found->second->position)+", "+decimal(low)+", "+decimal(high)+"))";
                return decimal(numeric(values,name,fallback,low,high));
            };
            const auto item_height=metric("itemHeight",0,0.000001,32767),item_width=metric("itemWidth",0,0,32767);
            const auto columns=offset("columns","0"),overscan=offset("overscan","1");
            const auto measured=values.contains("measureHeight")&&scoped_expression(values.at("measureHeight")->value)?
                "lvui::native::truth("+expression_code(values.at("measureHeight")->value,values.at("measureHeight")->position)+")":boolean(values,"measureHeight",false);
            const auto width=metric("width",0,0,8192),height=metric("height",0,0,8192);
            const auto flex=values.contains("flex")?metric("flex",0,0,4096):"("+height+" == 0 ? 1.0 : 0.0)";
            if(horizontal=="true"){
                if(measured=="true")error(view.position,"Height measurement requires a vertical list or grid");
                if(!values.contains("itemWidth")||(!scoped_expression(values.at("itemWidth")->value)&&numeric(values,"itemWidth",0,0,32767)==0))
                    error(view.position,"A horizontal virtual list requires positive itemWidth");
                if(values.contains("columns")&&!scoped_expression(values.at("columns")->value)&&numeric(values,"columns",0,0,65536)>1)
                    error(view.position,"A horizontal virtual list cannot have multiple columns");
            }
            const auto viewport_id=id+"/viewport";
            if(!ids.insert(viewport_id).second)error(view.position,"Duplicate virtual viewport id");
            body+="            auto "+variable+"_viewport = widgets.scroll_view("+parent+", "+literal(viewport_id)+", {.virtual_content_height = 0, .virtual_content_width = 0});\n            if (!"+variable+"_viewport) return std::unexpected("+variable+"_viewport.error());\n";
            body+="            auto "+variable+" = widgets.content_host(*"+variable+"_viewport, "+literal(id)+", {});\n            if (!"+variable+") return std::unexpected("+variable+".error());\n";
            body+="            { auto target = *"+variable+"; auto scroll_target = *"+variable+"_viewport;\n              auto viewport = std::make_shared<lvui::native::VirtualViewport>();\n              if (auto owned = widgets.retain(viewport); !owned) return owned;\n              if (auto retiring = widgets.on_retire([weak = std::weak_ptr(viewport)] { if (auto owner = weak.lock()) owner->close(); }); !retiring) return retiring;\n";
            body+="              auto geometry = native_bindings->bind([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return ii::gui::Size{"+width+", "+height+"}; }, [scroll_target](ii::gui::Size size) { return scroll_target.set_extent(size); });\n              if (!geometry) return std::unexpected(geometry.error());\n";
            body+="              auto flex = native_bindings->bind([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+flex+"; }, [scroll_target](double value) { return scroll_target.set_flex(value); });\n              if (!flex) return std::unexpected(flex.error());\n";
            body+="              using Collection = lvui::native::KeyedRows<lvui::native::collection_item_t<decltype("+initial_source+")>>;\n              auto rows = std::make_shared<Collection>();\n              if (auto owned = widgets.retain(rows); !owned) return owned;\n              if (auto retiring = widgets.on_retire([weak = std::weak_ptr(rows)] { if (auto owner = weak.lock()) (void)owner->close(); }); !retiring) return retiring;\n";
            body+="              using Measurements = lvui::native::VirtualMeasurements<lvui::native::collection_item_t<decltype("+initial_source+")>>;\n              auto measurements = std::make_shared<Measurements>();\n              if (auto owned = widgets.retain(measurements); !owned) return owned;\n              if (auto retiring = widgets.on_retire([weak = std::weak_ptr(measurements)] { if (auto owner = weak.lock()) owner->close(); }); !retiring) return retiring;\n";
            body+="              auto prepared = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return lvui::native::virtual_collection_source("+source+", [&](const auto& row_value, std::size_t row_index) { return "+key+"; }); });\n              if (!prepared) return std::unexpected(prepared.error());\n              auto source_state = *prepared;\n";
            if(navigated){
                const auto& input=*values.at("navigation");
                const auto found=backend_access.find(input.value.text);
                if(input.value.kind!=ValueKind::reference||found==backend_access.end()||found->second.revision.empty()||!found->second.dependency.empty())
                    error(input.position,"Navigation requires a stable scoped C++ backend");
                const auto access=found->second;
                std::string mode="single";
                if(auto selected=values.find("selectionMode");selected!=values.end()){
                    const auto value=resolve(selected->second->value,selected->second->position);
                    if(value.kind!=ValueKind::enumeration||(value.text!=".none"&&value.text!=".single"&&value.text!=".multiple"))error(selected->second->position,"Invalid collection selection mode");
                    mode=value.text.substr(1);
                }
                const auto reactive_bool=[&](std::string_view name,bool fallback){const auto item=values.find(name);return item!=values.end()&&scoped_expression(item->second->value)?"lvui::native::truth("+expression_code(item->second->value,item->second->position)+")":boolean(values,name,fallback);};
                body+="              auto selection_options = native_bindings->bind([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return lvui::native::CollectionNavigationOptions{.mode = lvui::native::CollectionSelectionMode::"+mode+", .select_on_move = "+reactive_bool("selectOnMove",true)+"}; }, [weak_scope = std::weak_ptr(scope), weak_activation = std::weak_ptr(native_bindings)](const auto& options) -> ii::core::Result<void> { auto scope = weak_scope.lock(); auto activation = weak_activation.lock(); if (!scope || !activation || !activation->available() || !scope->active()) return ii::core::fail(ii::core::ErrorCode::closed, \"Navigation owner retired\"); "+access.object+".configure(options); return {}; });\n              if (!selection_options) return std::unexpected(selection_options.error());\n";
                std::string filter="{}";
                if(auto item=values.find("navigationFilter");item!=values.end()){
                    const auto outer=environment();repetition_symbols(repetition,true);
                    const auto condition=expression_code(item->second->value,item->second->position,true);environment(outer);
                    filter="[weak_scope = std::weak_ptr(scope), weak_activation = std::weak_ptr(native_bindings)](const auto& row_value, std::size_t row_index) { auto scope = weak_scope.lock(); auto activation = weak_activation.lock(); return scope && activation && activation->available() && scope->active() && lvui::native::truth("+condition+"); }";
                }
                body+="              using Navigation = std::remove_reference_t<decltype("+access.object+")>;\n              using WidgetNavigation = lvui::widgets::CollectionWidgetNavigation<lvui::native::collection_item_t<decltype("+initial_source+")>, Navigation>;\n";
                body+="              auto attached_navigation = WidgetNavigation::create(widgets, target, "+access.object+", viewport, [weak_scope = std::weak_ptr(scope), weak_activation = std::weak_ptr(native_bindings)]() -> std::shared_ptr<void> { auto scope = weak_scope.lock(); auto activation = weak_activation.lock(); if (scope && activation && activation->available() && scope->active()) return scope; return {}; },\n                [weak_scope = std::weak_ptr(scope), weak_activation = std::weak_ptr(native_bindings)] { auto scope = weak_scope.lock(); auto activation = weak_activation.lock(); if (scope && activation && activation->available() && scope->active()) (void)lvui::native::invoke("+access.revision+", [] {}); },\n                {.current_changed = "+action(values,"onCurrentChanged","std::string key","key")+", .activated = "+action(values,"onActivate","std::string key","key")+", .selection_changed = "+action(values,"onSelectionChanged","std::vector<std::string> keys","keys")+"}, "+filter+");\n              if (!attached_navigation) return std::unexpected(attached_navigation.error());\n              auto navigation = *attached_navigation;\n";
                body+="              auto click_options = native_bindings->bind([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+reactive_bool("selectOnClick",true)+"; }, [navigation](bool enabled) -> ii::core::Result<void> { navigation->select_on_click(enabled); return {}; });\n              if (!click_options) return std::unexpected(click_options.error());\n";
            }
            body+="              auto ambient = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) {\n";
            for(const auto& read:row_ambient)body+="                (void)("+read+");\n";
            if(auto revision=values.find("measureRevision");revision!=values.end())body+="                (void)("+expression_code(revision->second->value,revision->second->position)+");\n";
            body+="                return std::make_shared<const int>(0);\n              });\n              if (!ambient) return std::unexpected(ambient.error());\n              auto ambient_state = *ambient;\n";
            body+="              auto binding = native_bindings->bind([scope, viewport, source_state, measurements, ambient_state]([[maybe_unused]] lvui::native::ReadContext& context) { measurements->track(context); return measurements->plan(context.read(*source_state), {.item_height = "+item_height+", .item_width = "+item_width+", .spacing = "+spacing+", .horizontal = "+horizontal+", .columns = "+columns+", .overscan = "+overscan+", .measure_height = "+measured+"}, viewport->read(context), viewport->anchor(), context.read(*ambient_state)); },\n";
            auto setter=std::string("                [weak_scope = std::weak_ptr(scope), weak_activation = std::weak_ptr(native_bindings), rows, viewport, measurements, target, scroll_target")+(navigated?", navigation":"")+"](const auto& frame) -> ii::core::Result<void> {\n                  auto scope = weak_scope.lock(); auto activation = weak_activation.lock();\n                  if (!scope || !activation || !activation->available() || !scope->active()) return ii::core::fail(ii::core::ErrorCode::closed, \"Virtual owner retired\");\n                  auto applied = rows->apply(frame.collection, [&](const auto& mounted) -> ii::core::Result<void> {\n                    std::vector<ii::gui::KeyedContentItem> items; items.reserve(mounted.size());\n                    for (std::size_t index = 0; index < mounted.size(); ++index) { const auto& item = mounted[index]; const auto& cell = frame.cells[index];\n                      items.push_back({item.key, [weak_scope, weak_activation, row = item.row](ii::gui::WidgetBuilder& widgets) -> ii::core::Result<void> {\n                        auto scope = weak_scope.lock(); auto activation = weak_activation.lock();\n                        if (!scope || !activation || !activation->available() || !scope->active()) return ii::core::fail(ii::core::ErrorCode::closed, \"Virtual row owner retired\");\n                        return "+row_view+"<Scope, typename Collection::Row>::build(widgets, {}, scope, row);\n                      }, ii::gui::Rect{float(cell.x), float(cell.y), float(cell.width), float(cell.height)}});\n                    }\n                    return target.set_keyed_content(std::move(items), {.viewport = ii::gui::Size{frame.width, frame.height}});\n                  });\n                  if (!applied) return applied;\n                  if (!viewport->commit(frame)) return ii::core::fail(ii::core::ErrorCode::busy, \"Viewport changed during row activation\");\n                  std::vector<lvui::native::VirtualRowMeasurement> natural; natural.reserve(frame.collection.items.size());\n                  for (std::size_t index = 0; index < frame.collection.items.size(); ++index) {\n                    const auto& item = frame.collection.items[index];\n                    auto root = target.keyed_content_item(item.key); if (!root) return std::unexpected(root.error());\n                    if (frame.measure_height) {\n                      auto observing = root->set_measure_handler([weak_scope, weak_activation, weak_measurements = std::weak_ptr(measurements), key = item.key, generation = frame.measurement_generation](const ii::gui::WidgetMeasurement& size) {\n                        auto scope = weak_scope.lock(); auto activation = weak_activation.lock(); auto owner = weak_measurements.lock();\n                        if (scope && activation && activation->available() && scope->active() && owner) (void)owner->report(generation, {{key, size.available_width, size.natural.height}});\n                      });\n                      if (!observing) return observing;\n                      const auto width = double(float(frame.cells[index].width));\n                      if (width > 0) { auto size = root->measure(width); if (!size) return std::unexpected(size.error()); natural.push_back({item.key, width, size->height}); }\n                    } else if (auto stopped = root->set_measure_handler({}); !stopped) return stopped;\n                  }\n                  if (frame.measure_height) (void)measurements->report(frame.measurement_generation, natural);\n";
            if(navigated)setter+="                  navigation->publish(frame);\n";
            body+=setter+"                  return scroll_target.set_virtual_scroll_content({frame.content_width, frame.content_height}, frame.x, frame.y);\n                });\n              if (!binding) return std::unexpected(binding.error());\n";
            body+="              auto observing = scroll_target.set_scroll_handler([weak_scope = std::weak_ptr(scope), weak_activation = std::weak_ptr(native_bindings), weak_viewport = std::weak_ptr(viewport)](const ii::gui::ScrollViewport& view) {\n                auto scope = weak_scope.lock(); auto activation = weak_activation.lock(); auto viewport = weak_viewport.lock();\n                if (scope && activation && activation->available() && scope->active() && viewport) viewport->observe({.width = view.bounds.width, .height = view.bounds.height, .x = view.horizontal_offset, .y = view.offset});\n              });\n              if (!observing) return observing; }\n";
            return;
        }
        body+="            auto "+variable+" = widgets.content_host("+parent+", "+literal(id)+", {});\n            if (!"+variable+") return std::unexpected("+variable+".error());\n";
        body+="            { using Collection = lvui::native::KeyedRows<lvui::native::collection_item_t<decltype("+initial_source+")>>;\n              auto rows = std::make_shared<Collection>();\n              if (auto owned = widgets.retain(rows); !owned) return owned;\n              if (auto retiring = widgets.on_retire([weak = std::weak_ptr(rows)] { if (auto owner = weak.lock()) (void)owner->close(); }); !retiring) return retiring;\n";
        body+="              auto target = *"+variable+";\n              auto binding = native_bindings->bind([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return lvui::native::collection_frame("+source+", [&](const auto& row_value, std::size_t row_index) { return "+key+"; }, {.first = "+first+", .count = "+count+", .spacing = "+spacing+", .horizontal = "+horizontal+"}); },\n";
        body+="                [weak_scope = std::weak_ptr(scope), weak_activation = std::weak_ptr(native_bindings), rows, target](const auto& frame) -> ii::core::Result<void> {\n                  auto scope = weak_scope.lock(); auto activation = weak_activation.lock();\n                  if (!scope || !activation || !activation->available() || !scope->active()) return ii::core::fail(ii::core::ErrorCode::closed, \"Repeated owner retired\");\n                  return rows->apply(frame, [&](const auto& mounted) -> ii::core::Result<void> {\n                    std::vector<ii::gui::KeyedContentItem> items; items.reserve(mounted.size());\n                    for (const auto& item : mounted) items.push_back({item.key, [weak_scope, weak_activation, row = item.row](ii::gui::WidgetBuilder& widgets) -> ii::core::Result<void> {\n                      auto scope = weak_scope.lock(); auto activation = weak_activation.lock();\n                      if (!scope || !activation || !activation->available() || !scope->active()) return ii::core::fail(ii::core::ErrorCode::closed, \"Repeated row owner retired\");\n                      return "+row_view+"<Scope, typename Collection::Row>::build(widgets, {}, scope, row);\n                    }});\n                    return target.set_keyed_content(std::move(items), {.horizontal = frame.options.horizontal, .layout = {.spacing = frame.options.spacing, .alignment = ii::gui::StackAlignment::stretch}});\n                  });\n                });\n              if (!binding) return std::unexpected(binding.error()); }\n";
    }
    std::string conditional_factory(const View& branch) {
        require_single_view(branch,"A conditional branch");
        auto outer_body = std::exchange(body, {});
        auto outer_ids = std::exchange(ids, {});
        auto outer_variables = std::exchange(widget_variables, {});
        auto outer_context = std::exchange(context_bindings, {});
        ++factory_depth;
        emit(branch, "{}");
        for (const auto& binding : context_bindings) {
            const auto target = widget_variables.find(binding.target);
            if (target == widget_variables.end()) error(binding.position, "Unknown targetId in conditional content");
            body += "            if (auto bound = " + (binding.tooltip ? binding.menu + "->set_target(*" + target->second + ")" : target->second + "->set_context_menu(*" + binding.menu + ")") + "; !bound) return bound;\n";
        }
        auto content = std::move(body);
        --factory_depth;
        body = std::move(outer_body); ids = std::move(outer_ids);
        widget_variables = std::move(outer_variables); context_bindings = std::move(outer_context);
        return std::string("ii::gui::WidgetFactory{[weak_scope = std::weak_ptr(scope), weak_activation = std::weak_ptr(native_bindings)](ii::gui::WidgetBuilder& widgets) -> ii::core::Result<void> {\n            auto scope = weak_scope.lock(); auto activation = weak_activation.lock();\n            if (!scope || !activation || !activation->available() || !scope->active()) return ii::core::fail(ii::core::ErrorCode::closed, \"Conditional owner retired\");\n") +
               (options.framework == "LVRS" ? "            lvrs::Widgets controls(widgets);\n" : "") + binding_owner() + content + "            return {};\n        }}";
    }
    void emit_conditional(const View& view, const std::string& parent) {
        native_runtime = true; needs_parent_scope = true;
        if (view.children.empty() || view.children.size() > 2) error(view.position, "A conditional requires one or two View branches");
        if (next >= 4096) error(view.position, "Native widget limit is 4096");
        const auto variable = "node_" + std::to_string(next++);
        const auto id = id_prefix + "@lvui/conditional/" + std::to_string(next);
        ids.insert(id); widget_variables.emplace(id, variable);
        const auto positive = conditional_factory(view.children[0]);
        const auto negative = view.children.size() == 2 ? conditional_factory(view.children[1]) : "ii::gui::WidgetFactory{}";
        const auto condition = scoped_expression(*view.condition) ? expression_code(*view.condition, view.position) : [&] {
            const auto value = resolve(*view.condition, view.position);
            if (value.kind != ValueKind::boolean) error(view.position, "A conditional View requires Bool");
            return value.text;
        }();
        body += "            auto " + variable + " = widgets.content_host(" + parent + ", " + literal(id) + ", {});\n            if (!" + variable + ") return std::unexpected(" + variable + ".error());\n";
        body += "            { auto target = *" + variable + "; auto binding = native_bindings->bind([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return lvui::native::truth(" + condition + "); }, [target, positive = " + positive + ", negative = " + negative + "](bool selected) -> ii::core::Result<void> { if (selected) return target.set_content(positive); if (negative) return target.set_content(negative); return target.clear_content(); });\n              if (!binding) return std::unexpected(binding.error()); }\n";
    }
    void emit(const View& view, const std::string& parent,const Inputs* menu_parent=nullptr) {
        if (view.lexical_context) {
            if (view.lexical_context > slot_environments.size()) error(view.position, "Invalid supplied View context");
            const auto outer = environment();
            const auto captured = slot_environments[view.lexical_context - 1];
            environment(captured);
            auto child = view; child.lexical_context = 0;
            emit(child, parent, menu_parent);
            environment(outer);
            return;
        }
        if(has_materialized_inputs(view)){emit_scoped(view,parent,menu_parent);return;}
        if (view.condition) { emit_conditional(view, parent); return; }
        if (view.repetition) { emit_repetition(view,parent); return; }
        if (!view.custom_component.empty()) {
            if (++view_expansion_depth > 64) error(view.position, "Maximum View value expansion depth is 64");
            if(++view_expansions>16384)error(view.position,"View composition expansion limit is 16384");
            const auto found = view_values.find(view.custom_component);
            if (found == view_values.end()) error(view.position, "Unknown lexical View value '" + view.custom_component + "'");
            const auto definition = found->second;
            const auto outer_environment = environment();
            const auto supplied = instantiate_view(view, *definition);
            const auto outer_prefix = id_prefix;
            id_prefix += view.custom_component + "@" + std::to_string(next_view_instance++) + "/";
            emit(supplied, parent, menu_parent);
            environment(outer_environment);
            id_prefix = outer_prefix;
            --view_expansion_depth;
            return;
        }
        if (!view.bindings.empty() || !view.imports.empty()) { emit_scoped(view, parent, menu_parent); return; }
        if(view.fragment){
            if(!view.inputs.empty())error(view.position,"A View sequence has no root inputs");
            for(const auto& child:view.children)emit(child,parent,menu_parent);
            return;
        }
        const auto kind=view.component->spelling;
        const bool stack=kind=="VStack" || kind=="HStack" || kind=="ZStack";
        const bool icon_button=kind=="IconButton" || kind=="IconMenuButton";
        const bool dropdown=kind=="DropdownButton" || kind=="IconMenuButton" || kind=="LabelMenuButton";
        const bool button=kind=="PushButton" || kind=="LabelButton" || kind=="AbstractButton" || icon_button || dropdown || kind=="HelpButton" || kind=="ListItem";
        const bool material=kind=="MaterialSurface"||kind=="PanelMaterial"||kind=="WindowMaterial";
        const bool code_editor=kind=="CodeEditor",tooltip=kind=="Tooltip";
        const bool progress_bar=kind=="ProgressBar"&&options.framework=="LVRS";
        const bool hierarchy_item=kind=="HierarchyItem",hierarchy_list=kind=="HierarchyList";
        const bool media_card=kind=="Card",avatar=kind=="Avatar",image_view=kind=="Image",native_view=kind=="NativeView";
        native_view_runtime|=native_view;
        if(native_view&&!view.children.empty())error(view.position,"NativeView content is built by its C++ source");
        const bool toolbar=kind=="ToolbarButton"||kind=="HierarchyToolbar"||kind=="ListToolbar"||kind=="ListFooter";
        const bool list=kind=="List"||kind=="Hierarchy",hierarchy_panel=kind=="Hierarchy"&&options.framework!="LVRS",list_panel=kind=="List"&&options.framework!="LVRS",card=kind=="AppCard",header=kind=="AppHeader",listener=kind=="EventListener",router=kind=="PageRouter",link=kind=="Link";
        const bool tabbar=kind=="TabBar",tab=kind=="Tab";
        const bool combo=kind=="ComboBox",stepper=kind=="Stepper",segmented=kind=="LabelSegmentedControl" || kind=="IconSegmentedControl";
        const bool menu=kind=="Menu" || kind=="ContextMenu",menu_item=kind=="MenuItem" || kind=="ContextMenuItem",divider=kind=="MenuDivider" || kind=="ContextMenuDivider";
        const auto submenu_owner=menu&&options.framework!="LVRS"?active_submenu_registration:std::string{};
        const bool color_surface=kind=="ColorPickerSurface" || kind=="ColorField";
        const bool color_picker=kind=="ColorPicker";
        const bool color_button=kind=="ColorPickerButton";
        const bool popover=kind=="Popover";
        const bool modal=kind=="Modal",sheet=kind=="Sheet";
        const bool table_cell=kind=="TableCellItem",table_row=kind=="TableRow",table_header=kind=="TableHeader",table_view=kind=="Table";
        const bool alert=kind=="Alert",alert_button=kind=="AlertButton";
        const bool check=kind=="CheckBox" || kind=="RadioButton" || kind=="ToggleSwitch";
        Inputs values;
        if(native_view)values=inputs(view,{"id","source","width","height","enabled","clip"});
        else if (stack)
            values = inputs(view, {"id", "spacing", "defaultSpacing", "padding", "leftPadding", "topPadding", "rightPadding", "bottomPadding",
                                   "alignment", "alignmentName", "intrinsicWidth", "measureHiddenChildren", "width", "height", "minimumWidth", "minimumHeight",
                                   "backgroundColor", "borderColor", "borderWidth", "cornerRadius"});
        else if(tooltip)values=inputs(view,{"id","targetId","target","text","anchorX","anchorY","availableRect","preferredPlacement","automatic","scrollContent","motionEnabled","maximumWidth","maximumHeight","contentPadding","edgeMargin","cornerRadius","tailLength","tailWidth","delay","timeout","hideDelay","animationDuration","surfaceColor","borderColor","primaryColor","surfaceOpacity","backdrop","motion","accessibilityName","onOpened","onClosed"});
        else if(code_editor&&options.framework!="LVRS")values=inputs(view,{"id","model","editor","sourceRevision","text","fontFamily","accessibilityName","width","height","editorHeight","insetHorizontal","insetVertical","cornerRadius","fontPixelSize","textLineHeight","tabStopDistance","enabled","readOnly","textColor","backgroundColor","backgroundColorHover","selectionColor","focusColor","caretColor","onDocumentEdited","onSelectionChanged"});
        else if(code_editor)values=inputs(view,{"id","model","text","placeholderText","snippetTitle","snippetLanguage","fontFamily","accessibilityName","width","height","editorHeight","fieldMinHeight","headerHeight","headerSpacing","insetHorizontal","insetVertical","cornerRadius","fontPixelSize","fontWeight","textLineHeight","centeredTextHeight","tabStopDistance","textMargin","placeholderOpacity","enabled","readOnly","showSnippetHeader","showScrollBar","autoFocusOnPress","selectByMouse","selectByKeyboard","persistentSelection","overwriteMode","textColor","textColorDisabled","placeholderColor","placeholderColorDisabled","selectedTextColor","headerTextColor","backgroundColor","backgroundColorHover","backgroundColorFocused","backgroundColorDisabled","selectionColor","cursorPosition","onDocumentEdited","onTextEdited","onSubmitted","onSelectionChanged"});
        else if(link)values=inputs(view,{"id","text","href","to","params","router","navigator","targetComponent","replace","enabled","underline","tone","linkColor","hoverColor","pressedColor","disabledColor","horizontalPadding","verticalPadding","minimumHeight","cornerRadius","width","height","showFocusRing","accessibilityName","method","methods","onClick","onPressed","onReleased","onCanceled","onNavigated","onNavigationFailed"},true);
        else if(router){values=inputs(view,{"id","router","width","height"});if(!values.contains("router"))error(view.position,"PageRouter requires a scoped router member");}
        else if(hierarchy_item&&options.framework!="LVRS")values=inputs(view,{"id","text","label","itemKey","itemId","count","enabled","activatable","selectable","selected","expanded","showChevron","hasChildItems","dragPreviewActive","indentLevel","width","itemWidth","height","rowHeight","indentStep","iconSize","chevronSize","baseLeftPadding","rowRightPadding","leadingSpacing","cornerRadius","countWidth","iconName","iconSource","iconRevision","accessibilityName","textColorNormal","textColorDisabled","rowBackgroundColorIdle","rowBackgroundColorHover","rowBackgroundColorPressed","rowBackgroundColorActive","rowBackgroundColorInactive","rowBackgroundColorDrag","focusColor","onClick","onExpansionChanged"},true);
        else if(hierarchy_item)values=inputs(view,{"id","controller","text","label","itemKey","itemId","count","enabled","activatable","selectable","selected","expanded","showChevron","hasChildItems","dragAllowed","dragPreviewActive","dragPreviewOpacity","indentLevel","width","itemWidth","height","rowHeight","indentStep","iconSize","chevronSize","baseLeftPadding","rowRightPadding","leadingSpacing","cornerRadius","countWidth","iconName","iconGlyph","textColorNormal","textColorDisabled","rowBackgroundColorIdle","rowBackgroundColorHover","rowBackgroundColorPressed","rowBackgroundColorActive","rowBackgroundColorInactive","rowBackgroundColorDrag","onClick","onExpansionChanged","countView"},true);
        else if(hierarchy_list&&options.framework!="LVRS")values=inputs(view,{"id","controller","editor","modelRevision","width","height","enabled","editable","showIndicator","keyboardNavigationEnabled","overscan","generatedRowHeight","rowHeight","generatedIndentStep","indentStep","generatedIconSize","iconSize","generatedChevronSize","chevronSize","baseLeftPadding","rowRightPadding","leadingSpacing","cornerRadius","countWidth","iconSource","iconRevision","textColorNormal","textColorDisabled","rowBackgroundColorIdle","rowBackgroundColorHover","rowBackgroundColorPressed","rowBackgroundColorActive","rowBackgroundColorInactive","rowBackgroundColorDrag","focusColor","onActiveChanged","onExpansionChanged","onItemMoved"});
        else if(hierarchy_list)values=inputs(view,{"id","model","treeModel","controller","width","height","enabled","editable","keyboardNavigationEnabled","autoExpandAncestorsOnActivate","generatedItemWidth","generatedRowHeight","rowHeight","generatedIndentStep","indentStep","generatedIconSize","iconSize","generatedChevronSize","chevronSize","baseLeftPadding","rowRightPadding","leadingSpacing","cornerRadius","countWidth","textColorNormal","textColorDisabled","rowBackgroundColorIdle","rowBackgroundColorHover","rowBackgroundColorPressed","rowBackgroundColorActive","rowBackgroundColorInactive","rowBackgroundColorDrag","onActiveChanged","onExpansionChanged","onItemMoved","onEnsureVisibleRequested","itemDelegate"});
        else if(kind=="ToolbarButton"&&options.framework!="LVRS")values=inputs(view,{"id","buttonId","iconName","icon","iconSource","iconRevision","accessibilityName","text","enabled","width","height","iconSize","leftPadding","rightPadding","spacing","cornerRadius","fontSize","tone","textColor","disabledTextColor","hoverColor","pressedColor","primaryColor","dangerColor","focusColor","onClick","onTriggered"},true);
        else if(kind=="ToolbarButton")values=inputs(view,{"id","buttonId","iconName","accessibilityName","text","icon","enabled","width","height","iconSize","leftPadding","rightPadding","cornerRadius","tone","onClick"},true);
        else if(kind=="ListToolbar"&&options.framework!="LVRS")values=inputs(view,{"id","icon1","icon2","icon3","iconSource","iconRevision","iconSize","buttonSize","horizontalPadding","verticalPadding","spacing","cornerRadius","backgroundColor","hoverColor","pressedColor","focusColor","interactive","enabled","width","height","onIconClicked"});
        else if(kind=="ListToolbar")values=inputs(view,{"id","icon1","icon2","icon3","iconSize","buttonSize","horizontalPadding","verticalPadding","spacing","backgroundColor","interactive","enabled","width","height","onIconClicked"});
        else if(kind=="ListFooter"&&options.framework!="LVRS")values=inputs(view,{"id","button1","button2","button3","iconSource","iconRevision","horizontalPadding","verticalPadding","spacing","iconSize","buttonHeight","fontSize","cornerRadius","textColor","disabledTextColor","hoverColor","pressedColor","primaryColor","dangerColor","focusColor","interactive","enabled","width","height","onButtonClicked"});
        else if(kind=="ListFooter")values=inputs(view,{"id","button1","button2","button3","horizontalPadding","verticalPadding","spacing","interactive","enabled","width","height","onButtonClicked"});
        else if(kind=="HierarchyToolbar"&&options.framework!="LVRS")values=inputs(view,{"id","minimumToolbarWidth","horizontalPadding","verticalPadding","spacing","slotSize","iconSize","cornerRadius","fontSize","backgroundColor","backgroundOpacity","textColor","disabledTextColor","hoverColor","pressedColor","primaryColor","dangerColor","focusColor","distributeSpacing","enabled","width","height","buttonItems","activeButtonId","iconSource","iconRevision","onActiveChanged","onButtonTriggered"});
        else if(kind=="HierarchyToolbar")values=inputs(view,{"id","minimumToolbarWidth","horizontalPadding","verticalPadding","spacing","slotSize","backgroundColor","backgroundOpacity","distributeSpacing","enabled","width","height","buttonItems","activeButtonId","onActiveChanged","onButtonTriggered","onButtonEventTriggered"});
        else if(material)values=inputs(view,{"id","density","primaryColor","color","borderColor","backdropBaseColor","tintOpacity","blurRadius","radius","innerHighlightOpacity","shadowRadius","shadowOffsetY","shadowOpacity","borderWidth","innerHighlightOffsetY","intenseOpacity","faintOpacity","blurEnabled","motionEnabled","enabled","backdrop","outlinePath","width","height","motion"});
        else if(media_card&&options.framework!="LVRS")values=inputs(view,{"id","type","size","detail","displayState","selected","selectable","enabled","title","filename","description","details","metadata","statusText","summary","footnote","actionText","domain","previewTitle","rows","progressLabel","progressText","progress","progressColor","statusColor","showDescription","showStatus","showMenu","showAction","menuAccessibleName","iconName","icon","iconSource","iconRevision","previewSource","width","height","padding","cornerRadius","backgroundColor","hoverColor","selectedColor","borderColor","selectedBorderColor","textColor","secondaryColor","disabledTextColor","progressTrackColor","focusColor","onClick","onSelectedChanged","onMenuRequested","onActionTriggered"},true);
        else if(media_card)values=inputs(view,{"id","type","size","detail","displayState","selected","selectable","enabled","title","filename","description","details","metadata","statusText","summary","footnote","actionText","domain","previewTitle","rows","progressLabel","progressText","progress","progressColor","statusColor","showDescription","showStatus","showMenu","showAction","menuAccessibleName","iconName","icon","iconSource","previewSource","previewComponent","iconComponent","width","height","onClick","onSelectedChanged","onMenuRequested","onActionTriggered"},true);
        else if(avatar)values=inputs(view,{"id","size","source","backgroundColor","width","height","enabled","accessibilityName"});
        else if(card)values=inputs(view,{"id","title","subtitle","enabled","width","height","padding","spacing","cornerRadius","shapeStyle","backgroundColor","titleColor","subtitleColor"});
        else if(header)values=inputs(view,{"id","title","subtitle","menuVisible","compact","enabled","contentHorizontalPadding","contentVerticalPadding","rowSpacing","actionSpacing","menuButtonPadding","width","height","backgroundColor","titleColor","subtitleColor","menuBackgroundColor","menuPressedBackgroundColor","onMenuClicked"});
        else if(listener)values=inputs(view,{"id","width","height","onInput","onResized"});
        else if(list_panel)values=inputs(view,{"id","controller","model","modelRevision","width","height","rowHeight","overscan","showIndicator","keyboardNavigationEnabled","leftPadding","rightPadding","iconSize","spacing","cornerRadius","iconSource","iconRevision","textColor","descriptionColor","disabledTextColor","rowBackgroundColor","hoverColor","pressedColor","selectedRowColor","focusColor","toolbarHeight","toolbarVisible","toolbarIcon1","toolbarIcon2","toolbarIcon3","toolbarIconSize","toolbarInteractive","footerHeight","footerVisible","footerInteractive","footerButton1","footerButton2","footerButton3","backgroundColor","enabled","interactive","onItemActivated","onToolbarIconTriggered","onFooterButtonTriggered"});
        else if(hierarchy_panel)values=inputs(view,{"id","controller","editor","editable","modelRevision","width","height","rowHeight","overscan","showIndicator","keyboardNavigationEnabled","indentStep","iconSize","chevronSize","baseLeftPadding","rowRightPadding","leadingSpacing","cornerRadius","countWidth","iconSource","iconRevision","textColorNormal","textColorDisabled","rowBackgroundColorIdle","rowBackgroundColorHover","rowBackgroundColorPressed","rowBackgroundColorActive","rowBackgroundColorInactive","rowBackgroundColorDrag","focusColor","toolbarHeight","toolbarVisible","toolbarItems","activeToolbarButtonId","footerHeight","footerVisible","footerInteractive","footerButton1","footerButton2","footerButton3","backgroundColor","enabled","onItemActivated","onExpansionChanged","onItemMoved","onToolbarActiveChanged","onToolbarTriggered","onFooterClicked"});
        else if(list)values=inputs(view,{"id","model","width","height","rowHeight","indent","backgroundColor","selectionColor","textColor","selectionRadius","fontSize","reorderable","editable","showChecks","onSelectionChanged","onItemActivated","onItemRenamed","onItemMoved","onItemChecked"});
        else if(table_view&&options.framework!="LVRS")values=inputs(view,{"id","controller","model","editor","orderEditor","sorter","structureEditor","editable","clipboardEnabled","sortingEnabled","columnResizeEnabled","rowResizeEnabled","columnResizeHandleWidth","rowResizeHandleHeight","modelRevision","width","height","headerHeight","cellPadding","fallbackCellWidth","minColumnWidth","fontSize","enabled","selectionEnabled","keyboardNavigationEnabled","fitColumnsToViewport","showIndicator","accessibilityName","backgroundColor","dividerColor","headerTextColor","cellTextColor","selectionColor","currentCellBorderColor","onSelectionChanged","onCellInputEdited","onCellInputSubmitted","onCellInputRejected","onRangeValuesChanged","onRowsSorted","onStructureChanged","onColumnResized","onRowResized"});
        else if(table_view)values=inputs(view,{"id","model","width","height","headerHeight","contentSpacing","borderWidth","selectionEnabled","keyboardNavigationEnabled","sortingEnabled","fitColumnsToViewport","selectionMode","backgroundColor","dividerColor","headerTextColor","cellTextColor","selectionColor","currentCellBorderColor","onCellInputEdited","onCellInputSubmitted","onCellInputRejected","onSelectionChanged","onCellActivated","onRowsSorted","resizeHandlesVisible","structureControlsVisible","addRowControlsVisible","addColumnControlsVisible","deleteContextMenuEnabled","selectOnContextClick","clipboardEnabled","columnResizeHandleWidth","rowResizeHandleHeight","structureGutterWidth","structureGutterHeight","resizeHandleColor","resizeHandleHoverColor","onRangeValuesChanged","onColumnResized","onRowResized"});
        else if(table_cell&&options.framework!="LVRS")values=inputs(view,{"id","text","width","height","cellHeight","contentSpacing","fontSize","dividerColor","backgroundColor","textColor","showDivider","clipContent","textStyle","inputable","enabled","selected","current","selectionColor","currentBorderColor","currentBorderWidth","valueType","valueValidator","accessibilityName","onInputEdited","onInputSubmitted","onInputRejected"});
        else if(table_cell)values=inputs(view,{"id","text","width","cellHeight","contentSpacing","dividerColor","textColor","showDivider","clipContent","textStyle","inputable","enabled","selected","current","selectionColor","currentBorderColor","currentBorderWidth","valueType","valueValidator","onInputEdited","onInputSubmitted","onInputRejected"});
        else if(table_row&&options.framework!="LVRS")values=inputs(view,{"id","cells","width","height","cellWidth","cellHeight","contentSpacing","fontSize","offset","inputable","enabled","showDivider","backgroundColor","dividerColor","textColor","selectionColor","currentBorderColor","onCellInputEdited","onCellInputSubmitted","onCellInputRejected"});
        else if(table_row)values=inputs(view,{"id","cells","width","cellWidth","cellHeight","contentSpacing","inputable","enabled","dividerColor","textColor","onCellInputEdited","onCellInputSubmitted"});
        else if(table_header&&options.framework!="LVRS")values=inputs(view,{"id","columns","width","rowHeight","cellHorizontalPadding","columnWidths","fallbackCellWidth","minColumnWidth","offset","fitColumnsToViewport","textColor","backgroundColor","focusColor","separatorHeight","separatorColor","interactive","enabled","onColumnClicked"});
        else if(table_header)values=inputs(view,{"id","columns","width","rowHeight","cellHorizontalPadding","columnWidths","fallbackCellWidth","minColumnWidth","textColor","separatorHeight","separatorColor","interactive","enabled","onColumnClicked"});
        else if(kind=="Spacer") values=inputs(view,{"id","minLength"});
        else if(image_view)values=inputs(view,{"id","source","width","height","accessibilityName","padding","cornerRadius","borderWidth","backgroundColor","borderColor","preserveAspect","fit","circular","allowEmpty"});
        else if(kind=="Label") values=inputs(view,{"id","text","fontSize","fontWeight","lineHeight","color","selectable","style"});
        else if(dropdown&&options.framework!="LVRS") values=inputs(view,{"id","text","enabled","tone","onClick","onPressed","onReleased","onCanceled","iconMode","iconName","iconSource","iconRevision","iconSize","icon","width","height","horizontalPadding","leftPadding","rightPadding","spacing","cornerRadius","indicatorSize","fontSize","lineHeight","accessibilityName","primaryColor","hoverColor","pressedColor","textColor","disabledTextColor"},true);
        else if(kind=="HelpButton"&&options.framework!="LVRS") values=inputs(view,{"id","text","enabled","tone","onClick","width","height","horizontalPadding","cornerRadius","fontSize","lineHeight","textOffsetY","accessibilityName","backgroundColor","hoverColor","pressedColor","textColor","disabledTextColor"},true);
        else if(kind=="ListItem"&&options.framework!="LVRS") values=inputs(view,{"id","type","size","label","text","description","detail","iconName","icon","iconSource","iconRevision","accessibilityName","selected","enabled","interactive","selectable","showDescription","showChevron","checked","toggleEnabled","selectionEnabled","showToggle","showSelection","primaryAction","secondaryAction","moreMenu","showPrimaryAction","showSecondaryAction","showMoreMenu","actionWidth","slotPadding","slotSpacing","quantity","minimumQuantity","maximumQuantity","stepSize","quantityWidth","quantitySpacing","stepperEnabled","showStepper","selector","selectorIndex","selectorWidth","selectorEnabled","showSelector","width","height","leftPadding","rightPadding","iconSize","spacing","cornerRadius","chevronSize","textColor","descriptionColor","disabledTextColor","backgroundColor","hoverColor","pressedColor","selectedColor","focusColor","onClick","onCheckedChanged","onEdited","onActionTriggered","onQuantityChanged","onSelectorIndexChanged"},true);
        else if(button&&options.framework!="LVRS") values=inputs(view,{"id","text","enabled","tone","onClick","iconMode","iconName","iconSource","iconRevision","iconSize","icon","height","horizontalPadding","cornerRadius","accessibilityName"},true);
        else if(button) values=inputs(view,{"id","text","enabled","tone","onClick","iconMode","iconName","iconSize","icon","height","horizontalPadding","cornerRadius","accessibilityName"},true);
        else if(tab&&options.framework!="LVRS") values=inputs(view,{"id","text","iconName","iconSource","iconRevision","badge","enabled","selected","tabStyle","iconSize","minimumWidth","height","accessibilityName","onClick"},true);
        else if(tab) values=inputs(view,{"id","text","iconName","badge","enabled","selected","tabStyle","iconSize","minimumWidth","onClick"},true);
        else if(tabbar&&options.framework!="LVRS") values=inputs(view,{"id","spacing","minimumTabWidth","currentIndex","autoSelect","accessibilityName","onCurrentIndexChanged"});
        else if(tabbar) values=inputs(view,{"id","spacing","minimumTabWidth","currentIndex","autoSelect","onCurrentIndexChanged"});
        else if(combo&&options.framework!="LVRS") values=inputs(view,{"id","text","enabled","tone","arrow","width","height","accessibilityName","leftPadding","rightPadding","indicatorSize","cornerRadius","fontSize","lineHeight","backgroundColor","hoverColor","pressedColor","disabledColor","textColor","disabledTextColor","focusColor","onClick","onPressed","onReleased","onCanceled"},true);
        else if(combo) values=inputs(view,{"id","text","enabled","tone","arrow","width","onClick","onPressed","onReleased","onCanceled"},true);
        else if(stepper&&options.framework!="LVRS") values=inputs(view,{"id","enabled","tone","arrow","cornerRadius","width","height","accessibilityName","primaryColor","hoverColor","pressedColor","disabledColor","textColor","disabledTextColor","onStepped","onClick","onPressed","onReleased","onCanceled"},true);
        else if(stepper) values=inputs(view,{"id","enabled","tone","arrow","cornerRadius","onStepped","onClick","onPressed","onReleased","onCanceled"},true);
        else if(segmented&&options.framework!="LVRS") values=inputs(view,{"id","horizontalPadding","verticalPadding","spacing","borderWidth","cornerRadius","shapeStyle","forceBorderlessTone","backgroundColor","borderColor","enabled"});
        else if(segmented) values=inputs(view,{"id","horizontalPadding","verticalPadding","spacing","borderWidth","cornerRadius","shapeStyle","forceBorderlessTone"});
        else if(menu&&options.framework!="LVRS") values=inputs(view,{"id","items","targetId","compactItems","itemWidth","itemSpacing","leftPadding","rightPadding","topPadding","bottomPadding","edgeMargin","cornerRadius","menuOpacity","selectedIndex","autoCloseOnTrigger","dismissOnGlobalPress","dismissOnGlobalContextRequest","showIconSlot","accessibilityName","baseColor","backgroundColor","borderColor","onItemTriggered","onOpened","onClosed","onCanceled"});
        else if(menu) values=inputs(view,{"id","items","targetId","compactItems","itemWidth","itemSpacing","leftPadding","rightPadding","topPadding","bottomPadding","edgeMargin","cornerRadius","menuOpacity","selectedIndex","autoCloseOnTrigger","dismissOnGlobalPress","dismissOnGlobalContextRequest","showIconSlot","onItemTriggered","onOpened","onClosed"});
        else if(menu_item&&options.framework!="LVRS") values=inputs(view,{"id","label","shortcut","key","keyPlaceholder","iconName","icon","iconSource","iconRevision","enabled","compact","state","selectionDirection","keyVisible","showIconSlot","showChevron","hasChildItems","expanded","itemWidth","itemHeight","iconSize","chevronSize","horizontalPadding","spacing","fontSize","lineHeight","accessibilityName","keepOpen","closeOnTrigger","onClick","onPressed","onReleased","onCanceled"},true);
        else if(menu_item) values=inputs(view,{"id","label","shortcut","key","keyPlaceholder","iconName","enabled","compact","state","selectionDirection","keyVisible","showIconSlot","showChevron","hasChildItems","expanded","itemWidth","itemHeight","iconSize","chevronSize","keepOpen","closeOnTrigger","onClick"},true);
        else if(divider && options.framework!="LVRS") values=inputs(view,{"id","axis","thickness","crossPadding","linePadding","lineLength","dividerColor"});
        else if(divider) values=inputs(view,{"id","axis","thickness","crossPadding","linePadding","lineLength"});
        else if(kind=="RadioButton" && options.framework!="LVRS") values=inputs(view,{"id","text","enabled","available","checked","state","group","onToggled","accessibilityName","indicatorSize","dotSize","labelGap","motionEnabled","motion","transitionDuration","onColor","offColor","onColorDisabled","offColorDisabled","dotColor","dotColorDisabled","textColor","disabledTextColor"});
        else if(kind=="CheckBox" && options.framework!="LVRS") values=inputs(view,{"id","text","enabled","checked","group","onToggled","accessibilityName","boxSize","framePadding","boxRadius","checkMarkStrokeWidth","labelGap","checkedColor","uncheckedColor","disabledCheckedColor","disabledUncheckedColor","checkColor","checkMarkColorDisabled","boxBorderWidthCheckedEnabled","boxBorderWidthUncheckedEnabled","boxBorderWidthCheckedDisabled","boxBorderWidthUncheckedDisabled","boxBorderColorCheckedEnabled","boxBorderColorUncheckedEnabled","boxBorderColorCheckedDisabled","boxBorderColorUncheckedDisabled","textColor","disabledTextColor"});
        else if(kind=="ToggleSwitch" && options.framework!="LVRS") values=inputs(view,{"id","text","enabled","checked","group","onToggled","accessibilityName","rightToLeft","motionEnabled","motion","transitionDuration","trackWidth","trackHeight","trackPadding","knobSize","trackCornerRadius","knobCornerRadius","labelGap","onColor","onColorHover","onColorPressed","offColor","offColorHover","offColorPressed","disabledTrackColor","knobFillColor","disabledKnobColor","textColor","disabledTextColor"});
        else if(check) values=inputs(view,{"id","text","enabled","checked","group","onToggled"});
        else if(kind=="InputField"&&options.framework!="LVRS") values=inputs(view,{"id","text","placeholder","enabled","readOnly","width","height","onEdited","onSubmit","multiline","password","accessibilityName","fontFamily","fontSize","lineHeight","horizontalPadding","verticalPadding","cornerRadius","backgroundColor","hoverColor","textColor","placeholderColor","selectionColor","caretColor","focusColor"});
        else if(kind=="InputField") values=inputs(view,{"id","text","placeholder","enabled","readOnly","width","height","onEdited","onSubmit","multiline","password"});
        else if(kind=="Slider"&&options.framework!="LVRS") values=inputs(view,{"id","from","to","value","stepSize","width","enabled","thumbDiameter","trackHeight","rangeInset","height","accessibilityName","trackColor","fillColor","thumbColor","focusColor","onMoved"});
        else if(kind=="Slider") values=inputs(view,{"id","from","to","value","stepSize","width","enabled","thumbDiameter","trackHeight","rangeInset","height","onMoved"});
        else if(progress_bar)values=inputs(view,{"id","minimumValue","maximumValue","endValue","startValue","currentValue","stateModel","minimumValueStateKey","maximumValueStateKey","startValueStateKey","currentValueStateKey","size","shapeStyle","width","height","largeHeight","regularHeight","cornerRadius","trackColor","fillColor","motionEnabled","motion","displayedStart","displayedProgress"});
        else if(kind=="ProgressBar") values=inputs(view,{"id","minimumValue","maximumValue","endValue","startValue","currentValue","stateModel","minimumValueStateKey","maximumValueStateKey","startValueStateKey","currentValueStateKey","size","shapeStyle","width","height","largeHeight","regularHeight","cornerRadius","trackColor","fillColor","motionEnabled","motion","displayedStart","displayedProgress"});
        else if(color_surface&&options.framework!="LVRS") values=inputs(view,{"id","model","modelRevision","field","width","height","markerDiameter","enabled","accessibilityName","onColorEdited"});
        else if(color_surface) values=inputs(view,{"id","model","field","width","height","enabled","accessibilityName","onColorEdited"});
        else if(color_picker&&options.framework!="LVRS") values=inputs(view,{"id","type","model","modelRevision","width","padding","enabled","showRecentColors","showActions","rememberOnAccept","recentColors","applyText","cancelText","accessibilityName","onColorEdited","onAccepted","onCanceled","onEyedropperRequested"});
        else if(color_picker) values=inputs(view,{"id","type","model","width","padding","enabled","showRecentColors","showActions","recentColors","applyText","cancelText","onColorEdited","onAccepted","onCanceled","onEyedropperRequested"});
        else if(color_button&&options.framework!="LVRS") values=inputs(view,{"id","model","modelRevision","currentColor","buttonSize","padding","cornerRadius","enabled","showHueRing","circular","borderOpacity","accessibilityName","backgroundColor","hoverColor","pressedColor","borderColor","focusColor","onClick","onPressed","onReleased","onCanceled"},true);
        else if(color_button) values=inputs(view,{"id","model","currentColor","buttonSize","enabled","showHueRing","circular","borderOpacity","accessibilityName","onClick","onPressed","onReleased","onCanceled"},true);
        else if(popover&&options.framework!="LVRS") values=inputs(view,{"id","width","height","padding","margins","cornerRadius","surfaceColor","surfaceOpacity","modal","dim","focus","clip","closeOnEscape","closeOnPressOutside","closeOnContextRequest","accessibilityName","onOpened","onClosed","onCanceled"});
        else if(popover) values=inputs(view,{"id","width","height","padding","margins","cornerRadius","surfaceColor","surfaceOpacity","modal","dim","focus","clip","closeOnEscape","closeOnPressOutside","accessibilityName","onOpened","onClosed"});
        else if(sheet) values=inputs(view,{"id","open","title","description","presentation","detent","cornerRadius","showHeader","showDescription","showCloseButton","showGrabber","scrollContent","preferredWidth","preferredHeight","desktopMargin","contentPadding","topSafeInset","bottomSafeInset","dismissOnBackground","dismissOnEscape","dismissOnDrag","motionEnabled","motion","animationDuration","surfaceColor","backdropColor","accessibilityName","onOpened","onClosed"});
        else if(modal&&options.framework!="LVRS") values=inputs(view,{"id","open","title","description","desc","message","iconName","iconSource","iconResolver","iconRevision","showIcon","iconSize","iconCornerRadius","iconBackgroundColor","iconBorderColor","buttonCount","primaryText","secondaryText","tertiaryText","primaryEnabled","secondaryEnabled","tertiaryEnabled","minWidth","maxWidth","preferredWidth","sidePadding","frameMinHeight","verticalOffset","frameCornerRadius","shapeStyle","frameColor","backdropColor","dismissOnBackground","dismissOnEscape","motionEnabled","motion","enterDuration","exitDuration","motionOvershoot","initialScale","contentPadding","sectionSpacing","actionSpacing","accessibilityName","onPrimaryClicked","onSecondaryClicked","onTertiaryClicked","onCanceled","onOpened","onClosed"});
        else if(modal) values=inputs(view,{"id","open","title","description","desc","message","iconName","iconSource","showIcon","iconSize","iconCornerRadius","iconBackgroundColor","iconBorderColor","buttonCount","primaryText","secondaryText","tertiaryText","primaryEnabled","secondaryEnabled","tertiaryEnabled","minWidth","maxWidth","preferredWidth","sidePadding","frameMinHeight","verticalOffset","frameCornerRadius","shapeStyle","frameColor","backdropColor","dismissOnBackground","motionEnabled","motion","accessibilityName","onPrimaryClicked","onSecondaryClicked","onTertiaryClicked","onCanceled","onOpened","onClosed"});
        else if(alert_button&&options.framework!="LVRS") values=inputs(view,{"id","text","enabled","tone","dialogStyle","width","height","horizontalPadding","cornerRadius","textColor","primaryColor","accessibilityName","method","onClick","onPressed","onReleased","onCanceled"},true);
        else if(alert_button) values=inputs(view,{"id","text","enabled","tone","dialogStyle","height","horizontalPadding","cornerRadius","textColor","primaryColor","method","onClick","onPressed","onReleased","onCanceled"},true);
        else if(alert&&options.framework!="LVRS") values=inputs(view,{"id","open","title","message","description","buttonCount","primaryText","secondaryText","tertiaryText","button1Text","button2Text","button3Text","button1Method","button2Method","button3Method","primaryEnabled","secondaryEnabled","tertiaryEnabled","secondaryDestructive","dismissOnBackground","closeOnEscape","minWidth","maxWidth","preferredWidth","sidePadding","cardCornerRadius","shapeStyle","backdropColor","cardBackgroundColor","glassEnabled","glassBlurRadius","shadowEnabled","motionEnabled","motion","enterDuration","exitDuration","motionOvershoot","initialScale","showIcon","appIconSource","imageSource","iconSource","iconName","iconResolver","iconRevision","iconFrameSize","primaryColor","accessibilityName","onPrimaryClicked","onSecondaryClicked","onTertiaryClicked","onDismissed","onOpened","onClosed"});
        else if(alert) values=inputs(view,{"id","open","title","message","description","buttonCount","primaryText","secondaryText","tertiaryText","button1Text","button2Text","button3Text","button1Method","button2Method","button3Method","primaryEnabled","secondaryEnabled","tertiaryEnabled","secondaryDestructive","dismissOnBackground","closeOnEscape","minWidth","maxWidth","preferredWidth","sidePadding","cardCornerRadius","shapeStyle","backdropColor","cardBackgroundColor","glassEnabled","glassBlurRadius","shadowEnabled","shadowColor","shadowBlurRadius","shadowOffsetX","shadowOffsetY","motionEnabled","motion","showIcon","appIconSource","imageSource","appIconSize","iconFrameSize","appIconBackgroundColor","appIconFrameColor","appIconInnerColor","primaryColor","accessibilityName","onPrimaryClicked","onSecondaryClicked","onTertiaryClicked","onDismissed","onOpened","onClosed"});
        else error(view.position,"Component '"+std::string(kind)+"' is not supported by native AOT yet");
        bool composite_list_item=false;
        if(kind=="ListItem"&&options.framework!="LVRS"){
            for(const auto name:{"checked","toggleEnabled","selectionEnabled","showToggle","showSelection","primaryAction","secondaryAction","moreMenu","showPrimaryAction","showSecondaryAction","showMoreMenu","actionWidth","slotPadding","slotSpacing","quantity","minimumQuantity","maximumQuantity","stepSize","quantityWidth","quantitySpacing","stepperEnabled","showStepper","selector","selectorIndex","selectorWidth","selectorEnabled","showSelector","onCheckedChanged","onEdited","onActionTriggered","onQuantityChanged","onSelectorIndexChanged"})
                composite_list_item|=values.contains(name);
            for(const auto name:{"type","size"})if(auto found=values.find(name);found!=values.end()){
                if(native_expression(found->second->value))composite_list_item=true;
                else {const auto value=resolve(found->second->value,found->second->position);
                    composite_list_item|=value.kind==ValueKind::enumeration&&(value.text==".Toggle"||value.text==".toggle"||value.text==".Checkable"||value.text==".checkable"||value.text==".Action"||value.text==".action"||value.text==".ActionGroup"||value.text==".actionGroup"||value.text==".Stepper"||value.text==".stepper"||value.text==".Select"||value.text==".select");}
            }
        }
        const auto children=component_kinds(view.children);
        if(kind=="HierarchyToolbar"&&options.framework!="LVRS"&&!children.empty())error(view.position,"Independent HierarchyToolbar currently accepts buttonItems, not direct children");
        if(kind!="HierarchyToolbar" && !stack && !link && !material && !media_card && !card && !header && !tabbar && !segmented && !menu && !menu_item && !popover && !modal && !sheet && !tooltip && !children.empty()) error(view.position,"This C++ control cannot accept children");
        if(media_card&&options.framework!="LVRS"&&!children.empty())error(view.position,"Independent Card does not yet accept child views");
        if(media_card&&values.contains("previewComponent")&&!children.empty())error(view.position,"Use previewComponent or inline Card content, not both");
        if(modal&&children.size()>1)error(view.position,"Modal accepts one content View; wrap children in a stack");
        if(popover){unsigned content=0;for(const auto child:children)if(child!="Menu"&&child!="ContextMenu"&&child!="Popover")++content;if(content>1)error(view.position,"Popover accepts one content View; wrap children in a stack");}
        if(toolbar&&kind!="HierarchyToolbar"&&!children.empty())error(view.position,"This toolbar component has fixed native slots and accepts no child views");
        if(tabbar)for(const auto child:children)if(child!="Tab")error(view.position,"TabBar accepts Tab children only");
        if(tabbar&&options.framework!="LVRS")for(const auto& child:view.children)
            if(!child.component||child.component->spelling!="Tab"||child.condition||child.repetition||child.fragment||!child.bindings.empty()||!child.imports.empty())
                error(child.position,"Independent TabBar currently requires direct, unconditional Tab children");
        if(menu)for(const auto child:children)if(child!="MenuItem"&&child!="ContextMenuItem"&&child!="MenuDivider"&&child!="ContextMenuDivider"&&child!="Menu"&&child!="ContextMenu")error(view.position,"Menu accepts menu items, dividers and owned popups only");
        if(menu&&options.framework!="LVRS")for(const auto& child:view.children)
            if(!child.component||(child.component->spelling!="MenuItem"&&child.component->spelling!="ContextMenuItem"&&child.component->spelling!="MenuDivider"&&child.component->spelling!="ContextMenuDivider")||child.condition||child.repetition||child.fragment||!child.bindings.empty()||!child.imports.empty())
                error(child.position,"Independent Menu currently requires direct, unconditional item or divider children");
        if(menu_item)for(const auto child:children)if(child!="Menu"&&child!="ContextMenu")error(view.position,"MenuItem children must be owned popups");
        if(menu_item&&options.framework!="LVRS"&&!view.children.empty()){
            if(view.children.size()!=1)error(view.position,"Independent MenuItem accepts one direct submenu");
            const auto& child=view.children.front();
            if(!child.component||(child.component->spelling!="Menu"&&child.component->spelling!="ContextMenu")||child.condition||child.repetition||child.fragment||!child.bindings.empty()||!child.imports.empty())
                error(child.position,"Independent submenu must be a direct, unconditional Menu");
            if(values.contains("hasChildItems")&&!native_expression(values.at("hasChildItems")->value)&&boolean(values,"hasChildItems",true)=="false")
                error(values.at("hasChildItems")->position,"A submenu requires hasChildItems");
        }
        if(menu_item&&options.framework!="LVRS"&&active_menu_registration.empty()&&(values.contains("keepOpen")||values.contains("closeOnTrigger")))
            error(view.position,"MenuItem close policy requires an independent Menu parent");
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
        if(tooltip&&options.framework=="LVRS")call=tooltip_call(args,values,view);
        else if(tooltip){validate_owned_tooltip(values,view);owned_tooltip_runtime=true;native_runtime=true;needs_parent_scope=true;}
        else if(code_editor&&options.framework=="LVRS")call=code_editor_call(args,values);
        else if(code_editor){
            if(!view.children.empty())error(view.position,"Independent CodeEditor does not accept child views");
            if(values.contains("height")&&values.contains("editorHeight"))error(values.at("height")->position,"Use height or editorHeight for CodeEditor");
            if(values.contains("model")&&values.contains("text"))error(values.at("text")->position,"Use a CodeEditor source or initial text");
            if(values.contains("editor")&&!values.contains("model"))error(values.at("editor")->position,"A CodeEditor editor requires a source model");
            if(values.contains("sourceRevision")&&!values.contains("model"))error(values.at("sourceRevision")->position,"A source revision requires a CodeEditor model");
            if(values.contains("text")&&native_expression(values.at("text")->value))error(values.at("text")->position,"Reactive code text requires an injected document model");
            if(values.contains("readOnly")&&native_expression(values.at("readOnly")->value))error(values.at("readOnly")->position,"CodeEditor readOnly is fixed when the view is created");
            (void)boolean(values,"readOnly",false);
            if(values.contains("editor")&&boolean(values,"readOnly",false)=="true")error(values.at("editor")->position,"Read-only CodeEditor cannot accept an editor");
            if(values.contains("sourceRevision")&&!native_expression(values.at("sourceRevision")->value)) {
                const auto revision=numeric(values,"sourceRevision",0,0,9007199254740991.);
                if(std::trunc(revision)!=revision)error(values.at("sourceRevision")->position,"CodeEditor sourceRevision must be integral");
            }
            if(values.contains("model"))(void)scoped_member(values,"model");
            if(values.contains("editor"))(void)scoped_member(values,"editor");
            owned_code_editor_runtime=true;native_runtime=true;needs_parent_scope=true;
        }
        else if(material){if(options.framework=="LVRS")call=material_call(kind,args,values);else owned_material_runtime=true;}
        else if((media_card||avatar)&&options.framework=="LVRS")call=media_call(kind,args,values);
        else if(media_card){owned_media_card_runtime=true;native_runtime=true;needs_parent_scope=true;}
        else if(avatar){owned_avatar_runtime=true;native_runtime=true;needs_parent_scope=true;}
        else if(kind=="ListToolbar"&&options.framework!="LVRS"){owned_list_toolbar_runtime=true;native_runtime=true;needs_parent_scope=true;}
        else if(kind=="ListFooter"&&options.framework!="LVRS"){owned_list_footer_runtime=true;native_runtime=true;needs_parent_scope=true;}
        else if(kind=="HierarchyToolbar"&&options.framework!="LVRS"){owned_hierarchy_toolbar_runtime=true;native_runtime=true;needs_parent_scope=true;}
        else if(kind=="ToolbarButton"&&options.framework!="LVRS"){owned_toolbar_button_runtime=true;native_runtime=true;needs_parent_scope=true;}
        else if(kind=="ListItem"&&options.framework!="LVRS"){if(composite_list_item)owned_list_item_composite_runtime=true;else owned_list_item_runtime=true;native_runtime=true;needs_parent_scope=true;}
        else if(toolbar)call=toolbar_call(kind,args,values);
        else if(router&&options.framework=="LVRS"){
            call=prefix+"page_router("+args+", "+scoped_member(values,"router")+", {"+decimal(numeric(values,"width",640,1,8192))+", "+decimal(numeric(values,"height",400,1,8192))+"})";
        } else if(router){
            owned_router_runtime=true;native_runtime=true;needs_parent_scope=true;
        } else
        if(link&&options.framework!="LVRS"){
            owned_link_runtime=true;native_runtime=true;needs_parent_scope=true;
        } else if(link){
            const auto href=alias_key(values,"href","to");std::string tone="ii::gui::ButtonTone::normal";
            if(auto found=values.find("tone");found!=values.end()){
                if(!property(found->second->value).empty())tone=link_value(values,"tone","");
                else{const std::map<std::string,std::string,std::less<>> tones{{".primary","primary"},{".default","normal"},{".borderless","borderless"},{".destructive","destructive"},{".disabled","disabled"}};auto selected=tones.find(found->second->value.text);if(found->second->value.kind!=ValueKind::enumeration||selected==tones.end())error(found->second->position,"Unknown Link tone");tone="ii::gui::ButtonTone::"+selected->second;}
            }
            const auto optional_size=[&](std::string_view name){return values.contains(name)?decimal(numeric(values,name,0,0,8192)):"std::nullopt";};
            call="lvrs::Link::create(widgets, "+args+", {.text = "+link_text(values,"text")+", .href = "+link_text(values,href,"/")+", .params = "+link_value(values,"params","{}")+", .router = "+link_value(values,"router","{}")+", .navigator = "+link_value(values,"navigator","{}")+", .target_component = "+link_value(values,"targetComponent","{}")+", .replace = "+link_bool(values,"replace",false)+", .enabled = "+link_bool(values,"enabled",true)+", .underline = "+link_bool(values,"underline",false)+", .tone = "+tone+", .link_color = "+link_color(values,"linkColor")+", .hover_color = "+link_color(values,"hoverColor")+", .pressed_color = "+link_color(values,"pressedColor")+", .disabled_color = "+link_color(values,"disabledColor")+", .horizontal_padding = "+decimal(numeric(values,"horizontalPadding",14,0,8192))+", .vertical_padding = "+decimal(numeric(values,"verticalPadding",10,0,8192))+", .minimum_height = "+decimal(numeric(values,"minimumHeight",36,0,8192))+", .corner_radius = "+decimal(numeric(values,"cornerRadius",8,0,8192))+", .width = "+optional_size("width")+", .height = "+optional_size("height")+", .show_focus_ring = "+boolean(values,"showFocusRing",true)+", .accessibility_name = "+literal(text(values,"accessibilityName"))+"}, {.method = "+button_method(values,"method")+", .methods = "+link_methods(values)+", .clicked = "+action(values)+", .pressed = "+action(values,"onPressed")+", .released = "+action(values,"onReleased")+", .canceled = "+action(values,"onCanceled")+", .navigated = "+action(values,"onNavigated","bool accepted","accepted")+", .navigation_failed = "+action(values,"onNavigationFailed","const ii::core::Error& error","error")+"})";
        } else
        if(card&&options.framework=="LVRS")call="lvrs::AppCard::create(widgets, "+args+", {.width = "+surface_number(values,"width","0")+", .height = "+surface_number(values,"height","0")+", .padding = "+surface_number(values,"padding","18")+", .spacing = "+surface_number(values,"spacing","10")+", .corner_radius = "+surface_number(values,"cornerRadius","12")+", .background = "+surface_color(values,"backgroundColor","ii::gui::WidgetColor{.1139791608,.1166428775,.1166428775,1}")+", .title = "+link_text(values,"title")+", .subtitle = "+link_text(values,"subtitle")+", .shape = "+surface_shape(values)+", .enabled = "+link_bool(values,"enabled",true)+", .title_color = "+surface_color(values,"titleColor","ii::gui::WidgetColor{1,1,1,.9}")+", .subtitle_color = "+surface_color(values,"subtitleColor","ii::gui::WidgetColor{1,1,1,.8}")+"})";
        else if(card){owned_card_runtime=true;native_runtime=true;needs_parent_scope=true;}
        else if(header&&options.framework=="LVRS")call="lvrs::AppHeader::create(widgets, "+args+", {.title = "+link_text(values,"title")+", .subtitle = "+link_text(values,"subtitle")+", .menu_visible = "+link_bool(values,"menuVisible",false)+", .compact = "+link_bool(values,"compact",false)+", .enabled = "+link_bool(values,"enabled",true)+", .content_horizontal_padding = "+surface_number(values,"contentHorizontalPadding","std::nullopt")+", .content_vertical_padding = "+surface_number(values,"contentVerticalPadding","std::nullopt")+", .row_spacing = "+surface_number(values,"rowSpacing","std::nullopt")+", .action_spacing = "+surface_number(values,"actionSpacing","std::nullopt")+", .menu_button_padding = "+surface_number(values,"menuButtonPadding","std::nullopt")+", .width = "+surface_number(values,"width","0")+", .height = "+surface_number(values,"height","0")+", .background = "+surface_color(values,"backgroundColor","ii::gui::WidgetColor{.083941631,.083941631,.086313099,1}")+", .title_color = "+surface_color(values,"titleColor","ii::gui::WidgetColor{1,1,1,.9}")+", .subtitle_color = "+surface_color(values,"subtitleColor","ii::gui::WidgetColor{1,1,1,.8}")+", .menu_background = "+surface_color(values,"menuBackgroundColor","ii::gui::WidgetColor{.1139791608,.1166428775,.1166428775,1}")+", .menu_pressed_background = "+surface_color(values,"menuPressedBackgroundColor","ii::gui::WidgetColor{.1230252981,.1252803057,.1275353134,1}")+"}, {.menu_clicked = "+action(values,"onMenuClicked")+"})";
        else if(header){owned_header_runtime=true;native_runtime=true;needs_parent_scope=true;}
        else if(listener&&options.framework=="LVRS"){
            std::string input="{}";if(values.contains("onInput"))input="[scope](const ii::gui::WidgetInput& value) -> ii::core::Result<bool> { return "+scoped_member(values,"onInput")+"(value); }";
            call=prefix+"event_listener("+args+", {"+decimal(numeric(values,"width",640,1,8192))+", "+decimal(numeric(values,"height",480,1,8192))+"}, "+input+", "+action(values,"onResized","ii::gui::Rect value")+")";
        }
        else if(listener){owned_listener_runtime=true;native_runtime=true;needs_parent_scope=true;}
        else if(list_panel){
            if(!view.children.empty())error(view.position,"Independent List accepts a model and slots rather than child views");
            if(values.contains("model")&&values.contains("controller"))error(values.at("model")->position,"Use a List source or controller, not both");
            if(values.contains("model"))(void)scoped_member(values,"model");
            owned_list_panel_runtime=true;native_runtime=true;needs_parent_scope=true;
        }
        else if(hierarchy_panel){
            if(!view.children.empty())error(view.position,"Independent Hierarchy accepts models and slots rather than child views");
            owned_hierarchy_panel_runtime=true;native_runtime=true;needs_parent_scope=true;
        }
        else if(list){
            call=prefix+(kind=="Hierarchy"?"hierarchy":"list")+"("+args+", {.model = "+scoped_member(values,"model")+", .width = "+decimal(numeric(values,"width",240,32,8192))+", .height = "+decimal(numeric(values,"height",400,24,8192))+", .row_height = "+decimal(numeric(values,"rowHeight",28,18,256))+", .indent = "+decimal(numeric(values,"indent",16,0,128))+", .hierarchy = "+std::string(kind=="Hierarchy"?"true":"false")+", .reorderable = "+boolean(values,"reorderable",true)+", .editable = "+boolean(values,"editable",true)+", .show_checks = "+boolean(values,"showChecks",false)+", .background = "+widget_color(values,"backgroundColor","ii::gui::WidgetColor{.1,.1,.1,1}")+", .selection_color = "+widget_color(values,"selectionColor","ii::gui::WidgetColor{.18,.31,.52,1}")+", .text_color = "+widget_color(values,"textColor","ii::gui::WidgetColor{.92,.92,.92,1}")+", .selection_radius = "+decimal(numeric(values,"selectionRadius",3,0,128))+", .font_size = "+decimal(numeric(values,"fontSize",13,1,256))+"}, {.selected = "+action(values,"onSelectionChanged","std::string value")+", .activated = "+action(values,"onItemActivated","std::string value")+", .renamed = "+action(values,"onItemRenamed","std::string key, std::string text","key, text")+", .moved = "+action(values,"onItemMoved","std::string key, std::string parent, std::string before","key, parent, before")+", .checked = "+action(values,"onItemChecked","std::string key, bool value","key, value")+"})";
        }
        else if(table_view&&options.framework!="LVRS"){owned_table_runtime=true;native_runtime=true;needs_parent_scope=true;}
        else if(table_row&&options.framework!="LVRS"){owned_table_row_runtime=true;native_runtime=true;needs_parent_scope=true;}
        else if(table_cell&&options.framework!="LVRS"){owned_table_cell_item_runtime=true;native_runtime=true;needs_parent_scope=true;}
        else if(table_header&&options.framework!="LVRS"){owned_table_header_runtime=true;native_runtime=true;needs_parent_scope=true;}
        else if(table_cell||table_row||table_header||table_view)call=table_call(kind,args,values);
        else if(alert_button&&options.framework!="LVRS"){
            owned_alert_button_runtime=true;native_runtime=true;needs_parent_scope=true;
        }
        else if(alert_button){
            std::string tone="normal";if(auto item=values.find("tone");item!=values.end()){const auto& input=*item->second;const auto v=resolve(input.value,input.position);const std::map<std::string,std::string,std::less<>> tones{{".primary","primary"},{".default","normal"},{".borderless","borderless"},{".destructive","destructive"},{".disabled","disabled"}};auto found=tones.find(v.text);if(v.kind!=ValueKind::enumeration||found==tones.end())error(input.position,"Unknown AlertButton tone");tone=found->second;}
            std::string geometry;for(auto [name,field]:{std::pair{"height","height"},{"horizontalPadding","horizontal_padding"},{"cornerRadius","corner_radius"}})if(values.contains(name))geometry+=", ."+std::string(field)+" = "+decimal(numeric(values,name,0,std::string_view(name)=="height"?1:0,8192));
            call=prefix+"alert_button("+args+", {.text = "+literal(text(values,"text","Button"))+", .enabled = "+boolean(values,"enabled",true)+", .dialog_style = "+boolean(values,"dialogStyle",false)+", .tone = ii::gui::ButtonTone::"+tone+geometry+", .text_color = "+widget_color(values,"textColor","std::nullopt")+", .primary_color = "+widget_color(values,"primaryColor","std::nullopt")+"}, {.method = "+button_method(values,"method")+", .clicked = "+action(values)+", .pressed = "+action(values,"onPressed")+", .released = "+action(values,"onReleased")+", .canceled = "+action(values,"onCanceled")+"})";
        }
        else if(alert&&options.framework!="LVRS"){
            owned_alert_runtime=true;native_runtime=true;needs_parent_scope=true;
            if(auto item=values.find("open");item!=values.end()&&native_expression(item->second->value))error(item->second->position,"Independent Alert open must be a static initial value");
            if(boolean(values,"open",false)=="true"){if(factory_depth)error(view.position,"Initial open inside a content factory is not supported; open after activation");initial_open.push_back(id);}
        }
        else if(alert){
            const auto motion=scoped_member(values,"motion");
            const auto message_key=alias_key(values,"message","description"),first=alias_key(values,"primaryText","button1Text"),second=alias_key(values,"secondaryText","button2Text"),third=alias_key(values,"tertiaryText","button3Text"),image_key=alias_key(values,"appIconSource","imageSource");
            const auto minimum=numeric(values,"minWidth",280,0,8192),maximum=numeric(values,"maxWidth",500,1,8192),count=numeric(values,"buttonCount",0,-1e9,1e9);if(minimum>maximum)error(view.position,"Alert minWidth exceeds maxWidth");if(std::trunc(count)!=count)error(view.position,"Alert buttonCount requires an integer");
            std::string shape="round_rect";if(auto item=values.find("shapeStyle");item!=values.end()){auto value=resolve(item->second->value,item->second->position);if(value.kind==ValueKind::enumeration&&(value.text==".cylinder"||value.text==".shapeCylinder"))shape="cylinder";else if(value.kind!=ValueKind::enumeration||(value.text!=".roundRect"&&value.text!=".shapeRoundRect"))error(item->second->position,"Unknown Alert shapeStyle");}
            std::string image="{}",asset="std::nullopt",builtin="true";if(auto item=values.find(image_key);item!=values.end()){const auto& input=*item->second;const auto& value=input.value;if(value.kind==ValueKind::string){if(value.text.empty())builtin="false";else if(value.text=="alert-file-text.svg"||value.text=="qrc:/qt/qml/LVRS/resources/images/alert-file-text.svg")asset="lvrs::AlertIcon::file_text";else if(value.text=="alert-adjustments.svg"||value.text=="qrc:/qt/qml/LVRS/resources/images/alert-adjustments.svg")asset="lvrs::AlertIcon::adjustments";else error(input.position,"Native imageSource requires a scoped image or an embedded Alert asset");}else{const auto dot=value.text.find('.');auto backend=backends.find(value.text.substr(0,dot));auto member=dot==value.text.npos?std::string{}:value.text.substr(dot+1);if(value.kind!=ValueKind::reference||backend==backends.end()||!identifier(member))error(input.position,"Native imageSource requires one scoped image member");image=backend_access.at(backend->first).object+"."+member;builtin="false";}}
            if(boolean(values,"open",false)=="true"){if(factory_depth)error(view.position,"Initial open inside a content factory is not supported; open after activation");initial_open.push_back(id);}
            call=prefix+"alert("+args+", {.title = "+literal(text(values,"title","Alert Dialog"))+", .message = "+literal(text(values,message_key,"It can have 2 or 3 actions depending on your needs."))+", .button_count = "+decimal(count)+", .primary_text = "+literal(text(values,first,"Button"))+", .secondary_text = "+literal(text(values,second,"Button"))+", .tertiary_text = "+literal(text(values,third))+", .primary_enabled = "+boolean(values,"primaryEnabled",true)+", .secondary_enabled = "+boolean(values,"secondaryEnabled",true)+", .tertiary_enabled = "+boolean(values,"tertiaryEnabled",true)+", .secondary_destructive = "+(values.contains("secondaryDestructive")?boolean(values,"secondaryDestructive",false):"std::nullopt")+", .dismiss_on_background = "+boolean(values,"dismissOnBackground",false)+", .dismiss_on_escape = "+boolean(values,"closeOnEscape",false)+", .minimum_width = "+decimal(minimum)+", .maximum_width = "+decimal(maximum)+", .preferred_width = "+decimal(numeric(values,"preferredWidth",500,1,8192))+", .side_padding = "+decimal(numeric(values,"sidePadding",24,0,8192))+", .corner_radius = "+decimal(numeric(values,"cardCornerRadius",36,0,8192))+", .shape = lvrs::AlertShape::"+shape+", .backdrop_color = "+widget_color(values,"backdropColor","ii::gui::WidgetColor{0,0,0,89./255}")+", .card_color = "+widget_color(values,"cardBackgroundColor","ii::gui::WidgetColor{29./255,31./255,33./255,.72}")+", .glass_enabled = "+boolean(values,"glassEnabled",true)+", .glass_blur_radius = "+decimal(numeric(values,"glassBlurRadius",28,-8192,8192))+", .shadow_enabled = "+boolean(values,"shadowEnabled",true)+", .shadow = {.offset_x = "+decimal(numeric(values,"shadowOffsetX",0,-8192,8192))+", .offset_y = "+decimal(numeric(values,"shadowOffsetY",16,-8192,8192))+", .blur_radius = "+decimal(numeric(values,"shadowBlurRadius",32,0,64))+", .color = "+widget_color(values,"shadowColor","ii::gui::WidgetColor{0,0,0,.28}")+"}, .motion_enabled = "+boolean(values,"motionEnabled",true)+", .motion = "+motion+", .show_icon = "+boolean(values,"showIcon",true)+", .use_builtin_image = "+builtin+", .image_source = "+image+", .builtin_icon = "+asset+", .icon_size = "+(values.contains("appIconSize")?decimal(numeric(values,"appIconSize",64,1,8192)):"std::nullopt")+", .icon_frame_size = "+(values.contains("iconFrameSize")?decimal(numeric(values,"iconFrameSize",86,1,8192)):"std::nullopt")+", .icon_background = "+widget_color(values,"appIconBackgroundColor","std::nullopt")+", .icon_border = "+widget_color(values,"appIconFrameColor","std::nullopt")+", .primary_color = "+widget_color(values,"primaryColor","std::nullopt")+", .icon_inner = "+widget_color(values,"appIconInnerColor","ii::gui::WidgetColor{244./255,245./255,247./255,1}")+", .accessibility_name = "+literal(text(values,"accessibilityName","Alert"))+"}, {.button1_method = "+button_method(values,"button1Method")+", .button2_method = "+button_method(values,"button2Method")+", .button3_method = "+button_method(values,"button3Method")+", .primary_clicked = "+action(values,"onPrimaryClicked")+", .secondary_clicked = "+action(values,"onSecondaryClicked")+", .tertiary_clicked = "+action(values,"onTertiaryClicked")+", .dismissed = "+action(values,"onDismissed")+", .opened = "+action(values,"onOpened")+", .closed = "+action(values,"onClosed")+"})";
        }
        else if(sheet&&options.framework!="LVRS") {
            owned_sheet_runtime=true;native_runtime=true;needs_parent_scope=true;
            if(auto item=values.find("open");item!=values.end()&&native_expression(item->second->value))error(item->second->position,"Independent Sheet open must be a static initial value");
            if(boolean(values,"open",false)=="true"){if(factory_depth)error(view.position,"Initial open inside a content factory is not supported; open after activation");initial_open.push_back(id);}
        }
        else if(sheet) {
            const auto choice=[&](std::string_view key,std::initializer_list<std::string_view> names,std::string fallback){auto found=values.find(key);if(found==values.end())return fallback;auto value=resolve(found->second->value,found->second->position);for(auto name:names){auto upper=std::string(name);upper[0]=char(upper[0]-'a'+'A');if(value.kind==ValueKind::enumeration&&(value.text=="."+std::string(name)||value.text=="."+upper))return std::string(name);}error(found->second->position,"Unknown Sheet "+std::string(key));};
            const auto duration=numeric(values,"animationDuration",420,0,86400000);if(duration!=std::trunc(duration))error(view.position,"Sheet animationDuration requires integral milliseconds");
            call=prefix+"sheet("+args+", {.title = "+literal(text(values,"title"))+", .description = "+literal(text(values,"description"))+", .presentation = lvrs::SheetPresentation::"+choice("presentation",{"automatic","mobile","desktop"},"automatic")+", .detent = lvrs::SheetDetent::"+choice("detent",{"fit","medium","large"},"fit")+", .corner_radius = "+(values.contains("cornerRadius")?decimal(numeric(values,"cornerRadius",16,0,8192)):"std::nullopt")+", .show_header = "+boolean(values,"showHeader",true)+", .show_description = "+boolean(values,"showDescription",true)+", .show_close_button = "+boolean(values,"showCloseButton",true)+", .show_grabber = "+(values.contains("showGrabber")?boolean(values,"showGrabber",false):"std::nullopt")+", .scroll_content = "+boolean(values,"scrollContent",true)+", .preferred_width = "+decimal(numeric(values,"preferredWidth",560,0,8192))+", .preferred_height = "+decimal(numeric(values,"preferredHeight",420,0,8192))+", .desktop_margin = "+decimal(numeric(values,"desktopMargin",24,0,8192))+", .content_padding = "+decimal(numeric(values,"contentPadding",20,0,8192))+", .top_safe_inset = "+decimal(numeric(values,"topSafeInset",0,0,8192))+", .bottom_safe_inset = "+decimal(numeric(values,"bottomSafeInset",0,0,8192))+", .dismiss_on_background = "+boolean(values,"dismissOnBackground",true)+", .dismiss_on_escape = "+boolean(values,"dismissOnEscape",true)+", .dismiss_on_drag = "+boolean(values,"dismissOnDrag",true)+", .motion_enabled = "+boolean(values,"motionEnabled",true)+", .motion = "+scoped_member(values,"motion")+", .animation_duration = std::chrono::milliseconds("+decimal(duration)+"), .surface_color = "+widget_color(values,"surfaceColor","ii::gui::WidgetColor{.1370945126,.1402995437,.1419020444,1}")+", .backdrop_color = "+widget_color(values,"backdropColor","ii::gui::WidgetColor{0,0,0,.48}")+", .accessibility_name = "+literal(text(values,"accessibilityName","Sheet"))+"}, {.opened = "+action(values,"onOpened")+", .closed = "+action(values,"onClosed")+"}, "+modal_content(view,"SheetContentFactory",false)+")";
            if(boolean(values,"open",false)=="true"){if(factory_depth)error(view.position,"Initial open inside a content factory is not supported; open after activation");initial_open.push_back(id);}
        }
        else if(modal&&options.framework!="LVRS") {
            owned_modal_runtime=true;native_runtime=true;needs_parent_scope=true;
            if(auto item=values.find("open");item!=values.end()&&native_expression(item->second->value))error(item->second->position,"Independent Modal open must be a static initial value");
            if(boolean(values,"open",false)=="true"){if(factory_depth)error(view.position,"Initial open inside a content factory is not supported; open after activation");initial_open.push_back(id);}
        }
        else if(modal) {
            const auto motion=scoped_member(values,"motion");
            if(values.contains("description")&&values.contains("desc"))error(view.position,"Use description or its desc alias, not both");
            const auto minimum=numeric(values,"minWidth",520,0,8192),maximum=numeric(values,"maxWidth",980,1,8192);if(minimum>maximum)error(view.position,"Modal minWidth exceeds maxWidth");
            std::string shape="round_rect";if(auto item=values.find("shapeStyle");item!=values.end()){auto value=resolve(item->second->value,item->second->position);if(value.kind==ValueKind::enumeration&&(value.text==".cylinder"||value.text==".shapeCylinder"))shape="cylinder";else if(value.kind!=ValueKind::enumeration||(value.text!=".roundRect"&&value.text!=".shapeRoundRect"))error(item->second->position,"Unknown Modal shapeStyle");}
            std::string image="{}";if(auto item=values.find("iconSource");item!=values.end()){const auto& input=*item->second;const auto& value=input.value;const auto dot=value.text.find('.');auto backend=backends.find(value.text.substr(0,dot));auto member=dot==value.text.npos?std::string{}:value.text.substr(dot+1);if(value.kind!=ValueKind::reference||backend==backends.end()||!identifier(member))error(input.position,"iconSource requires one scoped native image member");image=backend_access.at(backend->first).object+"."+member;}
            if(boolean(values,"open",false)=="true"){if(factory_depth)error(view.position,"Initial open inside a content factory is not supported; open after activation");initial_open.push_back(id);}
            call=prefix+"modal("+args+", {.title = "+literal(text(values,"title"))+", .description = "+literal(text(values,values.contains("desc")?"desc":"description"))+", .message = "+literal(text(values,"message"))+", .icon_name = "+literal(text(values,"iconName"))+", .icon_source = "+image+", .show_icon = "+boolean(values,"showIcon",true)+", .icon_size = "+decimal(numeric(values,"iconSize",72,1,8192))+", .icon_corner_radius = "+decimal(numeric(values,"iconCornerRadius",16,0,8192))+", .icon_background = "+widget_color(values,"iconBackgroundColor","ii::gui::WidgetColor{.1411764771,.1411764771,.1411764771,1}")+", .icon_border = "+widget_color(values,"iconBorderColor","ii::gui::WidgetColor{208./255,215./255,226./255,.18}")+", .button_count = "+decimal(std::trunc(numeric(values,"buttonCount",0,-1e9,1e9)))+", .primary_text = "+literal(text(values,"primaryText","OK"))+", .secondary_text = "+literal(text(values,"secondaryText"))+", .tertiary_text = "+literal(text(values,"tertiaryText"))+", .primary_enabled = "+boolean(values,"primaryEnabled",true)+", .secondary_enabled = "+boolean(values,"secondaryEnabled",true)+", .tertiary_enabled = "+boolean(values,"tertiaryEnabled",true)+", .minimum_width = "+decimal(minimum)+", .maximum_width = "+decimal(maximum)+", .preferred_width = "+decimal(numeric(values,"preferredWidth",760,1,8192))+", .side_padding = "+decimal(numeric(values,"sidePadding",24,0,8192))+", .minimum_height = "+decimal(numeric(values,"frameMinHeight",210,0,8192))+", .vertical_offset = "+decimal(numeric(values,"verticalOffset",-16,-32767,32767))+", .corner_radius = "+decimal(numeric(values,"frameCornerRadius",12,0,8192))+", .shape = lvrs::ModalShape::"+shape+", .frame_color = "+widget_color(values,"frameColor","ii::gui::WidgetColor{.1370945126,.1402995437,.1419020444,1}")+", .backdrop_color = "+widget_color(values,"backdropColor","ii::gui::WidgetColor{0,0,0,89./255}")+", .dismiss_on_background = "+boolean(values,"dismissOnBackground",true)+", .motion_enabled = "+boolean(values,"motionEnabled",true)+", .motion = "+motion+", .accessibility_name = "+literal(text(values,"accessibilityName","Modal"))+"}, {.primary_clicked = "+action(values,"onPrimaryClicked")+", .secondary_clicked = "+action(values,"onSecondaryClicked")+", .tertiary_clicked = "+action(values,"onTertiaryClicked")+", .canceled = "+action(values,"onCanceled")+", .opened = "+action(values,"onOpened")+", .closed = "+action(values,"onClosed")+"}, "+modal_content(view)+")";
        }
        else if(popover&&options.framework!="LVRS") {
            owned_popover_runtime=true;native_runtime=true;needs_parent_scope=true;
        }
        else if(popover) {
            std::string surface;
            if(auto item=values.find("surfaceColor");item!=values.end()){
                surface = ", .surface_color = " + widget_color(values, "surfaceColor", "std::nullopt");
            }
            call=prefix+"popover("+args+", {.width = "+decimal(numeric(values,"width",0,0,8192))+", .height = "+decimal(numeric(values,"height",0,0,8192))+", .padding = "+decimal(numeric(values,"padding",12,0,8192))+", .margins = "+decimal(numeric(values,"margins",8,0,32767))+", .corner_radius = "+decimal(numeric(values,"cornerRadius",12,0,32767))+", .surface_opacity = "+decimal(numeric(values,"surfaceOpacity",.25,0,1))+surface+", .modal = "+boolean(values,"modal",false)+", .dim = "+boolean(values,"dim",false)+", .focus_on_open = "+boolean(values,"focus",true)+", .clip_content = "+boolean(values,"clip",false)+", .dismiss_on_escape = "+boolean(values,"closeOnEscape",true)+", .dismiss_on_outside_press = "+boolean(values,"closeOnPressOutside",true)+", .accessibility_name = "+literal(text(values,"accessibilityName","Popover"))+"}, {.opened = "+action(values,"onOpened")+", .closed = "+action(values,"onClosed")+"})";
        }
        else if(color_button&&options.framework!="LVRS") {
            owned_color_button_runtime=true;native_runtime=true;needs_parent_scope=true;
        }
        else if(color_button) {
            std::string model="{}",current,size="medium";
            if(values.contains("model")&&values.contains("currentColor"))error(view.position,"ColorPickerButton accepts either model or currentColor");
            if(auto item=values.find("model");item!=values.end()) {
                const auto& input=*item->second;const auto& value=input.value;const auto dot=value.text.find('.');auto backend=backends.find(value.text.substr(0,dot));const auto member=dot==value.text.npos?std::string{}:value.text.substr(dot+1);
                if(value.kind!=ValueKind::reference||backend==backends.end()||!identifier(member))error(input.position,"Color model must be one public scoped C++ member");model=backend_access.at(backend->first).object+"."+member;
            }
            if(auto item=values.find("currentColor");item!=values.end()) {
                current = ", .current_color = " + widget_color(values, "currentColor", "std::nullopt");
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
        else if(color_picker&&options.framework!="LVRS") {
            owned_color_picker_runtime=true;owned_color_picker_default_runtime|=!values.contains("model");native_runtime=true;needs_parent_scope=true;
        }
        else if(color_picker) {
            std::string model="{}",type="wheel",recent;
            if(auto item=values.find("model");item!=values.end()) {
                const auto& input=*item->second;const auto& value=input.value;const auto dot=value.text.find('.');auto backend=backends.find(value.text.substr(0,dot));
                const auto member=dot==value.text.npos?std::string{}:value.text.substr(dot+1);
                if(value.kind!=ValueKind::reference || backend==backends.end() || !identifier(member))error(input.position,"Color model must be one public scoped C++ member");
                model=backend_access.at(backend->first).object+"."+member;
            }
            if(auto item=values.find("type");item!=values.end()) {
                const std::map<std::string,std::string,std::less<>> types{{".Wheel","wheel"},{".wheel","wheel"},{".HueSaturation","hue_saturation"},{".hueSaturation","hue_saturation"},{".SaturationBrightness","saturation_brightness"},{".saturationBrightness","saturation_brightness"},{".Grayscale","grayscale"},{".grayscale","grayscale"},{".RGB","rgb"},{".rgb","rgb"},{".CMYK","cmyk"},{".cmyk","cmyk"}};
                const auto entry=types.find(item->second->value.text);if(item->second->value.kind!=ValueKind::enumeration || entry==types.end())error(item->second->position,"Unknown color picker type");type=entry->second;
            }
            if(auto item=values.find("recentColors");item!=values.end()) {
                auto value = resolve(item->second->value, item->second->position);
                if (value.kind != ValueKind::list)
                    error(item->second->position, "Recent colors must be a Color/HEX String list");
                recent=", .recent_colors = std::vector<ii::gui::WidgetColor>{";unsigned count=0;
                for(const auto& element:value.elements) {
                    if (count++ == 10)
                        break;
                    if (count > 1)
                        recent += ", ";
                    recent += color_literal(color_value(element, item->second->position));
                }recent+="}";
            }
            const auto width=numeric(values,"width",320,320,1024),padding=numeric(values,"padding",12,0,64);if(width-2*padding<228)error(view.position,"ColorPicker padding must leave 228 content units");
            call=prefix+"color_picker("+args+", {.type = lvrs::ColorPickerType::"+type+", .model = "+model+", .width = "+decimal(width)+", .padding = "+decimal(padding)+", .enabled = "+boolean(values,"enabled",true)+", .show_recent_colors = "+boolean(values,"showRecentColors",true)+", .show_actions = "+boolean(values,"showActions",true)+recent+", .apply_text = "+literal(text(values,"applyText","Apply"))+", .cancel_text = "+literal(text(values,"cancelText","Cancel"))+"}, {.color_edited = "+action(values,"onColorEdited","ii::gui::WidgetColor value")+", .accepted = "+action(values,"onAccepted","ii::gui::WidgetColor value")+", .canceled = "+action(values,"onCanceled")+", .eyedropper_requested = "+action(values,"onEyedropperRequested")+"})";
        }
        else if(color_surface&&options.framework!="LVRS") {
            owned_color_surface_runtime=true;native_runtime=true;needs_parent_scope=true;
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
            call=prefix+"color_surface("+args+", {.model = "+backend_access.at(backend->first).object+"."+member+", .field = ii::gui::ColorField::"+field+
                ", .width = "+decimal(numeric(values,"width",field=="hue_rail" || field=="brightness_rail"?18:232,12,1024))+", .height = "+decimal(numeric(values,"height",232,12,1024))+
                ", .enabled = "+boolean(values,"enabled",true)+", .accessibility_name = "+literal(text(values,"accessibilityName","Color field"))+"}, "+action(values,"onColorEdited","ii::gui::WidgetColor value")+")";
        }
        else if(stack){
            call = prefix +
                   (kind == "VStack"   ? "vstack"
                    : kind == "HStack" ? "hstack"
                                       : "zstack") +
                   "(" + args + ", {})";
        }
        else if(kind=="Spacer") call=prefix+"spacer("+args+", "+decimal(numeric(values,"minLength",0,0,32767))+")";
        else if(native_view)call="ii::gui::NativeView::create(widgets, "+args+", "+native_view_options(values,true)+")";
        else if(image_view){if(options.framework=="LVRS")call=image_call(values,args);}
        else if(kind=="Label") {
            const auto [role,size]=text_role(values);
            const auto font_size = numeric(values, "fontSize", size, 1, 256);
            const auto line_height = values.contains("fontSize")
                                         ? font_size
                                         : token_number(text_metric(role) + "-line-height", 1, 8192);
            const auto weight = token_number(
                "--lvrs-theme-typography-weight-" +
                    std::string(role == "title" || role == "title2"                              ? "bold"
                                : role == "header" || role == "header2" || role == "description" ? "semi-bold"
                                : role == "body"                                                 ? "medium"
                                                                                                 : "regular"),
                100, 900);
            const auto font_weight = numeric(values, "fontWeight", weight, 400, 700);
            if (font_weight != 400 && font_weight != 500 && font_weight != 600 && font_weight != 700)
                error(view.position, "fontWeight must be 400/500/600/700 for native fonts");
            call = prefix + "label(" + args + ", {.text = " + text_expression(values, "text") +
                   ", .font_size = " + decimal(numeric(values, "fontSize", size, 1, 256)) +
                   ", .selectable = " + boolean(values, "selectable", true) +
                   ", .role = ii::gui::TextRole::" + role +
                   ", .color = " + widget_color(values, "color", color_literal(text_color(role))) +
                   ", .line_height = " + decimal(numeric(values, "lineHeight", line_height, 0, 8192)) +
                   ", .font_weight = " + decimal(font_weight) + "})";
        } else if(dropdown && options.framework!="LVRS") {
            owned_dropdown_runtime=true;native_runtime=true;needs_parent_scope=true;
        } else if(button && options.framework!="LVRS") {
            owned_button_runtime=true;
            if(kind=="HelpButton")owned_help_runtime=true;
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
        } else if(tab&&options.framework!="LVRS") {
            owned_tab_runtime=true;native_runtime=true;needs_parent_scope=true;
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
        } else if(tabbar&&options.framework!="LVRS") {
            owned_tabbar_runtime=true;native_runtime=true;needs_parent_scope=true;
        } else if(tabbar) {
            const auto current=numeric(values,"currentIndex",0,-1,4095);if(current!=std::trunc(current)) error(view.position,"TabBar currentIndex must be an integer");
            call=prefix+"tab_bar("+args+", {.spacing = "+decimal(numeric(values,"spacing",4,0,32767))+", .minimum_tab_width = "+decimal(numeric(values,"minimumTabWidth",120,1,8192))+
                ", .current_index = "+decimal(current)+", .auto_select = "+boolean(values,"autoSelect",true)+"}, "+action(values,"onCurrentIndexChanged","int value")+")";
        } else if(combo&&options.framework!="LVRS") {
            owned_combo_box_runtime=true;native_runtime=true;needs_parent_scope=true;
        } else if(stepper&&options.framework!="LVRS") {
            owned_stepper_runtime=true;native_runtime=true;needs_parent_scope=true;
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
        } else if(segmented&&options.framework!="LVRS") {
            owned_segmented_runtime=true;native_runtime=true;needs_parent_scope=true;
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
        } else if(menu&&options.framework!="LVRS") {
            owned_menu_runtime=true;owned_menu_item_runtime=true;native_runtime=true;needs_parent_scope=true;
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
        } else if(menu_item&&options.framework!="LVRS") {
            owned_menu_item_runtime=true;native_runtime=true;needs_parent_scope=true;
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
        } else if(divider && options.framework!="LVRS") {
            owned_divider_runtime=true;
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
        } else if(kind=="RadioButton" && options.framework!="LVRS") {
            owned_radio_runtime=true;native_runtime=true;needs_parent_scope=true;
        } else if(kind=="CheckBox" && options.framework!="LVRS") {
            owned_checkbox_runtime=true;
        } else if(kind=="ToggleSwitch" && options.framework!="LVRS") {
            owned_toggle_runtime=true;
        } else if(check) {
            const auto method=kind=="CheckBox"?"check_box":kind=="RadioButton"?"radio_button":"toggle_switch";
            call=prefix+method+"("+args+", {.text = "+text_expression(values,"text")+", .enabled = "+boolean(values,"enabled",true)+
                ", .checked = "+boolean(values,"checked",false)+", .group = "+literal(text(values,"group"))+"}, "+action(values,"onToggled","bool value")+")";
        } else if(kind=="InputField"&&options.framework!="LVRS") {
            owned_input_field_runtime=true;native_runtime=true;needs_parent_scope=true;
        } else if(kind=="InputField") {
            call=prefix+"input_field("+args+", {.text = "+text_expression(values,"text")+", .placeholder = "+literal(text(values,"placeholder"))+
                ", .enabled = "+boolean(values,"enabled",true)+", .read_only = "+boolean(values,"readOnly",false)+", .width = "+decimal(numeric(values,"width",206,1,8192))+
                (values.contains("height")?", .style = ii::gui::InputStyle{.height = "+decimal(numeric(values,"height",30,1,8192))+"}":"") +
                ", .multiline = "+boolean(values,"multiline",false)+", .password = "+boolean(values,"password",false)+"}, "+action(values,"onEdited","std::string value")+", "+action(values,"onSubmit")+")";
        } else if(kind=="Slider"&&options.framework!="LVRS") {
            owned_slider_runtime=true;native_runtime=true;needs_parent_scope=true;
        } else if(kind=="Slider") {
            const auto minimum=numeric(values,"from",0,-1e12,1e12),maximum=numeric(values,"to",1,-1e12,1e12);
            if(minimum>=maximum) error(view.position,"Slider from must be smaller than to");
            call=prefix+"slider("+args+", {.minimum = "+decimal(minimum)+", .maximum = "+decimal(maximum)+", .value = "+number_expression(values,"value",.5,-1e12,1e12)+
                ", .step = "+decimal(numeric(values,"stepSize",0,0,32767))+", .width = "+decimal(numeric(values,"width",320,18,8192))+", .enabled = "+boolean(values,"enabled",true)+", .thumb_diameter = "+decimal(numeric(values,"thumbDiameter",18,1,256))+", .track_height = "+decimal(numeric(values,"trackHeight",6,1,256))+", .range_inset = "+decimal(numeric(values,"rangeInset",9,0,128))+", .height = "+decimal(numeric(values,"height",32,1,8192))+"}, "+action(values,"onMoved","double value")+")";
        } else if(hierarchy_item&&options.framework!="LVRS"){
            owned_hierarchy_item_runtime=true;native_runtime=true;needs_parent_scope=true;
        } else if(hierarchy_item){
            const auto context=parent=="native_parent"?hierarchy_delegate_source:std::string{};
            if(!context.empty())for(auto field:{"controller","itemKey","itemId","indentLevel","width","itemWidth","height","rowHeight","selected","expanded","hasChildItems","dragPreviewActive"})if(auto found=values.find(field);found!=values.end())error(found->second->position,"Hierarchy itemDelegate geometry and structural state are controlled by the list");
            const auto count_factory=hierarchy_count_factory(values);
            call=(context.empty()?"lvrs::HierarchyItem::create(":context+".create_item(")+"widgets, "+args+", "+hierarchy_item_options(values,true,false,context)+", {.activated = "+action(values,"onClick")+", .expansion_changed = "+action(values,"onExpansionChanged","bool expanded","expanded")+"}, "+count_factory+")";
        } else if(hierarchy_list&&options.framework!="LVRS"){
            owned_hierarchy_list_runtime=true;native_runtime=true;needs_parent_scope=true;
        } else if(hierarchy_list){const auto delegate=hierarchy_delegate_factory(values);call="lvrs::HierarchyList::create(widgets, "+args+", "+hierarchy_list_options(values,true)+", {.active_changed = "+action(values,"onActiveChanged","std::string key, std::int64_t id, std::size_t index","key, id, index")+", .expansion_changed = "+action(values,"onExpansionChanged","std::string key, bool expanded","key, expanded")+", .ensure_visible = "+action(values,"onEnsureVisibleRequested","double y, double height","y, height")+", .item_moved = "+action(values,"onItemMoved","const lvrs::HierarchyMovePlan& plan","plan")+"}, {}, "+delegate+")";
        } else if(progress_bar){
            call="lvrs::ProgressBar::create(widgets, "+args+", "+progress_options(values,true)+")";
        } else {
            owned_progress_runtime=true;
            call=prefix+"absolute_stack("+args+", {progress_extent_"+variable+".width, progress_extent_"+variable+".height})";
        }
        if(sheet&&options.framework!="LVRS"){
            const auto native="sheet_"+variable,derived="sheet_inputs_"+variable;
            const auto content=modal_content(view,"SheetContentFactory",false);
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="open"&&pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeSheetCallbacks{.opened = "+action(values,"onOpened")+", .closed = "+action(values,"onClosed")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_sheet_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeSheet::create(widgets, "+args+", "+derived+"->get(), "+callbacks+", "+content+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::SheetInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeSheet::create(widgets, "+args+", "+owned_sheet_options(values,true)+", "+callbacks+", "+content+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(alert&&options.framework!="LVRS"){
            const auto native="alert_"+variable,derived="alert_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="open"&&pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&pair.first!="button1Method"&&pair.first!="button2Method"&&pair.first!="button3Method"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto click=[&](std::string_view name){auto callback=action(values,name);return callback=="{}"?std::string("std::function<void()>{}"):callback;};
            const auto method=[&](std::string_view name){auto callback=button_method(values,name,"lvui::native::AlertButtonEvent");return callback=="{}"?std::string("std::function<void(const lvui::native::AlertButtonEvent&)>{}"):callback;};
            const auto callbacks="lvui::native::NativeAlertCallbacks{.methods = std::array<std::function<void(const lvui::native::AlertButtonEvent&)>, 3>{"+method("button1Method")+", "+method("button2Method")+", "+method("button3Method")+"}, .clicked = std::array<std::function<void()>, 3>{"+click("onPrimaryClicked")+", "+click("onSecondaryClicked")+", "+click("onTertiaryClicked")+"}, .opened = "+action(values,"onOpened")+", .closed = "+action(values,"onClosed")+", .canceled = "+action(values,"onDismissed")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_alert_options(values,false,view.position)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeAlert::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::AlertInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeAlert::create(widgets, "+args+", "+owned_alert_options(values,true,view.position)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(alert_button&&options.framework!="LVRS"){
            const auto native="alert_button_"+variable,derived="alert_button_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="method"&&pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeAlertButtonCallbacks{.method = "+button_method(values,"method","lvui::native::AlertButtonEvent")+
                ", .clicked = "+action(values)+", .pressed = "+action(values,"onPressed")+", .released = "+action(values,"onReleased")+", .canceled = "+action(values,"onCanceled")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_alert_button_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeAlertButton::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::AlertButtonConfiguration& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeAlertButton::create(widgets, "+args+", "+owned_alert_button_options(values,true)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(modal&&options.framework!="LVRS"){
            const auto native="modal_"+variable,derived="modal_inputs_"+variable;
            const auto content=modal_content(view);
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="open"&&pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto action_callback=[&](std::string_view name){auto callback=action(values,name);return callback=="{}"?std::string("std::function<void()>{}"):callback;};
            const auto callbacks="lvui::native::NativeModalCallbacks{.actions = std::array<std::function<void()>, 3>{"+action_callback("onPrimaryClicked")+", "+action_callback("onSecondaryClicked")+", "+action_callback("onTertiaryClicked")+"}, .opened = "+action(values,"onOpened")+", .closed = "+action(values,"onClosed")+", .canceled = "+action(values,"onCanceled")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_modal_options(values,false,view.position)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeModal::create(widgets, "+args+", "+derived+"->get(), "+callbacks+", "+content+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::ModalInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeModal::create(widgets, "+args+", "+owned_modal_options(values,true,view.position)+", "+callbacks+", "+content+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(color_picker&&options.framework!="LVRS"){
            const auto native="color_picker_"+variable,derived="color_picker_inputs_"+variable;
            const bool owns_session=!values.contains("model");
            const auto session="color_picker_session_"+variable;
            if(owns_session){
                const auto editor="color_picker_editor_"+variable,source="color_picker_source_"+variable;
                body+="            auto "+editor+" = std::make_shared<lvui::ColorSelection>();\n"
                      "            auto "+source+" = std::make_shared<lvui::ColorSelectionSurface>("+editor+");\n"
                      "            auto "+session+" = std::make_shared<lvui::ColorPickerSession>(std::make_shared<lvui::ColorPicker>("+editor+"), "+source+", "+source+");\n";
            }
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeColorPickerCallbacks{.edited = "+action(values,"onColorEdited","ii::gui::WidgetColor value")+
                ", .accepted = "+action(values,"onAccepted","ii::gui::WidgetColor value")+", .canceled = "+action(values,"onCanceled")+
                ", .eyedropper_requested = "+action(values,"onEyedropperRequested")+"}";
            if(dynamic){
                const auto capture=owns_session?"[scope, "+session+"]":"[scope]";
                body+="            auto "+derived+"_result = native_bindings->derive("+capture+"([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_color_picker_options(values,false,view.position,session)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeColorPicker::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::ColorPickerInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeColorPicker::create(widgets, "+args+", "+owned_color_picker_options(values,true,view.position,session)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(color_surface&&options.framework!="LVRS"){
            const auto native="color_surface_"+variable,derived="color_surface_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callback=action(values,"onColorEdited","ii::gui::WidgetColor value");
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_color_surface_options(values,false,view.position)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeColorSurface::create(widgets, "+args+", "+derived+"->get(), "+callback+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::ColorSurfaceInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeColorSurface::create(widgets, "+args+", "+owned_color_surface_options(values,true,view.position)+", "+callback+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(color_button&&options.framework!="LVRS"){
            const auto native="color_button_"+variable,derived="color_button_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeColorPickerButtonCallbacks{.clicked = "+action(values)+", .pressed = "+action(values,"onPressed")+", .released = "+action(values,"onReleased")+", .canceled = "+action(values,"onCanceled")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_color_button_options(values,false,view.position)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeColorPickerButton::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::ColorPickerButtonInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeColorPickerButton::create(widgets, "+args+", "+owned_color_button_options(values,true,view.position)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(popover&&options.framework!="LVRS"){
            const auto native="popover_"+variable,derived="popover_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativePopoverCallbacks{.opened = "+action(values,"onOpened")+", .closed = "+action(values,"onClosed")+", .canceled = "+action(values,"onCanceled")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_popover_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativePopover::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::PopoverConfiguration& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativePopover::create(widgets, "+args+", "+owned_popover_options(values,true)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(menu&&options.framework!="LVRS"){
            const auto native="menu_"+variable,derived="menu_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="items"&&pair.first!="targetId"&&pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeMenuCallbacks{.triggered = "+action(values,"onItemTriggered","int value")+", .opened = "+action(values,"onOpened")+", .closed = "+action(values,"onClosed")+", .canceled = "+action(values,"onCanceled")+"}";
            const bool context=kind=="ContextMenu";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_menu_options(values,false,context)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeMenu::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::MenuInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeMenu::create(widgets, "+args+", "+owned_menu_options(values,true,context)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(tabbar&&options.framework!="LVRS"){
            const auto native="tabbar_"+variable,derived="tabbar_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_tabbar_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeTabBar::create(widgets, "+args+", "+derived+"->get(), "+action(values,"onCurrentIndexChanged","int value")+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::TabBarConfiguration& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeTabBar::create(widgets, "+args+", "+owned_tabbar_options(values,true)+", "+action(values,"onCurrentIndexChanged","int value")+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(list_panel){
            const auto native="list_panel_"+variable,derived="list_panel_inputs_"+variable;
            const bool owns_controller=!values.contains("controller");const auto controller="list_panel_controller_"+variable;
            if(owns_controller){
                const auto model="list_panel_model_"+variable,result="list_panel_controller_result_"+variable;
                if(values.contains("model"))body+="            auto "+result+" = lvui::ListController::create(lvui::native::widget_value<ii::gui::WidgetColor>("+scoped_member(values,"model")+"));\n";
                else body+="            auto "+model+" = lvui::ListMemory::create();\n            if (!"+model+") return ii::core::fail(ii::core::ErrorCode::invalid_argument, "+model+".error().message);\n"
                           "            auto "+result+" = lvui::ListController::create(*"+model+");\n";
                body+="            if (!"+result+") return ii::core::fail(ii::core::ErrorCode::invalid_argument, "+result+".error().message);\n"
                      "            auto "+controller+" = std::make_shared<lvui::ListController>(std::move(*"+result+"));\n";
            }
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="model"&&pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeListPanelCallbacks{.item_activated = "+action(values,"onItemActivated","const lvui::native::ListViewEvent& event","event.index, event.entry")+
                ", .toolbar_clicked = "+action(values,"onToolbarIconTriggered","std::size_t index, std::string icon","index, icon")+
                ", .footer_clicked = "+action(values,"onFooterButtonTriggered","std::size_t index, const lvui::ListFooterSlot& slot","index, slot")+"}";
            if(dynamic){
                const auto capture=owns_controller?"[scope, "+controller+"]":"[scope]";
                body+="            auto "+derived+"_result = native_bindings->derive("+capture+"([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_list_panel_options(values,false,controller)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeListPanel::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::ListPanelInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeListPanel::create(widgets, "+args+", "+owned_list_panel_options(values,true,controller)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(hierarchy_panel){
            const auto native="hierarchy_panel_"+variable,derived="hierarchy_panel_inputs_"+variable;
            const bool owns_controller=!values.contains("controller");const auto controller="hierarchy_panel_controller_"+variable;
            if(owns_controller)body+="            auto "+controller+" = std::make_shared<lvui::HierarchyController>(std::make_shared<lvui::HierarchyMemory>());\n";
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeHierarchyPanelCallbacks{.item_activated = "+action(values,"onItemActivated","const lvui::native::HierarchyListEvent& event","event.key, event.id.value_or(-1), event.source_index")+
                ", .expansion_changed = "+action(values,"onExpansionChanged","std::string key, bool expanded","key, expanded")+
                ", .item_moved = "+action(values,"onItemMoved","const lvui::HierarchyMovePlan& plan","plan")+
                ", .toolbar_active_changed = "+action(values,"onToolbarActiveChanged","const lvui::HierarchyToolbarSelection& value","value")+
                ", .toolbar_triggered = "+action(values,"onToolbarTriggered","const lvui::HierarchyToolbarSelection& value","value")+
                ", .footer_clicked = "+action(values,"onFooterClicked","std::size_t index, const lvui::ListFooterSlot& slot","index, slot")+"}";
            if(dynamic){
                const auto capture=owns_controller?"[scope, "+controller+"]":"[scope]";
                body+="            auto "+derived+"_result = native_bindings->derive("+capture+"([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_hierarchy_panel_options(values,false,controller)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeHierarchyPanel::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::HierarchyPanelInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeHierarchyPanel::create(widgets, "+args+", "+owned_hierarchy_panel_options(values,true,controller)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(hierarchy_item&&options.framework!="LVRS"){
            const auto native="hierarchy_item_"+variable,derived="hierarchy_item_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeHierarchyItemCallbacks{.activated = "+action(values,"onClick")+
                ", .expansion_changed = "+action(values,"onExpansionChanged","bool expanded","expanded")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_hierarchy_item_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeHierarchyItem::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::HierarchyItemInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeHierarchyItem::create(widgets, "+args+", "+owned_hierarchy_item_options(values,true)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(hierarchy_list&&options.framework!="LVRS"){
            const auto native="hierarchy_list_"+variable,derived="hierarchy_list_inputs_"+variable;
            const bool owns_controller=!values.contains("controller");
            const auto controller="hierarchy_list_controller_"+variable;
            if(owns_controller)body+="            auto "+controller+" = std::make_shared<lvui::HierarchyController>(std::make_shared<lvui::HierarchyMemory>());\n";
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeHierarchyListCallbacks{.activated = "+action(values,"onActiveChanged","const lvui::native::HierarchyListEvent& event","event.key, event.id.value_or(-1), event.source_index")+
                ", .expansion_changed = "+action(values,"onExpansionChanged","std::string key, bool expanded","key, expanded")+
                ", .moved = "+action(values,"onItemMoved","const lvui::HierarchyMovePlan& plan","plan")+"}";
            if(dynamic){
                const auto capture=owns_controller?"[scope, "+controller+"]":"[scope]";
                body+="            auto "+derived+"_result = native_bindings->derive("+capture+"([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_hierarchy_list_options(values,false,controller)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeHierarchyList::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::HierarchyListInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeHierarchyList::create(widgets, "+args+", "+owned_hierarchy_list_options(values,true,controller)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(hierarchy_item||hierarchy_list){const auto native="hierarchy_"+variable;
            body+="            auto "+native+" = "+call+";\n            if (!"+native+") return std::unexpected("+native+".error());\n            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
            const auto context=hierarchy_item&&parent=="native_parent"?hierarchy_delegate_source:std::string{};
            if(!context.empty()||std::ranges::any_of(values,[&](const auto& pair){return pair.first!="countView"&&pair.first!="itemDelegate"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);}))body+="            { auto binding = native_bindings->bind([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+(hierarchy_item?hierarchy_item_options(values,false,false,context):hierarchy_list_options(values,false))+"; }, ["+(context.empty()?"component = *"+native:"delegate_context = "+context)+"](const auto& value) { return "+(context.empty()?"component.set_options(value)":"delegate_context.apply(value)")+"; });\n              if (!binding) return std::unexpected(binding.error()); }\n";
        }
        else if(progress_bar){const auto native="progress_"+variable;
            body+="            auto "+native+" = "+call+";\n            if (!"+native+") return std::unexpected("+native+".error());\n            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
            emit_progress_bindings(values,native);
        }
        else if(table_view&&options.framework!="LVRS"){
            const auto native="table_"+variable,derived="table_inputs_"+variable;
            const bool owns_controller=!values.contains("controller")&&!values.contains("model");
            const auto controller="table_controller_"+variable;
            if(owns_controller)body+="            auto "+controller+" = std::make_shared<lvui::TableController>(std::make_shared<lvui::TableMemory>());\n";
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeTableCallbacks{.selection_changed = "+action(values,"onSelectionChanged","std::optional<lvui::TableSelection> value")+
                ", .edited = "+action(values,"onCellInputEdited","const lvui::native::TableCellEvent& event","event.row_index, event.column_index, event.text")+
                ", .submitted = "+action(values,"onCellInputSubmitted","const lvui::native::TableCellEvent& event","event.row_index, event.column_index, event.text")+
                ", .rejected = "+action(values,"onCellInputRejected","const lvui::native::TableCellEvent& event, [[maybe_unused]] const lvui::TableError& error","event.row_index, event.column_index, event.text, event.value_type")+
                ", .range_values_changed = "+action(values,"onRangeValuesChanged","const lvui::TableRange& value","value")+
                ", .sorted = "+action(values,"onRowsSorted","std::size_t column, lvui::TableSortOrder order","column, order")+
                ", .structure_changed = "+action(values,"onStructureChanged","const lvui::TableStructureEdit& value","value")+
                ", .column_resized = "+action(values,"onColumnResized","std::size_t column, double width","column, width")+
                ", .row_resized = "+action(values,"onRowResized","std::size_t row, double height","row, height")+"}";
            if(dynamic){
                const auto capture=owns_controller?"[scope, "+controller+"]":"[scope]";
                body+="            auto "+derived+"_result = native_bindings->derive("+capture+"([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_table_options(values,false,controller,view.position)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeTable::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::TableInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeTable::create(widgets, "+args+", "+owned_table_options(values,true,controller,view.position)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(kind=="HierarchyToolbar"&&options.framework!="LVRS"){
            const auto native="hierarchy_toolbar_"+variable,derived="hierarchy_toolbar_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeHierarchyToolbarCallbacks{.active_changed = "+action(values,"onActiveChanged","const lvui::HierarchyToolbarSelection& value","value")+
                ", .triggered = "+action(values,"onButtonTriggered","const lvui::HierarchyToolbarSelection& value","value")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_hierarchy_toolbar_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeHierarchyToolbar::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::HierarchyToolbarInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeHierarchyToolbar::create(widgets, "+args+", "+owned_hierarchy_toolbar_options(values,true)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(kind=="ListFooter"&&options.framework!="LVRS"){
            const auto native="list_footer_"+variable,derived="list_footer_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeListFooterCallbacks{.clicked = "+action(values,"onButtonClicked","std::size_t index, const lvui::ListFooterSlot& slot","index, slot")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_list_footer_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeListFooter::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::ListFooterInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeListFooter::create(widgets, "+args+", "+owned_list_footer_options(values,true)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(kind=="ListItem"&&options.framework!="LVRS"&&composite_list_item){
            const auto native="list_item_composite_"+variable,derived="list_item_composite_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeListItemCompositeCallbacks{.activated = "+action(values)+
                ", .checked_changed = "+action(values,"onCheckedChanged","bool value","value")+
                ", .edited = "+action(values,"onEdited","std::string field, const lvui::ListItemValue& value","field, value")+
                ", .action_triggered = "+action(values,"onActionTriggered","std::string name, const lvui::ListFooterSlot& value","name, value")+
                ", .quantity_changed = "+action(values,"onQuantityChanged","double value","value")+
                ", .selector_index_changed = "+action(values,"onSelectorIndexChanged","int value","value")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_list_item_options(values,false,view.position,true)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeListItemComposite::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::ListItemCompositeInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeListItemComposite::create(widgets, "+args+", "+owned_list_item_options(values,true,view.position,true)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(kind=="ListItem"&&options.framework!="LVRS"){
            const auto native="list_item_"+variable,derived="list_item_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeListItemCallbacks{.activated = "+action(values)+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_list_item_options(values,false,view.position)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeListItem::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::ListItemInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeListItem::create(widgets, "+args+", "+owned_list_item_options(values,true,view.position)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(kind=="ToolbarButton"&&options.framework!="LVRS"){
            const auto native="toolbar_button_"+variable,derived="toolbar_button_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeToolbarButtonCallbacks{.clicked = "+action(values)+
                ", .triggered = "+action(values,"onTriggered","const lvui::ToolbarButtonId& value","value")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_toolbar_button_options(values,false,view.position)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeToolbarButton::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::ToolbarButtonInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeToolbarButton::create(widgets, "+args+", "+owned_toolbar_button_options(values,true,view.position)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(kind=="ListToolbar"&&options.framework!="LVRS"){
            const auto native="list_toolbar_"+variable,derived="list_toolbar_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeListToolbarCallbacks{.clicked = "+action(values,"onIconClicked","std::size_t index, std::string source","index, source")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_list_toolbar_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeListToolbar::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::ListToolbarInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeListToolbar::create(widgets, "+args+", "+owned_list_toolbar_options(values,true)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(toolbar){const auto native="toolbar_"+variable;body+="            auto "+native+" = "+call+";\n            if (!"+native+") return std::unexpected("+native+".error());\n            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";emit_toolbar_bindings(values,variable,native,kind);
            if(kind=="HierarchyToolbar")toolbar_parents["*"+variable]=native;
            if(kind=="ToolbarButton")if(auto owner=toolbar_parents.find(parent);owner!=toolbar_parents.end())body+="            if (auto attached = "+owner->second+"->register_button(*"+native+", false); !attached) return attached;\n";
        }
        else if(segmented&&options.framework!="LVRS"){
            const auto native="segmented_"+variable,derived="segmented_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&native_expression(pair.second->value);});
            const bool icon=kind=="IconSegmentedControl";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_segmented_options(values,false,icon)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeSegmentedControl::create(widgets, "+args+", "+derived+"->get());\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::SegmentedControlConfiguration& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeSegmentedControl::create(widgets, "+args+", "+owned_segmented_options(values,true,icon)+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
            const auto style="segmented_style_"+variable;
            native_style_slots+="        lvui::native::SegmentedRegistrationPort "+style+";\n";
            body+="            scope->"+style+" = "+native+"->weak_tone_registration_port();\n";
        }
        else if(combo&&options.framework!="LVRS"){
            const auto native="combo_box_"+variable,derived="combo_box_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeComboBoxCallbacks{.clicked = "+action(values)+", .pressed = "+action(values,"onPressed")+", .released = "+action(values,"onReleased")+", .canceled = "+action(values,"onCanceled")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_combo_box_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeComboBox::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::ComboBoxConfiguration& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeComboBox::create(widgets, "+args+", "+owned_combo_box_options(values,true)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(stepper&&options.framework!="LVRS"){
            const auto native="stepper_"+variable,derived="stepper_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeStepperCallbacks{.stepped = "+action(values,"onStepped","int value")+", .clicked = "+action(values)+", .pressed = "+action(values,"onPressed")+", .released = "+action(values,"onReleased")+", .canceled = "+action(values,"onCanceled")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_stepper_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeStepper::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::StepperConfiguration& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeStepper::create(widgets, "+args+", "+owned_stepper_options(values,true)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(kind=="InputField"&&options.framework!="LVRS"){
            const auto native="input_field_"+variable,derived="input_field_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeInputFieldCallbacks{.edited = "+action(values,"onEdited","std::string value")+", .submitted = "+action(values,"onSubmit")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_input_field_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeInputField::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::InputFieldConfiguration& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeInputField::create(widgets, "+args+", "+owned_input_field_options(values,true)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(kind=="Slider"&&options.framework!="LVRS"){
            const auto native="slider_"+variable,derived="slider_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="onMoved"&&pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&native_expression(pair.second->value);});
            const auto callbacks=action(values,"onMoved","double value");
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_slider_options(values,false,view.position)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeSlider::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::SliderConfiguration& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeSlider::create(widgets, "+args+", "+owned_slider_options(values,true,view.position)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(tooltip&&options.framework=="LVRS"){const auto native="tooltip_"+variable;body+="            auto "+native+" = "+call+";\n            if (!"+native+") return std::unexpected("+native+".error());\n            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";emit_tooltip_bindings(values,variable,native);if(values.contains("targetId"))context_bindings.push_back({native,text(values,"targetId"),values.at("targetId")->position,true});}
        else if(tooltip){
            const auto native="tooltip_"+variable;
            body+="            auto "+native+" = lvui::native::NativeTooltip::create(widgets, "+args+", "+owned_tooltip_configuration(values)+", "+owned_tooltip_style(values)+", "+owned_tooltip_target(values)+", lvui::TooltipCallbacks{.opened = "+action(values,"onOpened")+", .closed = "+action(values,"onClosed")+"});\n"
                  "            if (!"+native+") return std::unexpected("+native+".error());\n"
                  "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
            if(values.contains("targetId"))context_bindings.push_back({native,text(values,"targetId"),values.at("targetId")->position,true});
            if(values.contains("text")){
                const auto label="tooltip_text_"+variable,label_id=id+"/text";
                if(!ids.insert(label_id).second)error(view.position,"Tooltip text widget id conflicts with another widget");
                body+="            auto "+label+" = widgets.label("+native+"->content_root(), "+literal(label_id)+", {.text = "+text_expression(values,"text")+", .font_size = 13, .selectable = false});\n"
                      "            if (!"+label+") return std::unexpected("+label+".error());\n";
                if(native_expression(values.at("text")->value))native_binding(*values.at("text"),label,"[tooltip = *"+native+"](ii::gui::Widget widget, const auto& value) -> ii::core::Result<void> { if (auto changed = widget.set_text(lvui::native::text(value)); !changed) return changed; return tooltip.refresh(); }");
            }
        }
        else if(code_editor&&options.framework=="LVRS"){const auto native="editor_"+variable;body+="            auto "+native+" = "+call+";\n            if (!"+native+") return std::unexpected("+native+".error());\n            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";emit_code_editor_bindings(values,variable,native);}
        else if(code_editor){
            const auto native="code_editor_"+variable,input="code_editor_input_"+variable,derived="code_editor_appearance_"+variable;
            if(!values.contains("model")){
                const auto document="code_editor_document_"+variable;
                body+="            auto "+document+" = lvui::CodeMemoryDocument::create("+literal(text(values,"text"))+");\n"
                      "            if (!"+document+") return ii::core::fail(ii::core::ErrorCode::invalid_argument, "+document+".error().message);\n";
                body+="            lvui::native::CodeEditorInput "+input+"; "+input+".source = *"+document+"; ";
                if(boolean(values,"readOnly",false)!="true")body+=input+".editor = *"+document+"; ";
                body+="\n";
            }else{
                body+="            lvui::native::CodeEditorInput "+input+"; "+input+".source = lvui::native::widget_value<ii::gui::WidgetColor>("+scoped_member(values,"model")+"); ";
                if(values.contains("editor"))body+=input+".editor = lvui::native::widget_value<ii::gui::WidgetColor>("+scoped_member(values,"editor")+"); ";
                body+="\n";
            }
            const auto appearance=[](std::string_view name){return name=="width"||name=="height"||name=="editorHeight"||name=="insetHorizontal"||name=="insetVertical"||name=="cornerRadius"||name=="fontPixelSize"||name=="textLineHeight"||name=="tabStopDistance"||name=="fontFamily"||name=="accessibilityName"||name=="enabled"||name=="textColor"||name=="backgroundColor"||name=="backgroundColorHover"||name=="selectionColor"||name=="focusColor"||name=="caretColor";};
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return appearance(pair.first)&&native_expression(pair.second->value);});
            if(dynamic){body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_code_editor_appearance(values,false)+"; });\n"
                "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                "            auto "+derived+" = *"+derived+"_result; "+input+".appearance = "+derived+"->get();\n";}
            else body+="            "+input+".appearance = "+owned_code_editor_appearance(values,true)+";\n";
            const auto callbacks="lvui::native::NativeCodeEditorCallbacks{.edited = "+action(values,"onDocumentEdited","const lvui::CodeDocumentChange& change","change")+
                ", .selection_changed = "+action(values,"onSelectionChanged","lvui::CodeSelection value","value")+"}";
            body+="            auto "+native+" = lvui::native::NativeCodeEditor::create(widgets, "+args+", std::move("+input+"), "+callbacks+");\n"
                  "            if (!"+native+") return std::unexpected("+native+".error());\n"
                  "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
            if(dynamic)body+="            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::CodeEditorAppearance& value) { return component.set_appearance(value); });\n"
                "              if (!binding) return std::unexpected(binding.error()); }\n";
            if(values.contains("sourceRevision")&&native_expression(values.at("sourceRevision")->value))
                native_binding(*values.at("sourceRevision"),variable,"[component = *"+native+"](ii::gui::Widget, const auto&) { return component.sync_source(); }");
        }
        else if(material&&options.framework=="LVRS"){const auto native="material_"+variable;body+="            auto "+native+" = "+call+";\n            if (!"+native+") return std::unexpected("+native+".error());\n            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";emit_material_bindings(values,variable,native);}
        else if(material){
            const auto native="material_"+variable,derived="material_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&native_expression(pair.second->value);});
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_material_options(kind,values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeMaterial::create(widgets, "+args+", "+derived+"->get());\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::MaterialConfiguration& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeMaterial::create(widgets, "+args+", "+owned_material_options(kind,values,true)+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(listener&&options.framework!="LVRS"){
            const auto native="listener_"+variable,derived="listener_inputs_"+variable;
            const auto callbacks="lvui::native::EventListenerCallbacks{.input = "+listener_input(values)+", .resized = "+action(values,"onResized","ii::gui::Rect value")+"}";
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return (pair.first=="width"||pair.first=="height")&&native_expression(pair.second->value);});
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_listener_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeEventListener::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::EventListenerConfiguration& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeEventListener::create(widgets, "+args+", "+owned_listener_options(values,true)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(table_row&&options.framework!="LVRS"){
            const auto native="table_row_"+variable,derived="table_row_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeTableRowCallbacks{.edited = "+action(values,"onCellInputEdited","std::size_t index, std::string value","index, value")+
                ", .submitted = "+action(values,"onCellInputSubmitted","std::size_t index, std::string value","index, value")+
                ", .rejected = "+action(values,"onCellInputRejected","std::size_t index, std::string value, std::string valueType","index, value, valueType")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_table_row_options(values,false,view.position)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeTableRow::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::TableRowInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeTableRow::create(widgets, "+args+", "+owned_table_row_options(values,true,view.position)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(table_cell&&options.framework!="LVRS"){
            const auto native="table_cell_item_"+variable,derived="table_cell_item_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeTableCellItemCallbacks{.edited = "+action(values,"onInputEdited","std::string value","value")+
                ", .submitted = "+action(values,"onInputSubmitted","std::string value","value")+
                ", .rejected = "+action(values,"onInputRejected","std::string value, std::string valueType","value, valueType")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_table_cell_item_options(values,false,view.position)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeTableCellItem::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::TableCellItemInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeTableCellItem::create(widgets, "+args+", "+owned_table_cell_item_options(values,true,view.position)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(table_header&&options.framework!="LVRS"){
            const auto native="table_header_"+variable,derived="table_header_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeTableHeaderCallbacks{.column_clicked = "+action(values,"onColumnClicked","std::size_t index, const lvui::TableColumn& column","index, column")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_table_header_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeTableHeader::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::TableHeaderInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeTableHeader::create(widgets, "+args+", "+owned_table_header_options(values,true)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(media_card&&options.framework!="LVRS"){
            const auto native="card_"+variable,derived="card_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeCardCallbacks{.clicked = "+action(values)+
                ", .selected_changed = "+action(values,"onSelectedChanged","bool value","value")+
                ", .menu_requested = "+action(values,"onMenuRequested")+", .action_triggered = "+action(values,"onActionTriggered")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_media_card_options(values,false,view.position)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeCard::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::CardInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeCard::create(widgets, "+args+", "+owned_media_card_options(values,true,view.position)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(media_card||(avatar&&options.framework=="LVRS")){const auto native="media_"+variable;body+="            auto "+native+" = "+call+";\n            if (!"+native+") return std::unexpected("+native+".error());\n            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";emit_media_bindings(values,variable,native,avatar);}
        else if(avatar){
            const auto native="avatar_"+variable,derived="avatar_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&native_expression(pair.second->value);});
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_avatar_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeAvatar::create(widgets, "+args+", "+derived+"->get());\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::AvatarInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeAvatar::create(widgets, "+args+", "+owned_avatar_options(values,true)+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(router&&options.framework!="LVRS"){
            const auto native="router_"+variable;
            const auto metric=[&](std::string_view name,double fallback,bool initial){
                auto input=values.find(name);if(input!=values.end()&&native_expression(input->second->value))
                    return "lvui::native::range("+expression_code(input->second->value,input->second->position,initial)+", 1, 8192)";
                return decimal(numeric(values,name,fallback,1,8192));
            };
            body+="            auto "+native+" = lvui::native::NativePageRouter::create(widgets, "+args+", "+scoped_member(values,"router")+", scope->navigation_context, {"+metric("width",640,true)+", "+metric("height",400,true)+"});\n"
                  "            if (!"+native+") return std::unexpected("+native+".error());\n"
                  "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return (pair.first=="width"||pair.first=="height")&&native_expression(pair.second->value);});
            if(dynamic)body+="            { auto target = *"+variable+"; auto binding = native_bindings->bind([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return ii::gui::Size{"+metric("width",640,false)+", "+metric("height",400,false)+"}; }, [target](ii::gui::Size value) { return target.set_extent(value); });\n"
                              "              if (!binding) return std::unexpected(binding.error()); }\n";
        }
        else if(link&&options.framework!="LVRS"){
            const auto native="link_"+variable,derived="link_inputs_"+variable;
            const auto callbacks="lvui::native::LinkCallbacks{.method = "+button_method(values,"method","lvui::LinkEvent")+
                ", .methods = "+link_methods(values,"lvui::LinkEvent")+", .clicked = "+action(values)+
                ", .pressed = "+action(values,"onPressed")+", .released = "+action(values,"onReleased")+
                ", .canceled = "+action(values,"onCanceled")+", .navigated = "+action(values,"onNavigated","bool value")+
                ", .navigation_failed = "+action(values,"onNavigationFailed","const ii::core::Error& error","error")+"}";
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&pair.first!="method"&&pair.first!="methods"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_link_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeLink::create(widgets, "+args+", scope->navigation_context, "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::LinkConfiguration& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeLink::create(widgets, "+args+", scope->navigation_context, "+owned_link_options(values,true)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
            if(auto methods=values.find("methods");methods!=values.end()&&native_expression(methods->second->value)){
                const auto expression=expression_code(methods->second->value,methods->second->position);
                body+="            { auto binding = native_bindings->bind([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return std::make_shared<const std::vector<lvui::LinkMethod>>(lvui::native::widget_value<ii::gui::WidgetColor>("+expression+")); }, [component = *"+native+"](const auto& value) { return component.set_methods(*value); });\n"
                      "              if (!binding) return std::unexpected(binding.error()); }\n";
            }
        }
        else if(card&&options.framework!="LVRS"){
            const auto native="surface_"+variable,derived="card_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&native_expression(pair.second->value);});
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_card_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeAppCard::create(widgets, "+args+", "+derived+"->get());\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::AppCardConfiguration& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeAppCard::create(widgets, "+args+", "+owned_card_options(values,true)+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(header&&options.framework!="LVRS"){
            const auto native="surface_"+variable,derived="header_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_header_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeAppHeader::create(widgets, "+args+", "+derived+"->get(), "+action(values,"onMenuClicked")+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::AppHeaderConfiguration& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeAppHeader::create(widgets, "+args+", "+owned_header_options(values,true)+", "+action(values,"onMenuClicked")+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(link||card||header){const auto native=std::string(link?"link_":"surface_")+variable;body+="            auto "+native+" = "+call+";\n            if (!"+native+") return std::unexpected("+native+".error());\n            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";if(link)emit_link_bindings(values,variable,native);else emit_surface_bindings(values,variable,native);}
        else if(native_view){const auto native="native_view_"+variable;
            body+="            auto "+native+" = "+call+";\n            if (!"+native+") return std::unexpected("+native+".error());\n            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
            if(std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&native_expression(pair.second->value);}))
                body+="            { auto binding = native_bindings->bind([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+native_view_options(values,false)+"; }, [component = *"+native+"](const ii::gui::NativeViewOptions& value) { return component.set_options(value); });\n              if (!binding) return std::unexpected(binding.error()); }\n";
        }
        else if(menu_item&&options.framework!="LVRS"){
            const auto native="menu_item_"+variable,derived="menu_item_inputs_"+variable;
            const bool compact=kind=="ContextMenuItem";
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeMenuItemCallbacks{.clicked = "+action(values)+", .pressed = "+action(values,"onPressed")+", .released = "+action(values,"onReleased")+", .canceled = "+action(values,"onCanceled")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_menu_item_options(values,false,compact,view.position)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeMenuItem::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::MenuItemInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeMenuItem::create(widgets, "+args+", "+owned_menu_item_options(values,true,compact,view.position)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
            if(!active_menu_registration.empty())body+="            if (auto attached = "+active_menu_registration+"("+native+"->weak_group_port()); !attached) return attached;\n";
        }
        else if(tab&&options.framework!="LVRS"){
            const auto native="tab_"+variable,derived="tab_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_tab_options(values,false,view.position)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeTab::create(widgets, "+args+", "+derived+"->get(), "+action(values)+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::TabInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeTab::create(widgets, "+args+", "+owned_tab_options(values,true,view.position)+", "+action(values)+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
            if(!active_tabbar_registration.empty())body+="            if (auto attached = "+active_tabbar_registration+"("+native+"->weak_group_port()); !attached) return attached;\n";
        }
        else if(dropdown&&options.framework!="LVRS"){
            const auto native="dropdown_"+variable,derived="dropdown_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeDropdownCallbacks{.clicked = "+action(values)+", .pressed = "+action(values,"onPressed")+", .released = "+action(values,"onReleased")+", .canceled = "+action(values,"onCanceled")+"}";
            const bool icon_default=kind=="IconMenuButton";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_dropdown_options(values,false,icon_default)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeDropdownButton::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::DropdownInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeDropdownButton::create(widgets, "+args+", "+owned_dropdown_options(values,true,icon_default)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
            if(!active_segmented_style.empty())
                body+="            if (auto attached = "+active_segmented_style+"("+native+"->weak_borderless_tone_port()); !attached) return attached;\n";
        }
        else if(button&&options.framework!="LVRS"){
            const auto native="button_"+variable,derived="button_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto button_options=[&](bool initial){return kind=="HelpButton"?owned_help_button_options(values,initial):owned_button_options(values,initial,icon_button,kind=="AbstractButton",view.position);};
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+button_options(false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeButton::create(widgets, "+args+", "+derived+"->get(), "+action(values)+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::ButtonInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeButton::create(widgets, "+args+", "+button_options(true)+", "+action(values)+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
            if(!active_segmented_style.empty())
                body+="            if (auto attached = "+active_segmented_style+"("+native+"->weak_borderless_tone_port()); !attached) return attached;\n";
        }
        else if(kind=="RadioButton"&&options.framework!="LVRS"){
            const auto native="radio_"+variable,derived="radio_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&pair.first!="onToggled"&&native_expression(pair.second->value);});
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_radio_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeRadioButton::create(widgets, "+args+", scope->radio_groups, "+derived+"->get(), "+action(values,"onToggled","bool value")+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::RadioButtonConfiguration& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeRadioButton::create(widgets, "+args+", scope->radio_groups, "+owned_radio_options(values,true)+", "+action(values,"onToggled","bool value")+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(kind=="CheckBox"&&options.framework!="LVRS"){
            const auto native="checkbox_"+variable,derived="checkbox_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&pair.first!="onToggled"&&native_expression(pair.second->value);});
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_checkbox_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeCheckBox::create(widgets, "+args+", "+derived+"->get(), "+action(values,"onToggled","bool value")+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::CheckBoxConfiguration& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeCheckBox::create(widgets, "+args+", "+owned_checkbox_options(values,true)+", "+action(values,"onToggled","bool value")+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(kind=="ToggleSwitch"&&options.framework!="LVRS"){
            const auto native="toggle_"+variable,derived="toggle_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("onToggled")&&native_expression(pair.second->value);});
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_toggle_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeToggleSwitch::create(widgets, "+args+", "+derived+"->get(), "+action(values,"onToggled","bool value")+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::ToggleSwitchConfiguration& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeToggleSwitch::create(widgets, "+args+", "+owned_toggle_options(values,true)+", "+action(values,"onToggled","bool value")+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(divider&&options.framework!="LVRS"){
            const auto native="divider_"+variable,derived="divider_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&native_expression(pair.second->value);});
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_divider_options(values,false,kind=="ContextMenuDivider")+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeDivider::create(widgets, "+args+", "+derived+"->get());\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::DividerConfiguration& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::NativeDivider::create(widgets, "+args+", "+owned_divider_options(values,true,kind=="ContextMenuDivider")+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
            if(!active_menu_divider_registration.empty()){
                if(auto visible=values.find("visible");visible!=values.end()){
                    if(native_expression(visible->second->value))body+="            { auto binding = native_bindings->bind([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return lvui::native::truth("+expression_code(visible->second->value,visible->second->position)+"); }, [component = *"+native+"](bool value) { return component.set_visible(value); });\n              if (!binding) return std::unexpected(binding.error()); }\n";
                    else body+="            if (auto updated = "+native+"->set_visible("+boolean(values,"visible",true)+"); !updated) return updated;\n";
                }
                body+="            if (auto attached = "+active_menu_divider_registration+"("+native+"->weak_group_port()); !attached) return attached;\n";
            }
        }
        else if(image_view&&options.framework=="LVRS"){const auto native="image_"+variable;body+="            auto "+native+" = "+call+";\n            if (!"+native+") return std::unexpected("+native+".error());\n            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";emit_image_bindings(values,variable,native);}
        else if(image_view){
            owned_image_runtime=true;
            const auto native="image_"+variable;
            const auto derived="image_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){
                return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&native_expression(pair.second->value);
            });
            if(dynamic){
                const auto options=owned_image_options(values,false);
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+options+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::NativeImage::create(widgets, "+args+", "+derived+"->get());\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::ImageInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            } else body+="            auto "+native+" = lvui::native::NativeImage::create(widgets, "+args+", "+owned_image_options(values,true)+");\n"
                          "            if (!"+native+") return std::unexpected("+native+".error());\n"
                          "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(kind=="ProgressBar") {
            const auto state="progress_state_"+variable;
            const auto candidate="progress_candidate_"+variable;
            const auto derived="progress_inputs_"+variable;
            const auto semantics="ii::gui::AccessibilitySemantics{.role = ii::gui::AccessibilityRole::progress, .name = \"Progress\", .range = ii::gui::AccessibilityRange{0, 1, "+state+"->normalized_current()}}";
            const bool dynamic=values.contains("stateModel")||values.contains("motion")||std::ranges::any_of(values,[&](const auto& entry){
                constexpr std::string_view fields=" minimumValue maximumValue endValue startValue currentValue stateModel minimumValueStateKey maximumValueStateKey startValueStateKey currentValueStateKey size shapeStyle width height largeHeight regularHeight cornerRadius trackColor fillColor motionEnabled motion displayedStart displayedProgress ";
                return fields.find(" "+entry.first+" ")!=fields.npos && native_expression(entry.second->value);
            });
            const auto options=owned_progress_options(values);
            if(dynamic) body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+options+"; });\n"
                "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                "            auto "+derived+" = *"+derived+"_result; auto "+candidate+" = "+derived+"->get();\n";
            else body+="            auto "+candidate+" = "+options+";\n";
            body+="            auto "+state+" = std::make_shared<lvui::ProgressBar>();\n"
                  "            if (!"+state+"->set_configuration("+candidate+")) return ii::core::fail(ii::core::ErrorCode::invalid_argument, \"Invalid LVUI ProgressBar configuration\");\n"
                  "            auto progress_extent_"+variable+" = "+state+"->extent();\n"
                  "            auto "+variable+" = "+call+";\n            if (!"+variable+") return std::unexpected("+variable+".error());\n";
            body+="            if (auto accessible = "+variable+"->set_accessibility_semantics("+semantics+"); !accessible) return accessible;\n";
            body+="            if (auto installed = "+variable+"->set_paint_handler(["+state+"](const ii::gui::WidgetPaintState& paint) -> ii::core::Result<std::shared_ptr<const ii::gui::WidgetDrawing>> {\n"
                  "                auto geometry = "+state+"->geometry(paint.size.width, paint.size.height);\n"
                  "                if (!geometry) return ii::core::fail(ii::core::ErrorCode::invalid_argument, \"Invalid LVUI ProgressBar extent\");\n"
                  "                auto drawing = std::make_shared<ii::gui::WidgetDrawing>();\n"
                  "                const auto linear = [](lvui::StyleColor color) { const auto channel = [](double c) { return c <= .04045 ? c / 12.92 : std::pow((c + .055) / 1.055, 2.4); }; return ii::gui::Color{static_cast<float>(channel(color.red)), static_cast<float>(channel(color.green)), static_cast<float>(channel(color.blue)), static_cast<float>(color.alpha)}; };\n"
                  "                const auto& style = "+state+"->configuration().style;\n"
                  "                if (geometry->width > 0 && geometry->height > 0) {\n"
                  "                    ii::gui::Rect bounds{0, 0, static_cast<float>(geometry->width), static_cast<float>(geometry->height)};\n"
                  "                    auto track = linear(style.track); drawing->panels.push_back({bounds, track, track, static_cast<float>(geometry->track_radius)});\n"
                  "                    if (geometry->fill_width > 0) { auto fill = linear(style.fill); drawing->panels.push_back({{static_cast<float>(geometry->fill_x), 0, static_cast<float>(geometry->fill_width), static_cast<float>(geometry->height)}, fill, fill, static_cast<float>(geometry->fill_radius)}); }\n"
                  "                    auto outline = linear(style.outline); drawing->panels.push_back({bounds, outline, outline, static_cast<float>(geometry->track_radius), static_cast<float>(geometry->stroke_width)});\n"
                  "                }\n                return std::shared_ptr<const ii::gui::WidgetDrawing>(std::move(drawing));\n"
                  "            }); !installed) return installed;\n";
            if(dynamic) {
                const auto frame="progress_frame_"+variable;
                body+="            ii::gui::WidgetFrameHandler "+frame+" = [target = *"+variable+", "+state+"](ii::runtime::Clock::time_point now) -> ii::core::Result<bool> { const bool active = "+state+"->advance(now); if (auto painted = target.request_repaint(); !painted) return std::unexpected(painted.error()); return active; };\n"
                      "            if (auto installed = "+variable+"->set_frame_handler("+frame+", false); !installed) return installed;\n"
                      "            { auto target = *"+variable+"; auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [target, "+state+", "+frame+"](const lvui::ProgressConfiguration& value) -> ii::core::Result<void> { if (!lvui::ProgressBar::validate(value)) return ii::core::fail(ii::core::ErrorCode::invalid_argument, \"Invalid LVUI ProgressBar configuration\"); auto next = lvui::ProgressBar{}; (void)next.set_configuration(value); const auto extent = next.extent(); if (auto resized = target.set_extent({extent.width, extent.height}); !resized) return resized; if (!"+state+"->set_animated_configuration(value, std::chrono::steady_clock::now())) return ii::core::fail(ii::core::ErrorCode::invalid_argument, \"Invalid LVUI ProgressBar configuration\"); if (auto accessible = target.set_accessibility_semantics("+semantics+"); !accessible) return accessible; if ("+state+"->animating()) { if (auto started = target.request_frames(); !started) { "+state+"->finish_animation(); (void)target.set_frame_handler("+frame+", false); (void)target.request_repaint(); return started; } } else if (auto stopped = target.set_frame_handler("+frame+", false); !stopped) return stopped; return target.request_repaint(); });\n              if (!binding) return std::unexpected(binding.error()); }\n";
            }
        }
        else{body+="            auto "+variable+" = "+call+";\n            if (!"+variable+") return std::unexpected("+variable+".error());\n";emit_bindings(values,variable,kind=="Label");}
        emit_common_layout_bindings(values,variable,stack,kind=="ToolbarButton"||((kind=="ListItem"||media_card||table_header||table_cell||table_row)&&options.framework!="LVRS")||((tab||menu_item||(divider&&!active_menu_divider_registration.empty()))&&options.framework!="LVRS"));
        if(stack) emit_stack_layout(values,variable,kind,view.position);
        if (stack)
            emit_stack_style(values, variable);
        if(menu && values.contains("targetId")) context_bindings.push_back({variable,text(values,"targetId"),values.at("targetId")->position});
        if(menu && values.contains("items")) {
            if(!children.empty()) error(view.position,"Use menu items or typed children, not both");
            const auto& input=*values.at("items");auto items=resolve(input.value,input.position);
            if(items.kind!=ValueKind::list) error(input.position,"Static menu items must be a String list");
            if(items.elements.size()>4096-next) error(input.position,"Native widget limit is 4096");
            for(const auto& item:items.elements) {
                const auto value=resolve(item,input.position);if(value.kind!=ValueKind::string) error(input.position,"Static menu items must contain Strings");
                const auto child=next++;const auto id="@lvui/"+std::to_string(child);if(!ids.insert(id).second) error(input.position,"Duplicate widget id");
                const auto name="node_"+std::to_string(child);const auto method=kind=="ContextMenu"?"context_menu_item":"menu_item";
                if(options.framework!="LVRS"){
                    body+="            auto "+name+" = lvui::native::NativeMenuItem::create(widgets, menu_"+variable+"->content_root(), "+literal(id)+", ([&] { lvui::native::MenuItemInput value; value.configuration.label = "+literal(value.text)+"; value.configuration.icon_name.clear(); value.configuration.has_children = false; value.configuration.key_visible = false; return value; }()));\n"
                        "            if (!"+name+") return std::unexpected("+name+".error());\n"
                        "            if (auto attached = menu_"+variable+"->register_item("+name+"->weak_group_port()); !attached) return attached;\n";
                }else body+="            auto "+name+" = "+prefix+method+"(*"+variable+", "+literal(id)+", {.label = "+literal(value.text)+", .shortcut = \"\", .icon_name = \"\", .key_visible = false, .show_icon_slot = "+boolean(values,"showIconSlot",true)+", .has_child_items = false});\n            if (!"+name+") return std::unexpected("+name+".error());\n";
            }
        }
        if(!modal&&!sheet&&(!tooltip||options.framework!="LVRS")){
            const auto previous_style=active_segmented_style;
            const auto previous_tabbar=active_tabbar_registration;
            const auto previous_menu=active_menu_registration;
            const auto previous_menu_divider=active_menu_divider_registration;
            const auto previous_submenu=active_submenu_registration;
            if(segmented&&options.framework!="LVRS")active_segmented_style="scope->segmented_style_"+variable;
            if(tabbar&&options.framework!="LVRS")active_tabbar_registration="tabbar_"+variable+"->register_tab";
            if(menu&&options.framework!="LVRS")active_menu_registration="menu_"+variable+"->register_item";
            if(menu&&options.framework!="LVRS")active_menu_divider_registration="menu_"+variable+"->register_divider";
            if(menu&&options.framework!="LVRS")active_submenu_registration.clear();
            if(menu_item&&options.framework!="LVRS"&&!view.children.empty())active_submenu_registration="menu_item_"+variable+"->set_submenu";
            for(const auto& child:view.children)
                emit(child,tooltip?"tooltip_"+variable+"->content_root()":menu&&options.framework!="LVRS"?"menu_"+variable+"->content_root()":(card||header)&&options.framework!="LVRS"?"surface_"+variable+(card?"->content_root()":"->actions_root()"):"*"+variable,menu?&values:nullptr);
            active_segmented_style=previous_style;
            active_tabbar_registration=previous_tabbar;
            active_menu_registration=previous_menu;
            active_menu_divider_registration=previous_menu_divider;
            active_submenu_registration=previous_submenu;
        }
        if(tabbar&&options.framework!="LVRS")body+="            if (auto finished = tabbar_"+variable+"->finalize(); !finished) return finished;\n";
        if(menu&&options.framework!="LVRS")body+="            if (auto finished = menu_"+variable+"->finalize(); !finished) return finished;\n";
        if(menu&&options.framework!="LVRS"&&!submenu_owner.empty())body+="            if (auto attached = "+submenu_owner+"(menu_"+variable+"->weak_popup_port()); !attached) return attached;\n";
        if(kind=="HierarchyToolbar"&&options.framework=="LVRS")body+="            if (auto normalized = toolbar_"+variable+"->normalize(); !normalized) return normalized;\n";
    }

public:
    Generator(const ViewDocument& value, const NativeCompileOptions& config) : document(value), options(config) {}
    void profile(StyleTokenOptions &output) {
        if (!document.imports().empty() || !document.root().children.empty() ||
            !document.root().inputs.empty())
            error({}, "A style profile contains only typed let declarations");
        for (const auto &binding : document.bindings()) {
            if (binding.mutable_value || (binding.type != "Color" && binding.type != "Metric") ||
                !identifier(binding.name))
                error(binding.position, "A style profile requires named Color/Metric let declarations");
            const auto kind = binding.type == "Color" ? StyleTokenType::color : StyleTokenType::number;
            auto value = resolve_in(binding.initializer, kind, binding.position);
            StyleTokenValue scalar;
            if (kind == StyleTokenType::color)
                scalar = color_value(value, binding.position);
            else
                scalar = number(value, binding.position);
            const auto name = binding.type + "." + binding.name;
            if (constants.contains(binding.name))
                error(binding.position, "Duplicate style declaration");
            constants.emplace(binding.name, style_token_literal(scalar));
            if (const auto builtin = default_style_tokens().find(name);
                builtin && builtin->selector == name) {
                // A profile supersedes the same builtin selected by any public
                // identity in the base options, without creating two overrides.
                std::erase_if(output.overrides, [&](const auto &entry) {
                    return default_style_tokens().find(entry.first) == builtin;
                });
                output.overrides[name] = std::move(scalar);
            } else
                output.members[name] = std::move(scalar);
        }
    }
    void declarations(const std::vector<BackendImport>& imports, const std::vector<Binding>& bindings,
                      std::string& scope, std::string& type_imports) {
        for (const auto& backend : imports) {
            if (backend.type_only) {
                if (!identifier(backend.name)) error(backend.position, "Invalid C++ type import name");
                (void)qualified(backend.type, backend.position);
                constants.erase(backend.name); reactive.erase(backend.name); native_values.erase(backend.name); fixed_values.erase(backend.name);
                backends.erase(backend.name); backend_access.erase(backend.name); backend_types.erase(backend.name); binding_kinds.erase(backend.name);
                variable_types.erase(backend.name); variable_sources.erase(backend.name);
                blueprint_types[backend.name] = backend.type;
                view_values.erase(backend.name);
                continue;
            }
            const auto index = next_backend++;
            native_runtime = true;
            const auto type = blueprint_types.contains(backend.type) ? blueprint_types.at(backend.type) : backend.type;
            blueprint_types.erase(backend.name);
            constants.erase(backend.name); reactive.erase(backend.name); native_values.erase(backend.name); fixed_values.erase(backend.name);
            view_values.erase(backend.name);
            variable_types.erase(backend.name); variable_sources.erase(backend.name);
            binding_kinds.erase(backend.name); backends[backend.name] = index;
            backend_access[backend.name] = {"scope->backend_" + std::to_string(index), "scope->revision_" + std::to_string(index)};
            backend_types[backend.name] = type;
            scope += "        " + qualified(type, backend.position) + " backend_" + std::to_string(index) + ";\n";
            scope_constructor+=", backend_"+std::to_string(index)+"(factories.create<"+qualified(type,backend.position)+">("+literal(type)+", "+literal(backend.name)+", this->backend_activation))";
            scope += "        ii::core::Observable<std::uint64_t> revision_" + std::to_string(index) + "{0};\n";
        }
        type_imports.clear();
        for (const auto& [name, type] : blueprint_types) type_imports += "    using " + name + " = " + qualified(type, {}) + ";\n";
        for (const auto& binding : bindings) {
            if(compiler_callback(binding)){callback_declaration(binding,scope);continue;}
            if(compiler_scalar_input(binding)){scalar_input_declaration(binding,scope);continue;}
            if(!identifier(binding.name))error(binding.position,"Invalid C++ binding identifier");
            if(binding.callable){callable_declaration(binding,scope);continue;}
            if (binding.type == "View" || binding.type=="[View]" || binding.initializer.kind == ValueKind::view) {
                std::shared_ptr<const ViewDefinition> definition;
                auto parameters = binding.parameters;
                    if (parameters) for (auto& parameter : *parameters) {
                        if (!identifier(parameter.name)) error(parameter.position, "Invalid C++ component parameter identifier");
                        if(parameter.callable){
                            *parameter.callable=normalized_signature(*parameter.callable,parameter.position);
                            continue;
                        }
                        if (parameter.type == "View" || parameter.type=="[View]" || parameter.type=="Color" || parameter.type == "String" || parameter.type == "Bool" || parameter.type == "Int" ||
                            parameter.type == "Float" || parameter.type == "Double" || parameter.type == "Metric") continue;
                        if (auto type = blueprint_types.find(parameter.type); type != blueprint_types.end()) parameter.type = type->second;
                        else error(parameter.position, "A backend component parameter requires an imported C++ type blueprint");
                    }
                if(binding.type=="[View]")definition=sequence_definition(binding.initializer,binding.position,std::move(parameters));
                else if (binding.initializer.kind == ValueKind::view && binding.initializer.view) {
                    definition = std::make_shared<ViewDefinition>(binding.initializer.view, environment(), std::move(parameters));
                }
                else if (binding.initializer.kind == ValueKind::reference && view_values.contains(binding.initializer.text))
                    definition = view_values.at(binding.initializer.text);
                else error(binding.position, "View initializer requires a visual construction or an existing View value");
                if(binding.type=="View"&&definition->sequence)
                    error(binding.position,"A View declaration cannot alias a [View] sequence");
                constants.erase(binding.name); reactive.erase(binding.name); native_values.erase(binding.name); fixed_values.erase(binding.name);
                backends.erase(binding.name); backend_access.erase(binding.name); backend_types.erase(binding.name); binding_kinds.erase(binding.name);
                variable_types.erase(binding.name); variable_sources.erase(binding.name);
                view_values[binding.name] = std::move(definition);
                blueprint_types.erase(binding.name);
                continue;
            }
            if (binding.initializer.kind==ValueKind::list || native_expression(binding.initializer) || callable_alias(binding.initializer) ||
                (binding.annotation == BindingAnnotation::static_value && scoped_expression(binding.initializer))) {
                native_declaration(binding, scope);
                continue;
            }
            auto value =
                binding.type == "Color" || binding.type == "Metric"
                    ? resolve_in(binding.initializer,
                                 binding.type == "Color" ? StyleTokenType::color : StyleTokenType::number,
                                 binding.position)
                    : resolve(binding.initializer, binding.position);
            if (binding.type == "Color" && value.kind == ValueKind::string)
                value = style_token_literal(color_value(value, binding.position));
            if (binding.type == "Metric" && value.kind == ValueKind::number)
                value.kind = ValueKind::metric;
            if ((binding.type == "Double" || binding.type == "Float" || binding.type == "Int") &&
                value.kind == ValueKind::metric)
                value.kind = ValueKind::number;
            if (value.kind != ValueKind::string && value.kind != ValueKind::number &&
                value.kind != ValueKind::boolean && value.kind != ValueKind::color &&
                value.kind != ValueKind::metric)
                error(binding.position, "Native let initializer must be a scalar constant");
            if (!binding.type.empty()) {
                const auto& type = binding.type;
                const bool valid = (type == "String" && value.kind == ValueKind::string) ||
                                   (type == "Bool" && value.kind == ValueKind::boolean) ||
                                   (type == "Color" && value.kind == ValueKind::color) ||
                                   (type == "Metric" && value.kind == ValueKind::metric) ||
                                   ((type == "Double" || type == "Float" || type == "Int") &&
                                    value.kind == ValueKind::number);
                if (!valid) error(binding.position, "Scalar declaration type does not match initializer or is unsupported");
                if (type == "Int") {
                    auto n = number(value, binding.position);
                    if (std::trunc(n) != n || std::abs(n) > 9007199254740991.0) error(binding.position, "Int constant must be an exactly representable integer");
                }
            }
            constants.erase(binding.name); reactive.erase(binding.name); native_values.erase(binding.name); fixed_values.erase(binding.name);
            view_values.erase(binding.name);
            backends.erase(binding.name); backend_access.erase(binding.name); backend_types.erase(binding.name);
            blueprint_types.erase(binding.name);
            variable_types.erase(binding.name); variable_sources.erase(binding.name);
            binding_kinds[binding.name] = value.kind;
            if(binding.mutable_value){
                native_runtime = true;
                const auto type=value.kind==ValueKind::color?"ii::gui::WidgetColor":value.kind==ValueKind::string?"std::string":value.kind==ValueKind::boolean?"bool":binding.type=="Int"?"std::int64_t":"double";
                const auto initial=value.kind==ValueKind::color?color_literal(color_value(value,binding.position)):value.kind==ValueKind::string?literal(value.text):value.kind==ValueKind::boolean?value.text:decimal(number(value,binding.position));
                const auto symbol = storage_symbol(binding.name);
                scope+="        ii::core::Observable<"+std::string(type)+"> value_"+symbol+"{"+initial+"};\n";
                variable_types.emplace(binding.name,type);reactive.emplace(binding.name,"scope->value_"+symbol);
                variable_sources.emplace(binding.name, VariableSource{"scope_->value_" + symbol});
            }else constants.emplace(binding.name, std::move(value));
        }
    }
    std::string run() {
        if (!identifier(options.name)) error({}, "Invalid or reserved C++ identifier for generated view name");
        if (options.framework!="iiGUI" && options.framework!="LVRS") error({}, "Unknown native component framework");
        std::string scope;
        std::string type_imports;
        declarations(document.imports(), document.bindings(), scope, type_imports);
        auto root = document.root();
        for (std::size_t depth = 0; !root.component; ++depth) {
            if(root.fragment)error(root.position,"The application root must be a Window, not a View sequence");
            if(has_materialized_inputs(root)){
                declarations({},root.bindings,scope,type_imports);root.bindings.clear();
            }
            if (root.condition || root.repetition) error(root.position, "Native AOT root must be a Window, with conditional or repeated content inside it");
            if (depth == 64) error(root.position, "Maximum View value expansion depth is 64");
            auto found = view_values.find(root.custom_component);
            if (found == view_values.end()) error(root.position, "Unknown root View value");
            const auto definition = found->second;
            const auto supplied = instantiate_view(root, *definition);
            root = supplied;
        }
        if (root.component->spelling != "ApplicationWindow" && root.component->spelling != "Window") error(root.position, "Native AOT root must be ApplicationWindow or Window");
        declarations(root.imports, root.bindings, scope, type_imports);
        root.imports.clear(); root.bindings.clear();
        if(component_kinds(root.children).size()!=1)error(root.position,"ApplicationWindow requires exactly one child View after sequence expansion");
        const auto values = options.framework == "LVRS"
                                ? inputs(root, {"title", "width", "height", "minimumWidth", "minimumHeight",
                                                "accent", "backgroundColor", "fontFamily"})
                                : inputs(root, {"title", "width", "height", "minimumWidth", "minimumHeight",
                                                "backgroundColor", "fontFamily"});
        std::string accent="blue";
        if(auto found=values.find("accent");found!=values.end() && !native_expression(found->second->value)) {
            const auto& v=found->second->value; constexpr std::string_view choices=" blue purple rose red orange yellow green gray brown cyan teal white ";
            if(v.kind!=ValueKind::enumeration || v.text.size()<2 || choices.find(" "+v.text.substr(1)+" ")==choices.npos) error(found->second->position,"Unknown LVRS accent");
            accent=v.text.substr(1);
        }
        Inputs bootstrap=values;
        for(auto it=bootstrap.begin();it!=bootstrap.end();)if(native_expression(it->second->value))it=bootstrap.erase(it);else ++it;
        auto width = numeric(bootstrap, "width", 640, 1, 32767), height = numeric(bootstrap, "height", 420, 1, 32767);
        auto min_width = numeric(bootstrap, "minimumWidth", 320, 1, 32767), min_height = numeric(bootstrap, "minimumHeight", 220, 1, 32767);
        if(values.contains("width")&&!bootstrap.contains("width"))width=std::max(width,min_width);
        if(values.contains("height")&&!bootstrap.contains("height"))height=std::max(height,min_height);
        if(values.contains("minimumWidth")&&!bootstrap.contains("minimumWidth"))min_width=std::min(min_width,width);
        if(values.contains("minimumHeight")&&!bootstrap.contains("minimumHeight"))min_height=std::min(min_height,height);
        if (width < min_width || height < min_height) error(root.position, "Window size must be at least its minimum size");
        emit_window_properties(values);
        for(const auto& child:root.children)emit(child,"native_parent");
        if (needs_parent_scope && scope.empty()) scope = "        // Lexical parent activation for locally owned child scopes.\n";
        if (window_runtime && scope.empty()) scope = "        // Native activation for Window properties.\n";
        for(const auto& binding:context_bindings) {
            auto target=widget_variables.find(binding.target);if(target==widget_variables.end()) error(binding.position,binding.tooltip?"Unknown Tooltip targetId":"Unknown context menu targetId");
            body+="            if (auto bound = "+(binding.tooltip?binding.menu+"->set_target(*"+target->second+")":target->second+"->set_context_menu(*"+binding.menu+")")+"; !bound) return bound;\n";
        }
        std::string result = "// Generated by lvui-compile. C++23 native widget construction; do not edit.\n#pragma once\n#include <iiGUI/NativeWidgets.h>\n#include <iiGUI/Bindings.h>\n#include <iiGUI/Host.h>\n#include <memory>\n";
        if (native_view_runtime) result += "#include <iiGUI/NativeView.h>\n";
        if (owned_progress_runtime) result += "#include <LVUI/ProgressBar.h>\n";
        if (owned_button_runtime) result += "#include <LVUI/NativeButton.h>\n";
        if (owned_alert_button_runtime) result += "#include <LVUI/NativeAlertButton.h>\n";
        if (owned_alert_runtime) result += "#include <LVUI/NativeAlert.h>\n";
        if (owned_sheet_runtime) result += "#include <LVUI/NativeSheet.h>\n";
        if (owned_toggle_runtime) result += "#include <LVUI/NativeToggleSwitch.h>\n";
        if (owned_checkbox_runtime) result += "#include <LVUI/NativeCheckBox.h>\n";
        if (owned_radio_runtime) result += "#include <LVUI/NativeRadioButton.h>\n";
        if (owned_material_runtime) result += "#include <LVUI/NativeMaterial.h>\n";
        if (owned_link_runtime) result += "#include <LVUI/NativeLink.h>\n";
        if (owned_router_runtime) result += "#include <LVUI/NativePageRouter.h>\n";
        if (owned_card_runtime) result += "#include <LVUI/NativeAppCard.h>\n";
        if (owned_media_card_runtime) result += "#include <LVUI/NativeCard.h>\n";
        if (owned_header_runtime) result += "#include <LVUI/NativeAppHeader.h>\n";
        if (owned_listener_runtime) result += "#include <LVUI/NativeEventListener.h>\n";
        if (owned_avatar_runtime) result += "#include <LVUI/NativeAvatar.h>\n";
        if (owned_tooltip_runtime) result += "#include <LVUI/NativeTooltip.h>\n";
        if (owned_slider_runtime) result += "#include <LVUI/NativeSlider.h>\n";
        if (owned_input_field_runtime) result += "#include <LVUI/NativeInputField.h>\n";
        if (owned_code_editor_runtime) result += "#include <LVUI/NativeCodeEditor.h>\n";
        if (owned_stepper_runtime) result += "#include <LVUI/NativeStepper.h>\n";
        if (owned_combo_box_runtime) result += "#include <LVUI/NativeComboBox.h>\n";
        if (owned_segmented_runtime) result += "#include <LVUI/NativeSegmentedControl.h>\n";
        if (owned_help_runtime) result += "#include <LVUI/HelpButton.h>\n";
        if (owned_dropdown_runtime) result += "#include <LVUI/NativeDropdownButton.h>\n";
        if (owned_tab_runtime) result += "#include <LVUI/NativeTab.h>\n";
        if (owned_tabbar_runtime) result += "#include <LVUI/NativeTabBar.h>\n";
        if (owned_menu_runtime) result += "#include <LVUI/NativeMenu.h>\n";
        if (owned_popover_runtime) result += "#include <LVUI/NativePopover.h>\n";
        if (owned_modal_runtime) result += "#include <LVUI/NativeModal.h>\n";
        if (owned_color_button_runtime) result += "#include <LVUI/NativeColorPickerButton.h>\n";
        if (owned_color_surface_runtime) result += "#include <LVUI/NativeColorSurface.h>\n";
        if (owned_color_picker_runtime) result += "#include <LVUI/NativeColorPicker.h>\n";
        if (owned_color_picker_default_runtime) result += "#include <LVUI/ColorSelectionSurface.h>\n";
        if (owned_hierarchy_item_runtime) result += "#include <LVUI/NativeHierarchyItem.h>\n";
        if (owned_hierarchy_list_runtime) result += "#include <LVUI/NativeHierarchyList.h>\n";
        if (owned_hierarchy_toolbar_runtime) result += "#include <LVUI/NativeHierarchyToolbar.h>\n";
        if (owned_hierarchy_panel_runtime) result += "#include <LVUI/NativeHierarchyPanel.h>\n";
        if (owned_list_panel_runtime) result += "#include <LVUI/NativeListPanel.h>\n";
        if (owned_list_item_runtime) result += "#include <LVUI/NativeListItem.h>\n";
        if (owned_list_item_composite_runtime) result += "#include <LVUI/NativeListItemComposite.h>\n";
        if (owned_list_toolbar_runtime) result += "#include <LVUI/NativeListToolbar.h>\n";
        if (owned_list_footer_runtime) result += "#include <LVUI/NativeListFooter.h>\n";
        if (owned_toolbar_button_runtime) result += "#include <LVUI/NativeToolbarButton.h>\n";
        if (owned_table_runtime) result += "#include <LVUI/NativeTable.h>\n";
        if (owned_table_row_runtime) result += "#include <LVUI/NativeTableRow.h>\n";
        if (owned_table_cell_item_runtime) result += "#include <LVUI/NativeTableCellItem.h>\n";
        if (owned_table_header_runtime) result += "#include <LVUI/NativeTableHeader.h>\n";
        if (owned_menu_item_runtime) result += "#include <LVUI/NativeMenuItem.h>\n";
        if (owned_divider_runtime) result += "#include <LVUI/NativeDivider.h>\n";
        if (owned_image_runtime) result += "#include <LVUI/NativeImage.h>\n";
        if (native_runtime) result += "#include <LVUI/NativeValues.h>\n#include <LVUI/NativeBackendFactories.h>\n";
        if (component_input_runtime) result += "#include <LVUI/NativeComponentInputs.h>\n";
        if (object_input_runtime) result += "#include <LVUI/NativeObjectInputs.h>\n";
        if (method_runtime) result += "#include <LVUI/NativeMethods.h>\n";
        if (inferred_runtime) result += "#include <LVUI/NativeInferredValues.h>\n";
        if (function_variable_runtime) result += "#include <LVUI/NativeFunctionVariable.h>\n";
        if (collection_runtime) result += "#include <LVUI/NativeCollection.h>\n#include <LVUI/NativeVirtualCollection.h>\n#include <LVUI/NativeMeasuredCollection.h>\n#include <LVUI/NativeCollectionWidgets.h>\n";
        if(widget_color_runtime)result+="#include <LVUI/NativeWidgetColors.h>\n";
        if(widget_layout_runtime)result+="#include <LVUI/NativeWidgetLayout.h>\n";
        if(window_runtime)result+="#include <LVUI/NativeWindowValues.h>\n";
        if(options.framework=="LVRS") result += "#include <LVRS/Widgets.h>\n";
        for (const auto& include : options.includes) {
            if (include.empty() || include.front() == '/' || include.find("..") != include.npos ||
                !std::ranges::all_of(include, [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                    (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.' || c == '/'; }))
                error({}, "Invalid relative C++ include path");
            result += "#include \"" + include + "\"\n";
        }
        if(options.framework!="LVRS")result+="\nnamespace lvui { class RadioGroups; class NavigationContext; }\n";
        result += "\nnamespace lvui_generated {\nstruct " + options.name + " final {\n    static ii::gui::HostOptions options() {\n        return {.title = " + literal(text(bootstrap, "title", "LVUI")) +
            ", .size = {" + decimal(width) + ", " + decimal(height) + "}, .minimum_size = {" + decimal(min_width) + ", " + decimal(min_height) + "}};\n    }\n";
        result += appearance_code(bootstrap, accent);
        result += type_imports;
        result += nested_views;
        if (!scope.empty()) result += scope_declaration(scope);
        result += value_initialization();
        result += "    " + options.name + "() = default;\n    " + options.name + "(const " + options.name + "&) = delete;\n    " + options.name + "& operator=(const " + options.name + "&) = delete;\n";
        const auto scope_parameters=scope.empty()?std::string{}:", const std::function<void(Scope&)>& configure = {}, std::shared_ptr<Scope>* scope_owner = nullptr, const lvui::native::BackendFactories& factories = {}";
        const auto scope_arguments=scope.empty()?std::string{}:", configure, scope_owner, factories";
        result += "private:\n    static ii::core::Result<void> construct(ii::gui::WidgetBuilder& widgets, ii::gui::Widget native_parent"+scope_parameters+") {\n";
        if (!scope.empty()) result += "            auto scope = std::make_shared<Scope>(factories);\n            if (auto owned = widgets.retain(scope); !owned) return owned;\n";
        if (owned_radio_runtime) result += "            scope->radio_groups = std::make_shared<lvui::RadioGroups>();\n";
        if (owned_link_runtime||owned_router_runtime) result += "            scope->navigation_context = std::make_shared<lvui::NavigationContext>();\n";
        if(options.framework=="LVRS") result += "            lvrs::Widgets controls(widgets);\n";
        result += binding_owner();
        if (native_runtime && !scope.empty()) result += "            scope->native_lifetime = native_bindings;\n";
        if (!scope.empty()) result += "            if (auto activated = lvui::native::attach_backend_activation(scope, widgets); !activated) return activated;\n            if (configure) configure(*scope);\n";
        if (!native_initializers.empty()) result += "            if (auto initialized = initialize_values(scope, native_bindings); !initialized) return initialized;\n";
        result += body;
        if (!scope.empty()) result += "            if (scope_owner) *scope_owner = scope;\n";
        result += "            return {};\n    }\npublic:\n    static ii::core::Result<void> build(ii::gui::WidgetBuilder& widgets, ii::gui::Widget native_parent = {}"+scope_parameters+") {\n";
        if(!initial_open.empty())result+="        (void)widgets; (void)native_parent;"+std::string(scope.empty()?"":" (void)configure; (void)scope_owner; (void)factories;")+"\n        return ii::core::fail(ii::core::ErrorCode::unsupported, \"Embedded pages must open dialogs after activation\");\n";
        else result+="        return construct(widgets, native_parent"+scope_arguments+");\n";
        result+="    }\n    static ii::core::Result<ii::gui::WidgetMount> mount(ii::gui::Host& host"+scope_parameters+") {\n";
        result += std::string(initial_open.empty() ? "        return host.mount_widgets("
                                                   : "        auto mounted = host.mount_widgets(") +
                  std::string(scope.empty() ? "[]" : "[&configure, scope_owner, &factories]") +
                  "(ii::gui::WidgetBuilder& widgets) -> ii::core::Result<void> {\n            return "
                  "construct(widgets, {}" +
                  scope_arguments + ");\n        }, appearance());\n";
        if(!initial_open.empty()){result+="        if (!mounted) return mounted;\n";for(const auto& id:initial_open)result+="        { auto dialog = mounted->find("+literal(id)+"); if (!dialog) { (void)host.unmount_widgets(*mounted); return std::unexpected(dialog.error()); } if (auto opened = dialog->open_at(0,0); !opened) { (void)host.unmount_widgets(*mounted); return std::unexpected(opened.error()); } }\n";result+="        return mounted;\n";}
        result+="    }\n";
        const auto configuration=scope.empty() ? std::string{} : ", const std::function<void(Scope&)>& configure = {}, const lvui::native::BackendFactories& factories = {}";
        std::string accessors;
        for(const auto& [name,type]:variable_types) {
            const auto& source=variable_sources.at(name);
            const auto guard=std::string("        auto current = native_widget_.snapshot();\n        if (!current) return std::unexpected(current.error());\n        if (!scope_ || !scope_->active()) return ii::core::fail(ii::core::ErrorCode::closed, \"View scope has closed\");\n");
            accessors+="    ii::core::Result<void> set_"+name+"("+type+" value) {\n"+guard+"        try { return lvui::native::assign_value("+source.target+", std::move(value)); }\n        catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, \"Observable update rejected\", std::current_exception()); }\n    }\n";
            if(source.callable||source.inferred)accessors+="    template<class NativeFactory> ii::core::Result<void> set_"+name+"_native(NativeFactory factory) {\n"+guard+"        return "+(source.inferred?"lvui::native::inferred::replace("+source.target+", std::move(factory))":source.target+".replace_native(std::move(factory))")+";\n    }\n";
        }
        result += "    ii::core::Result<void> attach(ii::gui::Host& host" + configuration + ") {\n"
            "        auto current = native_widget_.snapshot();\n"
            "        if (current) return ii::core::fail(ii::core::ErrorCode::busy, \"Native C++ widget is already attached\");\n"
            "        if (current.error().code != ii::core::ErrorCode::closed) return std::unexpected(current.error());\n"
            + std::string(scope.empty() ? "" : "        std::shared_ptr<Scope> owner;\n") +
            "        auto created = mount(host" + std::string(scope.empty() ? "" : ", configure, &owner, factories") + ");\n"
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
    auto valid = default_style_tokens().validate(options.style_tokens);
    if (!valid)
        return std::unexpected(Diagnostic{0, 1, 1, valid.error().message});
    const auto first=source.find_first_not_of(" \t\r\n");
    auto document = first != source.npos && source[first] == '<'
                        ? parse_native_markup(source, options.style_tokens)
                        : parse_view(source);
    if (!document) return std::unexpected(std::move(document.error()));
    try { return Generator(*document, options).run(); }
    catch (Diagnostic& diagnostic) { return std::unexpected(std::move(diagnostic)); }
}
StyleProfileResult compile_style_profile(std::string_view source, const StyleTokenOptions &base) {
    if (source.size() > 1024 * 1024)
        return std::unexpected(Diagnostic{0, 1, 1, "Style profile exceeds 1 MiB"});
    if (auto valid = default_style_tokens().validate(base); !valid)
        return std::unexpected(Diagnostic{0, 1, 1, valid.error().message});
    auto parsed = parse_view(std::string(source) + "\nWindow {}\n");
    if (!parsed)
        return std::unexpected(parsed.error());
    NativeCompileOptions options;
    options.style_tokens = base;
    try {
        Generator(*parsed, options).profile(options.style_tokens);
        auto valid = default_style_tokens().validate(options.style_tokens);
        if (!valid)
            error({}, valid.error().message);
        return std::move(options.style_tokens);
    } catch (const Diagnostic &diagnostic) {
        return std::unexpected(diagnostic);
    }
}
NativeCompileResult compile_style_header(const StyleTokenOptions &options, std::string_view cpp_namespace) {
    try {
        auto valid = default_style_tokens().validate(options);
        if (!valid)
            error({}, valid.error().message);
        auto ns = qualified(cpp_namespace, {}).substr(2);
        std::map<std::string, StyleTokenValue, std::less<>> values;
        for (const auto &token : default_style_tokens().tokens())
            if (!token.selector.empty())
                values.emplace(token.selector, *default_style_tokens().resolve(token.id, options));
        for (const auto &[name, value] : options.members)
            values.emplace(name, value);
        std::string output = "// Generated by lvui-compile from a typed style profile. Do not edit.\n#pragma "
                             "once\n#include <iiGUI/Appearance.h>\nnamespace " +
                             ns + " {\n";
        for (auto type : {std::string("Color"), std::string("Metric")}) {
            output += "struct " + type + " final {\n";
            std::set<std::string> names;
            for (const auto &[selector, value] : values)
                if (selector.starts_with(type + ".")) {
                    std::string field;
                    bool upper = false;
                    for (char c : std::string_view(selector).substr(type.size() + 1)) {
                        if (c == '.') {
                            upper = true;
                            continue;
                        }
                        field += upper && c >= 'a' && c <= 'z' ? char(c - 'a' + 'A') : c;
                        upper = false;
                    }
                    if (!identifier(field) || !names.insert(field).second)
                        error({}, "C++ style member collision or invalid name: " + field);
                    if (auto c = std::get_if<StyleColor>(&value))
                        output += "    static constexpr ii::gui::WidgetColor " + field + "{" +
                                  decimal(c->red) + ", " + decimal(c->green) + ", " + decimal(c->blue) +
                                  ", " + decimal(c->alpha) + "};\n";
                    else {
                        auto n = std::get<double>(value);
                        // Integer geometry/weights retain integral constant conversion;
                        // fractional geometry remains float-compatible with native Rect.
                        auto scalar = n >= -2147483647. && n <= 2147483647. && std::trunc(n) == n
                                          ? std::to_string(static_cast<int>(n))
                                          : decimal(n);
                        if (std::trunc(n) != n) {
                            if (scalar.find_first_of(".eE") == scalar.npos)
                                scalar += ".0";
                            scalar += "f";
                        } else if (n > 2147483647. || n < -2147483647.) {
                            if (scalar.find_first_of(".eE") == scalar.npos)
                                scalar += ".0";
                        }
                        output += "    static constexpr auto " + field + " = " + scalar + ";\n";
                    }
                }
            output += "};\n";
        }
        return output + "}\n";
    } catch (const Diagnostic &diagnostic) {
        return std::unexpected(diagnostic);
    }
}
} // namespace lvui
