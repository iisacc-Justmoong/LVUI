#include <LVUI/Compiler.h>
#include <LVUI/Language.h>
#include <LVUI/ConstantExpression.h>
#include <LVUI/NativeLayoutValues.h>
#include <LVUI/NativeAppStrings.h>
#include <LVUI/Material.h>
#include <LVUI/WindowAccent.h>
#include <LVUI/Slider.h>
#include "FigmaComponents.h"
#include <iiSDKCore/Core/Text/Number.h>
#include <iiSDKCore/Core/Text/Utf8.h>
#include <algorithm>
#include <charconv>
#include <cctype>
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
    struct Writable final { std::string target, type; bool object = false; };
    std::map<std::string, Writable, std::less<>> writable;
    std::map<std::string, ValueKind, std::less<>> binding_kinds;
    struct ViewDefinition;
    using ViewValues = std::map<std::string, std::shared_ptr<const ViewDefinition>, std::less<>>;
    struct Environment final {
        std::map<std::string, Value, std::less<>> constants;
        std::map<std::string, std::size_t, std::less<>> backends;
        std::map<std::string, BackendAccess, std::less<>> access;
        std::map<std::string, std::string, std::less<>> blueprints, reactive, native, backend_types;
        std::map<std::string, ValueKind, std::less<>> kinds;
        std::map<std::string, Writable, std::less<>> writable;
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
    struct BoundValue final {
        std::string initial, reactive;
        std::size_t depth;
        bool member_reference;
        std::set<std::string> reads{};
        std::string writable{};
    };
    struct BoundMethod final { BackendAccess access; std::string method; std::size_t depth; std::string factory{}; std::optional<CallableSignature> signature{}; std::string shared{},factory_initial{},block{}; };
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
    struct ScriptLocal final { std::string name, expression; };
    mutable std::vector<ScriptLocal> script_locals;
    mutable std::size_t next_script_local = 0, script_loop_depth = 0, script_generation_depth = 0,
                        script_retained_read_depth = 0;
    struct ScriptGeneration final {
        std::size_t& depth;
        explicit ScriptGeneration(std::size_t& value) : depth(value) { ++depth; }
        ~ScriptGeneration() { --depth; }
    };
    std::string native_initializers;
    std::string native_scope_parts, native_scope_base, scope_constructor, native_style_slots, active_segmented_style,active_tabbar_registration,active_tabbar_section,active_tabbar_row,active_tabbar_capture,active_menu_registration,active_menu_divider_registration,active_menu_section,active_menu_row,active_menu_capture,active_submenu_registration;
    std::string window_accent_name="blue", active_window_accent, window_accent_derived, window_appearance_name="appearance";
    std::size_t native_scope_next = 0;
    mutable bool native_runtime = false;
    bool application_generation = false, scene_lifecycle_runtime = false, scene_registry_runtime = false, window_close_runtime = false;
    mutable bool application_quit_runtime = false;
    std::vector<std::string> application_scene_ids;
    std::vector<bool> application_scene_initial;
    std::map<std::string, std::size_t, std::less<>> application_scene_index;
    struct PendingSceneCommand final { SourcePosition position; std::optional<std::string> id; };
    mutable std::vector<PendingSceneCommand> pending_scene_commands;
    bool scene_declarations_complete = false;
    mutable bool method_runtime=false, query_runtime=false, query_connection_runtime=false, object_value_runtime=false, localization_runtime=false;
    mutable bool image_resolver_runtime=false;
    mutable bool scoped_image_runtime=false;
    mutable bool inferred_runtime=false;
    mutable bool capturing_receiver=false;
    bool function_variable_runtime = false,object_input_runtime=false;
    struct AmbientReads final {std::size_t depth;std::set<std::string> expressions;};
    mutable std::vector<AmbientReads> ambient_reads;
    bool collection_runtime = false,widget_color_runtime=false,panel_paint_runtime=false,widget_layout_runtime=false,window_runtime=false,native_view_runtime=false,owned_progress_runtime=false,owned_image_runtime=false,owned_icon_runtime=false,owned_button_runtime=false,owned_alert_button_runtime=false,owned_alert_runtime=false,owned_sheet_runtime=false,owned_help_runtime=false,owned_dropdown_runtime=false,owned_tab_runtime=false,owned_tabbar_runtime=false,owned_menu_runtime=false,owned_menu_item_runtime=false,owned_popover_runtime=false,owned_modal_runtime=false,owned_color_button_runtime=false,owned_color_surface_runtime=false,owned_color_picker_runtime=false,owned_color_picker_default_runtime=false,owned_hierarchy_item_runtime=false,owned_hierarchy_list_runtime=false,owned_hierarchy_toolbar_runtime=false,owned_hierarchy_panel_runtime=false,owned_list_panel_runtime=false,owned_list_item_runtime=false,owned_list_item_composite_runtime=false,owned_list_item_selector_runtime=false,owned_list_toolbar_runtime=false,owned_list_footer_runtime=false,owned_toolbar_button_runtime=false,owned_table_runtime=false,owned_table_row_runtime=false,owned_table_cell_item_runtime=false,owned_table_header_runtime=false,owned_code_editor_runtime=false,owned_divider_runtime=false,owned_toggle_runtime=false,owned_checkbox_runtime=false,owned_radio_runtime=false,owned_material_runtime=false,owned_link_runtime=false,owned_router_runtime=false,owned_card_runtime=false,owned_media_card_runtime=false,owned_header_runtime=false,owned_listener_runtime=false,owned_avatar_runtime=false,owned_tooltip_runtime=false,owned_slider_runtime=false,owned_input_field_runtime=false,owned_stepper_runtime=false,owned_combo_box_runtime=false,owned_segmented_runtime=false;
    bool owned_focus_ring_runtime=false;
    bool owned_window_noise_runtime=false;
    bool owned_wheel_guard_runtime=false;
    bool owned_text_editor_runtime=false;
    bool owned_mobile_navigation_runtime=false;
    bool owned_mobile_tab_runtime=false;
    bool owned_search_field_runtime=false;
    std::string nested_views;
    std::size_t next_scope = 0;
    mutable bool needs_parent_scope = false;
    std::set<std::string, std::less<>> storage_names;
    std::size_t next_backend = 0;
    std::size_t lexical_depth = 0, view_expansion_depth = 0, next_view_instance = 0, view_expansions = 0;
    std::string id_prefix;
    std::set<std::string, std::less<>> ids;
    std::map<std::string,std::string,std::less<>> widget_variables;
    struct ContextBinding final {
        std::string menu,target;SourcePosition position;bool tooltip=false;
        bool material=false;
        std::string background,background_expression,content_expression;
        bool wheel=false;
    };
    std::vector<ContextBinding> context_bindings;
    bool popup_open_runtime=false,popup_mount_validation=false;
    std::size_t factory_depth=0;
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
        return {constants, backends, backend_access, blueprint_types, reactive, native_values, backend_types, binding_kinds, writable, view_values, fixed_values, lexical_depth};
    }
    const ScriptLocal* script_local(std::string_view name) const {
        for (auto it = script_locals.rbegin(); it != script_locals.rend(); ++it)
            if (it->name == name) return &*it;
        return nullptr;
    }
    bool script_name_bound(const std::string& name) const {
        return script_local(name) || constants.contains(name) || backends.contains(name) ||
            blueprint_types.contains(name) || reactive.contains(name) || native_values.contains(name) ||
            view_values.contains(name) || writable.contains(name) || binding_kinds.contains(name);
    }
    template<class Build>
    std::string script_sequence(const std::vector<std::string>& expressions, bool initial, Build build) const {
        if ((!initial && !script_retained_read_depth) || !script_generation_depth || expressions.size() < 2) return build(expressions);
        std::string result = "([&]() -> decltype(auto) { ";
        std::vector<std::string> values;
        values.reserve(expressions.size());
        for (const auto& expression : expressions) {
            const auto name = "_lvui_script_argument_" + std::to_string(next_script_local++);
            result += "auto&& " + name + " = (" + expression + "); ";
            values.push_back("std::forward<decltype(" + name + ")>(" + name + ")");
        }
        return result + "return " + build(values) + "; }())";
    }
    void environment(const Environment& value) {
        if (lexical_depth < value.depth) error({}, "A View value escaped its lexical owner");
        constants = value.constants; backends = value.backends; backend_access = value.access;
        blueprint_types = value.blueprints; reactive = value.reactive; native_values = value.native;
        backend_types = value.backend_types;
        binding_kinds = value.kinds; writable = value.writable; view_values = value.views;
        fixed_values = value.fixed;
        for (auto depth = value.depth; depth < lexical_depth; ++depth) {
            for (auto& [key, access] : backend_access) { borrow_path(access.object); borrow_path(access.revision); borrow_path(access.dependency); }
            for (auto& [key, access] : reactive) borrow_path(access);
            for (auto& [key, access] : native_values) borrow_path(access);
            for (auto& [key, access] : writable) borrow_path(access.target);
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
            // A transparent branch can own declarations. Inspect its local
            // View names in lexical order without constructing backends or
            // evaluating scalar bindings just to discover child kinds.
            for(const auto& imported:current.imports){
                view_values.erase(imported.name);
                if(imported.type_only)blueprint_types[imported.name]=imported.type;
                else blueprint_types.erase(imported.name);
            }
            for(const auto& binding:current.bindings){
                std::shared_ptr<const ViewDefinition> local;
                if(binding.type=="[View]")local=sequence_definition(binding.initializer,binding.position,binding.parameters);
                else if(binding.type=="View"||binding.initializer.kind==ValueKind::view){
                    if(binding.initializer.kind==ValueKind::view&&binding.initializer.view)
                        local=std::make_shared<ViewDefinition>(binding.initializer.view,environment(),binding.parameters);
                    else if(binding.initializer.kind==ValueKind::reference&&view_values.contains(binding.initializer.text))
                        local=view_values.at(binding.initializer.text);
                    else error(binding.position,"View initializer requires a visual construction or an existing View value");
                }
                view_values.erase(binding.name);
                if(local)view_values[binding.name]=std::move(local);
            }
            for(const auto& child:current.children)collect_component_kinds(child,kinds,depth+1,work);
        }else if(current.component&&current.component->spelling=="QueryConnection"){
            // A connection belongs to the activation but contributes no View.
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
    static bool direct_event_call(const Value& value) {
        return (value.kind == ValueKind::call || value.kind == ValueKind::invocation) &&
            value.text != "callback" && value.text != "quitApplication" &&
            !scene_command(value) && !localization_command(value);
    }
    std::string event_revision(const Value& value) const {
        const Value* root = &value;
        for (std::size_t n = 0; n < 64; ++n) {
            const auto found = backend_access.find(root->text.substr(0, root->text.find('.')));
            if (found != backend_access.end()) return found->second.revision;
            if (root->elements.empty() || (root->kind != ValueKind::member && root->kind != ValueKind::index &&
                                           root->kind != ValueKind::invocation)) break;
            root = &root->elements.front();
        }
        return {};
    }
    std::string event_expression(const Value& value, SourcePosition position) const {
        if (!scoped_expression(value)) error(position, "An event call requires an imported C++ callable");
        native_runtime = true;
        if (auto target = function_target(value); target && target->signature && target->signature->result == "Void")
            return function_expression(value, *target, position, true, true);
        return expression_code(value, position, true);
    }
    BoundMethod callback_target(const Value& value, SourcePosition position, std::size_t depth=0, bool event_call=false) const {
        if(depth>=64)error(position,"Maximum callback alias depth is 64");
        if(value.kind==ValueKind::reference)if(auto found=constants.find(value.text);found!=constants.end())
            return callback_target(found->second,position,depth+1,event_call);
        if (value.kind == ValueKind::call && value.text == "callback") {
            if (value.elements.size() != 1 || (value.elements.front().kind != ValueKind::call &&
                                               value.elements.front().kind != ValueKind::invocation))
                error(position, "callback() requires one C++ callable factory invocation");
            return callback_target(value.elements.front(), position, depth + 1, false);
        }
        if (event_call && direct_event_call(value)) {
            const auto expression = event_expression(value, position);
            const auto revision = event_revision(value);
            BoundMethod target{{}, {}, lexical_depth};
            target.block = revision.empty() ? "(void)(" + expression + "); " :
                "(void)lvui::native::invoke(" + revision + ", [&] { (void)(" + expression + "); }); ";
            native_runtime = true;
            return target;
        }
        if (value.kind == ValueKind::action) {
            BoundMethod target{{}, {}, lexical_depth};
            target.block = action_block_body(value);
            return target;
        }
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
                borrow_path(result.block);
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
            bound_methods.emplace(key, callback_target(value, position, 0, name.starts_with("on")));
            return {ValueKind::internal_argument, key, {}, value.position};
        }
        if (scoped_expression(value)) {
            const auto initial = expression_code(value, position, true);
            auto [formula,reads]=expression_reads(value,position);
            const auto key = "_lvui_supplied_" + std::to_string(next_argument++);
            const bool member_reference = value.kind == ValueKind::reference ||
                (value.kind == ValueKind::internal_argument && bound_values.at(value.text).member_reference);
            bound_values.emplace(key, BoundValue{initial, formula, lexical_depth, member_reference,std::move(reads),bindable_target(value)});
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
                    (target.block.empty() ?
                    "[] { return []([[maybe_unused]] const auto& scope, auto&&... arguments) -> decltype(auto) { return "+target.access.object+"."+target.method+"(std::forward<decltype(arguments)>(arguments)...); }; }" :
                    "[] { return []([[maybe_unused]] const auto& scope, auto&&... arguments) { "+target.block+" }; }"):
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
    std::string function_expression(const Value& value,const BoundMethod& target,SourcePosition at,bool initial,bool allow_void=false)const{
        if(!target.signature||(!allow_void&&target.signature->result=="Void"))error(at,"A Void callable has no display value");
        const auto begin=value.kind==ValueKind::invocation?std::size_t(1):std::size_t(0);
        if(value.elements.size()-begin!=target.signature->arguments.size())error(at,"Callable argument count does not match its signature");
        std::vector<std::string> arguments;
        for(std::size_t index=begin;index<value.elements.size();++index){
            const auto& argument=value.elements[index];const auto& type=target.signature->arguments[index-begin];
            auto expression=argument.kind==ValueKind::reference&&backend_access.contains(argument.text)?backend_access.at(argument.text).object:expression_code(argument,at,initial);
            if(type=="std::int64_t")expression="lvui::native::integer("+expression+")";
            arguments.push_back(std::move(expression));
        }
        auto result=script_sequence(arguments,initial,[&](const auto& values){
            std::string args, effects;
            for (const auto& expression : values) {
                if(!args.empty())args+=", ";args+=expression;
                effects += "(void)(" + expression + "); "
                    "if (!lvui::native::detail::settle_scope_chain(scope) || !scope->active()) return; ";
            }
            auto selected=target.block.empty()?method_invocation(target,args):"([&] { "+effects+target.block+"}())";
            if(!target.shared.empty()){
                auto field=callback_path(target);
                if(!initial)(void)record_ambient_read("lvui::native::track_function(context, "+field+")");
                selected=std::string(initial?"lvui::native::call_function(":"lvui::native::read_function(context, ")+field+(args.empty()?std::string{}:", "+args)+")";
            }
            return selected;
        });
        if(!initial&&!target.access.revision.empty()){
            const auto read=record_ambient_read("context.read("+target.access.revision+")");result="((void)"+read+", "+result+")";
        }
        if(!initial&&!target.access.dependency.empty()){
            const auto read=record_ambient_read("context.read("+target.access.dependency+")");result="((void)"+read+", "+result+")";
        }
        if(initial&&allow_void&&target.shared.empty()&&!target.access.revision.empty())
            result="lvui::native::invoke("+target.access.revision+", [&]() -> decltype(auto) { return "+result+"; })";
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
        auto writable_target = captured.writable;
        std::set<std::string> shifted_reads;
        for (auto read : reads) {
            for (auto depth = captured.depth; depth < lexical_depth; ++depth) borrow_path(read);
            shifted_reads.insert(std::move(read));
        }
        for (auto depth = captured.depth; depth < lexical_depth; ++depth) borrow_path(writable_target);
        const auto name = "_lvui_input_" + std::to_string(next_argument++);
        const auto field = "input_value_" + std::to_string(next_argument++);
        const auto type = parameter.type == "String" ? "std::string" : parameter.type == "Bool" ? "bool" :
            parameter.type == "Int" ? "std::int64_t" : parameter.type == "Color" ? "ii::gui::WidgetColor" :
            parameter.type == "Object" ? "ii::core::Value" :
            parameter.type=="Metric"||parameter.type=="Float"||parameter.type=="Double" ? "double" : "";
        scalar_inputs.emplace(name, ScalarInput{original, type, field});
        bindings.push_back({name, parameter.type, original, false, parameter.position});
        const auto storage = "lvui::native::component_input(scope, [](auto& owner) -> decltype((owner." + field + ")) { return (owner." + field + "); })";
        const auto reference = "_lvui_supplied_" + std::to_string(next_argument++);
        bound_values.emplace(reference, BoundValue{"lvui::native::initial(" + storage + ")", "context.read(" + storage + ")",
            lexical_depth, captured.member_reference, std::move(shifted_reads), std::move(writable_target)});
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
            if (!target.block.empty() && parameter.callable->result != "Void")
                error(at, "An event block requires a Void callback result");
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
            parameter.type == "Float" || parameter.type == "Double" || parameter.type == "Metric" || parameter.type=="Color" || parameter.type=="Object";
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
                    (parameter.type == "Object" && kind->second == ValueKind::object) ||
                    ((parameter.type == "Int" || parameter.type == "Float" || parameter.type == "Double" || parameter.type == "Metric") && numeric);
                if (!compatible) error(at, "Component parameter '" + parameter.name + "' does not match its reactive scalar type");
            }
            const Binding binding{parameter.name, parameter.type, value, false, at};
            const auto initial = typed_expression(binding, true);
            ambient_reads.push_back({lexical_depth,{}});
            const auto formula=typed_expression(binding);
            auto reads=std::move(ambient_reads.back().expressions);ambient_reads.pop_back();
            const auto key = "_lvui_supplied_" + std::to_string(next_argument++);
            bound_values.emplace(key, BoundValue{initial, formula, lexical_depth, false,std::move(reads),bindable_target(value)});
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
            writable.erase(name);
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
                else if(parameter.type=="Object")binding_kinds[name]=ValueKind::object;
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
    std::string binding_owner(bool diagnose=true, const std::string& before_retirement={}) const {
        std::string result;
        if (native_runtime){
            result += "            auto native_bindings = std::make_shared<lvui::native::Bindings>();\n            if (auto owned_native = widgets.retain(native_bindings); !owned_native) return owned_native;\n";
            result += before_retirement;
            if(scene_lifecycle_runtime) result += "            if (scope->scene_effects) if (auto tracked = scope->scene_effects->track(native_bindings); !tracked) return tracked;\n";
            if(diagnose)result += "            if (!scope->binding_diagnostics) return ii::core::fail(ii::core::ErrorCode::closed, \"Binding diagnostics owner retired\");\n"
                "            auto diagnostic_activation = scope->binding_diagnostics->attach(native_bindings);\n"
                "            if (!diagnostic_activation) return std::unexpected(diagnostic_activation.error());\n"
                "            if (auto retiring = widgets.on_retire([weak = std::weak_ptr(native_bindings), diagnostics = std::weak_ptr(scope->binding_diagnostics), id = *diagnostic_activation] { if (auto owner = weak.lock()) (void)owner->close(); if (auto owner = diagnostics.lock()) (void)owner->detach(id); }); !retiring) { (void)scope->binding_diagnostics->detach(*diagnostic_activation); return retiring; }\n";
            else result += "            if (auto retiring = widgets.on_retire([weak = std::weak_ptr(native_bindings)] { if (auto owner = weak.lock()) (void)owner->close(); }); !retiring) return retiring;\n";
        }
        return result;
    }

    bool local_member(const Value& value)const{
        if(value.kind!=ValueKind::reference)return false;
        const auto dot=value.text.find('.');if(dot==value.text.npos)return false;
        const auto name=value.text.substr(0,dot);
        if(script_local(name))return true;
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
        std::vector<std::string> factory_arguments;
        const auto begin=value.kind==ValueKind::invocation?std::size_t(1):std::size_t(0);
        for(auto index=begin;index<value.elements.size();++index)factory_arguments.push_back(expression_code(value.elements[index],at,initial));
        std::vector<std::string> inputs{receiver};
        inputs.insert(inputs.end(),factory_arguments.begin(),factory_arguments.end());
        auto result=script_sequence(inputs,initial,[&](const auto& values){
            std::string arguments;
            for(std::size_t index=1;index<values.size();++index)arguments+=", "+values[index];
            return "lvui::native::Methods::query("+std::string(initial?"":"context, ")+values[0]+
                ", [](auto&& object, auto&&... arguments) -> decltype(auto) { return std::forward<decltype(object)>(object)."+query.method+"(std::forward<decltype(arguments)>(arguments)...); }"+arguments+")";
        });
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
        if (value.kind == ValueKind::reference && script_local(value.text)) return true;
        if (value.kind == ValueKind::object) return true;
        if (value.kind == ValueKind::call && value.text == "localized") return true;
        if (value.kind == ValueKind::call && value.text == "query") return true;
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
        if (value.kind == ValueKind::object) return true;
        if (value.kind == ValueKind::call && value.text == "localized") return true;
        if (value.kind == ValueKind::call && value.text == "query") return true;
        if(inferred_call_source(value))return true;
        if(method_query(value))return true;
        return local_member(value) || bool(function_target(value)) || (value.kind == ValueKind::internal_argument && bound_values.contains(value.text)) || !property(value).empty() ||
               (value.kind == ValueKind::reference && scoped_expression(value)) ||
               (is_expression(value.kind) && scoped_expression(value)) ||
               (value.kind == ValueKind::call && scoped_expression(value));
    }
    std::string expression_code(const Value& value, SourcePosition at, bool initial = false) const {
        auto result = expression_code_impl(value, at, initial);
        const bool dynamic = value.kind == ValueKind::call || value.kind == ValueKind::invocation ||
            value.kind == ValueKind::member || value.kind == ValueKind::index || value.kind == ValueKind::list ||
            value.kind == ValueKind::object ||
            ((value.kind == ValueKind::reference || value.kind == ValueKind::internal_argument) &&
             !script_local(value.text) && scoped_expression(value));
        if ((initial || script_retained_read_depth) && script_generation_depth && dynamic)
            return "lvui::native::script_value(scope, [&]() -> decltype(auto) { return (" + result + "); })";
        return result;
    }
    std::string expression_code_impl(const Value& value, SourcePosition at, bool initial) const {
        native_runtime = true;
        if (value.kind == ValueKind::reference)
            if (const auto* local = script_local(value.text)) return local->expression;
        if (value.kind==ValueKind::call && value.text=="localized") {
            if(value.elements.size()!=1)error(at,"localized requires one String key");
            const auto& key=value.elements.front();
            if(key.kind==ValueKind::number||key.kind==ValueKind::boolean||key.kind==ValueKind::list||key.kind==ValueKind::object)
                error(at,"localized requires one String key");
            if(key.kind==ValueKind::string&&(key.text.empty()||key.text.size()>lvui::native::AppStrings::maximum_key_bytes||key.text.find('\0')!=key.text.npos||!ii::core::valid_utf8(key.text)))
                error(at,"localized requires a nonempty bounded UTF-8 key");
            if(key.kind==ValueKind::reference)if(auto kind=binding_kinds.find(key.text);kind!=binding_kinds.end()&&kind->second!=ValueKind::string)
                error(at,"localized requires one String key");
            localization_runtime=true;needs_parent_scope=true;
            const auto argument="lvui::native::string_view("+expression_code(key,at,initial)+")";
            const std::string storage="scope->backend_factories.app_strings()";
            if(initial)return storage+"->get("+argument+")";
            return record_ambient_read(storage+"->read(context, "+argument+")");
        }
        if (value.kind == ValueKind::call && value.text == "query") {
            if (value.elements.size() != 2 ||
                (value.elements[0].kind != ValueKind::reference && value.elements[0].kind != ValueKind::call))
                error(at, "query requires a scoped C++ port and one typed request");
            const auto& port = value.elements[0];
            const auto dot = port.text.find('.');
            if (dot == port.text.npos || port.text.find('.', dot + 1) != port.text.npos || !identifier(port.text.substr(dot + 1)))
                error(at, "query port must be a direct public member of a scoped C++ object");
            const auto owner = backend_access.find(port.text.substr(0, dot));
            const bool accessor = port.kind == ValueKind::call;
            if (accessor && (!port.elements.empty() || !method_query(port)))
                error(at, "query port accessor must be a public no-argument scoped C++ method");
            if (!accessor && owner == backend_access.end() && !local_member(port))
                error(at, "query port must be a direct public member of a scoped C++ object");
            const bool retained = accessor || owner == backend_access.end();
            std::string selected_port, request;
            if (retained && initial && script_generation_depth) {
                const ScriptGeneration retained_read(script_retained_read_depth);
                selected_port = expression_code(port, at, false);
                request = expression_code(value.elements[1], at, false);
            } else {
                selected_port = retained ? expression_code(port, at, false) : owner->second.object + "." + port.text.substr(dot + 1);
                request = expression_code(value.elements[1], at, retained ? false : initial);
            }
            query_runtime = method_runtime = true;
            auto call = script_sequence({selected_port, request}, initial, [&](const auto& values) {
                return "lvui::native::send_query(" + values[0] + ", " + values[1] + ")";
            });
            if (retained && initial) return "lvui::native::Methods::with_context([&]([[maybe_unused]] lvui::native::ReadContext& context) -> decltype(auto) { return lvui::native::Methods::evaluate(context, [&]() -> decltype(auto) { return " + call + "; }); })";
            if (retained) return "lvui::native::Methods::evaluate(context, [&]() -> decltype(auto) { return " + call + "; })";
            const auto& access = owner->second;
            if (initial) return "lvui::native::Methods::evaluate([&]() -> decltype(auto) { return " + call + "; })";
            if (!access.revision.empty()) (void)record_ambient_read("context.read(" + access.revision + ")");
            if (!access.dependency.empty()) (void)record_ambient_read("context.read(" + access.dependency + ")");
            auto tracked = "lvui::native::Methods::evaluate(context, [&]() -> decltype(auto) { return " + call + "; })";
            if (!access.revision.empty()) tracked = "((void)context.read(" + access.revision + "), " + tracked + ")";
            if (!access.dependency.empty()) tracked = "((void)context.read(" + access.dependency + "), " + tracked + ")";
            return tracked;
        }
        if(auto target=function_target(value))return function_expression(value,*target,at,initial);
        if(auto source=inferred_call_source(value)){
            inferred_runtime=true;
            std::vector<std::string> inputs{*source};
            for(auto index=value.kind==ValueKind::invocation?std::size_t(1):std::size_t(0);index<value.elements.size();++index)
                inputs.push_back(expression_code(value.elements[index],at,initial));
            if(!initial)(void)record_ambient_read("lvui::native::inferred::track(context, "+*source+")");
            return script_sequence(inputs,initial,[&](const auto& values){
                std::string arguments;
                for(std::size_t index=1;index<values.size();++index)arguments+=", "+values[index];
                return "lvui::native::inferred::call("+std::string(initial?"":"context, ")+values[0]+arguments+")";
            });
        }
        const auto recurse = [&](const Value& child) { return expression_code(child, at, initial); };
        const auto prefix = std::string("lvui::native::");
        if (value.kind == ValueKind::object) {
            object_value_runtime = true;
            std::string result = "ii::core::Value(ii::core::Value::Object{";
            bool first = true;
            for (const auto& field : value.elements) {
                if (field.kind != ValueKind::object_field || field.elements.size() != 1)
                    error(at, "Invalid object field");
                if (!first) result += ", ";
                first = false;
                result += "{" + literal(field.text) + ", lvui::native::object_value(" + recurse(field.elements[0]) + ")}";
            }
            return result + "})";
        }
        if(value.kind==ValueKind::list){
            if(value.elements.empty())error(at,"An empty list requires a typed C++ collection");
            std::vector<std::string> items;
            for(const auto& item:value.elements)items.push_back(recurse(item));
            return script_sequence(items, initial, [&](const auto& values) {
                std::string result=prefix+"list(";
                for(const auto& item:values){if(result.back()!='(')result+=", ";result+=item;}
                return result+")";
            });
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
            return script_sequence({left, right}, initial, [&](const auto& operands) -> std::string {
                const auto& lhs = operands[0]; const auto& rhs = operands[1];
                static const std::map<std::string, std::string, std::less<>> arithmetic{
                    {"+", "add"}, {"-", "subtract"}, {"*", "multiply"}, {"/", "divide"}, {"%", "remainder"}};
                if (auto op = arithmetic.find(value.text); op != arithmetic.end()) return prefix + op->second + "(" + lhs + ", " + rhs + ")";
                if (value.text == "==" || value.text == "!=")
                    return std::string(value.text == "!=" ? "(!" : "(") + prefix + "equal(" + lhs + ", " + rhs + "))";
                if (value.text == "<") return prefix + "less(" + lhs + ", " + rhs + ")";
                if (value.text == ">") return prefix + "less(" + rhs + ", " + lhs + ")";
                if (value.text == "<=") return "(!" + prefix + "less(" + rhs + ", " + lhs + "))";
                if (value.text == ">=") return "(!" + prefix + "less(" + lhs + ", " + rhs + "))";
                error(at, "Unknown native binary operator");
            });
        }
        if(value.kind==ValueKind::conditional&&capturing_receiver&&!initial)
            return "(lvui::native::truth("+recurse(value.elements.at(0))+") ? "+recurse(value.elements.at(1))+" : "+recurse(value.elements.at(2))+")";
        if (value.kind == ValueKind::conditional)
            return prefix + "choose(" + prefix + "truth(" + recurse(value.elements.at(0)) + "), [&] { return " + recurse(value.elements.at(1)) + "; }, [&] { return " + recurse(value.elements.at(2)) + "; })";
        if(value.kind==ValueKind::index&&capturing_receiver&&!initial)
            return "lvui::native::Methods::receiver_index(context, [scope]([[maybe_unused]] lvui::native::ReadContext& context) -> decltype(auto) { return ("+recurse(value.elements.at(0))+"); }, [scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+recurse(value.elements.at(1))+"; })";
        if (value.kind == ValueKind::index) {
            const auto base = recurse(value.elements.at(0)), offset = recurse(value.elements.at(1));
            return script_sequence({base, offset}, initial, [&](const auto& items) {
                return prefix + "index(" + items[0] + ", " + items[1] + ")";
            });
        }
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
                std::vector<std::string> channels;
                for(const auto& channel:value.elements)channels.push_back(recurse(channel));
                return script_sequence(channels,initial,[&](const auto& values){
                    std::string result="lvui::native::color<ii::gui::WidgetColor>(";
                    for(const auto& channel:values){if(result.back()!='(')result+=", ";result+=channel;}return result+")";
                });
            }
            if(value.text=="Metric"){
                if(value.elements.size()!=1)error(at,"Metric requires one numeric value");
                return "lvui::native::number("+recurse(value.elements.front())+")";
            }
            if(value.text=="composite"){
                if(value.elements.size()!=2)error(at,"composite requires two Colors");
                const auto first=color_expression(value.elements[0],at,initial),second=color_expression(value.elements[1],at,initial);
                return script_sequence({first,second},initial,[&](const auto& values){
                    return "lvui::native::composite_color<ii::gui::WidgetColor>("+values[0]+", "+values[1]+")";
                });
            }
            const auto dot = value.text.find('.');
            if (const auto backend = backends.find(value.text.substr(0, dot)); backend != backends.end() && dot != value.text.npos && value.text.find('.',dot+1)==value.text.npos) {
                const auto method = value.text.substr(dot + 1);
                if (!identifier(method)) error(at, "Invalid public C++ method name");
                std::vector<std::string> factory_arguments;
                for (const auto& argument : value.elements) {
                    factory_arguments.push_back(recurse(argument));
                }
                const auto& access = backend_access.at(backend->first);
                const auto call = capturing_receiver ? factory_call(access.object,method,factory_arguments) :
                    script_sequence(factory_arguments, initial, [&](const auto& values) {
                        std::string arguments;
                        for (const auto& item : values) { if (!arguments.empty()) arguments += ", "; arguments += item; }
                        return access.object + "." + method + "(" + arguments + ")";
                    });
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
        else if (binding.type == "String") expression = "std::string(lvui::native::string_view(" + expression + "))";
        else if (binding.type == "Object") {
            object_value_runtime = true;
            expression = "lvui::native::require_object(lvui::native::object_value(" + expression + "))";
        }
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
        else if(binding.initializer.kind==ValueKind::object)inferred_kind=ValueKind::object;
        else if(binding.initializer.kind==ValueKind::reference)
            if(auto known=binding_kinds.find(binding.initializer.text);known!=binding_kinds.end())inferred_kind=known->second;
        if (const auto known = binding_kinds.find(binding.initializer.text);
            binding.initializer.kind == ValueKind::reference && known != binding_kinds.end() && !binding.type.empty()) {
            const auto kind = known->second;
            const bool valid = (binding.type == "String" && kind == ValueKind::string) ||
                               (binding.type == "Bool" && kind == ValueKind::boolean) ||
                               (binding.type == "Color" && (kind == ValueKind::color || kind == ValueKind::string)) ||
                               (binding.type == "Object" && kind == ValueKind::object) ||
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
                writable[key] = {"(*scope->"+field+")", binding.type, inferred_kind == ValueKind::object};
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
            writable[key] = {"(*scope->state_" + name + ")", binding.type, binding.type == "Object"};
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
        if (!binding.type.empty()) binding_kinds[binding.name] = binding.type == "String" ? ValueKind::string : binding.type == "Bool" ? ValueKind::boolean : binding.type == "Color" ? ValueKind::color : binding.type == "Metric" ? ValueKind::metric : binding.type == "Object" ? ValueKind::object : ValueKind::number;
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
    void emit_stack_style(const Inputs &values, const std::string &variable) {
        const auto background = values.find("backgroundColor"), border = values.find("borderColor");
        if (background == values.end() && border == values.end() && !values.contains("borderWidth") &&
            !values.contains("cornerRadius")) return;
        const bool dynamic=std::ranges::any_of(values,[&](const auto& item){return (item.first=="backgroundColor"||item.first=="borderColor"||item.first=="borderWidth"||item.first=="cornerRadius")&&native_expression(item.second->value);});
        if(!dynamic && border==values.end() && values.contains("borderWidth") &&
           numeric(values,"borderWidth",0,0,8192)!=0)
            error(values.at("borderWidth")->position,"A stack borderWidth requires borderColor");
        panel_paint_runtime=true;if(dynamic)widget_color_runtime=true;
        const auto optional_color=[&](std::string_view name){auto found=values.find(name);return found==values.end()?std::string("std::optional<ii::gui::WidgetColor>{}"):"std::optional<ii::gui::WidgetColor>{"+color_expression(found->second->value,found->second->position)+"}";};
        const auto metric=[&](std::string_view name,double fallback){auto found=values.find(name);return found!=values.end()&&native_expression(found->second->value)?"lvui::native::range("+expression_code(found->second->value,found->second->position)+", 0, 8192)":decimal(numeric(values,name,fallback,0,8192));};
        const auto colors="ii::gui::PanelColors{"+optional_color("backgroundColor")+", "+optional_color("borderColor")+", "+
            metric("borderWidth",border==values.end()?0:token_number("--lvui-theme-metrics-stroke-1",0,8192))+", "+metric("cornerRadius",0)+"}";
        if(dynamic){
            body+="            { auto bound = lvui::native::bind_panel_colors<ii::gui::PanelPainter>(*native_bindings, *"+variable+", [scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+colors+"; }); if (!bound) return std::unexpected(bound.error()); }\n";
        } else {
            body+="            if (auto painted = ii::gui::install_panel_paint(*"+variable+", "+colors+"); !painted) return painted;\n";
        }
    }
    struct AccentColors final { StyleColor primary, hover, pressed, subtle, selected; };
    AccentColors accent_colors(std::string_view accent) const {
        const auto selected=parse_window_accent(accent);
        if(!selected) error({}, "Unknown LVUI Window accent");
        const auto base="--lvui-theme-colors-accent-"+std::string(accent);
        if(default_style_tokens().find(base+"-default"))
            return {token_color(accent=="blue"?"--lvui-theme-colors-action-primary":base+"-default"),
                    token_color(base+"-hover"),token_color(base+"-pressed"),token_color(base+"-subtle"),token_color(base+"-selected")};
        auto primary=*fallback_window_accent(*selected);
        auto hover=primary,pressed=primary,subtle=primary,selected_color=primary;
        constexpr auto surface=figma_defaults::panel_surface_interactive;
        for(auto channel:{&StyleColor::red,&StyleColor::green,&StyleColor::blue}) {
            hover.*channel/=1.12; pressed.*channel/=1.2;
            subtle.*channel=surface.*channel*.84+primary.*channel*.16;
            selected_color.*channel=surface.*channel*.75+primary.*channel*.25;
        }
        return {primary,hover,pressed,subtle,selected_color};
    }
    std::string accent_style(std::size_t role,bool initial) const {
        if(role>4)throw std::logic_error("Invalid Window accent role");
        if(!active_window_accent.empty()&&!initial)
            return "([&] { const auto c = window_accent_palette(context.read("+active_window_accent+"))["+
                std::to_string(role)+"]; return lvui::StyleColor{c.red, c.green, c.blue, c.alpha}; }())";
        const auto palette=accent_colors(window_accent_name);
        const std::array colors{palette.primary,palette.hover,palette.pressed,palette.subtle,palette.selected};
        const auto c=colors[role];
        return "lvui::StyleColor{"+decimal(c.red)+", "+decimal(c.green)+", "+decimal(c.blue)+", "+decimal(c.alpha)+"}";
    }
    std::string accent_tint(double alpha,bool initial) const {
        return "([&] { auto color = "+accent_style(0,initial)+"; color.alpha *= "+decimal(alpha)+"; return color; }())";
    }
    std::string danger_state_setup(std::string_view palette) const {
        std::string result;
        for(const auto& [field,key]:{std::pair{"danger_reference","--lvui-theme-colors-action-danger"},
                std::pair{"danger_hover_default","--lvui-theme-colors-accent-red-hover"},
                std::pair{"danger_pressed_default","--lvui-theme-colors-accent-red-pressed"}}){
            const auto color=token_color(key);
            result+=std::string(palette)+field+" = lvui::StyleColor{"+decimal(color.red)+", "+
                decimal(color.green)+", "+decimal(color.blue)+", "+decimal(color.alpha)+"}; ";
        }
        return result;
    }
    std::string hierarchy_accent_setup(const Inputs& values,std::string_view palette,bool initial) const {
        std::string result;
        if(!values.contains("rowBackgroundColorActive"))result+=std::string(palette)+"active = "+accent_tint(.24,initial)+"; ";
        if(!values.contains("rowBackgroundColorDrag"))result+=std::string(palette)+"drag = "+accent_tint(.16,initial)+"; ";
        if(!values.contains("focusColor"))result+=std::string(palette)+"focus = "+accent_style(0,initial)+"; ";
        return result;
    }
    std::string list_accent_setup(const Inputs& values,std::string_view palette,std::string_view selected,bool initial) const {
        std::string result;
        if(!values.contains("pressedColor"))result+=std::string(palette)+"pressed = "+accent_style(3,initial)+"; ";
        if(!values.contains(selected))result+=std::string(palette)+"selected = "+accent_tint(.30,initial)+"; ";
        if(!values.contains("focusColor"))result+=std::string(palette)+"focus = "+accent_style(0,initial)+"; ";
        return result;
    }
    std::string table_accent_setup(const Inputs& values,std::string_view palette,std::string_view current,bool initial) const {
        std::string result;
        if(!values.contains("selectionColor"))result+=std::string(palette)+"selection = "+accent_tint(31./255.,initial)+"; ";
        if(!values.contains(current))result+=std::string(palette)+"current = "+accent_style(0,initial)+"; ";
        return result;
    }
    std::string toolbar_accent_setup(const Inputs& values,std::string_view palette,bool initial) const {
        std::string result;
        if(!values.contains("pressedColor"))result+=std::string(palette)+"pressed = "+accent_style(3,initial)+"; ";
        if(!values.contains("primaryColor"))result+=std::string(palette)+"primary = "+accent_style(0,initial)+"; ";
        if(!values.contains("focusColor"))result+=std::string(palette)+"focus = "+accent_style(0,initial)+"; ";
        return result;
    }
    std::string menu_accent_setup(bool initial) const {
        return "value.configuration.palette.selected = "+accent_style(0,initial)+
            "; value.configuration.palette.pressed = "+accent_style(3,initial)+
            "; value.configuration.palette.focus = "+accent_style(0,initial)+"; ";
    }
    std::string accent_palette_code() const {
        if(active_window_accent.empty())return {};
        std::string result="    static std::array<ii::gui::WidgetColor, 5> window_accent_palette(lvui::WindowAccent value) {\n        switch (value) {\n";
        for(const auto name:window_accent_names){
            const auto colors=accent_colors(name);
            result+="        case lvui::WindowAccent::"+std::string(name)+": return {"+color_literal(colors.primary)+", "+
                    color_literal(colors.hover)+", "+color_literal(colors.pressed)+", "+color_literal(colors.subtle)+", "+color_literal(colors.selected)+"};\n";
        }
        return result+"        }\n        throw std::invalid_argument(\"Invalid Window accent\");\n    }\n";
    }
    std::string appearance_code(const Inputs &values, const std::string &accent) const {
        std::string code =
            "    static ii::gui::WidgetAppearance appearance() {\n        ii::gui::require_text_weight_rules_abi_v1();\n        auto result = " +
            ((std::string("ii::gui::WidgetAppearance{}"))) +
            ";\n        result.styled = true;\n";
        const auto font =
            default_style_tokens().string("--lvui-theme-typography-font-font-body", options.style_tokens);
        if (!font)
            error({}, font.error().message);
        const auto family = text(values, "fontFamily", *font);
        if (family.empty())
            error({}, "Default font family cannot be empty");
        code += "        result.window = " +
                widget_color(values, "backgroundColor",
                             color_literal(token_color("--lvui-theme-colors-panel-window"))) +
                ";\n        result.font_family = " + literal(family) + ";\n";
        code += "        result.disabled_background = " +
                color_literal(token_color("--lvui-theme-colors-panel-sub-surface")) + ";\n";
        const auto palette=accent_colors(accent);
        code += "        result.primary = " + color_literal(palette.primary) +
                ";\n        result.hover = " + color_literal(palette.hover) +
                ";\n        result.pressed = " + color_literal(palette.pressed) + ";\n";
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
        code += "        result.text_weight_rules = std::vector<ii::gui::TextWeightRule>{";
        first = true;
        for (const auto& [size_token,weight_token] : {
                std::pair{"title-text-title", "bold"}, std::pair{"subtitle-text-subtitle", "bold"},
                std::pair{"header-text-header", "semi-bold"}, std::pair{"subheader-text-subheader", "semi-bold"},
                std::pair{"body-text-body", "medium"}, std::pair{"description-text-description", "semi-bold"},
                std::pair{"caption-text-caption", "regular"}}) {
            const auto size=token_number("--lvui-theme-typography-"+std::string(size_token),1,256);
            const auto weight=token_number("--lvui-theme-typography-weight-"+std::string(weight_token),400,700);
            if(weight!=400&&weight!=500&&weight!=600&&weight!=700)
                error({}, "Typography weight must be 400/500/600/700 for native fonts");
            if(!first)code+=", ";
            first=false;
            code+="{"+decimal(size)+", "+decimal(weight)+"}";
        }
        code += "};\n";
        code += "        result.button_height = " +
                decimal(token_number("--lvui-theme-metrics-size-22", 1, 8192)) +
                ";\n        result.button_padding = " +
                decimal(token_number("--lvui-theme-metrics-space-8", 0, 8192)) +
                ";\n        result.button_radius = " +
                decimal(token_number("--lvui-theme-metrics-radius-8", 0, 8192)) +
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
            result+=", .appearance = [&] { auto value = "+window_appearance_name+"(); ";
            if(auto input=values.find("accent");input!=values.end()&&native_expression(input->second->value)){
                checked_kind(*input->second,{ValueKind::string});
                const auto selected=materialize_window_input("lvui::native::window_accent("+expression_code(input->second->value,input->second->position)+")");
                window_accent_derived=window_input_names.back();
                result+="auto palette = window_accent_palette("+selected+"); value.primary = palette[0]; value.hover = palette[1]; value.pressed = palette[2]; ";
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
        if(window_accent_derived.empty()){
            body+="            { auto endpoint = widgets.window_properties(); if (endpoint) { auto target = *endpoint;\n"+window_input_setup+"            auto binding = native_bindings->bind(["+captures+"]([[maybe_unused]] lvui::native::ReadContext& context) { return "+expression+"; }, [target](const ii::gui::WindowProperties& value) { return target.update(value); }); if (!binding) return std::unexpected(binding.error()); } else if (endpoint.error().code != ii::core::ErrorCode::unsupported) return std::unexpected(endpoint.error()); }\n";
        }else{
            body+="            { auto endpoint = widgets.window_properties(); if (endpoint) { auto target = *endpoint;\n"+window_input_setup+
                  "            auto binding = native_bindings->bind(["+captures+"]([[maybe_unused]] lvui::native::ReadContext& context) { auto properties = "+expression+
                  "; return std::pair{std::move(properties), context.read(*"+window_accent_derived+")}; }, [target, scope](const auto& value) -> ii::core::Result<void> { if (auto updated = target.update(value.first); !updated) return updated; scope->window_accent.set(value.second); return {}; }); if (!binding) return std::unexpected(binding.error()); } else if (endpoint.error().code != ii::core::ErrorCode::unsupported) return std::unexpected(endpoint.error()); }\n";
        }
    }
    Inputs inputs(const View& view, std::initializer_list<std::string_view> allowed, bool action = false) const {
        Inputs out;
        for (const auto& input : view.inputs) {
            auto name = input.name;
            if (name.empty() && action) name = "onClick";
            const auto kind = view.component->spelling;
            if ((kind == "HStack" || kind == "VStack" || kind == "ZStack") && name == "gap") name = "spacing";
            if (kind == "IconButton" && name == "label") name = "text";
            const bool common=(name=="visible"||name=="flex"||name=="opacity")&&view.component->spelling!="ApplicationWindow"&&view.component->spelling!="Window"&&view.component->spelling!="Application";
            if (std::ranges::find(allowed, name) == allowed.end() && !common) error(input.position, "Unknown input '" + name + "' for " + std::string(view.component->spelling));
            if (!out.emplace(name, &input).second) error(input.position, "Duplicate component input '" + name + "'");
        }
        if (!view.bindings.empty() || !view.imports.empty()) error(view.position, "Nested declarations are not supported by native AOT yet");
        return out;
    }
    std::string text(const Inputs& values, std::string_view name, std::string fallback = {}) const {
        auto it = values.find(name);
        if (it == values.end()) return (name == "id" || name == "targetId" ? id_prefix : "") + fallback;
        const auto& source = it->second->value;
        if ((name == "id" || name == "targetId") && source.kind == ValueKind::reference &&
            identifier(source.text) && !constants.contains(source.text) && !native_values.contains(source.text) &&
            !reactive.contains(source.text) && !backend_access.contains(source.text))
            return id_prefix + source.text;
        auto value = resolve(source, it->second->position);
        if (value.kind != ValueKind::string) error(it->second->position, "Expected String for '" + std::string(name) + "'");
        return (name == "id" || name == "targetId" ? id_prefix : "") + value.text;
    }
    void emit_context_bindings(std::string_view scope_label){
        for(const auto& binding:context_bindings){
            if(binding.material){
                const bool dynamic=!binding.background_expression.empty()||!binding.content_expression.empty();
                std::string captures="scope";
                const auto source=[&](const std::string& id,const std::string& expression,std::string_view alias){
                    if(!expression.empty())return expression;
                    if(id.empty())return std::string("ii::gui::Widget{}");
                    const auto target=widget_variables.find(id);
                    if(target==widget_variables.end())error(binding.position,"Unknown material backdrop source "+std::string(scope_label));
                    if(dynamic){captures+=", "+std::string(alias)+" = *"+target->second;return std::string(alias);}
                    return "*"+target->second;
                };
                const auto background=source(binding.background,binding.background_expression,"material_background");
                const auto content=source(binding.target,binding.content_expression,"material_content");
                if(dynamic){
                    body+="            { auto binding = native_bindings->bind(["+captures+"]([[maybe_unused]] lvui::native::ReadContext& context) { return lvui::native::MaterialSourcesValue<ii::gui::Widget>{.background = "+background+", .content = "+content+"}; }, [component = *"+binding.menu+"](const lvui::native::MaterialSourcesValue<ii::gui::Widget>& value) { return component.set_sources(value.background, value.content); }); if (!binding) return std::unexpected(binding.error()); }\n";
                }else body+="            if (auto bound = "+binding.menu+"->set_sources("+background+", "+content+"); !bound) return bound;\n";
                continue;
            }
            auto target=widget_variables.find(binding.target);
            if(target==widget_variables.end())error(binding.position,std::string(binding.wheel?"Unknown WheelScrollGuard targetId ":binding.tooltip?"Unknown Tooltip targetId ":"Unknown context menu targetId ")+std::string(scope_label));
            body+="            if (auto bound = "+((binding.tooltip||binding.wheel)?binding.menu+"->set_target(*"+target->second+")":target->second+"->set_context_menu(*"+binding.menu+")")+"; !bound) return bound;\n";
        }
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
        std::string result="lvui::native::stack_layout<ii::gui::StackLayoutOptions>({.spacing = "+numeric_input("spacing",-1,-32767,32767)+
            ", .default_spacing = "+numeric_input("defaultSpacing",token_number("--lvui-theme-metrics-space-8",0,32767),0,32767)+
            ", .padding = "+numeric_input("padding",token_number("--lvui-theme-metrics-space-0",0,32767),0,32767);
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
        input.default_spacing=numeric(values,"defaultSpacing",token_number("--lvui-theme-metrics-space-8",0,32767),0,32767);
        input.padding=numeric(values,"padding",token_number("--lvui-theme-metrics-space-0",0,32767),0,32767);
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
    static bool scene_command(const Value& value) {
        return value.kind == ValueKind::call &&
            (value.text == "openWindow" || value.text == "closeWindow" ||
             value.text == "openScene" || value.text == "closeScene");
    }
    static bool localization_command(const Value& value) {
        return value.kind==ValueKind::call&&(value.text=="setLocale"||value.text=="setString");
    }
    std::string localization_statement(const Value& value, SourcePosition at) const {
        const auto count=value.text=="setLocale"?std::size_t(1):std::size_t(2);
        if(value.elements.size()!=count)error(at,value.text+" requires "+std::to_string(count)+" String argument(s)");
        std::string arguments;
        for(std::size_t index=0;index<value.elements.size();++index){
            const auto& element=value.elements[index];
            if(element.kind==ValueKind::number||element.kind==ValueKind::boolean||element.kind==ValueKind::list||element.kind==ValueKind::object)
                error(at,value.text+" requires String arguments");
            if(element.kind==ValueKind::reference)if(auto kind=binding_kinds.find(element.text);kind!=binding_kinds.end()&&kind->second!=ValueKind::string)
                error(at,value.text+" requires String arguments");
            if(index==0&&element.kind==ValueKind::string){
                if(value.text=="setLocale"&&!lvui::native::AppStrings::normalize_locale(element.text))error(at,"setLocale requires a valid locale tag");
                if(value.text=="setString"&&(element.text.empty()||element.text.size()>lvui::native::AppStrings::maximum_key_bytes||element.text.find('\0')!=element.text.npos||!ii::core::valid_utf8(element.text)))
                    error(at,"setString requires a nonempty bounded UTF-8 key");
            }
            if(!arguments.empty())arguments+=", ";
            arguments+="lvui::native::string_view("+expression_code(element,at,true)+")";
        }
        localization_runtime=native_runtime=true;needs_parent_scope=true;
        return "{ auto result = scope->backend_factories.app_strings()->"+std::string(value.text=="setLocale"?"set_locale":"set_string")+"("+arguments+"); if (!result) { if (result.error().cause) std::rethrow_exception(result.error().cause); throw std::runtime_error(result.error().message); } } ";
    }
    void validate_scene_command(const PendingSceneCommand& command) const {
        if (!application_generation || application_scene_ids.size() < 2 || !scene_registry_runtime ||
            (command.id && !application_scene_index.contains(*command.id)))
            error(command.position, "Scene or Window command requires the id of an independently managed scene");
    }
    void finish_scene_declarations() {
        scene_declarations_complete = true;
        for (const auto& command : pending_scene_commands) validate_scene_command(command);
        pending_scene_commands.clear();
    }
    std::string scene_command_statement(const Value& value, SourcePosition at) const {
        if (value.elements.size() != 1)
            error(at, "Scene or Window command requires one scene id");
        const auto& argument = value.elements.front();
        std::string target, prepare;
        PendingSceneCommand command{at, std::nullopt};
        if (native_expression(argument)) {
            const auto selected = expression_code(argument, at, true);
            prepare = "if (!lvui::native::detail::settle_scope_chain(scope) || !scope->active()) return; "
                      "auto&& selected_window = (" + selected + "); if (!scope->active()) return; "
                      "auto selected_window_id = lvui::native::string_view(selected_window); if (!scope->active()) return; ";
            // The registry resolves the id before queueing, while the local extends any temporary.
            target = "selected_window_id";
        } else {
            const auto id = resolve(argument, at);
            if (id.kind != ValueKind::string)
                error(at, "Scene or Window command requires a string id");
            command.id = id.text;
            target = literal(id.text);
        }
        if (scene_declarations_complete) validate_scene_command(command);
        else pending_scene_commands.push_back(std::move(command));
        native_runtime = true;
        return "{ auto scenes = scope->scene_registry.lock(); if (!scenes) throw std::runtime_error(\"Application scenes are unavailable\"); " +
            prepare + "auto result = scenes->" +
            std::string(value.text == "openWindow" || value.text == "openScene" ? "open" : "close") +
            "(" + target + "); if (!result) { if (result.error().cause) std::rethrow_exception(result.error().cause); throw std::runtime_error(result.error().message); } } ";
    }
    std::string application_quit_statement(const Value& value, SourcePosition at) const {
        if (!application_generation || !value.elements.empty())
            error(at, "quitApplication requires an Application and no arguments");
        application_quit_runtime = native_runtime = true;
        std::string root = "scope";
        for (std::size_t depth = 0; depth < lexical_depth; ++depth) root += "->parent";
        return "{ auto commands = " + root +
            "->application_commands.lock(); if (!commands) throw std::runtime_error(\"Application commands are unavailable\"); "
            "auto result = commands->quit(); if (!result) { if (result.error().cause) std::rethrow_exception(result.error().cause); throw std::runtime_error(result.error().message); } } ";
    }
    std::string action_block_body(const Value& value) const {
            const ScriptGeneration generation(script_generation_depth);
            std::string statements;
            // The statement boundary settles dependencies. A lifetime check
            // during snapshot copying must not trigger additional evaluations.
            const std::string active = "scope->active()";
            for (const auto& statement : value.elements) {
                statements += "if (!lvui::native::detail::settle_scope_chain(scope) || !scope->active()) return; ";
                if (statement.kind == ValueKind::action_if) {
                    const auto& condition = statement.elements.at(0);
                    if (condition.kind == ValueKind::reference) {
                        const auto known = binding_kinds.find(condition.text);
                        if (known != binding_kinds.end() && known->second != ValueKind::boolean)
                            error(condition.position, "An event condition requires Bool");
                    }
                    if (!scoped_expression(condition) && resolve(condition, condition.position).kind != ValueKind::boolean)
                        error(condition.position, "An event condition requires Bool");
                    const auto test = expression_code(condition, condition.position, true);
                    const auto name = "_lvui_script_condition_" + std::to_string(next_script_local++);
                    const auto accepted = action_block_body(statement.elements.at(1));
                    const auto rejected = statement.elements.size() == 3 ? action_block_body(statement.elements.at(2)) : std::string{};
                    statements += "{ const bool " + name + " = lvui::native::truth(" + test + "); if (!" + active + ") return; if (" + name + ") { " + accepted + " }";
                    if (statement.elements.size() == 3) statements += " else { " + rejected + " }";
                    statements += " } ";
                } else if (statement.kind == ValueKind::action_for) {
                    if (script_name_bound(statement.text)) error(statement.position, "Event iteration name is already bound");
                    const auto& source = statement.elements.at(0);
                    if (source.kind == ValueKind::number || source.kind == ValueKind::boolean || source.kind == ValueKind::string ||
                        source.kind == ValueKind::object || source.kind == ValueKind::color || source.kind == ValueKind::metric)
                        error(source.position, "Event iteration requires a collection");
                    if (source.kind == ValueKind::reference) {
                        const auto known = binding_kinds.find(source.text);
                        if (known != binding_kinds.end() && known->second != ValueKind::list && known->second != ValueKind::reference)
                            error(source.position, "Event iteration requires a collection");
                    }
                    const auto input = expression_code(source, source.position, true);
                    const auto suffix = std::to_string(next_script_local++);
                    const auto item = "_lvui_script_item_" + suffix;
                    const auto source_name = "_lvui_script_source_" + suffix;
                    const auto snapshot = "_lvui_script_snapshot_" + suffix;
                    script_locals.push_back({statement.text, item});
                    ++script_loop_depth;
                    std::string body;
                    try { body = action_block_body(statement.elements.at(1)); }
                    catch (...) { --script_loop_depth; script_locals.pop_back(); throw; }
                    --script_loop_depth;
                    script_locals.pop_back();
                    statements += "{ auto&& " + source_name + " = (" + input + "); if (!" + active + ") return; "
                        "auto " + snapshot + " = lvui::native::script_snapshot(std::forward<decltype(" + source_name + ")>(" + source_name + "), [&] { return " + active + "; }); "
                        "if (!" + snapshot + ") return; for ([[maybe_unused]] const auto& " + item + " : *" + snapshot + ") { if (!" + active + ") return; " + body + " } } ";
                } else if (statement.kind == ValueKind::action_break || statement.kind == ValueKind::action_continue) {
                    if (!script_loop_depth) error(statement.position, "break and continue are only valid inside a for");
                    statements += statement.kind == ValueKind::action_break ? "break; " : "continue; ";
                } else if (statement.kind == ValueKind::action_return) {
                    statements += "return; ";
                } else if (statement.kind == ValueKind::assignment) {
                    const auto dot = statement.text.find('.');
                    const auto target = writable.find(statement.text.substr(0, dot));
                    if (target == writable.end()) error(statement.position, "Event assignment requires a writable lexical var");
                    std::string operation;
                    if (dot != statement.text.npos) {
                        if (!target->second.object) error(statement.position, "Only a mutable LVUI Object permits field assignment");
                        object_value_runtime = true;
                        std::string path = "{";
                        auto remaining = std::string_view(statement.text).substr(dot + 1);
                        while (!remaining.empty()) {
                            const auto next = remaining.find('.');
                            if (path.size() > 1) path += ", ";
                            path += literal(remaining.substr(0, next));
                            if (next == remaining.npos) break;
                            remaining.remove_prefix(next + 1);
                        }
                        path += "}";
                        operation = "lvui::native::assign_object_field(" + target->second.target + ", " + path + ", " +
                                    expression_code(statement.elements.at(0), statement.position, true) + ")";
                    } else {
                        const Binding assignment{statement.text, target->second.type, statement.elements.at(0), false, statement.position};
                        auto expression = typed_expression(assignment, true);
                        if (target->second.type.empty())
                            expression = "lvui::native::inferred_assignment(" + target->second.target + ", " + expression + ")";
                        if (target->second.object) {
                            object_value_runtime = true;
                            expression = "lvui::native::require_object(" + expression + ")";
                        }
                        operation = "lvui::native::assign_value(" + target->second.target + ", " + expression + ")";
                    }
                    statements += "{ auto result = " + operation + "; "
                        "if (!result) { if (result.error().cause) std::rethrow_exception(result.error().cause); throw std::runtime_error(result.error().message); } } ";
                } else if (scene_command(statement)) {
                    statements += scene_command_statement(statement, statement.position);
                } else if (localization_command(statement)) {
                    statements += localization_statement(statement, statement.position);
                } else if (statement.kind == ValueKind::call && statement.text == "quitApplication") {
                    statements += application_quit_statement(statement, statement.position);
                } else if (statement.kind == ValueKind::call && statement.text == "query") {
                    const auto call = expression_code(statement, statement.position, true);
                    const auto& port = statement.elements.at(0).text;
                    const auto owner = backend_access.find(port.substr(0, port.find('.')));
                    if (owner == backend_access.end() || owner->second.revision.empty()) statements += "(void)(" + call + "); ";
                    else statements += "(void)lvui::native::invoke(" + owner->second.revision + ", [&] { (void)(" + call + "); }); ";
                } else if (direct_event_call(statement)) {
                    const auto call = event_expression(statement, statement.position);
                    const auto revision = event_revision(statement);
                    if (revision.empty()) statements += "(void)(" + call + "); ";
                    else statements += "(void)lvui::native::invoke(" + revision + ", [&] { (void)(" + call + "); }); ";
                } else error(statement.position, "Unsupported event statement");
            }
            native_runtime = true;
            return "try { " + statements + " } catch (const lvui::native::detail::ScriptRetired&) { return; } ";
    }
    std::string action(const Inputs& values, std::string_view name="onClick", std::string parameter={},std::string arguments="value",bool query_notification=false) const {
        auto it = values.find(name);
        if (it == values.end()) return "{}";
        const auto& value = it->second->value;
        if (value.kind == ValueKind::action) {
            return "lvui::native::callback(scope, native_bindings, []([[maybe_unused]] const auto& scope" +
                (parameter.empty() ? std::string{} : ", [[maybe_unused]] auto&&... ignored") + ") { " + action_block_body(value) + "})";
        }
        if (localization_command(value)) {
            return "lvui::native::callback(scope, native_bindings, []([[maybe_unused]] const auto& scope" +
                (parameter.empty() ? std::string{} : ", [[maybe_unused]] auto&&... ignored") +
                ") { " + localization_statement(value, it->second->position) + "})";
        }
        if (value.kind == ValueKind::call && value.text == "quitApplication") {
            if (!parameter.empty())
                error(it->second->position, "quitApplication requires an Application and a no-argument event");
            return "lvui::native::callback(scope, native_bindings, []([[maybe_unused]] const auto& scope) { " +
                application_quit_statement(value, it->second->position) + "})";
        }
        if (scene_command(value)) {
            return "lvui::native::callback(scope, native_bindings, []([[maybe_unused]] const auto& scope" +
                (parameter.empty() ? std::string{} : ", [[maybe_unused]] auto&&... ignored") +
                ") { " + scene_command_statement(value, it->second->position) + "})";
        }
        const auto target = callback_target(value, it->second->position, 0,
                                            it->second->name == name && name.starts_with("on"));
        if(target.signature&&target.signature->result!="Void")error(it->second->position,"An event callback requires a Void result");
        auto args=parameter.empty()?std::string{}:arguments;
        if(query_notification&&target.signature&&target.signature->arguments.empty())args.clear();
        native_runtime = true;
        const auto& access = target.access;
        if(!target.shared.empty())return shared_action(target,parameter,args);
        if(!target.block.empty())return "lvui::native::callback(scope, native_bindings, []([[maybe_unused]] const auto& scope" +
            (parameter.empty() ? std::string{} : ", [[maybe_unused]] auto&&... ignored") + ") { " + target.block + "})";
        if(!target.factory.empty())return returned_action(target,parameter,args,false,{},query_notification);
        if(query_notification&&!target.signature){
            const auto call="[]<class B, class V>(B& backend, const V& value) { if constexpr (requires { backend."+target.method+"(value); }) (void)backend."+target.method+"(value); else (void)backend."+target.method+"(); }("+access.object+", "+arguments+")";
            const auto invoked=access.revision.empty()?call+";":"(void)lvui::native::invoke("+access.revision+", [&] { "+call+"; });";
            return "lvui::native::callback(scope, native_bindings, []([[maybe_unused]] const auto& scope, "+parameter+") { "+invoked+" })";
        }
        return "lvui::native::callback(scope, native_bindings, []([[maybe_unused]] const auto& scope" + (parameter.empty()?std::string{}:", "+parameter) + ") { (void)lvui::native::invoke(" + access.revision + ", [&]() -> decltype(auto) { return " + method_invocation(target,args) + "; }); })";
    }
    std::string bindable_target(const Value& value, std::size_t depth=0) const {
        if(depth>64)error(value.position,"Maximum binding alias depth is 64");
        if(value.kind==ValueKind::internal_argument){
            const auto found=bound_values.find(value.text);
            if(found==bound_values.end()||found->second.writable.empty())return {};
            if(lexical_depth<found->second.depth)error(value.position,"A binding escaped its lexical owner");
            auto target=found->second.writable;
            for(auto step=found->second.depth;step<lexical_depth;++step)borrow_path(target);
            return target;
        }
        if(value.kind!=ValueKind::reference)return {};
        if(auto local=writable.find(value.text);local!=writable.end())return local->second.target;
        if(auto alias=constants.find(value.text);alias!=constants.end())return bindable_target(alias->second,depth+1);
        const auto dot=value.text.find('.');
        if(dot==value.text.npos||value.text.find('.',dot+1)!=value.text.npos)return {};
        const auto backend=backend_access.find(value.text.substr(0,dot));
        const auto member=value.text.substr(dot+1);
        if(backend==backend_access.end()||!identifier(member))return {};
        return backend->second.object+"."+member;
    }
    std::string bindable_action(const Inputs& values, std::string_view input,
                                std::string_view event, std::string_view type) const {
        const auto notification=action(values,event,std::string(type)+" value");
        // An explicit command can validate or reject an edit. It remains the
        // authority for that control rather than being bypassed by a write.
        if(values.contains(event))return notification;
        const auto found=values.find(input);
        if(found==values.end())return notification;
        const auto target=bindable_target(found->second->value);
        if(target.empty())return notification;
        native_runtime=true;
        return "lvui::native::automatic_binding<"+std::string(type)+">(scope, native_bindings, "
            "[](const auto& scope) -> decltype(auto) { return ("+target+"); })";
    }
    std::string close_decision(const Inputs& values) const {
        auto found = values.find("onCloseRequested");
        if (found == values.end()) return "{}";
        if (direct_event_call(found->second->value)) {
            const auto call = event_expression(found->second->value, found->second->position);
            const auto revision = event_revision(found->second->value);
            const auto result = "lvui::native::invoke_callback<bool()>([&]() -> decltype(auto) { return " + call + "; })";
            const auto invoked = revision.empty() ? result : "lvui::native::invoke(" + revision + ", [&]() -> decltype(auto) { return " + result + "; })";
            return "lvui::native::scoped_function<bool>(scope, native_bindings, []([[maybe_unused]] const auto& scope) -> bool { return " + invoked + "; })";
        }
        const auto target = callback_target(found->second->value, found->second->position);
        if (!target.block.empty()) error(found->second->position, "A close decision requires a Bool-returning C++ callable");
        if (target.signature && (target.signature->result != "bool" || !target.signature->arguments.empty()))
            error(found->second->position, "Application close decision requires () -> Bool");
        native_runtime = true;
        if (!target.shared.empty())
            return "lvui::native::scoped_function<bool>(scope, native_bindings, []([[maybe_unused]] const auto& scope) -> bool { auto handler = " + callback_path(target) + "; if (!handler) throw std::logic_error(\"Close callback is unavailable\"); return lvui::native::invoke_callback<bool()>(*handler); })";
        if (!target.factory.empty()) {
            const auto call = std::string("lvui::native::invoke_callback<bool()>(handler)");
            const auto invoked = target.access.revision.empty() ? call : "lvui::native::invoke(" + target.access.revision + ", [&]() -> decltype(auto) { return " + call + "; })";
            return "lvui::native::returned_function<bool>(scope, native_bindings, [scope]([[maybe_unused]] lvui::native::ReadContext& context) -> decltype(auto) { return " + target.factory + "; }, []([[maybe_unused]] const auto& scope, auto& handler) -> bool { return " + invoked + "; })";
        }
        const auto call = "lvui::native::invoke_callback<bool()>([&]() -> decltype(auto) { return " + method_invocation(target, "") + "; })";
        const auto invoked = target.access.revision.empty() ? call : "lvui::native::invoke(" + target.access.revision + ", [&]() -> decltype(auto) { return " + call + "; })";
        return "lvui::native::scoped_function<bool>(scope, native_bindings, []([[maybe_unused]] const auto& scope) -> bool { return " + invoked + "; })";
    }
    std::string returned_action(const BoundMethod& target,const std::string& parameter,const std::string& arguments,bool button=false,std::string_view event_type={},bool query_notification=false)const{
        auto invocation=button?"[]<class H>(H& handler, [[maybe_unused]] const "+std::string(event_type)+"& event) { if constexpr (std::invocable<H&, const "+std::string(event_type)+"&>) (void)std::invoke(handler, event); else (void)std::invoke(handler); }(handler, event)":"std::invoke(handler"+(arguments.empty()?std::string{}:", "+arguments)+")";
        if(query_notification&&!target.signature)invocation="[]<class H, class V>(H& handler, const V& value) { if constexpr (std::invocable<H&, const V&>) (void)std::invoke(handler, value); else (void)std::invoke(handler); }(handler, "+arguments+")";
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
            error(found->second->position, "Unknown LVUI text style");
        const auto role =
            found == values.end() ? std::string("description") : found->second->value.text.substr(1);
        const auto metric = text_metric(role);
        if (metric.empty())
            error(found->second->position, "Unknown LVUI text style");
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
                return "--lvui-theme-typography-" + std::string(metric);
        return {};
    }
    StyleColor text_color(std::string_view role) const {
        return token_color("--lvui-theme-colors-text-" +
                           std::string(role == "body"          ? "body-color"
                                       : role == "description" ? "description-color"
                                       : role == "caption"     ? "caption-color"
                                       : role == "disabled"    ? "disabled-color"
                                                               : "title-header-color"));
    }
    std::string stepper_arrow(const Inputs& values,std::string_view name="arrow") const {
        auto it=values.find(name);if(it==values.end()) return "up_down";
        const auto& v=it->second->value;
        if(v.kind==ValueKind::enumeration) {
            if(v.text==".upDown" || v.text==".UpDown") return "up_down";
            if(v.text==".up" || v.text==".Up") return "up";
            if(v.text==".down" || v.text==".Down") return "down";
        }
        error(it->second->position,"Unknown Stepper arrow");
    }

    std::string modal_content(const View& view,std::string_view factory_type="ModalContentFactory",bool isolated=true) {
        if(view.children.empty())return "{}";
        auto outer_body=std::move(body);auto outer_ids=std::move(ids);auto outer_variables=std::move(widget_variables);auto outer_bindings=std::move(context_bindings);
        body.clear();ids=isolated?std::set<std::string,std::less<>>{}:outer_ids;widget_variables.clear();context_bindings.clear();++factory_depth;
        for(const auto& child:view.children)emit(child,"content");
        emit_context_bindings("in content scope");
        auto children=std::move(body);--factory_depth;body=std::move(outer_body);if(isolated)ids=std::move(outer_ids);widget_variables=std::move(outer_variables);context_bindings=std::move(outer_bindings);
        if(children.empty())return "{}";
        const auto capture = native_runtime ? "[weak_scope = std::weak_ptr(scope), weak_activation = std::weak_ptr(native_bindings)]" : "[]";
        const auto admission = native_runtime ? "            auto scope = weak_scope.lock(); auto activation = weak_activation.lock();\n            if (!scope || !activation || !activation->available() || !scope->active()) return ii::core::fail(ii::core::ErrorCode::closed, \"Content owner retired\");\n" : "";
        const auto type=factory_type=="ModalContentFactory"
            ? std::string("lvui::native::ModalContentFactoryValue<ii::gui::WidgetBuilder,ii::gui::Widget>")
            : (factory_type=="SheetContentFactory" ? std::string("ii::gui::") : std::string("lvui::native::"))+std::string(factory_type);
        return type+"{"+capture+"(ii::gui::WidgetBuilder& widgets, ii::gui::Widget content) -> ii::core::Result<void> {\n" + admission +
            ((""))+binding_owner()+children+"            return {};\n        }}";
    }
    void emit_popup_open(const Inputs& values,const std::string& variable){
        const auto found=values.find("open");if(found==values.end())return;
        const auto& input=*found->second;const bool dynamic=native_expression(input.value);
        if(!dynamic&&boolean(values,"open",false)!="true")return;
        popup_open_runtime=true;
        const auto name="popup_open_binding_"+variable;
        const auto desired=dynamic?"lvui::native::truth("+expression_code(input.value,input.position)+")":"true";
        std::string visibility;
        if(auto item=values.find("visible");item!=values.end()&&native_expression(item->second->value))
            visibility="(void)lvui::native::truth("+expression_code(item->second->value,item->second->position)+"); ";
        body+="            ii::gui::require_committed_publication_abi_v1();\n"
            "            auto "+name+" = lvui::native::bind_popup_open(widgets, native_bindings, *"+variable+
            ", [scope]([[maybe_unused]] lvui::native::ReadContext& context) { "+visibility+"return "+desired+"; });\n"
            "            if (!"+name+") return std::unexpected("+name+".error());\n";
        if(!factory_depth){
            popup_mount_validation=true;std::string owner="scope";
            for(std::size_t depth=0;depth<lexical_depth;++depth)owner+="->parent";
            body+="            "+owner+"->initial_popup_bindings.emplace_back(native_bindings, *"+name+");\n";
        }
    }
    std::string widget_color(const Inputs& values,std::string_view name,std::string fallback) const {
        auto item = values.find(name);
        if (item == values.end())
            return fallback;
        return color_expression(item->second->value,item->second->position,true);
    }
    std::string alias_key(const Inputs& values,std::string_view name,std::string_view alias)const{const auto found=values.find(alias);if(values.contains(name)&&found!=values.end())error(found->second->position,"Use a property or its alias, not both");return std::string(found!=values.end()?alias:name);}
    std::string button_method(const Inputs& values,std::string_view name,std::string_view event_type={})const{
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
        return "lvui::native::callback(scope, native_bindings, []([[maybe_unused]] const auto& scope, "+argument+") { lvui::native::invoke(" + access.revision + ", [&] { []<class B>(B& backend, "+argument+") { if constexpr (requires { backend."+method+"(event); }) (void)backend."+method+"(event); else if constexpr (requires { backend."+method+"(); }) (void)backend."+method+"(); else if constexpr (requires { backend."+method+".invoke(event); }) (void)backend."+method+".invoke(event); else if constexpr (requires { backend."+method+".invoke(); }) (void)backend."+method+".invoke(); else if constexpr (requires { backend."+method+".trigger(event); }) (void)backend."+method+".trigger(event); else (void)backend."+method+".trigger(); }(" + access.object + ", event); }); })";
    }
    std::string link_value(const Inputs& values,std::string_view name,std::string fallback)const{
        auto found=values.find(name);if(found==values.end())return fallback;
        if (native_expression(found->second->value)) return expression_code(found->second->value, found->second->position, true);
        auto source=property(found->second->value);if(source.empty())error(found->second->position,"Link data input requires one scoped C++ member");return "ii::gui::binding_value("+source+")";
    }


    std::string link_methods(const Inputs& values,std::string_view event_type={})const{
        auto found=values.find("methods");if(found==values.end())return "{}";const auto& input=*found->second;
        if(native_expression(input.value))return link_value(values,"methods","{}");
        auto list=resolve(input.value,input.position);if(list.kind!=ValueKind::list||list.elements.size()>256)error(input.position,"Link methods require at most 256 deferred references");
        std::string result="{";for(const auto& value:list.elements){Input candidate{"method",value,input.position};if(result.size()>1)result+=", ";result+=button_method(Inputs{{"method",&candidate}},"method",event_type);}return result+"}";
    }
    std::string button_methods(const Input& input,std::string_view event_type)const{
        auto list=resolve(input.value,input.position);
        if(list.kind!=ValueKind::list)error(input.position,"Button methods require a list of deferred references");
        std::string result="{";std::size_t count=0;
        const auto append=[&](auto&& self,const auto& value)->void{
            if(value.kind==ValueKind::list){for(const auto& child:value.elements)self(self,child);return;}
            if(value.kind==ValueKind::reference){auto item=constants.find(value.text);
                if(item!=constants.end()&&item->second.kind==ValueKind::list){self(self,item->second);return;}}
            if(++count>256)error(input.position,"Button methods require at most 256 deferred references");
            Input candidate{"method",value,input.position};if(result.size()>1)result+=", ";
            result+=button_method(Inputs{{"method",&candidate}},"method",event_type);
        };
        for(const auto& value:list.elements)append(append,value);
        return result+"}";
    }
    void emit_button_methods(const Inputs& values,const std::string& native,std::string_view event_type){
        const auto method_type="lvui::native::ButtonMethodValue<"+std::string(event_type)+">";
        if(values.contains("method"))
            body+="            if (auto installed = "+native+"->set_method("+button_method(values,"method",event_type)+"); !installed) return std::unexpected(installed.error());\n";
        if(auto found=values.find("methods");found!=values.end()){
            if(native_expression(found->second->value)){
                const auto expression=expression_code(found->second->value,found->second->position);
                body+="            { auto binding = native_bindings->bind([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return std::make_shared<const std::vector<"+method_type+">>(lvui::native::collect_button_methods<"+std::string(event_type)+">(lvui::native::widget_value<ii::gui::WidgetColor>("+expression+"))); }, [component = *"+native+"](const auto& value) { return component.set_methods(*value); });\n"
                      "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            if (auto installed = "+native+"->set_methods(std::vector<"+method_type+">"+button_methods(*found->second,event_type)+"); !installed) return std::unexpected(installed.error());\n";
        }
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
        setup+="value.font_size = "+decimal(token_number("--lvui-theme-typography-description-text-description",1,256))+
            "; value.line_height = "+decimal(token_number("--lvui-theme-typography-description-text-description-line-height",0,8192))+
            "; value.font_weight = "+decimal(token_number("--lvui-theme-typography-weight-semi-bold",400,700))+"; ";
        setup+="value.palette.link = "+(values.contains("linkColor")?color("linkColor","--lvui-theme-colors-action-primary"):accent_style(0,initial))+
            "; value.palette.hover = "+color("hoverColor","--lvui-theme-colors-text-title-header-color")+
            "; value.palette.disabled = "+color("disabledColor","--lvui-theme-colors-text-description-color")+
            "; value.palette.focus = "+accent_style(0,initial)+"; ";
        if(auto item=values.find("pressedColor");item!=values.end())setup+="value.palette.pressed = lvui::native::link_color("+color_expression(item->second->value,item->second->position,initial)+"); ";
        return "([&] { lvui::LinkConfiguration value; "+setup+"return value; }())";
    }
    void native_binding(const Input& input, const std::string& variable, const std::string& setter) {
        const auto expression = expression_code(input.value, input.position);
        body += "            { auto binding = native_bindings->bind([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return lvui::native::widget_value<ii::gui::WidgetColor>(" + expression + "); }, [widget = *" + variable + ", setter = " + setter + "](const auto& value) { return setter(widget, value); });\n              if (!binding) return std::unexpected(binding.error()); }\n";
    }


    std::string owned_card_options(const Inputs& values,bool initial)const{
        const std::map<std::string,std::string,std::less<>> fields{{"title","title"},{"subtitle","subtitle"},{"enabled","enabled"},
            {"width","width"},{"height","height"},{"padding","padding"},{"spacing","spacing"},{"cornerRadius","corner_radius"},
            {"shapeStyle","shape"},{"backgroundColor","background"},{"titleColor","title_color"},{"subtitleColor","subtitle_color"}};
        std::string setup="value.background = "+color_literal(token_color("--lvui-theme-colors-panel-surface-solid"))+
            "; value.title_color = "+color_literal(token_color("--lvui-theme-colors-text-title-header-color"))+
            "; value.subtitle_color = "+color_literal(token_color("--lvui-theme-colors-text-body-color"))+
            "; value.divider_color = "+color_literal(token_color("--lvui-material-surface-dense-edge"))+
            "; value.padding = "+decimal(token_number("--lvui-theme-metrics-space-18",0,8192))+
            "; value.spacing = "+decimal(token_number("--lvui-theme-metrics-space-10",0,8192))+
            "; value.heading_spacing = "+decimal(token_number("--lvui-theme-metrics-space-4",0,8192))+
            "; value.corner_radius = "+decimal(token_number("--lvui-theme-metrics-radius-12",0,8192))+
            "; value.title_size = "+decimal(token_number("--lvui-theme-typography-header-text-header",1,256))+
            "; value.subtitle_size = "+decimal(token_number("--lvui-theme-typography-description-text-description",1,256))+"; ";
        for(const auto& [name,input]:values){auto field=fields.find(name);if(field==fields.end())continue;
            const auto expression=[&]{return "lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input->value,input->position,initial)+")";};
            std::string value;
            if(name=="title"||name=="subtitle")value=native_expression(input->value)?"lvui::native::text("+expression()+")":literal(text(values,name));
            else if(name=="enabled")value=native_expression(input->value)?"lvui::native::truth("+expression()+")":boolean(values,name,true);
            else if(name=="backgroundColor"||name=="titleColor"||name=="subtitleColor")
                value=color_expression(input->value,input->position,initial);
            else if(name=="shapeStyle"){
                if(native_expression(input->value))value="static_cast<ii::gui::CardShape>("+expression()+")";
                else {auto item=resolve(input->value,input->position);
                    if(item.kind==ValueKind::enumeration&&(item.text==".cylinder"||item.text==".shapeCylinder"))value="ii::gui::CardShape::cylinder";
                    else if(item.kind==ValueKind::enumeration&&(item.text==".roundRect"||item.text==".shapeRoundRect"))value="ii::gui::CardShape::round_rect";
                    else error(input->position,"Unknown independent AppCard shapeStyle");}
            }else value=native_expression(input->value)?"lvui::native::number("+expression()+")":decimal(numeric(values,name,0,0,8192));
            setup+="value."+field->second+" = "+value+"; ";
        }
        return "([&] { ii::gui::AppCardOptions value; "+setup+"return value; }())";
    }
    std::string owned_header_options(const Inputs& values,bool initial)const{
        const std::map<std::string,std::string,std::less<>> fields{{"title","title"},{"subtitle","subtitle"},{"enabled","enabled"},
            {"width","width"},{"height","height"},{"menuVisible","menu_visible"},{"compact","compact"},
            {"contentHorizontalPadding","content_horizontal_padding"},{"contentVerticalPadding","content_vertical_padding"},
            {"rowSpacing","row_spacing"},{"actionSpacing","action_spacing"},{"menuButtonPadding","menu_button_padding"},
            {"backgroundColor","background"},{"titleColor","title_color"},{"subtitleColor","subtitle_color"},
            {"menuBackgroundColor","menu_background"},{"menuPressedBackgroundColor","menu_pressed_background"}};
        const auto metric=[&](std::string_view name,double low=0,double high=8192){return decimal(token_number(name,low,high));};
        const auto focus=!active_window_accent.empty()&&!initial
            ?"window_accent_palette(context.read("+active_window_accent+"))[0]"
            :color_literal(accent_colors(window_accent_name).primary);
        std::string setup="value.background = "+color_literal(token_color("--lvui-theme-colors-panel-window-alt"))+
            "; value.title_color = "+color_literal(token_color("--lvui-theme-colors-text-title-header-color"))+
            "; value.subtitle_color = "+color_literal(token_color("--lvui-theme-colors-text-body-color"))+
            "; value.menu_background = "+color_literal(token_color("--lvui-theme-colors-panel-surface-solid"))+
            "; value.menu_pressed_background = "+color_literal(token_color("--lvui-theme-colors-panel-surface-alt"))+
            "; value.menu_focus = "+focus+
            "; value.normal_metrics.horizontal_padding = "+metric("--lvui-theme-metrics-space-16")+
            "; value.normal_metrics.vertical_padding = "+metric("--lvui-theme-metrics-space-16")+
            "; value.normal_metrics.row_spacing = "+metric("--lvui-theme-metrics-space-12")+
            "; value.normal_metrics.action_spacing = "+metric("--lvui-theme-metrics-space-8")+
            "; value.normal_metrics.menu_padding = "+metric("--lvui-theme-metrics-space-10",0,4090)+
            "; value.normal_metrics.menu_height = "+metric("--lvui-theme-metrics-size-32")+
            "; value.compact_metrics.horizontal_padding = "+metric("--lvui-theme-metrics-space-10")+
            "; value.compact_metrics.vertical_padding = "+metric("--lvui-theme-metrics-space-10")+
            "; value.compact_metrics.row_spacing = "+metric("--lvui-theme-metrics-space-8")+
            "; value.compact_metrics.action_spacing = "+metric("--lvui-theme-metrics-space-6")+
            "; value.compact_metrics.menu_padding = "+metric("--lvui-theme-metrics-space-8",0,4090)+
            "; value.compact_metrics.menu_height = "+metric("--lvui-theme-metrics-space-28")+
            "; value.title_size = "+metric("--lvui-theme-typography-subtitle-text-subtitle",1,256)+
            "; value.subtitle_size = "+metric("--lvui-theme-typography-description-text-description",1,256)+
            "; value.title_spacing = "+metric("--lvui-theme-metrics-space-4")+
            "; value.minimum_height = "+metric("--lvui-theme-metrics-size-56")+
            "; value.implicit_vertical_padding = "+metric("--lvui-theme-metrics-space-16")+
            "; value.menu_corner_radius = "+metric("--lvui-theme-metrics-radius-8")+
            "; value.menu_line_spacing = "+metric("--lvui-theme-metrics-space-4")+
            "; value.title_weight = "+metric("--lvui-theme-typography-weight-bold",400,700)+
            "; value.subtitle_weight = "+metric("--lvui-theme-typography-weight-semi-bold",400,700)+"; ";
        for(const auto& [name,input]:values){auto field=fields.find(name);if(field==fields.end())continue;
            const auto expression=[&]{return "lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input->value,input->position,initial)+")";};
            std::string value;
            if(name=="title"||name=="subtitle")value=native_expression(input->value)?"lvui::native::text("+expression()+")":literal(text(values,name));
            else if(name=="enabled"||name=="menuVisible"||name=="compact")value=native_expression(input->value)?"lvui::native::truth("+expression()+")":boolean(values,name,name=="enabled");
            else if(name=="backgroundColor"||name=="titleColor"||name=="subtitleColor"||name=="menuBackgroundColor"||name=="menuPressedBackgroundColor")
                value=color_expression(input->value,input->position,initial);
            else value=native_expression(input->value)?"lvui::native::number("+expression()+")":decimal(numeric(values,name,0,0,name=="menuButtonPadding"?4090:8192));
            setup+="value."+field->second+" = "+value+"; ";
        }
        return "([&] { ii::gui::AppHeaderOptions value; "+setup+"return value; }())";
    }
    std::string owned_listener_options(const Inputs& values,bool initial)const{
        if(auto found=values.find("sizeToContent");found!=values.end()&&native_expression(found->second->value))
            error(found->second->position,"EventListener sizeToContent must be constant");
        const auto content=boolean(values,"sizeToContent",false);
        const auto metric=[&](std::string_view name,double fallback){auto found=values.find(name);
            if(found!=values.end()&&native_expression(found->second->value))
                return "lvui::native::number(lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(found->second->value,found->second->position,initial)+"))";
            return decimal(numeric(values,name,fallback,0,8192));};
        return "ii::gui::EventListenerOptions{.width = "+metric("width",content=="true"?0:640)+
            ", .height = "+metric("height",content=="true"?0:480)+", .size_to_content = "+content+"}";
    }
    std::string listener_input(const Inputs& values)const{
        auto found=values.find("onInput");if(found==values.end())return "{}";
        if(direct_event_call(found->second->value)) {
            const auto call=event_expression(found->second->value,found->second->position);
            const auto revision=event_revision(found->second->value);
            const auto invoked=revision.empty()?call:"lvui::native::invoke("+revision+", [&]() -> decltype(auto) { return "+call+"; })";
            return "lvui::native::scoped_function<ii::core::Result<bool>>(scope, native_bindings, []([[maybe_unused]] const auto& scope, [[maybe_unused]] const ii::gui::WidgetInput& value) -> ii::core::Result<bool> { return "+invoked+"; })";
        }
        const auto target=callback_target(found->second->value,found->second->position);native_runtime=true;
        if(!target.block.empty())error(found->second->position,"An input decision requires a result-returning C++ callable");
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


    std::string native_view_options(const Inputs& v,bool initial)const{
        const auto expression=[&](std::string_view name,std::string fallback){auto it=v.find(name);return it==v.end()?fallback:expression_code(it->second->value,it->second->position,initial);};
        const auto number=[&](std::string_view name){auto it=v.find(name);if(it==v.end())return std::string("0");return native_expression(it->second->value)?"lvui::native::range("+expression(name,"")+", 0, 8192)":decimal(numeric(v,name,0,0,8192));};
        const auto flag=[&](std::string_view name){auto it=v.find(name);if(it==v.end())return std::string("true");return native_expression(it->second->value)?"lvui::native::truth("+expression(name,"")+")":boolean(v,name,true);};
        if(auto it=v.find("source");it!=v.end()&&!native_expression(it->second->value))error(it->second->position,"NativeView.source requires a native C++ view source");
        return "ii::gui::NativeViewOptions{.source = "+expression("source","{}")+", .width = "+number("width")+", .height = "+number("height")+", .enabled = "+flag("enabled")+", .clip = "+flag("clip")+"}";
    }

    std::string scroll_view_options(const Inputs& values,bool initial)const{
        const auto metric=[&](std::string_view name){
            const auto found=values.find(name);
            if(found!=values.end()&&native_expression(found->second->value))
                return "lvui::native::range("+expression_code(found->second->value,found->second->position,initial)+", 0, 8192)";
            return decimal(numeric(values,name,0,0,8192));
        };
        const auto flag=[&](std::string_view name,bool fallback){
            const auto found=values.find(name);
            if(found!=values.end()&&native_expression(found->second->value))
                return "lvui::native::truth("+expression_code(found->second->value,found->second->position,initial)+")";
            return boolean(values,name,fallback);
        };
        return "ii::gui::ScrollOptions{.width = "+metric("width")+", .height = "+metric("height")+
            ", .enabled = "+flag("enabled",true)+", .show_indicator = "+flag("showIndicator",true)+
            ", .horizontal_enabled = "+flag("horizontal",false)+"}";
    }


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
                {"selected","--lvui-theme-colors-text-title-header-color"},{"text","--lvui-theme-colors-text-body-color"},
                {"disabled_text","--lvui-theme-colors-text-disabled-color"},{"badge","--lvui-theme-colors-text-description-color"},
                {"surface_selected","--lvui-theme-colors-panel-surface-raised"},{"hover","--lvui-theme-colors-panel-surface-alt"},
                {"pressed","--lvui-theme-colors-panel-surface-raised"}}){
            const auto c=token_color(key);setup+="value.configuration.palette."+field+" = lvui::StyleColor{"+decimal(c.red)+", "+decimal(c.green)+", "+decimal(c.blue)+", "+decimal(c.alpha)+"}; ";
        }
        setup+="value.configuration.palette.accent = "+accent_style(0,initial)+"; ";
        setup+="value.configuration.font_size = "+decimal(token_number("--lvui-theme-typography-body-text-body",1,256))+"; ";
        setup+="value.configuration.line_height = "+decimal(token_number("--lvui-theme-typography-body-text-body-line-height",0,8192))+"; ";
        setup+="value.configuration.font_weight = "+decimal(token_number("--lvui-theme-typography-weight-medium",400,700))+"; ";
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
    std::string owned_mobile_tab_options(const Inputs& values,bool initial)const {
        const auto dynamic=[&](std::string_view name){auto item=values.find(name);return item!=values.end()&&native_expression(item->second->value);};
        const auto text_value=[&](std::string_view name){auto item=values.find(name);if(item==values.end())return literal("");
            return dynamic(name)?"lvui::native::text("+expression_code(item->second->value,item->second->position,initial)+")":literal(text(values,name));};
        if(!values.contains("iconName"))error({},"Mobile tab iconName is required");
        if(!dynamic("iconName")){const auto name=text(values,"iconName");if(name.empty()||name.size()>256||name.find_first_not_of(" \t\r\n")==name.npos)error(values.at("iconName")->position,"Mobile tab iconName must be nonempty");}
        std::string setup="value.focus_color = "+accent_style(0,initial)+"; value.item.icon_name = "+text_value("iconName")+"; value.item.text = "+text_value("text")+"; ";
        for(const auto& [name,field]:std::map<std::string,std::string,std::less<>>{{"badge","badge"},{"accessibilityName","accessibility_name"},{"badgeAccessibilityName","badge_accessibility_name"}})
            if(values.contains(name))setup+="value.item."+field+" = "+text_value(name)+"; ";
        if(!values.contains("accessibilityName"))setup+="if (value.item.text.empty()) value.item.accessibility_name = value.item.icon_name; ";
        for(const auto& [name,field]:std::map<std::string,std::string,std::less<>>{{"badgeDot","item.badge_dot"},{"selected","selected"},{"enabled","enabled"},{"showLabel","show_label"},{"drawSelection","draw_selection"},{"motionEnabled","motion_enabled"}})
            if(auto item=values.find(name);item!=values.end())setup+="value."+field+" = "+(dynamic(name)?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(values,name,name=="enabled"||name=="showLabel"||name=="drawSelection"||name=="motionEnabled"))+"; ";
        for(const auto& [name,field]:std::map<std::string,std::string,std::less<>>{{"width","width"},{"height","height"}})
            if(auto item=values.find(name);item!=values.end())setup+="value."+field+" = "+(dynamic(name)?"lvui::native::number("+expression_code(item->second->value,item->second->position,initial)+")":decimal(numeric(values,name,0,1,8192)))+"; ";
        if(auto item=values.find("iconSource");item!=values.end()){
            if(!dynamic("iconSource"))error(item->second->position,"Mobile tab iconSource requires a C++ source");
            setup+="value.icon_source = "+expression_code(item->second->value,item->second->position,initial)+"; ";
        }
        if(auto item=values.find("iconRevision");item!=values.end()){
            std::string revision;
            if(dynamic("iconRevision"))revision="lvui::native::integer(lvui::native::range("+expression_code(item->second->value,item->second->position,initial)+", 0, 9007199254740991.0))";
            else {const auto n=numeric(values,"iconRevision",0,0,9007199254740991.);if(std::trunc(n)!=n)error(item->second->position,"Mobile tab iconRevision must be integral");revision=std::to_string(std::int64_t(n));}
            setup+="value.icon_revision = "+revision+"; ";
        }
        if(auto item=values.find("focusColor");item!=values.end())setup+="value.focus_color = lvui::native::mobile_tab_color("+color_expression(item->second->value,item->second->position,initial)+"); ";
        if(auto item=values.find("platformStyle");item!=values.end()){
            if(dynamic("platformStyle"))setup+="value.platform = lvui::mobile_navigation_platform("+expression_code(item->second->value,item->second->position,initial)+"); ";
            else{const auto v=resolve(item->second->value,item->second->position);std::string platform;
                if(v.kind==ValueKind::enumeration&&(v.text==".ios"||v.text==".IOS"))platform="ios";
                else if(v.kind==ValueKind::enumeration&&(v.text==".android"||v.text==".Android"))platform="android";
                else if(v.kind==ValueKind::enumeration&&(v.text==".automatic"||v.text==".Automatic"))platform="automatic";
                else error(item->second->position,"Unknown mobile tab platform");
                setup+="value.platform = lvui::MobileNavigationPlatform::"+platform+"; ";}
        }
        return "([&] { lvui::native::MobileTabInput value; "+setup+"return value; }())";
    }
    std::string owned_mobile_navigation_options(const Inputs& values,bool initial,bool tab_bar)const {
        const auto dynamic=[&](std::string_view name){auto item=values.find(name);return item!=values.end()&&native_expression(item->second->value);};
        std::string setup="value.focus_color = "+accent_style(0,initial)+"; value.material_enabled = "+std::string(tab_bar?"true":"false")+"; ";
        for(const auto& [name,field]:std::map<std::string,std::string,std::less<>>{{"model","items"},{"search","search"}})
            if(auto item=values.find(name);item!=values.end()){
                if(!dynamic(name))error(item->second->position,"Mobile navigation model requires a typed C++ value");
                setup+="value."+field+" = lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(item->second->value,item->second->position,initial)+"); ";
            }
        for(const auto& [name,field,low,high]:{std::tuple{"width","width",1.,8192.},std::tuple{"bottomSafeInset","bottom_safe_inset",0.,8192.}})
            if(auto item=values.find(name);item!=values.end())setup+="value."+std::string(field)+" = "+
                (dynamic(name)?"lvui::native::number(lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(item->second->value,item->second->position,initial)+"))":decimal(numeric(values,name,0,low,high)))+"; ";
        if(auto item=values.find("currentIndex");item!=values.end()){
            std::string current;
            if(dynamic("currentIndex"))current="static_cast<int>(lvui::native::integer(lvui::native::range("+expression_code(item->second->value,item->second->position,initial)+", -1, 4)))";
            else{const auto n=numeric(values,"currentIndex",-1,-1,4);if(std::trunc(n)!=n)error(item->second->position,"Mobile navigation index must be integral");current=std::to_string(int(n));}
            setup+="value.current_index = "+current+"; ";
        }
        for(const auto& [name,field]:std::map<std::string,std::string,std::less<>>{{"autoSelect","auto_select"},{"enabled","enabled"},{"materialEnabled","material_enabled"},{"motionEnabled","motion_enabled"}})
            if(auto item=values.find(name);item!=values.end())setup+="value."+field+" = "+(dynamic(name)?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(values,name,name=="materialEnabled"?tab_bar:true))+"; ";
        if(auto item=values.find("accessibilityName");item!=values.end())setup+="value.accessibility_name = "+
            (dynamic("accessibilityName")?"lvui::native::text("+expression_code(item->second->value,item->second->position,initial)+")":literal(text(values,"accessibilityName")))+"; ";
        if(auto item=values.find("platformStyle");item!=values.end()){
            if(dynamic("platformStyle"))setup+="value.platform = lvui::mobile_navigation_platform("+expression_code(item->second->value,item->second->position,initial)+"); ";
            else{const auto v=resolve(item->second->value,item->second->position);std::string platform;
                if(v.kind==ValueKind::enumeration&&(v.text==".ios"||v.text==".IOS"))platform="ios";
                else if(v.kind==ValueKind::enumeration&&(v.text==".android"||v.text==".Android"))platform="android";
                else if(v.kind==ValueKind::enumeration&&(v.text==".automatic"||v.text==".Automatic"))platform="automatic";
                else error(item->second->position,"Unknown mobile navigation platform");
                setup+="value.platform = lvui::MobileNavigationPlatform::"+platform+"; ";}
        }
        return "([&] { lvui::MobileNavigationConfiguration value; "+setup+"return value; }())";
    }
    std::string label_presentation_options(const Inputs& values,bool initial,SourcePosition position)const {
        const auto dynamic=[&](std::string_view name){auto item=values.find(name);return item!=values.end()&&native_expression(item->second->value);};
        const auto [role,size]=text_role(values);
        const auto font=numeric(values,"fontSize",size,1,256);
        const auto line=values.contains("fontSize")?font:token_number(text_metric(role)+"-line-height",1,8192);
        const auto weight=token_number("--lvui-theme-typography-weight-"+
            std::string(role=="title"||role=="title2"?"bold":role=="header"||role=="header2"||role=="description"?"semi-bold":role=="body"?"medium":"regular"),100,900);
        const auto font_weight=numeric(values,"fontWeight",weight,400,700);
        if(font_weight!=400&&font_weight!=500&&font_weight!=600&&font_weight!=700)error(position,"fontWeight must be 400/500/600/700 for native fonts");
        const std::string content=dynamic("text")?"lvui::native::text("+expression_code(values.at("text")->value,values.at("text")->position,initial)+")":literal(text(values,"text"));
        const std::string selected=dynamic("selectable")?"lvui::native::truth("+expression_code(values.at("selectable")->value,values.at("selectable")->position,initial)+")":boolean(values,"selectable",true);
        std::string ink=color_literal(text_color(role));
        if(auto item=values.find("color");item!=values.end())ink=color_expression(item->second->value,item->second->position,initial);
        std::string alignment="ii::gui::TextAlignment::left";
        if(auto item=values.find("alignment");item!=values.end()){
            if(dynamic("alignment"))alignment="static_cast<ii::gui::TextAlignment>(lvui::native::integer(lvui::native::range("+expression_code(item->second->value,item->second->position,initial)+", 0, 2)))";
            else{const auto v=resolve(item->second->value,item->second->position);std::string name;
                if(v.kind==ValueKind::enumeration&&(v.text==".left"||v.text==".leading"))name="left";
                else if(v.kind==ValueKind::enumeration&&v.text==".center")name="center";
                else if(v.kind==ValueKind::enumeration&&(v.text==".right"||v.text==".trailing"))name="right";
                else error(item->second->position,"Unknown label alignment");
                alignment="ii::gui::TextAlignment::"+name;}
        }
        const std::string elide=dynamic("elideRight")?"lvui::native::truth("+expression_code(values.at("elideRight")->value,values.at("elideRight")->position,initial)+")":boolean(values,"elideRight",false);
        return "ii::gui::LabelOptions{.text = "+content+", .font_size = "+decimal(font)+", .selectable = "+selected+
            ", .role = ii::gui::TextRole::"+role+", .color = "+ink+", .alignment = "+alignment+
            ", .line_height = "+decimal(numeric(values,"lineHeight",line,0,8192))+", .font_weight = "+decimal(font_weight)+", .elide_right = "+elide+"}";
    }
    std::string owned_color_button_options(const Inputs& values,bool initial,SourcePosition position)const {
        const auto dynamic=[&](std::string_view name){auto item=values.find(name);return item!=values.end()&&native_expression(item->second->value);};
        if(values.contains("model")&&values.contains("currentColor"))error(position,"ColorPickerButton accepts either model or currentColor");
        if(values.contains("modelRevision")&&!values.contains("model"))error(values.at("modelRevision")->position,"modelRevision requires a color source");
        std::string setup;
        for(const auto& [field,key]:{std::pair{"current_color","--lvui-theme-colors-accent-purple-default"},
                std::pair{"palette.normal","--lvui-theme-colors-panel-surface-alt"},std::pair{"palette.hover","--lvui-theme-colors-panel-surface-raised"},
                std::pair{"palette.pressed","--lvui-theme-colors-panel-window-alt"}}){
            const auto c=token_color(key);setup+="value.configuration."+std::string(field)+" = lvui::StyleColor{"+decimal(c.red)+", "+decimal(c.green)+", "+decimal(c.blue)+", "+decimal(c.alpha)+"}; ";
        }
        if(!values.contains("focusColor"))setup+="value.configuration.palette.focus = "+accent_style(0,initial)+"; ";
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
        return "([&] { lvui::native::ModalInputValue<ii::gui::RasterImage> value; "+setup+"return value; }())";
    }
    // The generated application supplies authored values to iiGUI's own button.
    std::string owned_sheet_close_factory() const {
        return R"LVUI([](ii::gui::WidgetBuilder& builder, ii::gui::Widget parent, std::string_view id,
                ii::runtime::Task action) -> ii::core::Result<ii::gui::Widget> {
            ii::gui::ButtonInput input;
            auto& configuration = input.configuration;
            configuration.text = "×"; configuration.accessibility_name = "Close";
            configuration.tone = ii::gui::ButtonTone::borderless;
            configuration.width = 26; configuration.height = 26; configuration.horizontal_padding = 0;
            const lvui::ButtonPalette authored{};
            const auto color = [](lvui::StyleColor value) {
                return ii::gui::WidgetColor{value.red, value.green, value.blue, value.alpha};
            };
            auto& palette = configuration.palette;
            palette.primary = color(authored.primary); palette.primary_hover = color(authored.primary_hover);
            palette.primary_pressed = color(authored.primary_pressed); palette.normal = color(authored.normal);
            palette.normal_hover = color(authored.normal_hover); palette.normal_pressed = color(authored.normal_pressed);
            if (authored.selected) palette.selected = color(*authored.selected);
            palette.danger = color(authored.danger); palette.disabled_background = color(authored.disabled_background);
            palette.text = color(authored.text); palette.disabled_text = color(authored.disabled_text);
            if (authored.danger_hover) palette.danger_hover = color(*authored.danger_hover);
            if (authored.danger_pressed) palette.danger_pressed = color(*authored.danger_pressed);
            palette.danger_reference = color(authored.danger_reference);
            palette.danger_hover_default = color(authored.danger_hover_default);
            palette.danger_pressed_default = color(authored.danger_pressed_default);
            palette.selected_surface = color(authored.selected_surface);
            palette.selected_reference = color(authored.selected_reference);
            palette.selected_default = color(authored.selected_default);
            input.timing.hover = lvui::button_motion_defaults::hover;
            input.timing.press = lvui::button_motion_defaults::press;
            input.timing.release = lvui::button_motion_defaults::release;
            input.timing.post_release = lvui::button_motion_defaults::post_release;
            input.timing.programmatic = std::chrono::milliseconds{130};
            input.timing.focus_opacity = std::chrono::milliseconds{160};
            input.timing.focus_scale = std::chrono::milliseconds{360};
            auto button = ii::gui::NativeButton::create(builder, parent, id, input, std::move(action));
            if (!button) return std::unexpected(button.error());
            return button->root();
        })LVUI";
    }
    std::string owned_sheet_options(const Inputs& values,bool initial)const {
        const auto dynamic=[&](std::string_view name){auto item=values.find(name);return item!=values.end()&&native_expression(item->second->value);};
        if(auto item=values.find("motion");item!=values.end())error(item->second->position,"The independent Sheet does not support a motion model yet");
        std::string setup = "value.configuration.palette.surface = lvui::native::color<ii::gui::WidgetColor>(lvui::figma_defaults::panel_surface_panel); ";
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
                    result="ii::gui::"+std::string(type)+"::"+choice;}
                setup+="value.configuration."+std::string(field)+" = "+result+"; ";
            }
        for(const auto& [name,field]:{std::pair{"surfaceColor","surface"},std::pair{"backdropColor","backdrop"}})
            if(auto item=values.find(name);item!=values.end())setup+="value.configuration.palette."+std::string(field)+" = "+
                color_expression(item->second->value,item->second->position,initial)+"; ";
        return "([&] { ii::gui::SheetInput value; "+setup+"return value; }())";
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
        for(const auto& [field,key]:{
                std::pair{"action_horizontal_inset","--lvui-theme-metrics-space-24"},
                std::pair{"action_gap","--lvui-theme-metrics-space-14"},
                std::pair{"action_height","--lvui-theme-metrics-size-56"},
                std::pair{"tertiary_action_height","--lvui-theme-metrics-size-44"},
                std::pair{"three_top_spacing","--lvui-theme-metrics-space-18"},
                std::pair{"three_bottom_spacing","--lvui-theme-metrics-space-18"},
                std::pair{"action_vertical_gap","--lvui-theme-metrics-space-12"},
                std::pair{"two_top_spacing","--lvui-theme-metrics-space-28"},
                std::pair{"bottom_spacing","--lvui-theme-metrics-space-32"}})
            setup+="value.configuration.metrics."+std::string(field)+" = "+
                decimal(token_number(key,std::string_view(key).find("-size-")!=std::string_view::npos?1:0,8192))+"; ";
        if(!values.contains("primaryColor"))setup+="value.configuration.primary_color = "+accent_style(0,initial)+"; ";
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
        return "([&] { lvui::native::AlertInputValue<ii::gui::RasterImage> value; "+setup+"return value; }())";
    }
    std::string owned_alert_button_options(const Inputs& values,bool initial)const {
        const auto dynamic=[&](std::string_view name){auto item=values.find(name);return item!=values.end()&&native_expression(item->second->value);};
        std::string setup;
        for(const auto& [field,key]:{std::pair{"palette.normal","--lvui-theme-colors-panel-surface-raised"},std::pair{"palette.danger","--lvui-theme-colors-action-danger"},
                std::pair{"palette.text","--lvui-theme-colors-text-body-color"},std::pair{"palette.disabled_text","--lvui-theme-colors-text-disabled-color"},
                std::pair{"dialog_text","--lvui-material-alert-text-primary"},std::pair{"dialog_border","--lvui-material-alert-action-border"},
                std::pair{"disabled_background","--lvui-theme-colors-panel-surface-control"},std::pair{"normal_pressed","--lvui-theme-colors-panel-surface-input"}}){
            const auto color=token_color(key);setup+="value."+std::string(field)+" = lvui::StyleColor{"+decimal(color.red)+", "+decimal(color.green)+", "+decimal(color.blue)+", "+decimal(color.alpha)+"}; ";
        }
        setup+=danger_state_setup("value.palette.");
        if(!values.contains("primaryColor"))setup+="value.palette.primary = "+accent_style(0,initial)+"; ";
        for(const auto& [name,field]:{std::pair{"text","text"},std::pair{"accessibilityName","accessibility_name"}})
            if(auto item=values.find(name);item!=values.end())setup+="value."+std::string(field)+" = "+
                (dynamic(name)?"lvui::native::text("+expression_code(item->second->value,item->second->position,initial)+")":literal(text(values,name)))+"; ";
        for(const auto& [name,field,fallback]:{std::tuple{"enabled","enabled",true},std::tuple{"dialogStyle","dialog_style",false},
                std::tuple{"motionEnabled","motion_enabled",true},std::tuple{"enterKeyActivation","enter_key_activation",false}})
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
        if(!values.contains("primaryColor")){
            if(!active_window_accent.empty())
                setup+="value.primary_color = lvui::native::popover_color(window_accent_palette(context.read("+active_window_accent+"))[0]); ";
            else {
                const auto primary=accent_colors(window_accent_name).primary;
                setup+="value.primary_color = lvui::StyleColor{"+decimal(primary.red)+", "+decimal(primary.green)+", "+decimal(primary.blue)+", "+decimal(primary.alpha)+"}; ";
            }
        }
        for(const auto& [name,field,high]:{std::tuple{"width","width",8192.},std::tuple{"height","height",8192.},
                std::tuple{"padding","padding",8192.},std::tuple{"margins","margins",8192.},std::tuple{"cornerRadius","corner_radius",8192.},
                std::tuple{"surfaceOpacity","surface_opacity",1.}})
            if(auto item=values.find(name);item!=values.end())setup+="value."+std::string(field)+" = "+
                (dynamic(name)?"lvui::native::number("+expression_code(item->second->value,item->second->position,initial)+")":decimal(numeric(values,name,0,0,high)))+"; ";
        for(const auto& [name,field,fallback]:{std::tuple{"modal","modal",false},std::tuple{"dim","dim",false},std::tuple{"focus","focus_on_open",true},
                std::tuple{"clip","clip_content",false},std::tuple{"closeOnEscape","dismiss_on_escape",true},
                std::tuple{"closeOnPressOutside","dismiss_on_outside_press",true},std::tuple{"closeOnContextRequest","dismiss_on_context_request",true},
                std::tuple{"blurEnabled","blur_enabled",true},std::tuple{"motionEnabled","motion_enabled",true}})
            if(auto item=values.find(name);item!=values.end())setup+="value."+std::string(field)+" = "+
                (dynamic(name)?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(values,name,fallback))+"; ";
        if(auto item=values.find("surfaceColor");item!=values.end())setup+="value.surface_color = lvui::native::popover_color("+
            color_expression(item->second->value,item->second->position,initial)+"); ";
        if(auto item=values.find("primaryColor");item!=values.end())setup+="value.primary_color = lvui::native::popover_color("+
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
                {"text","--lvui-theme-colors-text-body-color"},{"disabled_text","--lvui-theme-colors-text-disabled-color"},
                {"detail","--lvui-theme-colors-text-description-color"},{"hover","--lvui-theme-colors-panel-surface-alt"}}){
            const auto c=token_color(key);setup+="value.configuration.palette."+field+" = lvui::StyleColor{"+decimal(c.red)+", "+decimal(c.green)+", "+decimal(c.blue)+", "+decimal(c.alpha)+"}; ";
        }
        setup+=menu_accent_setup(initial);
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
        setup+="value.configuration.font_size = value.configuration.compact ? 12 : "+decimal(token_number("--lvui-theme-typography-body-text-body",1,256))+"; ";
        setup+="value.configuration.line_height = value.configuration.font_size; value.configuration.font_weight = value.configuration.compact ? 400 : "+decimal(token_number("--lvui-theme-typography-weight-medium",400,700))+"; ";
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
        return "([&] { lvui::native::MenuItemInputValue<ii::gui::RasterImage> value; "+setup+"return value; }())";
    }
    std::string owned_dropdown_options(const Inputs& values,bool initial,bool icon_default)const {
        const auto dynamic=[&](std::string_view name){auto item=values.find(name);return item!=values.end()&&native_expression(item->second->value);};
        if(values.contains("leftPadding")&&values.contains("horizontalPadding"))error(values.at("leftPadding")->position,"Use leftPadding or horizontalPadding, not both");
        if(auto icon=values.find("icon");icon!=values.end()&&!dynamic("icon"))error(icon->second->position,"Dropdown icon requires a native image from a C++ backend");
        std::string setup;
        for(const auto& [field,key]:std::map<std::string,std::string,std::less<>>{
                {"normal","--lvui-theme-colors-panel-surface-raised"},{"normal_hover","--lvui-theme-colors-panel-surface-alt"},
                {"danger","--lvui-theme-colors-action-danger"},{"disabled_background","--lvui-theme-colors-panel-sub-surface"},
                {"text","--lvui-theme-colors-text-body-color"},{"disabled_text","--lvui-theme-colors-text-disabled-color"}}){
            const auto color=token_color(key);setup+="value.configuration.palette."+field+" = lvui::StyleColor{"+decimal(color.red)+", "+decimal(color.green)+", "+decimal(color.blue)+", "+decimal(color.alpha)+"}; ";
        }
        setup+=danger_state_setup("value.configuration.palette.");
        setup+="value.configuration.palette.primary = "+accent_style(0,initial)+"; value.configuration.palette.primary_hover = "+accent_style(1,initial)+
            "; value.configuration.palette.primary_pressed = "+accent_style(2,initial)+"; value.configuration.palette.normal_pressed = "+accent_style(3,initial)+"; ";
        setup+="value.configuration.font_size = "+decimal(token_number("--lvui-theme-typography-body-text-body",1,256))+"; ";
        setup+="value.configuration.line_height = "+decimal(token_number("--lvui-theme-typography-body-text-body-line-height",0,8192))+"; ";
        setup+="value.configuration.font_weight = "+decimal(token_number("--lvui-theme-typography-weight-medium",400,700))+"; ";
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
        return "([&] { lvui::native::DropdownInputValue<ii::gui::RasterImage> value; "+setup+"return value; }())";
    }
    std::string owned_help_button_options(const Inputs& values,bool initial)const {
        const auto dynamic=[&](std::string_view name){auto item=values.find(name);return item!=values.end()&&native_expression(item->second->value);};
        std::string setup="value.configuration = lvui::help_button_configuration(); ";
        for(const auto& [field,key]:std::map<std::string,std::string,std::less<>>{
                {"primary","--lvui-theme-colors-action-primary"},{"primary_hover","--lvui-theme-colors-accent-blue-hover"},
                {"primary_pressed","--lvui-theme-colors-accent-blue-pressed"},{"normal","--lvui-theme-colors-panel-sub-surface"},
                {"normal_hover","--lvui-theme-colors-panel-surface-alt"},{"normal_pressed","--lvui-theme-colors-accent-blue-subtle"},
                {"danger","--lvui-theme-colors-action-danger"},{"disabled_background","--lvui-theme-colors-panel-sub-surface"},
                {"text","--lvui-theme-colors-text-description-color"},{"disabled_text","--lvui-theme-colors-text-disabled-color"}}){
            const auto color=token_color(key);setup+="value.configuration.palette."+field+" = lvui::StyleColor{"+decimal(color.red)+", "+decimal(color.green)+", "+decimal(color.blue)+", "+decimal(color.alpha)+"}; ";
        }
        setup+=danger_state_setup("value.configuration.palette.");
        setup+="value.configuration.font_size = "+decimal(token_number("--lvui-theme-typography-description-text-description",1,256))+"; ";
        setup+="value.configuration.line_height = "+decimal(token_number("--lvui-theme-typography-description-text-description-line-height",0,8192))+"; ";
        setup+="value.configuration.font_weight = "+decimal(token_number("--lvui-theme-typography-weight-semi-bold",400,700))+"; ";
        for(const auto& [name,field]:std::map<std::string,std::string,std::less<>>{{"text","text"},{"accessibilityName","accessibility_name"}})
            if(auto item=values.find(name);item!=values.end())setup+="value.configuration."+field+" = "+(dynamic(name)?"lvui::native::text("+expression_code(item->second->value,item->second->position,initial)+")":literal(text(values,name)))+"; ";
        if(auto item=values.find("enabled");item!=values.end())setup+="value.configuration.enabled = "+(dynamic("enabled")?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(values,"enabled",true))+"; ";
        if(auto item=values.find("motionEnabled");item!=values.end())setup+="value.configuration.motion_enabled = "+(dynamic("motionEnabled")?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(values,"motionEnabled",true))+"; ";
        if(auto item=values.find("enterKeyActivation");item!=values.end())setup+="value.configuration.enter_key_activation = "+(dynamic("enterKeyActivation")?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(values,"enterKeyActivation",false))+"; ";
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
        return "([&] { lvui::native::ButtonInputValue<ii::gui::RasterImage> value; "+setup+"return value; }())";
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
        const auto accent=accent_colors(window_accent_name);
        auto palette="lvui::ButtonPalette{.primary = "+color_code(accent.primary)+
            ", .primary_hover = "+color_code(accent.hover)+
            ", .primary_pressed = "+color_code(accent.pressed)+
            ", .normal = "+color("--lvui-theme-colors-panel-surface-solid")+
            ", .normal_hover = "+color("--lvui-theme-colors-panel-surface-alt")+
            ", .normal_pressed = "+color_code(accent.selected)+
            ", .selected = "+color_code(accent.selected)+
            ", .danger = "+color("--lvui-theme-colors-action-danger")+
            ", .disabled_background = "+color("--lvui-theme-colors-panel-sub-surface")+
            ", .text = "+color("--lvui-theme-colors-text-title-header-color")+
            ", .disabled_text = "+color("--lvui-theme-colors-text-disabled-color")+"}";
        if(!active_window_accent.empty()){
            palette="([&] { auto palette = "+palette+"; const auto accent = window_accent_palette(context.read("+active_window_accent+")); "
                    "palette.primary = lvui::native::button_color(accent[0]); palette.primary_hover = lvui::native::button_color(accent[1]); "
                    "palette.primary_pressed = lvui::native::button_color(accent[2]); palette.normal_pressed = lvui::native::button_color(accent[4]); palette.selected = lvui::native::button_color(accent[4]); return palette; }())";
        }
        const auto padding=v.contains("horizontalPadding")?number("horizontalPadding",0,0,8192):
            "(icon_mode ? lvui::icon_button_defaults::horizontal_padding : "+decimal(token_number("--lvui-theme-metrics-space-8",0,8192))+")";
        const auto height=number("height",token_number("--lvui-theme-metrics-size-22",1,8192),1,8192);
        const auto radius=number("cornerRadius",token_number("--lvui-theme-metrics-radius-8",0,8192),0,8192);
        const auto size=v.contains("iconSize")?number("iconSize",16,1,256):icon_default?
            "(icon_mode ? lvui::icon_button_defaults::icon_size : 16)":"16";
        std::string appearance;
        appearance+=danger_state_setup("value.configuration.palette.");
        for(const auto& [name,field]:std::map<std::string,std::string,std::less<>>{
                {"primaryColor","primary"},{"hoverColor","primary_hover"},{"pressedColor","primary_pressed"},
                {"normalColor","normal"},{"normalHoverColor","normal_hover"},{"normalPressedColor","normal_pressed"},{"selectedColor","selected"},
                {"dangerColor","danger"},{"dangerHoverColor","danger_hover"},{"dangerPressedColor","danger_pressed"},{"disabledBackgroundColor","disabled_background"},
                {"textColor","text"},{"disabledTextColor","disabled_text"}})
            if(auto input=v.find(name);input!=v.end())
                appearance+="value.configuration.palette."+field+" = lvui::native::button_color("+
                            color_expression(input->second->value,input->second->position,initial)+"); ";
        for(const auto [name,field,low,high]:{
                std::tuple{"width","width",1.,8192.},std::tuple{"iconSpacing","icon_spacing",0.,8192.},
                std::tuple{"fontSize","font_size",1.,256.},std::tuple{"lineHeight","line_height",0.,8192.},
                std::tuple{"textOffsetY","text_vertical_offset",-8192.,8192.},std::tuple{"borderWidth","border_width",0.,8192.}})
            if(v.contains(name))appearance+="value.configuration."+std::string(field)+" = "+number(name,0,low,high)+"; ";
        if(v.contains("checkable"))appearance+="value.configuration.checkable = "+flag("checkable",false)+"; ";
        if(v.contains("checked"))appearance+="value.configuration.checked = "+flag("checked",false)+"; ";
        if(auto item=v.find("selectionStyle");item!=v.end()){
            std::string style;
            if(native_expression(item->second->value))style="lvui::native::button_selection_style("+expression("selectionStyle","")+")";
            else {
                const std::map<std::string,std::string,std::less<>> styles{{".accent","accent"},{".segmented","segmented"}};
                const auto selected=styles.find(item->second->value.text);
                if(item->second->value.kind!=ValueKind::enumeration||selected==styles.end())error(item->second->position,"Unknown Button selection style");
                style="lvui::ButtonSelectionStyle::"+selected->second;
            }
            appearance+="value.configuration.selection_style = "+style+"; ";
        }
        if(v.contains("motionEnabled"))appearance+="value.configuration.motion_enabled = "+flag("motionEnabled",true)+"; ";
        appearance+="value.configuration.enter_key_activation = "+flag("enterKeyActivation",!normal_default)+"; ";
        if(v.contains("fontSize")&&!v.contains("lineHeight"))appearance+="value.configuration.line_height = value.configuration.font_size; ";
        if(v.contains("borderWidth")&&!v.contains("borderColor"))error(v.at("borderWidth")->position,"Button borderWidth requires borderColor");
        if(auto border=v.find("borderColor");border!=v.end()){
            appearance+="value.configuration.border_color = lvui::native::button_color("+color_expression(border->second->value,border->second->position,initial)+"); ";
            if(!v.contains("borderWidth"))appearance+="value.configuration.border_width = "+decimal(token_number("--lvui-theme-metrics-stroke-1",0,8192))+"; ";
        }
        return "([&] { const bool icon_mode = "+mode+"; auto value = lvui::native::ButtonInputValue<ii::gui::RasterImage>{.icon = "+expression("icon","{}")+
            ", .configuration = lvui::ButtonConfiguration{.text = "+text_value("text")+", .accessibility_name = "+text_value("accessibilityName")+
            ", .enabled = "+flag("enabled",true)+", .tone = "+tone+", .height = "+height+", .horizontal_padding = "+padding+
            ", .corner_radius = "+radius+", .icon_size = "+size+", .font_size = "+decimal(token_number("--lvui-theme-typography-body-text-body",1,256))+
            ", .line_height = "+decimal(token_number("--lvui-theme-typography-body-text-body-line-height",0,8192))+
            ", .font_weight = "+decimal(token_number("--lvui-theme-typography-weight-medium",400,700))+
            ", .icon_mode = icon_mode, .palette = "+palette+"}}; "+appearance+owned_named_icon_setup(v,initial)+"return value; }())";
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
        const auto palette="lvui::RadioButtonPalette{.on = "+(v.contains("onColor")?color("onColor","--lvui-theme-colors-action-primary"):accent_style(0,initial))+
            ", .off = "+color("offColor","--lvui-theme-colors-text-title-header-color")+
            ", .disabled_on = "+color("onColorDisabled","--lvui-theme-colors-panel-surface-raised")+
            ", .disabled_off = "+color("offColorDisabled","--lvui-theme-colors-panel-surface-raised")+
            ", .dot = "+color("dotColor","--lvui-theme-colors-text-title-header-color")+
            ", .disabled_dot = "+color("dotColorDisabled","--lvui-theme-colors-text-caption-color")+
            ", .text = "+color("textColor","--lvui-theme-colors-text-body-color")+
            ", .disabled_text = "+color("disabledTextColor","--lvui-theme-colors-text-disabled-color")+
            ", .focus = "+accent_style(0,initial)+"}";
        std::string motion="lvui::Motion{.duration = std::chrono::milliseconds{360}, .curve = lvui::MotionCurve::out_back, .overshoot = 1.45}";
        if(auto item=v.find("motion");item!=v.end()){
            if(!native_expression(item->second->value))error(item->second->position,"RadioButton motion requires a scoped native value");
            motion="lvui::motion_value("+expression("motion")+")";
        }
        if(auto item=v.find("transitionDuration");item!=v.end()){
            if(!native_expression(item->second->value)){const auto duration=numeric(v,"transitionDuration",360,0,86400000);if(std::floor(duration)!=duration)error(item->second->position,"RadioButton transitionDuration must be whole milliseconds");}
            motion="lvui::native::radio_motion_duration("+motion+", "+number("transitionDuration",360,0,86400000)+")";
        }
        return "lvui::RadioButtonConfiguration{.text = "+text_value("text")+", .accessibility_name = "+text_value("accessibilityName")+
            ", .group = "+text_value("group")+", .enabled = "+flag(v.contains("enabled")?"enabled":"available",true)+
            ", .checked = "+flag(v.contains("checked")?"checked":"state",false)+", .motion_enabled = "+flag("motionEnabled",true)+
            ", .indicator_size = "+number("indicatorSize",18,1,8192)+", .dot_size = "+number("dotSize",8,0,8192)+
            ", .label_gap = "+number("labelGap",8,0,8192)+
            ", .font_size = "+decimal(token_number("--lvui-theme-typography-body-text-body",1,256))+
            ", .line_height = "+decimal(token_number("--lvui-theme-typography-body-text-body-line-height",0,8192))+
            ", .font_weight = "+decimal(token_number("--lvui-theme-typography-weight-medium",400,700))+
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
        const auto shadow=[&](std::string_view field,double alpha){auto item=v.find(field);return item==v.end()?color_code(StyleColor{0,0,0,alpha}):
            "lvui::native::checkbox_color("+color_expression(item->second->value,item->second->position,initial)+")";};
        const auto transparent=std::string("lvui::StyleColor{0, 0, 0, 0}");
        const auto border_color=[&](std::string_view field){auto item=v.find(field);return item==v.end()?transparent:
            "lvui::native::checkbox_color("+color_expression(item->second->value,item->second->position,initial)+")";};
        const auto palette="lvui::CheckBoxPalette{.checked = "+(v.contains("checkedColor")?color("checkedColor","--lvui-theme-colors-action-primary"):accent_style(0,initial))+
            ", .unchecked = "+color("uncheckedColor","--lvui-theme-colors-text-body-color")+
            ", .disabled_checked = "+color("disabledCheckedColor","--lvui-theme-colors-panel-surface-raised")+
            ", .disabled_unchecked = "+color("disabledUncheckedColor","--lvui-theme-colors-panel-surface-raised")+
            ", .mark = "+color("checkColor","--lvui-theme-colors-text-body-color")+
            ", .disabled_mark = "+color("checkMarkColorDisabled","--lvui-theme-colors-text-disabled-color")+
            ", .checked_border = "+border_color("boxBorderColorCheckedEnabled")+
            ", .unchecked_border = "+color("boxBorderColorUncheckedEnabled","--lvui-theme-colors-text-body-color")+
            ", .disabled_checked_border = "+color("boxBorderColorCheckedDisabled","--lvui-theme-colors-panel-surface-raised")+
            ", .disabled_unchecked_border = "+border_color("boxBorderColorUncheckedDisabled")+
            ", .text = "+color("textColor","--lvui-theme-colors-text-body-color")+
            ", .disabled_text = "+color("disabledTextColor","--lvui-theme-colors-text-disabled-color")+
            ", .focus = "+accent_style(0,initial)+
            ", .inner_shadow_soft = "+shadow("innerShadowSoftColor",20./255.)+
            ", .inner_shadow_strong = "+shadow("innerShadowStrongColor",26./255.)+"}";
        const auto size=number("boxSize",17,1,8192);
        const auto scaled=[&](std::string_view name,double fallback,double factor){return v.contains(name)?number(name,fallback,0,8192):"("+size+" * "+decimal(factor/17)+")";};
        std::string shape="lvui::CheckBoxShape::round_rect";
        if(auto input=v.find("shapeStyle");input!=v.end()){
            if(native_expression(input->second->value))shape="lvui::native::checkbox_shape("+expression("shapeStyle")+")";
            else{const auto item=resolve(input->second->value,input->second->position);
                if(item.kind==ValueKind::enumeration&&(item.text==".cylinder"||item.text==".shapeCylinder"))shape="lvui::CheckBoxShape::cylinder";
                else if(item.kind==ValueKind::enumeration&&(item.text==".roundRect"||item.text==".shapeRoundRect"))shape="lvui::CheckBoxShape::round_rect";
                else if(item.kind==ValueKind::number||item.kind==ValueKind::metric){const auto n=numeric(v,"shapeStyle",0,0,1);
                    if(n==0)shape="lvui::CheckBoxShape::round_rect";else if(n==1)shape="lvui::CheckBoxShape::cylinder";
                    else error(input->second->position,"Unknown CheckBox shapeStyle");}
                else error(input->second->position,"Unknown CheckBox shapeStyle");}
        }
        std::string motion="lvui::Motion{.duration = std::chrono::milliseconds{360}, .curve = lvui::MotionCurve::out_back, .overshoot = 1.45}";
        if(auto input=v.find("motion");input!=v.end()){
            if(!native_expression(input->second->value))error(input->second->position,"CheckBox motion requires a scoped native value");
            motion="lvui::motion_value("+expression("motion")+")";
        }
        if(auto input=v.find("transitionDuration");input!=v.end()){
            if(!native_expression(input->second->value)){const auto duration=numeric(v,"transitionDuration",360,0,86400000);
                if(std::floor(duration)!=duration)error(input->second->position,"CheckBox transitionDuration must be whole milliseconds");}
            motion="lvui::native::checkbox_motion_duration("+motion+", "+number("transitionDuration",360,0,86400000)+")";
        }
        return "lvui::CheckBoxConfiguration{.text = "+text_value("text")+", .accessibility_name = "+text_value("accessibilityName")+
            ", .enabled = "+flag("enabled",true)+", .checked = "+flag("checked",false)+", .box_size = "+size+
            ", .frame_padding = "+number("framePadding",.5,0,8192)+", .box_radius = "+scaled("boxRadius",3.5,3.5)+
            ", .mark_stroke_width = "+scaled("checkMarkStrokeWidth",2,2)+", .label_gap = "+number("labelGap",6,0,8192)+
            ", .checked_border_width = "+number("boxBorderWidthCheckedEnabled",0,0,8192)+
            ", .unchecked_border_width = "+scaled("boxBorderWidthUncheckedEnabled",.5,.5)+
            ", .disabled_checked_border_width = "+scaled("boxBorderWidthCheckedDisabled",.5,.5)+
            ", .disabled_unchecked_border_width = "+number("boxBorderWidthUncheckedDisabled",0,0,8192)+
            ", .font_size = "+decimal(token_number("--lvui-theme-typography-body-text-body",1,256))+
            ", .line_height = "+decimal(token_number("--lvui-theme-typography-body-text-body-line-height",0,8192))+
            ", .font_weight = "+decimal(token_number("--lvui-theme-typography-weight-medium",400,700))+
            ", .palette = "+palette+", .use_figma_checked_assets = "+
            (v.contains("useFigmaCheckedAssets")?"std::optional<bool>{"+flag("useFigmaCheckedAssets",true)+"}":"std::nullopt")+
            ", .shape_style = "+shape+", .motion_enabled = "+flag("motionEnabled",true)+", .motion = "+motion+"}";
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
        const auto palette="lvui::ToggleSwitchPalette{.on = "+(v.contains("onColor")?color("onColor","--lvui-theme-colors-action-primary"):accent_style(0,initial))+
            ", .on_hover = "+(v.contains("onColorHover")?color("onColorHover","--lvui-theme-colors-accent-blue-hover"):accent_style(1,initial))+
            ", .on_pressed = "+(v.contains("onColorPressed")?color("onColorPressed","--lvui-theme-colors-accent-blue-pressed"):accent_style(2,initial))+
            ", .off = "+color("offColor","--lvui-theme-colors-panel-surface-raised")+
            ", .off_hover = "+color("offColorHover","--lvui-theme-colors-panel-surface-alt")+
            ", .off_pressed = "+color("offColorPressed","--lvui-theme-colors-panel-sub-surface")+
            ", .disabled = "+color("disabledTrackColor","--lvui-theme-colors-panel-surface-raised")+
            ", .knob = "+color("knobFillColor","--lvui-theme-colors-text-title-header-color")+
            ", .disabled_knob = "+color("disabledKnobColor","--lvui-theme-colors-text-caption-color")+
            ", .text = "+color("textColor","--lvui-theme-colors-text-body-color")+
            ", .disabled_text = "+color("disabledTextColor","--lvui-theme-colors-text-disabled-color")+
            ", .focus = "+accent_style(0,initial)+"}";
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
            ", .font_size = "+decimal(token_number("--lvui-theme-typography-body-text-body",1,256))+
            ", .line_height = "+decimal(token_number("--lvui-theme-typography-body-text-body-line-height",0,8192))+
            ", .font_weight = "+decimal(token_number("--lvui-theme-typography-weight-medium",400,700))+
            ", .motion = "+motion+", .palette = "+palette+"}";
    }
    std::string owned_wheel_guard_options(const Inputs& v,bool initial)const{
        const auto number=[&](std::string_view name){auto item=v.find(name);if(item==v.end())return std::string("0");
            return native_expression(item->second->value)?"lvui::native::number(lvui::native::range("+expression_code(item->second->value,item->second->position,initial)+", 0, 8192))":decimal(numeric(v,name,0,0,8192));};
        const auto flag=[&](std::string_view name){auto item=v.find(name);if(item==v.end())return std::string("true");
            return native_expression(item->second->value)?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(v,name,true);};
        return "ii::gui::WheelScrollOptions{.width = "+number("width")+", .height = "+number("height")+
            ", .enabled = "+flag("enabled")+", .consume_inside = "+flag("consumeInside")+"}";
    }
    std::string owned_window_noise_options(const Inputs& v,bool initial)const{
        const auto number=[&](std::string_view name,double fallback,double high){auto item=v.find(name);if(item==v.end())return decimal(fallback);
            return native_expression(item->second->value)?"lvui::native::number(lvui::native::range("+expression_code(item->second->value,item->second->position,initial)+", 0, "+decimal(high)+"))":decimal(numeric(v,name,fallback,0,high));};
        auto color=color_literal(token_color("--lvui-theme-colors-panel-window"));
        if(auto item=v.find("color");item!=v.end())color=color_expression(item->second->value,item->second->position,initial);
        auto primary="([&] { const auto c = "+accent_style(0,initial)+"; return ii::gui::WidgetColor{c.red, c.green, c.blue, c.alpha}; }())";
        if(auto item=v.find("primaryColor");item!=v.end())primary=color_expression(item->second->value,item->second->position,initial);
        return "ii::gui::WindowNoiseViewOptions{.width = "+number("width",0,8192)+", .height = "+number("height",0,8192)+
            ", .texture = ii::gui::WindowNoiseOptions{.color = "+color+", .primary_color = "+primary+
            ", .noise_strength = "+number("noiseStrength",0,1)+", .blur_radius = "+number("blurRadius",0,50)+
            ", .grain_strength = "+number("grainStrength",0,1)+"}}";
    }
    std::string owned_focus_ring_options(const Inputs& v,bool initial)const{
        const auto number=[&](std::string_view name,double fallback){auto item=v.find(name);if(item==v.end())return decimal(fallback);
            return native_expression(item->second->value)?"lvui::native::number("+expression_code(item->second->value,item->second->position,initial)+")":decimal(numeric(v,name,fallback,0,8192));};
        const auto flag=[&](std::string_view name,bool fallback){auto item=v.find(name);if(item==v.end())return std::string(fallback?"true":"false");
            return native_expression(item->second->value)?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(v,name,fallback);};
        auto color=color_literal(token_color("--lvui-theme-colors-action-primary"));
        if(auto item=v.find("color");item!=v.end())color=color_expression(item->second->value,item->second->position,initial);
        std::string motion="ii::gui::WidgetMotion{}";
        if(auto item=v.find("motion");item!=v.end()){
            if(!native_expression(item->second->value))error(item->second->position,"FocusRing motion requires a C++ motion value");
            motion="ii::gui::widget_motion_value("+expression_code(item->second->value,item->second->position,initial)+")";
        }
        return "ii::gui::FocusRingOptions{.width = "+number("width",0)+", .height = "+number("height",0)+
            ", .radius = "+number("radius",token_number("--lvui-theme-metrics-radius-4",0,8192))+
            ", .stroke = "+number("strokeWidth",token_number("--lvui-theme-metrics-stroke-1-5",0,8192))+
            ", .color = "+color+", .active = "+flag("active",false)+", .motion_enabled = "+flag("motionEnabled",true)+", .motion = "+motion+"}";
    }
    std::string owned_divider_options(const Inputs& v,bool initial,bool context)const{
        const auto expression=[&](std::string_view name){const auto& input=*v.find(name)->second;return "lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input.value,input.position,initial)+")";};
        const auto number=[&](std::string_view name,double fallback){auto item=v.find(name);if(item==v.end())return decimal(fallback);return native_expression(item->second->value)?"lvui::native::number("+expression(name)+")":decimal(numeric(v,name,fallback,0,8192));};
        std::string axis="ii::gui::DividerAxis::horizontal";
        if(auto item=v.find("axis");item!=v.end()){
            if(native_expression(item->second->value))axis="ii::gui::divider_axis("+expression("axis")+")";
            else {const auto value=resolve(item->second->value,item->second->position);auto name=value.text;
                if(value.kind==ValueKind::enumeration&&!name.empty())name.erase(0,1);
                else if(value.kind!=ValueKind::string)error(item->second->position,"Expected a divider axis");
                for(auto& c:name)if(c>='A'&&c<='Z')c=char(c-'A'+'a');
                if(name!="horizontal"&&name!="vertical")error(item->second->position,"Unknown divider axis");
                axis="ii::gui::DividerAxis::"+name;}
        }
        std::string color;
        if(auto item=v.find("dividerColor");item!=v.end())color="ii::gui::divider_color("+color_expression(item->second->value,item->second->position,initial)+")";
        else {const auto c=token_color(context?"--lvui-theme-colors-text-disabled-color":"--lvui-theme-colors-panel-surface-panel");
            color="ii::gui::WidgetColor{"+decimal(c.red)+", "+decimal(c.green)+", "+decimal(c.blue)+", "+decimal(c.alpha)+"}";}
        return "ii::gui::DividerConfiguration{.axis = "+axis+", .length = "+number("lineLength",context?145:220)+
            ", .thickness = "+number("thickness",1)+", .cross_padding = "+number("crossPadding",1)+
            ", .line_padding = "+number("linePadding",context?4:0)+", .color = "+color+"}";
    }
    std::string resolved_image_expression(const Inputs& values,std::string_view source,bool initial,
                                          std::string_view width,std::string_view height)const{
        const auto item=values.find(source),resolver=values.find("imageLoader");
        if(item==values.end())error({},"Image resolution requires a source");
        if(!native_expression(item->second->value)&&resolve(item->second->value,item->second->position).kind!=ValueKind::string)
            error(item->second->position,"An image source requires a URL string or native image");
        if(resolver!=values.end()&&!native_expression(resolver->second->value))error(resolver->second->position,"imageLoader requires a native image resolver");
        if(initial)error(item->second->position,"An image resolver requires a reactive view scope");
        image_resolver_runtime=true;method_runtime=true;
        const auto value=expression_code(item->second->value,item->second->position,false);
        if(resolver!=values.end())return "lvui::native::Methods::evaluate(context, [&] { return lvui::native::resolve_image<ii::gui::RasterImage>("+
            value+", "+expression_code(resolver->second->value,resolver->second->position,false)+", "+std::string(width)+", "+std::string(height)+"); })";
        scoped_image_runtime=true;
        return "lvui::native::Methods::evaluate(context, [&] { return lvui::native::resolve_scoped_image<ii::gui::RasterImage>("+value+
            ", [&] { if (!scope->image_services) throw std::invalid_argument(\"Image services are not configured\"); return context.read(scope->image_services->resolver); }, "+
            std::string(width)+", "+std::string(height)+"); })";
    }
    std::string owned_image_options(const Inputs& v,bool initial)const{
        const auto expression=[&](std::string_view name,std::string fallback){auto item=v.find(name);if(item==v.end())return fallback;return "lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(item->second->value,item->second->position,initial)+")";};
        const auto number=[&](std::string_view name,std::string fallback){auto item=v.find(name);if(item==v.end())return fallback;if(native_expression(item->second->value))return "lvui::native::number("+expression(name,"")+")";return decimal(numeric(v,name,0,0,8192));};
        const auto flag=[&](std::string_view name,bool fallback){auto item=v.find(name);if(item==v.end())return std::string(fallback?"true":"false");if(native_expression(item->second->value))return "lvui::native::truth("+expression(name,"")+")";return boolean(v,name,fallback);};
        const auto color=[&](std::string_view name){auto item=v.find(name);return item==v.end()?std::string("std::nullopt"):"ii::gui::image_color("+color_expression(item->second->value,item->second->position,initial)+")";};
        std::string fit="std::nullopt";
        if(auto item=v.find("fit");item!=v.end()){
            if(native_expression(item->second->value))fit="ii::gui::image_fit("+expression("fit","")+")";
            else{const auto value=resolve(item->second->value,item->second->position);
                if(value.text==".Contain")fit="ii::gui::ImageFit::contain";
                else if(value.text==".Cover")fit="ii::gui::ImageFit::cover";
                else if(value.text==".Stretch")fit="ii::gui::ImageFit::stretch";
                else error(item->second->position,"Unknown native Image.fit value");}
        }
        std::string name=literal("");if(auto item=v.find("accessibilityName");item!=v.end())name=native_expression(item->second->value)?"ii::gui::binding_text("+expression("accessibilityName","")+")":literal(text(v,"accessibilityName"));
        const auto configuration="ii::gui::ImageConfiguration{.width = "+number("width","32")+", .height = "+number("height","32")+", .padding = "+number("padding","0")+", .corner_radius = "+number("cornerRadius","0")+", .border_width = "+number("borderWidth","0")+", .background = "+color("backgroundColor")+", .border = "+color("borderColor")+", .fit = "+fit+", .preserve_aspect = "+flag("preserveAspect",false)+", .circular = "+flag("circular",false)+", .allow_empty = "+flag("allowEmpty",true)+", .accessibility_name = "+name+"}";
        if(v.contains("source"))return "([&] { ii::gui::ImageInput value; value.configuration = "+configuration+"; value.image = "+resolved_image_expression(v,"source",initial,"value.configuration.width","value.configuration.height")+"; return value; }())";
        return "ii::gui::ImageInput{.image = "+expression("source","{}")+", .configuration = "+configuration+"}";
    }
    std::string owned_icon_options(const Inputs& values,bool initial,std::string_view sources)const{
        const auto dynamic=[&](std::string_view name){auto item=values.find(name);return item!=values.end()&&native_expression(item->second->value);};
        const auto expression=[&](std::string_view name){const auto& item=*values.find(name)->second;
            return "lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(item.value,item.position,initial)+")";};
        std::string setup="value.size = lvui::design_icon_default_extent; ";
        for(const auto& [name,field]:std::map<std::string,std::string,std::less<>>{{"iconName","name"},{"accessibilityName","accessibility_name"}})
            if(auto item=values.find(name);item!=values.end())
                setup+="value."+field+" = "+(dynamic(name)?"lvui::native::text("+expression(name)+")":literal(text(values,name)))+"; ";
        if(auto item=values.find("iconSize");item!=values.end())
            setup+="value.size = "+(dynamic("iconSize")?"lvui::native::number("+expression("iconSize")+")":decimal(numeric(values,"iconSize",18,1,256)))+"; ";
        if(auto item=values.find("iconSource");item!=values.end()){
            if(!dynamic("iconSource"))error(item->second->position,"iconSource requires a native IconSource from a C++ backend");
            setup+="value.source = "+std::string(sources)+"->get("+expression("iconSource")+"); ";
        }else setup+="value.source = "+std::string(sources)+"->get({}); ";
        if(auto item=values.find("iconRevision");item!=values.end()){
            std::string value;
            if(dynamic("iconRevision"))value="lvui::native::integer(lvui::native::range("+expression("iconRevision")+", 0, 9007199254740991.0))";
            else {const auto revision=numeric(values,"iconRevision",0,0,9007199254740991.0);
                if(std::trunc(revision)!=revision)error(item->second->position,"iconRevision must be an integer");value=decimal(revision);}
            setup+="value.revision = "+value+"; ";
        }
        return "([&] { ii::gui::IconViewInput value; "+setup+"return value; }())";
    }
    std::string owned_avatar_options(const Inputs& values,bool initial)const{
        const std::map<std::string,std::string,std::less<>> fields{{"size","size"},{"width","width"},{"height","height"},
            {"backgroundColor","background"},{"enabled","enabled"},{"accessibilityName","accessibility_name"}};
        std::string setup="value.placeholder_source = avatar_placeholder_source(); value.placeholder_color = ii::gui::avatar_color("+
            color_literal(token_color("--lvui-theme-colors-text-body-color"))+"); ";
        if(!values.contains("backgroundColor"))
            setup+="value.configuration.background = ii::gui::avatar_color("+
                color_literal(token_color("--lvui-theme-colors-panel-surface-raised"))+"); ";
        for(const auto& [name,input]:values){auto field=fields.find(name);if(field==fields.end())continue;
            const auto expression=[&]{return "lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input->value,input->position,initial)+")";};
            std::string value;
            if(name=="size"){
                if(native_expression(input->value))value="ii::gui::avatar_size("+expression()+")";
                else {const auto item=resolve(input->value,input->position);
                    if(item.kind==ValueKind::enumeration&&(item.text==".Small"||item.text==".small"))value="ii::gui::AvatarSize::small";
                    else if(item.kind==ValueKind::enumeration&&(item.text==".Medium"||item.text==".medium"))value="ii::gui::AvatarSize::medium";
                    else if(item.kind==ValueKind::enumeration&&(item.text==".Large"||item.text==".large"))value="ii::gui::AvatarSize::large";
                    else error(input->position,"Unknown independent Avatar size");}
            }else if(name=="backgroundColor")value="ii::gui::avatar_color("+color_expression(input->value,input->position,initial)+")";
            else if(name=="enabled")value=native_expression(input->value)?"lvui::native::truth("+expression()+")":boolean(values,name,true);
            else if(name=="accessibilityName")value=native_expression(input->value)?"lvui::native::text("+expression()+")":literal(text(values,name));
            else value=native_expression(input->value)?"lvui::native::number("+expression()+")":decimal(numeric(values,name,0,0,8192));
            setup+="value.configuration."+field->second+" = "+value+"; ";
        }
        if(values.contains("source")){
            setup+="ii::gui::Avatar model; if (!model.set_configuration(value.configuration)) throw std::invalid_argument(\"Invalid avatar configuration\"); auto image_size = model.image_configuration(); ";
            setup+="value.image = "+resolved_image_expression(values,"source",initial,"image_size.width","image_size.height")+"; ";
        }
        return "([&] { ii::gui::AvatarInput value; "+setup+"return value; }())";
    }


    const std::map<std::string,std::string,std::less<>> material_fields{{"density","density"},{"primaryColor","primary_color"},{"color","color"},{"borderColor","border_color"},{"backdropBaseColor","backdrop_base_color"},{"tintOpacity","tint_opacity"},{"blurRadius","blur_radius"},{"radius","radius"},{"innerHighlightOpacity","inner_highlight_opacity"},{"shadowRadius","shadow_radius"},{"shadowOffsetY","shadow_offset_y"},{"shadowOpacity","shadow_opacity"},{"borderWidth","border_width"},{"innerHighlightOffsetY","inner_highlight_offset_y"},{"intenseOpacity","intense_opacity"},{"faintOpacity","faint_opacity"},{"blurEnabled","blur_enabled"},{"motionEnabled","motion_enabled"},{"enabled","enabled"},{"backdrop","backdrop"},{"outlinePath","outline_path"},{"width","width"},{"height","height"},{"motion","motion"}};
    std::string owned_material_options(std::string_view kind,const Inputs& values,bool initial)const {
        const auto expression=[&](const Input& input){return "lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input.value,input.position,initial)+")";};
        const auto primary=accent_colors(window_accent_name).primary;
        std::string setup="value.kind = lvui::MaterialKind::"+std::string(kind=="WindowMaterial"?"window":kind=="PanelMaterial"?"panel":"surface")+"; ";
        if(kind=="WindowMaterial"&&!values.contains("color")){
            const auto color=token_color("--lvui-theme-colors-panel-window");
            setup+="value.color = lvui::StyleColor{"+decimal(color.red)+", "+decimal(color.green)+", "+decimal(color.blue)+", "+decimal(color.alpha)+"}; ";
        }
        if(kind!="WindowMaterial"&&!values.contains("primaryColor")){
            if(!active_window_accent.empty())
                setup+="value.primary_color = lvui::native::material_color(window_accent_palette(context.read("+active_window_accent+"))[0]); ";
            else
                setup+="value.primary_color = lvui::StyleColor{"+decimal(primary.red)+", "+decimal(primary.green)+", "+decimal(primary.blue)+", "+decimal(primary.alpha)+"}; ";
        }
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
        std::string result="([&] { lvui::native::HierarchyItemInputValue<ii::gui::RasterImage> value; ";
        result+=hierarchy_accent_setup(values,"value.configuration.palette.",initial);
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
        result+=table_accent_setup(values,"value.appearance.palette.","currentCellBorderColor",initial);
        if(auto item=values.find("editable");item!=values.end()&&!values.contains("editor") &&
            (native_expression(item->second->value)||boolean(values,"editable",false)=="true"))
            error(item->second->position,"Editable tables require an explicit TableEditor");
        if(auto item=values.find("sortingEnabled");item!=values.end()&&!values.contains("orderEditor") &&
            (native_expression(item->second->value)||boolean(values,"sortingEnabled",false)=="true"))
            error(item->second->position,"Sortable tables require an explicit TableOrderEditor");
        for(auto name:{"columnResizeEnabled","rowResizeEnabled","structureControlsVisible"})if(auto item=values.find(name);item!=values.end()&&!values.contains("structureEditor") &&
            (native_expression(item->second->value)||boolean(values,name,false)=="true"))
            error(item->second->position,"Table structure controls require an explicit TableStructureEditor");
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
            }else if(key=="orderEditor"||key=="sorter"||key=="structureEditor"||key=="structureFactory"||key=="mergeEditor"){
                if(!dynamic_value)error(input->position,"Table editor dependencies require native C++ handles");
                field=key=="orderEditor"?"order_editor":key=="structureEditor"?"structure_editor":key=="structureFactory"?"structure_factory":key=="mergeEditor"?"merge_editor":"sorter";
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
            }else if(key=="structureControlsVisible"||key=="addRowControlsVisible"||key=="addColumnControlsVisible"||key=="deleteContextMenuEnabled"||key=="selectOnContextClick"){
                field=key=="structureControlsVisible"?"structure_controls_visible":key=="addRowControlsVisible"?"add_row_controls_visible":
                    key=="addColumnControlsVisible"?"add_column_controls_visible":key=="deleteContextMenuEnabled"?"delete_context_menu_enabled":"select_on_context_click";
                expression=dynamic_value?"lvui::native::truth("+expression_code(input->value,input->position,initial)+")":boolean(values,key,key!="structureControlsVisible");
            }else if(key=="structureGutterWidth"||key=="structureGutterHeight"){
                field=key=="structureGutterWidth"?"structure_gutter_width":"structure_gutter_height";
                expression=dynamic_value?"lvui::native::number("+expression_code(input->value,input->position,initial)+")":decimal(numeric(values,key,20,1,256));
            }else if(key=="accessibilityName"){
                field="appearance.accessibility_name";
                expression=dynamic_value?"lvui::native::text("+expression_code(input->value,input->position,initial)+")":literal(text(values,key));
            }else if(key=="accessibilityText"){
                if(!dynamic_value)error(input->position,"Table accessibilityText requires a native TableText value");
                field="text";expression="lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input->value,input->position,initial)+")";
            }else if(key=="headerColumnAppearance"||key=="headerIconSource"){
                if(!dynamic_value)error(input->position,"Table headerColumnAppearance and headerIconSource require native values");
                field=key=="headerColumnAppearance"?"header_presentation.column_appearance":"header_icon_source";
                expression="lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input->value,input->position,initial)+")";
            }else if(key=="headerDefaultLeadingIcon"||key=="headerDefaultTrailingIcon"){
                field=key=="headerDefaultLeadingIcon"?"header_presentation.default_leading_icon":"header_presentation.default_trailing_icon";
                expression=dynamic_value?"lvui::native::text("+expression_code(input->value,input->position,initial)+")":literal(text(values,key));
            }else if(key=="headerIconSize"||key=="headerIconSpacing"){
                field=key=="headerIconSize"?"header_presentation.icon_size":"header_presentation.icon_spacing";
                if(dynamic_value)expression="lvui::native::number("+expression_code(input->value,input->position,initial)+")";
                else{const auto n=numeric(values,key,0,0,key=="headerIconSize"?256.:8192.);
                    if(key=="headerIconSize"&&n>0&&n<1)error(input->position,"Table headerIconSize must be zero or at least one");expression=decimal(n);}
            }else if(key=="headerIconRevision"){
                field="header_icon_revision";
                if(dynamic_value)expression="lvui::native::integer(lvui::native::range("+expression_code(input->value,input->position,initial)+", 0, 9007199254740991.0))";
                else{const auto n=numeric(values,key,0,0,9007199254740991.);if(std::trunc(n)!=n)error(input->position,"headerIconRevision must be integral");expression="static_cast<std::int64_t>("+decimal(n)+")";}
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
        result+=table_accent_setup(values,"value.configuration.palette.","currentBorderColor",initial);
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
        result+=table_accent_setup(values,"value.configuration.palette.","currentBorderColor",initial);
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
            {"separatorHeight","separator_height"},{"minColumnWidth","minimum_column_width"},{"fallbackCellWidth","fallback_column_width"},
            {"iconSize","icon_size"},{"iconSpacing","icon_spacing"}};
        const std::map<std::string,std::string,std::less<>> colors{{"textColor","header_text"},{"backgroundColor","background"},{"separatorColor","divider"},{"focusColor","current"}};
        std::string result="([&] { lvui::native::TableHeaderInput value; ",widths;
        if(!values.contains("focusColor"))result+="value.configuration.palette.current = "+accent_style(0,initial)+"; ";
        for(const auto& [key,input]:values){
            const bool dynamic=native_expression(input->value);std::string field,expression;
            if(auto found=numbers.find(key);found!=numbers.end()){
                field="configuration."+found->second;const auto low=key=="width"||key=="rowHeight"?1.:key=="minColumnWidth"?32.:0.;
                if(dynamic)expression="lvui::native::number("+expression_code(input->value,input->position,initial)+")";
                else{const auto n=numeric(values,key,0,low,key=="iconSize"?256.:8192.);
                    if(key=="iconSize"&&n>0&&n<1)error(input->position,"TableHeader iconSize must be zero or at least one");expression=decimal(n);}
            }else if(auto found=colors.find(key);found!=colors.end()){
                field="configuration.palette."+found->second;expression="lvui::native::table_header_color("+color_expression(input->value,input->position,initial)+")";
            }else if(key=="interactive"||key=="enabled"||key=="visible"||key=="fitColumnsToViewport"){
                field="configuration."+std::string(key=="fitColumnsToViewport"?"fit_flexible_columns":key);
                expression=dynamic?"lvui::native::truth("+expression_code(input->value,input->position,initial)+")":boolean(values,key,key!="interactive");
            }else if(key=="offset"){
                field="offset";expression=dynamic?"lvui::native::number("+expression_code(input->value,input->position,initial)+")":decimal(numeric(values,key,0,0,1e15));
            }else if(key=="accessibilityText"){
                if(!dynamic)error(input->position,"TableHeader accessibilityText requires a native TableText value");
                field="text";expression="lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input->value,input->position,initial)+")";
            }else if(key=="sort"){
                if(!dynamic)error(input->position,"TableHeader sort requires a native optional TableHeaderSort value");
                field="configuration.sort";expression="lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input->value,input->position,initial)+")";
            }else if(key=="columnAppearance"||key=="iconSource"){
                if(!dynamic)error(input->position,"TableHeader columnAppearance and iconSource require native values");
                field=key=="columnAppearance"?"configuration.column_appearance":"icon_source";
                expression="lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input->value,input->position,initial)+")";
            }else if(key=="defaultLeadingIcon"||key=="defaultTrailingIcon"){
                field=key=="defaultLeadingIcon"?"configuration.default_leading_icon":"configuration.default_trailing_icon";
                expression=dynamic?"lvui::native::text("+expression_code(input->value,input->position,initial)+")":literal(text(values,key));
            }else if(key=="iconRevision"){
                field="icon_revision";
                if(dynamic)expression="lvui::native::integer(lvui::native::range("+expression_code(input->value,input->position,initial)+", 0, 9007199254740991.0))";
                else{const auto n=numeric(values,key,0,0,9007199254740991.);if(std::trunc(n)!=n)error(input->position,"iconRevision must be integral");expression="static_cast<std::int64_t>("+decimal(n)+")";}
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
    std::string owned_media_card_options(const Inputs& values,bool initial,SourcePosition position,bool custom_content=false)const {
        if(values.contains("title")&&values.contains("filename"))error(position,"Use title or filename, not both");
        if(values.contains("onDoubleClick")&&values.contains("onDoubleClicked"))error(position,"Use onDoubleClick or onDoubleClicked, not both");
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
        std::string result="([&] { lvui::native::CardInputValue<ii::gui::RasterImage> value; ",preview;
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
                if(key=="previewSource"){
                    if(!custom_content&&!values.contains("previewComponent"))preview=resolved_image_expression(values,"previewSource",initial,"image_size.width","image_size.height");continue;
                }
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
        if(!preview.empty())result+="if (value.configuration.type == lvui::CardType::file || value.configuration.type == lvui::CardType::file_preview) { lvui::Card model; if (!model.set_configuration(value.configuration)) throw std::invalid_argument(\"Invalid card configuration\"); auto image_size = model.intrinsic_size(); value.preview = "+preview+"; } ";
        return result+"return value; }())";
    }
    std::string owned_selector_options(const Inputs& values,bool initial)const {
        std::string result="([&] { lvui::native::ListItemSelectorInput value; ";
        if(!active_window_accent.empty())result+="value.accent = lvui::WindowAccentPalette{"+accent_style(0,initial)+", "+accent_style(1,initial)+", "+accent_style(2,initial)+", "+accent_style(3,initial)+", "+accent_style(4,initial)+"}; ";
        if(auto found=values.find("options");found!=values.end()){
            const auto* input=found->second;
            if(native_expression(input->value))
                result+="value.configuration = lvui::native::list_item_selector(lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input->value,input->position,initial)+")); ";
            else{
                const auto list=resolve(input->value,input->position);
                if(list.kind!=ValueKind::list||list.elements.size()>400)error(input->position,"List selector requires at most 400 Strings");
                std::string items;
                for(const auto& element:list.elements){
                    const auto item=resolve(element,input->position);
                    if(item.kind!=ValueKind::string)error(input->position,"List selector options must be Strings");
                    if(!items.empty())items+=", ";items+=literal(item.text);
                }
                result+="value.configuration = lvui::native::list_item_selector(std::vector<std::string>{"+items+"}); ";
            }
        }
        for(const auto& [key,input]:values){
            const bool dynamic=native_expression(input->value);std::string field,expression;
            if(key=="currentIndex"){
                field="configuration.selected_index";
                if(dynamic)expression="lvui::native::integer(lvui::native::range("+expression_code(input->value,input->position,initial)+", 0, 399))";
                else{const auto n=numeric(values,key,0,0,399);if(std::trunc(n)!=n)error(input->position,"currentIndex must be integral");expression=decimal(n);}
            }else if(key=="placeholder"||key=="accessibilityName"){
                field=key=="placeholder"?"configuration.placeholder":"accessibility_name";
                expression=dynamic?"lvui::native::text("+expression_code(input->value,input->position,initial)+")":literal(text(values,key));
            }else if(key=="enabled"||key=="visible"){
                field=key=="enabled"?"configuration.enabled":"visible";
                expression=dynamic?"lvui::native::truth("+expression_code(input->value,input->position,initial)+")":boolean(values,key,true);
            }else if(key=="width"||key=="height"){
                field=key;
                expression=dynamic?"lvui::native::number("+expression_code(input->value,input->position,initial)+")":decimal(numeric(values,key,key=="width"?97:20,key=="width"?19:1,key=="width"?8176:8192));
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
        if(values.contains("showChevron")&&values.contains("showTrailingIcon"))error(position,"Use showChevron or showTrailingIcon, not both");
        if(values.contains("slotSpacing")&&values.contains("contentSpacing"))error(position,"Use slotSpacing or contentSpacing, not both");
        const std::map<std::string,std::string,std::less<>> numbers{{"width","width"},{"height","height"},{"leftPadding","left_padding"},
            {"rightPadding","right_padding"},{"iconSize","icon_size"},{"spacing","spacing"},{"cornerRadius","corner_radius"},{"chevronSize","chevron_size"},{"valueWidth","value_width"}};
        const std::map<std::string,std::string,std::less<>> colors{{"textColor","text"},{"descriptionColor","description"},{"disabledTextColor","disabled_text"},
            {"backgroundColor","idle"},{"hoverColor","hover"},{"pressedColor","pressed"},{"selectedColor","selected"},{"focusColor","focus"}};
        std::string result=composite?"([&] { detail_list_composite::Input value; ":
            "([&] { lvui::native::ListItemInputValue<ii::gui::RasterImage> value; ";
        std::string preview;
        result+=list_accent_setup(values,composite?"value.row.configuration.palette.":"value.configuration.palette.","selectedColor",initial);
        if(composite){
            result+="value.accent = lvui::WindowAccentPalette{"+accent_style(0,initial)+", "+accent_style(1,initial)+", "+
                accent_style(2,initial)+", "+accent_style(3,initial)+", "+accent_style(4,initial)+"}; ";
            auto type=values.find("type");if(type==values.end())type=values.find("size");
            if(type!=values.end()&&!native_expression(type->second->value)){
                const auto variant=resolve(type->second->value,type->second->position);
                if(variant.kind==ValueKind::enumeration&&(variant.text==".DetailActions"||variant.text==".detailActions")){
                    if(!values.contains("description")&&!values.contains("detail"))result+="value.row.configuration.description = \"Supporting text\"; ";
                    if(!values.contains("primaryAction"))result+="value.primary_action.text = \"Open\"; ";
                    if(!values.contains("secondaryAction"))result+="value.secondary_action.text = \"Edit\"; ";
                }
                if(variant.kind==ValueKind::enumeration&&(variant.text==".Task"||variant.text==".task")){
                    if(!values.contains("description")&&!values.contains("detail"))result+="value.row.configuration.description = \"Supporting text\"; ";
                    if(!values.contains("primaryAction"))result+="value.primary_action.text = \"Done\"; ";
                    if(!values.contains("secondaryAction"))result+="value.secondary_action.text = \"Assign\"; ";
                }
                if(variant.kind==ValueKind::enumeration&&(variant.text==".Form"||variant.text==".form")){
                    if(!values.contains("description")&&!values.contains("detail"))result+="value.row.configuration.description = \"Supporting text\"; ";
                    if(!values.contains("primaryAction"))result+="value.primary_action.text = \"Save\"; ";
                    if(!values.contains("secondaryAction"))result+="value.secondary_action.text = \"Cancel\"; ";
                }
                if(variant.kind==ValueKind::enumeration&&(variant.text==".Resource"||variant.text==".resource"||variant.text==".Media"||variant.text==".media")){
                    const bool media=variant.text==".Media"||variant.text==".media";
                    if(!values.contains("description")&&!values.contains("detail"))result+="value.row.configuration.description = \"Supporting text\"; ";
                    if(!values.contains("primaryAction"))result+=media?"value.primary_action.text = \"Play\"; ":"value.primary_action.text = \"Open\"; ";
                    if(!values.contains("secondaryAction"))result+=media?"value.secondary_action.text = \"Stop\"; ":"value.secondary_action.text = \"Reveal\"; ";
                }
                if(variant.kind==ValueKind::enumeration&&(variant.text==".DetailQuantity"||variant.text==".detailQuantity"||
                        variant.text==".DetailSettings"||variant.text==".detailSettings")){
                    const bool settings=variant.text==".DetailSettings"||variant.text==".detailSettings";
                    if(!values.contains("description")&&!values.contains("detail"))result+="value.row.configuration.description = \"Supporting text\"; ";
                    if(!values.contains("primaryAction"))result+="value.primary_action.text = \"Apply\"; ";
                    if(settings&&!values.contains("secondaryAction"))result+="value.secondary_action.text = \"Reset\"; ";
                }
            }
        }
        for(const auto& [key,input]:values){
            const bool dynamic=native_expression(input->value);std::string field,expression;
            if(auto found=numbers.find(key);found!=numbers.end()){
                field="configuration."+found->second;const auto high=key=="iconSize"||key=="chevronSize"?256.:8192.;
                expression=dynamic?"lvui::native::number("+expression_code(input->value,input->position,initial)+")":decimal(numeric(values,key,0,0,high));
            }else if(auto found=colors.find(key);found!=colors.end()){
                field="configuration.palette."+found->second;expression="lvui::native::list_item_color("+color_expression(input->value,input->position,initial)+")";
            }else if(key=="label"||key=="text"||key=="description"||key=="detail"||key=="iconName"||key=="accessibilityName"||key=="value"||
                     key=="folderLabel1"||key=="folderLabel2"||key=="tagLabel1"||key=="tagLabel2"){
                field="configuration."+std::string(key=="label"||key=="text"?"label":key=="description"||key=="detail"?"description":key=="iconName"?"icon_name":key=="value"?"value":
                    key=="folderLabel1"?"detail_folder1":key=="folderLabel2"?"detail_folder2":key=="tagLabel1"?"detail_tag1":key=="tagLabel2"?"detail_tag2":"accessibility_name");
                expression=dynamic?"lvui::native::text("+expression_code(input->value,input->position,initial)+")":literal(text(values,key));
            }else if(key=="selected"||key=="enabled"||key=="visible"||key=="interactive"||key=="selectable"||key=="showDescription"||key=="showChevron"||key=="showTrailingIcon"||key=="showValue"||key=="showDate"||key=="showFolders"||key=="showTags"){
                field="configuration."+std::string(key=="showDescription"?"show_description":key=="showChevron"||key=="showTrailingIcon"?"show_chevron":key=="showValue"?"show_value":
                    key=="showDate"?"show_detail_date":key=="showFolders"?"show_detail_folders":key=="showTags"?"show_detail_tags":key);
                expression=dynamic?"lvui::native::truth("+expression_code(input->value,input->position,initial)+")":boolean(values,key,key!="selected"&&key!="showChevron");
            }else if(key=="type"||key=="size"){
                field="configuration.variant";
                if(dynamic)expression="lvui::native::list_item_variant("+expression_code(input->value,input->position,initial)+")";
                else {const auto variant=resolve(input->value,input->position);const std::map<std::string,std::string,std::less<>> variants{{".mini","mini"},{".Mini","mini"},{".detail","detail"},{".Detail","detail"},{".navigation","navigation"},{".Navigation","navigation"},{".toggle","toggle"},{".Toggle","toggle"},{".checkable","checkable"},{".Checkable","checkable"},{".action","action"},{".Action","action"},{".actionGroup","action_group"},{".ActionGroup","action_group"},{".stepper","stepper"},{".Stepper","stepper"},{".select","select"},{".Select","select"},{".inlineEdit","inline_edit"},{".InlineEdit","inline_edit"},{".detailActions","detail_actions"},{".DetailActions","detail_actions"},{".detailQuantity","detail_quantity"},{".DetailQuantity","detail_quantity"},{".detailSettings","detail_settings"},{".DetailSettings","detail_settings"},{".task","task"},{".Task","task"},{".resource","resource"},{".Resource","resource"},{".media","media"},{".Media","media"},{".form","form"},{".Form","form"}};
                    auto found=variants.find(variant.text);if(variant.kind!=ValueKind::enumeration||found==variants.end())error(input->position,"Unsupported independent ListItem type");expression="lvui::ListItemVariant::"+found->second;}
            }else if(key=="dateText"||key=="metadata1"||key=="metadata2"||key=="statusText"||key=="bookmarkIconName"||
                     key=="quantityLabel"||key=="optionLabel"||key=="modeLabel"||key=="scopeLabel"||
                     key=="durationText"||key=="mediaDetail"||key=="previewIconName"||key=="fieldLabel1"||key=="fieldLabel2"){
                field=key=="dateText"?"date_text":key=="statusText"?"status_text":key=="bookmarkIconName"?"bookmark_icon_name":
                    key=="quantityLabel"?"quantity_label":key=="optionLabel"?"option_label":key=="modeLabel"?"mode_label":key=="scopeLabel"?"scope_label":
                    key=="durationText"?"duration_text":key=="mediaDetail"?"media_detail":key=="previewIconName"?"preview_icon_name":
                    key=="fieldLabel1"?"field_label1":key=="fieldLabel2"?"field_label2":key;
                expression=dynamic?"lvui::native::text("+expression_code(input->value,input->position,initial)+")":literal(text(values,key));
            }else if(key=="showLeadingIcon"||key=="showBookmark"||key=="showMetadata"||key=="showPreview"){
                field=key=="showLeadingIcon"?"show_leading_icon":key=="showBookmark"?"show_bookmark":key=="showPreview"?"show_preview":"show_metadata";
                expression=dynamic?"lvui::native::truth("+expression_code(input->value,input->position,initial)+")":boolean(values,key,true);
            }else if(key=="detailHorizontalPadding"||key=="sectionSpacing"||key=="previewSize"){
                field=key=="detailHorizontalPadding"?"detail_horizontal_padding":key=="previewSize"?"preview_size":"detail_section_spacing";
                expression=dynamic?"lvui::native::number("+expression_code(input->value,input->position,initial)+")":decimal(numeric(values,key,0,0,8192));
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
            }else if(key=="moreItems"){
                field="more_items";
                if(dynamic){result+="lvui::native::assign_list_item_menu_options(value, lvui::native::list_item_menu_options(lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input->value,input->position,initial)+"))); ";continue;}
                else{const auto list=resolve(input->value,input->position);if(list.kind!=ValueKind::list||list.elements.size()>400)error(input->position,"List item menu requires at most 400 Strings");
                    std::string items;for(const auto& element:list.elements){const auto item=resolve(element,input->position);if(item.kind!=ValueKind::string)error(input->position,"List item menu entries must be Strings");
                        if(!items.empty())items+=", ";items+=literal(item.text);}
                    expression="lvui::native::list_item_menu_options(std::vector<std::string>{"+items+"})";}
            }else if(key=="slotPadding"||key=="slotSpacing"||key=="contentSpacing"||key=="actionWidth"){
                field=key=="slotPadding"?"slot_padding":key=="slotSpacing"||key=="contentSpacing"?"slot_spacing":"action_width";
                expression=dynamic?"lvui::native::number("+expression_code(input->value,input->position,initial)+")":decimal(numeric(values,key,0,key=="actionWidth"?1.:0.,8192));
            }else if(key=="trailingWidth"||key=="trailingHeight"){
                field=key=="trailingWidth"?"trailing_width":"trailing_height";
                expression=dynamic?"lvui::native::number("+expression_code(input->value,input->position,initial)+")":decimal(numeric(values,key,0,0,8192));
            }else if(key=="quantity"||key=="minimumQuantity"||key=="maximumQuantity"||key=="stepSize"||key=="quantityWidth"||key=="quantitySpacing"){
                field=key=="quantity"?"quantity":key=="minimumQuantity"?"minimum_quantity":key=="maximumQuantity"?"maximum_quantity":key=="stepSize"?"step_size":key=="quantityWidth"?"quantity_width":"quantity_spacing";
                if(dynamic)expression="lvui::native::number("+expression_code(input->value,input->position,initial)+")";
                else{const auto bound=std::numeric_limits<double>::max();const bool layout=key=="quantityWidth"||key=="quantitySpacing";const auto n=numeric(values,key,0,layout?0.:-bound,layout?8192.:bound);
                    if(key=="stepSize"&&n<=0)error(input->position,"stepSize must be positive");expression=decimal(n);}
            }else if(key=="stepperEnabled"||key=="showStepper"){
                field=key=="stepperEnabled"?"stepper_enabled":"show_stepper";
                expression=dynamic?"lvui::native::truth("+expression_code(input->value,input->position,initial)+")":boolean(values,key,true);
            }else if(key=="stepperArrow"){
                field="stepper_arrow";expression=dynamic?"lvui::native::stepper_arrow("+expression_code(input->value,input->position,initial)+")":
                    "lvui::StepperArrow::"+stepper_arrow(values,"stepperArrow");
            }else if(key=="stepperTone"){
                field="stepper_tone";
                if(dynamic)expression="lvui::native::stepper_tone("+expression_code(input->value,input->position,initial)+")";
                else{const std::map<std::string,std::string,std::less<>> tones{{".primary","primary"},{".Primary","primary"},{".borderless","borderless"},{".Borderless","borderless"},{".default","normal"},{".destructive","destructive"},{".disabled","disabled"}};
                    const auto value=resolve(input->value,input->position);const auto found=tones.find(value.text);
                    if(value.kind!=ValueKind::enumeration||found==tones.end())error(input->position,"Unknown Stepper tone");expression="lvui::ButtonTone::"+found->second;}
            }else if(key=="selector"||key=="unitSelector"){
                field=key=="selector"?"selector":"unit_selector";
                const auto helper=key=="selector"?"list_item_selector":"list_item_unit_selector";
                std::string fallback;
                if(key=="selector"){
                    std::string variant="lvui::ListItemVariant::mini";auto type=values.find("type");if(type==values.end())type=values.find("size");
                    if(type!=values.end()){
                        if(native_expression(type->second->value))variant="lvui::native::list_item_variant("+expression_code(type->second->value,type->second->position,initial)+")";
                        else{const auto name=resolve(type->second->value,type->second->position).text;
                            if(name==".DetailQuantity"||name==".detailQuantity")variant="lvui::ListItemVariant::detail_quantity";
                            else if(name==".DetailSettings"||name==".detailSettings")variant="lvui::ListItemVariant::detail_settings";
                            else if(name==".Task"||name==".task")variant="lvui::ListItemVariant::task";
                            else if(name==".Form"||name==".form")variant="lvui::ListItemVariant::form";}
                    }
                    fallback=", "+variant;
                }
                if(dynamic)expression="lvui::native::"+std::string(helper)+"(lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input->value,input->position,initial)+")"+fallback+")";
                else{const auto list=resolve(input->value,input->position);if(list.kind!=ValueKind::list||list.elements.size()>400)error(input->position,"List selector requires at most 400 Strings");
                    std::string items;for(const auto& element:list.elements){const auto item=resolve(element,input->position);if(item.kind!=ValueKind::string)error(input->position,"List selector options must be Strings");
                        if(!items.empty())items+=", ";items+=literal(item.text);}
                    expression="lvui::native::"+std::string(helper)+"(std::vector<std::string>{"+items+"}"+fallback+")";}
            }else if(key=="segments"){
                field="segments";
                if(dynamic)expression="lvui::native::list_item_segments(lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input->value,input->position,initial)+"))";
                else{const auto list=resolve(input->value,input->position);if(list.kind!=ValueKind::list||list.elements.size()>400)error(input->position,"List segments require at most 400 Strings");
                    std::string items;for(const auto& element:list.elements){const auto item=resolve(element,input->position);if(item.kind!=ValueKind::string)error(input->position,"List segment labels must be Strings");
                        if(!items.empty())items+=", ";items+=literal(item.text);}
                    expression="lvui::native::list_item_segments(std::vector<std::string>{"+items+"})";}
            }else if(key=="selectorIndex"||key=="unitIndex"||key=="segmentIndex"){
                field=key=="selectorIndex"?"selector_index":key=="unitIndex"?"unit_index":"segment_index";
                if(dynamic)expression="lvui::native::integer(lvui::native::range("+expression_code(input->value,input->position,initial)+", 0, 399))";
                else{const auto n=numeric(values,key,0,0,399);if(std::trunc(n)!=n)error(input->position,"selectorIndex must be integral");expression=decimal(n);}
            }else if(key=="selectorWidth"||key=="unitSelectorWidth"||key=="segmentWidth"){
                field=key=="selectorWidth"?"selector_width":key=="unitSelectorWidth"?"unit_selector_width":"segment_width";
                expression=dynamic?"lvui::native::number("+expression_code(input->value,input->position,initial)+")":decimal(numeric(values,key,key=="segmentWidth"?56:97,key=="segmentWidth"?1:19,key=="segmentWidth"?8192:8176));
            }else if(key=="selectorEnabled"||key=="showSelector"||key=="unitSelectorEnabled"||key=="showUnitSelector"||key=="segmentsEnabled"||key=="showSegments"){
                field=key=="selectorEnabled"?"selector_enabled":key=="showSelector"?"show_selector":key=="unitSelectorEnabled"?"unit_selector_enabled":
                    key=="showUnitSelector"?"show_unit_selector":key=="segmentsEnabled"?"segments_enabled":"show_segments";
                expression=dynamic?"lvui::native::truth("+expression_code(input->value,input->position,initial)+")":boolean(values,key,true);
            }else if(key=="inputText1"||key=="inputPlaceholder1"||key=="inputText2"||key=="inputPlaceholder2"){
                field=key=="inputText1"?"input_text":key=="inputText2"?"input_text2":key=="inputPlaceholder2"?"input_placeholder2":"input_placeholder";
                expression=dynamic?"lvui::native::text("+expression_code(input->value,input->position,initial)+")":literal(text(values,key));
            }else if(key=="inputWidth"||key=="inputHeight"){
                field=key=="inputWidth"?"input_width":"input_height";
                expression=dynamic?"lvui::native::number("+expression_code(input->value,input->position,initial)+")":decimal(numeric(values,key,0,1,8192));
            }else if(key=="inputEnabled1"||key=="inputReadOnly1"||key=="showInput1"||key=="inputEnabled2"||key=="inputReadOnly2"||key=="showInput2"){
                field=key=="inputEnabled1"?"input_enabled":key=="inputReadOnly1"?"input_read_only":key=="showInput1"?"show_input":
                    key=="inputEnabled2"?"input2_enabled":key=="inputReadOnly2"?"input2_read_only":"show_input2";
                expression=dynamic?"lvui::native::truth("+expression_code(input->value,input->position,initial)+")":boolean(values,key,key!="inputReadOnly1"&&key!="inputReadOnly2");
            }else if(key=="previewSource"){
                preview=resolved_image_expression(values,"previewSource",initial,"image_size","image_size");continue;
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
            if(composite&&key=="dateText")result+="value.row.configuration.detail_date = value.date_text; ";
            if(composite&&key=="iconSize")result+="value.detail_icon_size = "+expression+"; value.leading_size = value.detail_icon_size; ";
            if(composite&&key=="actionWidth"&&!values.contains("valueWidth"))result+="value.row.configuration.value_width = "+expression+"; ";
            if(composite&&key=="sectionSpacing")result+="value.detail_vertical_padding = "+expression+"; ";
        }
        if(composite){
            auto type=values.find("type");if(type==values.end())type=values.find("size");
            if(type!=values.end()&&native_expression(type->second->value)){
                if(!values.contains("description")&&!values.contains("detail"))result+="value.row.configuration.description = lvui::native::list_item_default_description(value.row.configuration.variant); ";
                if(!values.contains("primaryAction"))result+="value.primary_action.text = lvui::native::list_item_default_primary(value.row.configuration.variant); ";
                if(!values.contains("secondaryAction"))result+="value.secondary_action.text = lvui::native::list_item_default_secondary(value.row.configuration.variant); ";
            }
            if(!values.contains("selector"))result+="value.selector.placeholder = lvui::native::list_item_selector_placeholder(value.row.configuration.variant); ";
            if(!preview.empty()&&!values.contains("previewComponent"))result+="if (value.show_preview && (value.row.configuration.variant == lvui::ListItemVariant::resource || value.row.configuration.variant == lvui::ListItemVariant::media)) { const auto image_size = value.preview_size.value_or(48.); if (image_size > 0) value.preview = "+preview+"; } ";
        }
        return result+"return value; }())";
    }
    std::string owned_toolbar_button_options(const Inputs& values,bool initial,SourcePosition position)const {
        if(values.contains("icon")&&values.contains("iconName"))error(position,"Use icon or iconName, not both");
        const std::map<std::string,std::string,std::less<>> numbers{{"width","width"},{"height","height"},{"iconSize","icon_size"},
            {"leftPadding","left_padding"},{"rightPadding","right_padding"},{"spacing","spacing"},{"cornerRadius","corner_radius"},{"fontSize","font_size"}};
        const std::map<std::string,std::string,std::less<>> colors{{"textColor","text"},{"disabledTextColor","disabled_text"},
            {"hoverColor","hover"},{"pressedColor","pressed"},{"primaryColor","primary"},{"dangerColor","danger"},{"focusColor","focus"}};
        std::string result="([&] { lvui::native::ToolbarButtonInputValue<ii::gui::RasterImage> value; ";
        if(!values.contains("primaryColor"))result+="value.configuration.palette.primary = "+accent_style(0,initial)+"; ";
        if(!values.contains("pressedColor"))result+="value.configuration.palette.pressed = "+accent_style(3,initial)+"; ";
        if(!values.contains("focusColor"))result+="value.configuration.palette.focus = "+accent_style(0,initial)+"; ";
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
        const auto slot_spacing=decimal(token_number("--lvui-theme-metrics-space-4",0,8192));
        result+="value.appearance.slot_padding = "+slot_spacing+"; value.appearance.icon_text_spacing = "+slot_spacing+"; ";
        result+=toolbar_accent_setup(values,"value.appearance.palette.",initial);
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
        result+=toolbar_accent_setup(values,"value.configuration.palette.",initial);
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
    std::string owned_list_toolbar_options(const Inputs& values,bool initial,std::string_view sources={})const {
        const std::map<std::string,std::string,std::less<>> numbers{{"width","width"},{"height","height"},{"iconSize","icon_size"},
            {"buttonSize","button_size"},{"horizontalPadding","horizontal_padding"},{"verticalPadding","vertical_padding"},
            {"spacing","spacing"},{"cornerRadius","corner_radius"}};
        const std::map<std::string,std::string,std::less<>> colors{{"backgroundColor","background"},{"hoverColor","hover"},
            {"pressedColor","pressed"},{"focusColor","focus"}};
        const bool framework=!sources.empty();
        std::string result=framework?"([&] { ii::gui::ListBarInput value; ":"([&] { lvui::native::ListToolbarInput value; ";
        if(framework){
            result+="value.configuration.palette.background = "+color_literal(token_color("--lvui-theme-colors-panel-sub-surface"))+"; ";
            result+="value.configuration.palette.hover = "+color_literal(token_color("--lvui-theme-colors-panel-surface-alt"))+"; ";
            result+="value.configuration.palette.pressed = "+color_literal(token_color("--lvui-theme-colors-panel-surface-alt"))+"; ";
        }
        if(!values.contains("focusColor")){
            const auto focus=framework?(!active_window_accent.empty()&&!initial
                ?"window_accent_palette(context.read("+active_window_accent+"))[0]"
                :color_literal(accent_colors(window_accent_name).primary)):accent_style(0,initial);
            result+="value.configuration.palette.focus = "+focus+"; ";
        }
        if(framework&&!values.contains("iconSource"))result+="value.icon_source = "+std::string(sources)+"->get({}, true); ";
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
                expression=framework?color_expression(input->value,input->position,initial):
                    "lvui::native::list_toolbar_color("+color_expression(input->value,input->position,initial)+")";
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
                if(framework)expression=std::string(sources)+"->get("+expression+", true)";
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
        result+=hierarchy_accent_setup(values,"value.row_appearance.palette.",initial);
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
            }else if(name=="modelRevision"||name=="viewRevision"||name=="iconRevision"||name=="overscan"){
                field=name=="modelRevision"?"source_revision":name=="viewRevision"?"view_revision":name=="iconRevision"?"icon_revision":"overscan";
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
        const std::set<std::string,std::less<>> list_fields{"controller","editor","editable","modelRevision","viewRevision","rowHeight","overscan",
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
        for(const auto name:{"editable","reorderable"})if(auto item=values.find(name);item!=values.end()&&!values.contains("editor")&&
           (native_expression(item->second->value)||boolean(values,name,false)=="true"))
            error(item->second->position,"Editable or reorderable List requires a native ListEditor");
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
        result+=list_accent_setup(values,"value.list.palette.","selectedRowColor",initial);
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
            }else if(name=="modelRevision"||name=="viewRevision"||name=="iconRevision"||name=="overscan"){
                field=name=="modelRevision"?"list.source_revision":name=="viewRevision"?"list.view_revision":name=="iconRevision"?"list.icon_revision":"list.overscan";
                const auto high=name=="overscan"?128.:9007199254740991.;
                if(dynamic)expression="lvui::native::integer(lvui::native::range("+expression_code(input->value,input->position,initial)+", 0, "+decimal(high)+"))";
                else{const auto n=numeric(values,name,0,0,high);if(std::trunc(n)!=n)error(input->position,"List integer input must be integral");expression="static_cast<std::int64_t>("+decimal(n)+")";}
                if(name=="overscan")expression="static_cast<std::size_t>("+expression+")";
            }else if(name=="iconSource"){
                if(!dynamic)error(input->position,"List iconSource requires a native IconSource");field="list.icon_source";
                expression="lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input->value,input->position,initial)+")";
            }else if(name=="editor"){
                if(!dynamic)error(input->position,"List editor requires a native ListEditor");field="list.editor";
                expression="lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(input->value,input->position,initial)+")";
            }else if(name=="editable"||name=="reorderable"){
                field=name=="editable"?"list.editable":"list.reorderable";
                expression=dynamic?"lvui::native::truth("+expression_code(input->value,input->position,initial)+")":boolean(values,name,false);
            }else if(name=="showIndicator"||name=="keyboardNavigationEnabled"||name=="scrollable"){
                field=name=="showIndicator"?"list.show_indicator":name=="scrollable"?"list.scrollable":"list.keyboard_navigation";
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


    std::string owned_progress_options(const Inputs& values,bool initial) {
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
            return "ii::gui::WidgetColor{"+decimal(c.red)+", "+decimal(c.green)+", "+decimal(c.blue)+", "+decimal(c.alpha)+"}";
        };
        std::string result="([&] { ii::gui::ProgressConfiguration value; ";
        if(!values.contains("fillColor"))result+="value.style.fill = ii::gui::progress_color("+accent_style(0,initial)+"); ";
        for(const auto& [name,input]:values){
            auto field=fields.find(name); if(field==fields.end())continue;
            const bool dynamic=native_expression(input->value);
            std::string expression;
            if(name=="motion") {
                const bool imported=input->value.kind==ValueKind::reference && backend_access.contains(input->value.text);
                if(!dynamic&&!imported)error(input->position,"ProgressBar motion requires a native LVUI Motion value");
                expression="ii::gui::widget_motion_value("+expression_code(input->value,input->position)+")";
            } else if(name=="motionEnabled") {
                expression=dynamic?"lvui::native::truth("+expression_code(input->value,input->position)+")":boolean(values,name,true);
            } else if(name=="size"||name=="shapeStyle") {
                const bool size=name=="size";
                if(dynamic) expression=std::string("ii::gui::progress_")+(size?"size(":"shape(")+expression_code(input->value,input->position)+")";
                else {
                    const auto v=resolve(input->value,input->position);
                    int selected=-1;
                    if(v.kind==ValueKind::enumeration) {
                        if(v.text==(size?".large":".shapeRoundRect"))selected=0;
                        if(v.text==(size?".regular":".shapeCylinder"))selected=1;
                    } else if(v.kind==ValueKind::number&&(v.text=="0"||v.text=="1"))selected=v.text=="0"?0:1;
                    if(selected<0)error(input->position,"Unknown ProgressBar "+name+" value");
                    expression=size?(selected?"ii::gui::ProgressSize::regular":"ii::gui::ProgressSize::large"):
                        (selected?"ii::gui::ProgressShape::cylinder":"ii::gui::ProgressShape::round_rect");
                }
            } else if(name=="trackColor"||name=="fillColor") {
                expression=dynamic?"lvui::native::color<ii::gui::WidgetColor>("+color_expression(input->value,input->position)+")":
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
            result+="ii::gui::ProgressStateKeys keys; ";
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
            result+="value.values = ii::gui::progress_values(value.values, keys, "+
                    expression_code(source->second->value,source->second->position)+"); ";
        } else {
            for(auto name:{"minimumValueStateKey","maximumValueStateKey","startValueStateKey","currentValueStateKey"})
                if(auto key=values.find(name);key!=values.end())
                    error(key->second->position,"ProgressBar state keys require stateModel");
        }
        return result+"return value; }())";
    }

    std::string owned_segmented_options(const Inputs& values,bool initial,bool icon)const {
        const auto dynamic=[&](std::string_view name){auto item=values.find(name);return item!=values.end()&&native_expression(item->second->value);};
        std::string setup=icon?"value = lvui::icon_segmented_configuration(); ":"";
        for(const auto& [field,key]:std::map<std::string,std::string,std::less<>>{{"background","--lvui-theme-colors-panel-sub-surface"},{"border","--lvui-theme-colors-panel-surface-raised"}}){
            const auto color=token_color(key);setup+="value."+field+" = lvui::StyleColor{"+decimal(color.red)+", "+decimal(color.green)+", "+decimal(color.blue)+", "+decimal(color.alpha)+"}; ";
        }
        for(const auto& [name,field]:std::map<std::string,std::string,std::less<>>{{"enabled","enabled"},{"forceBorderlessTone","force_borderless_tone"}})
            if(auto item=values.find(name);item!=values.end())setup+="value."+field+" = "+(dynamic(name)?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(values,name,true))+"; ";
        if(auto item=values.find("orientation");item!=values.end()){
            const auto& supplied=item->second->value;
            if(!icon || dynamic("orientation") || supplied.kind!=ValueKind::enumeration)
                error(item->second->position,"Icon segmented orientation must be a static .horizontal or .vertical value");
            if(supplied.text==".horizontal") setup+="value.orientation = lvui::SegmentedOrientation::horizontal; ";
            else if(supplied.text==".vertical") setup+="value.orientation = lvui::SegmentedOrientation::vertical; ";
            else error(item->second->position,"Unknown segmented orientation");
        }
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
                {"surface","--lvui-theme-colors-panel-surface-input"},{"hover","--lvui-theme-colors-panel-surface-input-hover"},
                {"pressed","--lvui-theme-colors-panel-surface-raised"},{"disabled","--lvui-theme-colors-panel-sub-surface"}}){
            const auto color=token_color(key);setup+="value.palette."+field+" = lvui::StyleColor{"+decimal(color.red)+", "+decimal(color.green)+", "+decimal(color.blue)+", "+decimal(color.alpha)+"}; ";
        }
        setup+="value.palette.focus = "+accent_style(0,initial)+"; ";
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
        for(const auto& [field,key]:std::map<std::string,std::string,std::less<>>{{"normal","--lvui-theme-colors-panel-surface-raised"},{"normal_hover","--lvui-theme-colors-panel-surface-alt"},{"danger","--lvui-theme-colors-action-danger"},{"disabled_background","--lvui-theme-colors-panel-sub-surface"}}){
            const auto color=token_color(key);setup+="value.palette."+field+" = lvui::StyleColor{"+decimal(color.red)+", "+decimal(color.green)+", "+decimal(color.blue)+", "+decimal(color.alpha)+"}; ";
        }
        setup+=danger_state_setup("value.palette.");
        setup+="value.palette.primary = "+accent_style(0,initial)+"; value.palette.primary_hover = "+accent_style(1,initial)+
            "; value.palette.primary_pressed = "+accent_style(2,initial)+"; value.palette.normal_pressed = "+accent_style(3,initial)+"; ";
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
    std::string owned_search_field_options(const Inputs& values,bool initial)const {
        const auto dynamic=[&](std::string_view name){auto item=values.find(name);return item!=values.end()&&native_expression(item->second->value);};
        std::string setup;
        for(const auto& [name,field]:std::map<std::string,std::string,std::less<>>{{"text","text"},{"placeholder","placeholder"},{"accessibilityName","accessibility_name"},{"clearAccessibilityName","clear_accessibility_name"}})
            if(auto item=values.find(name);item!=values.end())setup+="value."+field+" = "+(dynamic(name)?"lvui::native::text("+expression_code(item->second->value,item->second->position,initial)+")":literal(text(values,name)))+"; ";
        for(const auto& [name,field]:std::map<std::string,std::string,std::less<>>{{"enabled","enabled"},{"readOnly","read_only"},{"clearButtonVisible","clear_button_visible"}})
            if(auto item=values.find(name);item!=values.end())setup+="value."+field+" = "+(dynamic(name)?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(values,name,name!="readOnly"))+"; ";
        if(auto item=values.find("surface");item!=values.end()){
            std::string surface;
            if(dynamic("surface"))surface="lvui::search_field_surface("+expression_code(item->second->value,item->second->position,initial)+")";
            else{
                const auto v=resolve(item->second->value,item->second->position);
                if((v.kind==ValueKind::enumeration&&v.text==".rounded")||(v.kind==ValueKind::number&&v.text=="1"))
                    surface="lvui::InputFieldSurface::rounded";
                else if((v.kind==ValueKind::enumeration&&v.text==".cylinder")||(v.kind==ValueKind::number&&v.text=="2"))
                    surface="lvui::InputFieldSurface::cylinder";
                else if((v.kind==ValueKind::enumeration&&v.text==".inline")||(v.kind==ValueKind::number&&v.text=="3"))
                    surface="lvui::InputFieldSurface::inline_";
                else error(item->second->position,"Unknown SearchField surface value");
            }
            setup+="value.surface = "+surface+"; ";
        }
        if(auto item=values.find("width");item!=values.end())setup+="value.width = "+(dynamic("width")?"lvui::native::number(lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(item->second->value,item->second->position,initial)+"))":decimal(numeric(values,"width",0,1,8192)))+"; ";
        return "([&] { lvui::SearchFieldConfiguration value; "+setup+"return value; }())";
    }
    std::string owned_input_field_options(const Inputs& values,bool initial,bool text_editor=false)const {
        const auto dynamic=[&](std::string_view name){auto item=values.find(name);return item!=values.end()&&native_expression(item->second->value);};
        std::string setup="value.palette.focus = "+accent_style(0,initial)+"; value.palette.selection = value.palette.focus; value.palette.selection.alpha *= .38; ";
        if(auto item=values.find("surface");item!=values.end()) {
            std::string surface;
            if(dynamic("surface"))surface="lvui::input_field_surface("+expression_code(item->second->value,item->second->position,initial)+")";
            else {
                const auto v=resolve(item->second->value,item->second->position);
                if((v.kind==ValueKind::enumeration&&v.text==".flat")||(v.kind==ValueKind::number&&v.text=="0"))
                    surface="lvui::InputFieldSurface::flat";
                else if((v.kind==ValueKind::enumeration&&v.text==".rounded")||(v.kind==ValueKind::number&&v.text=="1"))
                    surface="lvui::InputFieldSurface::rounded";
                else if((v.kind==ValueKind::enumeration&&v.text==".cylinder")||(v.kind==ValueKind::number&&v.text=="2"))
                    surface="lvui::InputFieldSurface::cylinder";
                else if((v.kind==ValueKind::enumeration&&v.text==".inline")||(v.kind==ValueKind::number&&v.text=="3"))
                    surface="lvui::InputFieldSurface::inline_";
                else error(item->second->position,"Unknown InputField surface value");
            }
            setup+="value.surface = "+surface+"; ";
        }
        if(auto item=values.find("size");item!=values.end()) {
            std::string size;
            if(dynamic("size"))size="lvui::input_field_size("+expression_code(item->second->value,item->second->position,initial)+")";
            else {
                const auto v=resolve(item->second->value,item->second->position);
                if((v.kind==ValueKind::enumeration&&v.text==".regular")||(v.kind==ValueKind::number&&v.text=="0"))
                    size="lvui::InputFieldSize::regular";
                else if((v.kind==ValueKind::enumeration&&v.text==".compact")||(v.kind==ValueKind::number&&v.text=="1"))
                    size="lvui::InputFieldSize::compact";
                else error(item->second->position,"Unknown InputField size value");
            }
            setup+="lvui::apply_input_field_size(value, "+size+"); ";
        }
        if(auto item=values.find("maximumLength");item!=values.end()) {
            if(dynamic("maximumLength"))
                setup+="value.maximum_length = static_cast<std::size_t>(lvui::native::integer(lvui::native::range(lvui::native::widget_value<ii::gui::WidgetColor>("+
                    expression_code(item->second->value,item->second->position,initial)+"), 0, 2147483647))); ";
            else {
                const auto maximum=numeric(values,"maximumLength",32767,0,2147483647);
                if(std::floor(maximum)!=maximum)error(item->second->position,"InputField maximumLength must be an integer");
                setup+="value.maximum_length = "+std::to_string(static_cast<std::int64_t>(maximum))+"; ";
            }
        } else if(!text_editor) setup+="value.maximum_length = 32767; ";
        for(const auto& [name,field]:std::map<std::string,std::string,std::less<>>{{"text","text"},{"placeholder","placeholder"},{"accessibilityName","accessibility_name"},{"clearAccessibilityName","clear_accessibility_name"},{"fontFamily","font_family"}})
            if(auto item=values.find(name);item!=values.end())setup+="value."+field+" = "+(dynamic(name)?"lvui::native::text("+expression_code(item->second->value,item->second->position,initial)+")":literal(text(values,name)))+"; ";
        for(const auto& [name,field]:std::map<std::string,std::string,std::less<>>{{"enabled","enabled"},{"readOnly","read_only"},{"multiline","multiline"},{"password","password"},{"wrapEnabled","wrap_enabled"},{"clearButtonVisible","clear_button_visible"}})
            if(auto item=values.find(name);item!=values.end())setup+="value."+field+" = "+(dynamic(name)?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(values,name,name=="enabled"||name=="wrapEnabled"))+"; ";
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
        if(text_editor){
            std::string minimum="1";
            if(auto item=values.find("minimumHeight");item!=values.end())
                minimum=dynamic("minimumHeight")?"lvui::native::number(lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(item->second->value,item->second->position,initial)+"))":decimal(numeric(values,"minimumHeight",1,1,8192));
            return "([&] { auto value = lvui::text_editor_field_defaults(); "+setup+"lvui::TextEditorConfiguration result; result.field = std::move(value); result.minimum_height = "+minimum+"; return result; }())";
        }
        return "([&] { lvui::InputFieldConfiguration value; "+setup+"return value; }())";
    }
    std::string owned_slider_options(const Inputs& values,bool initial,SourcePosition position)const {
        const auto dynamic=[&](std::string_view name){auto item=values.find(name);return item!=values.end()&&native_expression(item->second->value);};
        if(!dynamic("from")&&!dynamic("to")&&numeric(values,"from",0,-1e12,1e12)>=numeric(values,"to",1,-1e12,1e12))
            error(position,"Slider from must be smaller than to");
        SliderConfiguration defaults;std::string size_expression,type_expression;
        if(auto item=values.find("size");item!=values.end()){
            if(dynamic("size"))size_expression="lvui::slider_size("+expression_code(item->second->value,item->second->position,initial)+")";
            else {
                const auto v=resolve(item->second->value,item->second->position);
                const std::array names{"mini","small","regular","large"};int selected=-1;
                for(std::size_t i=0;i<names.size();++i)
                    if((v.kind==ValueKind::enumeration&&v.text=="."+std::string(names[i]))||
                       (v.kind==ValueKind::number&&v.text==std::to_string(i)))selected=static_cast<int>(i);
                if(selected<0)error(item->second->position,"Unknown Slider size value");
                apply_slider_size(defaults,slider_size(selected));size_expression="lvui::SliderSize::"+std::string(names[selected]);
            }
        }
        if(auto item=values.find("type");item!=values.end()){
            if(dynamic("type"))type_expression="lvui::slider_type("+expression_code(item->second->value,item->second->position,initial)+")";
            else {
                const auto v=resolve(item->second->value,item->second->position);
                const std::array names{std::pair{"default","normal"},std::pair{"centerBiased","center_biased"},
                    std::pair{"ticks","ticks"},std::pair{"centerBiasedTicks","center_biased_ticks"},std::pair{"filled","filled"},
                    std::pair{"minMaxLabels","min_max_labels"},std::pair{"segmented","segmented"}};
                for(std::size_t i=0;i<names.size();++i){
                    auto upper=std::string(names[i].first);upper[0]=char(std::toupper(static_cast<unsigned char>(upper[0])));
                    if((v.kind==ValueKind::enumeration&&(v.text=="."+std::string(names[i].first)||v.text=="."+upper))||
                       (v.kind==ValueKind::number&&v.text==std::to_string(i))){
                        type_expression="lvui::SliderType::"+std::string(names[i].second);
                        apply_slider_type(defaults,slider_type(i));break;
                    }
                }
                if(type_expression.empty())error(item->second->position,"Unknown or unsupported Slider type");
            }
        }
        if(!dynamic("type")&&!dynamic("size")&&!dynamic("width")&&!dynamic("height")&&!dynamic("thumbDiameter")&&!dynamic("trackHeight")){
            const auto width=numeric(values,"width",defaults.width,18,8192),height=numeric(values,"height",defaults.height,1,8192);
            const auto thumb=numeric(values,"thumbDiameter",defaults.thumb_diameter,1,256),track=numeric(values,"trackHeight",defaults.track_height,1,256);
            if(width<thumb||height<thumb||height<track)error(position,"Slider dimensions must contain the thumb and track");
        }
        std::string setup="value.palette.fill = "+accent_style(0,initial)+"; value.palette.focus = value.palette.fill; ";
        setup+="value.palette.track = lvui::native::slider_color("+
            color_literal(token_color("--lvui-theme-colors-panel-surface-solid"))+"); ";
        setup+="value.palette.thumb = lvui::native::slider_color("+
            color_literal(token_color("--lvui-theme-colors-text-title-header-color"))+"); ";
        setup+="value.palette.mark = lvui::native::slider_color("+
            color_literal(token_color("--lvui-theme-colors-text-caption-color"))+"); ";
        setup+="value.palette.label = lvui::native::slider_color("+
            color_literal(token_color("--lvui-theme-colors-text-body-color"))+"); ";
        setup+="value.palette.symbol = lvui::native::slider_color("+
            color_literal(token_color("--lvui-material-alert-action-border"))+"); ";
        setup+="value.palette.inactive_fill = lvui::native::slider_color("+
            color_literal(token_color("--lvui-theme-colors-text-disabled-color"))+"); ";
        if(!type_expression.empty())setup+="lvui::apply_slider_type(value, "+type_expression+"); ";
        if(!size_expression.empty())setup+="lvui::apply_slider_size(value, "+size_expression+"); ";
        const auto assign_number=[&](std::string_view name,std::string_view field,double low,double high){
            auto item=values.find(name);if(item==values.end())return;
            const auto expression=dynamic(name)?"lvui::native::number(lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(item->second->value,item->second->position,initial)+"))":decimal(numeric(values,name,0,low,high));
            setup+="value."+std::string(field)+" = "+expression+"; ";
        };
        for(const auto& [name,field]:std::map<std::string,std::string,std::less<>>{{"from","minimum"},{"to","maximum"},{"value","value"}})
            assign_number(name,field,-1e12,1e12);
        assign_number("stepSize","step",0,32767);assign_number("width","width",18,8192);
        if(values.contains("stepSize"))setup+="value.automatic_segment_step = false; ";
        assign_number("height","height",1,8192);assign_number("thumbDiameter","thumb_diameter",1,256);
        assign_number("trackHeight","track_height",1,256);assign_number("rangeInset","range_inset",0,128);
        if(auto item=values.find("enabled");item!=values.end())setup+="value.enabled = "+(dynamic("enabled")?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(values,"enabled",true))+"; ";
        if(auto item=values.find("active");item!=values.end())setup+="value.active = "+(dynamic("active")?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(values,"active",true))+"; ";
        if(auto item=values.find("showTicks");item!=values.end())setup+="value.show_ticks = "+(dynamic("showTicks")?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(values,"showTicks",false))+"; ";
        if(auto item=values.find("showSymbol");item!=values.end())setup+="value.show_symbol = "+(dynamic("showSymbol")?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(values,"showSymbol",false))+"; ";
        for(const auto& [name,field,fallback]:{std::tuple{"showMinMax","show_min_max",false},
                std::tuple{"showLabels","show_labels",true},std::tuple{"showEndpointIcons","show_endpoint_icons",false},
                std::tuple{"rightToLeft","right_to_left",false}})
            if(auto item=values.find(name);item!=values.end())setup+="value."+std::string(field)+" = "+
                (dynamic(name)?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(values,name,fallback))+"; ";
        for(const auto& [name,field,is_icon]:{std::tuple{"minimumLabel","minimum_label",false},
                std::tuple{"maximumLabel","maximum_label",false},std::tuple{"minimumIconName","minimum_icon_name",true},
                std::tuple{"maximumIconName","maximum_icon_name",true}}){
            auto item=values.find(name);if(item==values.end())continue;
            if(!dynamic(name)){const auto value=text(values,name);
                if(!valid_accessibility_text(value)||(is_icon&&!value.empty()&&(value.size()>256||value.find_first_not_of(" \t\r\n")==std::string::npos)))
                    error(item->second->position,"Invalid Slider endpoint text or icon name");}
            setup+="value."+std::string(field)+" = "+(dynamic(name)?"lvui::native::text("+expression_code(item->second->value,item->second->position,initial)+")":literal(text(values,name)))+"; ";
        }
        if(auto item=values.find("symbolName");item!=values.end()){
            if(!dynamic("symbolName")){const auto name=text(values,"symbolName");
                if(name.size()>256||name.find_first_not_of(" \t\r\n")==std::string::npos)error(item->second->position,"Slider symbol name must contain 1 to 256 bytes");}
            setup+="value.symbol_name = "+(dynamic("symbolName")?"lvui::native::text("+expression_code(item->second->value,item->second->position,initial)+")":literal(text(values,"symbolName")))+"; ";
        }
        if(auto item=values.find("tickCount");item!=values.end()){
            std::string count;
            if(dynamic("tickCount"))count="lvui::native::number(lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(item->second->value,item->second->position,initial)+"))";
            else {const auto n=numeric(values,"tickCount",11,2,512);if(std::trunc(n)!=n)error(item->second->position,"Slider tick count must be a whole number");count=decimal(n);}
            setup+="value.tick_count = lvui::slider_tick_count("+count+"); ";
        }
        if(auto item=values.find("segmentCount");item!=values.end()){
            std::string count;
            if(dynamic("segmentCount"))count="lvui::native::number(lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(item->second->value,item->second->position,initial)+"))";
            else {const auto n=numeric(values,"segmentCount",slider_segmented_defaults::default_segment_count,2,512);if(std::trunc(n)!=n)error(item->second->position,"Slider segment count must be a whole number");count=decimal(n);}
            setup+="value.segment_count = lvui::slider_segment_count("+count+"); ";
        }
        if(auto item=values.find("accessibilityName");item!=values.end())setup+="value.accessibility_name = "+(dynamic("accessibilityName")?"lvui::native::text("+expression_code(item->second->value,item->second->position,initial)+")":literal(text(values,"accessibilityName")))+"; ";
        for(const auto& [name,field]:std::map<std::string,std::string,std::less<>>{{"trackColor","track"},{"fillColor","fill"},{"inactiveFillColor","inactive_fill"},{"thumbColor","thumb"},{"focusColor","focus"},{"labelColor","label"}})
            if(auto item=values.find(name);item!=values.end())setup+="value.palette."+field+" = lvui::native::slider_color("+color_expression(item->second->value,item->second->position,initial)+"); ";
        return "([&] { lvui::SliderConfiguration value; "+setup+"return value; }())";
    }
    void validate_owned_tooltip(const Inputs& values,const View& view)const {
        if(values.contains("text")&&!view.children.empty())error(view.position,"Use Tooltip text or inline content, not both");
        if(auto item=values.find("motion");item!=values.end()&&!native_expression(item->second->value))
            error(item->second->position,"Tooltip motion requires native motion preferences");
        if(values.contains("target")&&values.contains("targetId"))error(view.position,"Use Tooltip target or targetId, not both");
        for(const auto name:{"target","availableRect"})
            if(auto item=values.find(name);item!=values.end()&&!native_expression(item->second->value))
                error(item->second->position,"Tooltip target and availableRect require typed backend values");
        if(auto item=values.find("targetId");item!=values.end()&&native_expression(item->second->value))
            error(item->second->position,"Tooltip targetId must be a static widget id");
    }
    std::string owned_tooltip_configuration(const Inputs& values,bool initial)const {
        const auto dynamic=[&](std::string_view name){auto item=values.find(name);return item!=values.end()&&native_expression(item->second->value);};
        const auto number=[&](std::string_view name,double fallback,double low,double high){
            auto item=values.find(name);if(item==values.end())return decimal(fallback);
            return dynamic(name)?"lvui::native::number(lvui::native::widget_value<ii::gui::WidgetColor>("+
                expression_code(item->second->value,item->second->position,initial)+"))":decimal(numeric(values,name,fallback,low,high));
        };
        std::string setup;
        for(const auto& [name,field]:tooltip_numbers){
            if(name=="surfaceOpacity"||!values.contains(name))continue;
            if((name=="maximumWidth"||name=="maximumHeight")&&!dynamic(name)&&numeric(values,name,0,0,8192)<=0)
                error(values.at(name)->position,"Tooltip maximum size must be positive");
            setup+="value."+field+" = "+number(name,0,0,8192)+"; ";
        }
        if(auto item=values.find("automatic");item!=values.end())setup+="value.automatic = "+
            (dynamic("automatic")?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(values,"automatic",true))+"; ";
        for(const auto& [name,field]:tooltip_durations){
            if(!values.contains(name))continue;
            if(dynamic(name))setup+="value."+field+" = lvui::native::tooltip_duration("+number(name,0,0,86400000)+"); ";
            else{
                const auto duration=numeric(values,name,0,0,86400000);
                if(std::trunc(duration)!=duration)error(values.at(name)->position,"Tooltip duration must be whole milliseconds");
                setup+="value."+field+" = std::chrono::milliseconds{"+std::to_string(static_cast<std::int64_t>(duration))+"}; ";
            }
        }
        if(auto item=values.find("preferredPlacement");item!=values.end()){
            if(dynamic("preferredPlacement"))setup+="value.preferred = lvui::native::tooltip_placement("+expression_code(item->second->value,item->second->position,initial)+"); ";
            else{
                const auto choice=resolve(item->second->value,item->second->position);
                const std::map<std::string,std::string,std::less<>> choices{{".Automatic","automatic"},{".Above","above"},{".Below","below"},{".LeftSide","left"},{".RightSide","right"}};
                auto found=choices.find(choice.text);if(choice.kind!=ValueKind::enumeration||found==choices.end())error(item->second->position,"Unknown independent Tooltip placement");
                setup+="value.preferred = lvui::TooltipPlacement::"+found->second+"; ";
            }
        }
        return "([&] { lvui::TooltipConfiguration value; "+setup+"return value; }())";
    }
    std::string owned_tooltip_style(const Inputs& values,bool initial,bool include_padding=true)const {
        const auto dynamic=[&](std::string_view name){auto item=values.find(name);return item!=values.end()&&native_expression(item->second->value);};
        const auto number=[&](std::string_view name,double fallback,double low,double high){
            auto item=values.find(name);if(item==values.end())return decimal(fallback);
            return dynamic(name)?"lvui::native::number(lvui::native::widget_value<ii::gui::WidgetColor>("+
                expression_code(item->second->value,item->second->position,initial)+"))":decimal(numeric(values,name,fallback,low,high));
        };
        std::string setup;
        if(include_padding&&values.contains("contentPadding"))setup+="value.content_padding = "+number("contentPadding",12,0,8192)+"; ";
        if(auto item=values.find("scrollContent");item!=values.end())setup+="value.scroll_content = "+
            (dynamic("scrollContent")?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(values,"scrollContent",true))+"; ";
        if(auto item=values.find("motionEnabled");item!=values.end())setup+="value.motion_enabled = "+
            (dynamic("motionEnabled")?"lvui::native::truth("+expression_code(item->second->value,item->second->position,initial)+")":boolean(values,"motionEnabled",true))+"; ";
        if(auto item=values.find("motion");item!=values.end())setup+="value.motion = lvui::native::widget_value<ii::gui::WidgetColor>("+
            expression_code(item->second->value,item->second->position,initial)+"); ";
        if(values.contains("surfaceOpacity"))setup+="value.material.tint_opacity = "+number("surfaceOpacity",1,0,1)+"; ";
        if(auto item=values.find("accessibilityName");item!=values.end())setup+="value.accessibility_name = "+
            (dynamic("accessibilityName")?"lvui::native::text("+expression_code(item->second->value,item->second->position,initial)+")":literal(text(values,"accessibilityName")))+"; ";
        for(const auto [name,field]:{std::pair{"surfaceColor","color"},{"borderColor","border_color"},{"primaryColor","primary_color"}})
            if(auto item=values.find(name);item!=values.end())setup+="value.material."+std::string(field)+" = lvui::native::material_color("+color_expression(item->second->value,item->second->position,initial)+"); ";
        if(auto item=values.find("backdrop");item!=values.end()){
            if(dynamic("backdrop"))setup+="value.material.backdrop = lvui::native::material_backdrop("+expression_code(item->second->value,item->second->position,initial)+"); ";
            else{
                const auto choice=resolve(item->second->value,item->second->position);
                const std::map<std::string,std::string,std::less<>> choices{{".None","none"},{".Content","content"},{".PreviousLayers","previous_layers"}};
                auto found=choices.find(choice.text);if(choice.kind!=ValueKind::enumeration||found==choices.end())error(item->second->position,"Unknown independent Tooltip backdrop");
                setup+="value.material.backdrop = lvui::MaterialBackdrop::"+found->second+"; ";
            }
        }
        return "([&] { detail_tooltip::Style value; "+setup+"return value; }())";
    }
    std::string owned_tooltip_target(const Inputs& values,bool initial)const {
        if(!values.contains("anchorX")&&!values.contains("anchorY")&&!values.contains("availableRect"))return "lvui::native::TooltipTargetOptions{}";
        const auto coordinate=[&](std::string_view name){auto item=values.find(name);
            if(item!=values.end()&&native_expression(item->second->value))return "lvui::native::number(lvui::native::widget_value<ii::gui::WidgetColor>("+
                expression_code(item->second->value,item->second->position,initial)+"))";
            return decimal(numeric(values,name,0,-1000000000,1000000000));};
        std::string setup;
        if(values.contains("anchorX")||values.contains("anchorY"))setup+="value.anchor = lvui::TooltipPoint{"+coordinate("anchorX")+", "+coordinate("anchorY")+"}; ";
        if(auto item=values.find("availableRect");item!=values.end())setup+="value.available = lvui::native::tooltip_available_rect("+
            expression_code(item->second->value,item->second->position,initial)+"); ";
        return "([&] { lvui::native::TooltipTargetOptions value; "+setup+"return value; }())";
    }
    std::string owned_tooltip_options(const Inputs& values,bool initial)const {
        return "([&] { auto configuration = "+owned_tooltip_configuration(values,initial)+"; auto style = "+
            owned_tooltip_style(values,initial,false)+"; style.content_padding = configuration.content_padding; return detail_tooltip::Input{configuration, style, "+
            owned_tooltip_target(values,initial)+"}; }())";
    }


    std::string scope_declaration(const std::string& scope, bool nested = false) const {
        const auto base=native_scope_base.empty()?"ScopeFactoryStorage":native_scope_base;
        std::string revisions;
        for(std::size_t index=0;index<next_backend;++index)revisions+="            if (this->revision_"+std::to_string(index)+".get() == UINT64_MAX) throw std::length_error(\"Backend revision exhausted\"); this->revision_"+std::to_string(index)+".set(this->revision_"+std::to_string(index)+".get()+1);\n";
        const auto publisher=native_runtime?"        void publish_native_changes() { auto graph = native_lifetime.lock(); if (!graph || !active()) return; lvui::native::batch(*graph, [&] {\n"+revisions+"        }); }\n":"";
        return "    struct ScopeFactoryStorage {\n        lvui::native::BackendFactories backend_factories;\n        lvui::native::BackendActivation backend_activation;\n"+
               std::string(("        std::shared_ptr<ii::gui::RadioGroups> radio_groups;\n        std::shared_ptr<lvui::NavigationContext> navigation_context;\n"))+
               std::string(scene_lifecycle_runtime?"        std::shared_ptr<lvui::native::SceneEffectGroup> scene_effects;\n":"")+
               std::string(scene_registry_runtime?"        std::weak_ptr<lvui::native::SceneRegistry> scene_registry;\n":"")+
               std::string(scoped_image_runtime?"        std::shared_ptr<lvui::native::ImageServices<ii::gui::RasterImage>> image_services;\n":"")+
               std::string(native_runtime?"        std::shared_ptr<lvui::native::BindingDiagnostics> binding_diagnostics;\n":"")+
               "        explicit ScopeFactoryStorage(const lvui::native::BackendFactories& factories) : backend_factories("+
               std::string(localization_runtime?"factories.app_strings() ? factories : factories.with_app_strings(std::make_shared<lvui::native::AppStrings>())":"factories")+"), backend_activation("+std::string(next_backend||(scoped_image_runtime&&!nested)?"factories.activate()":"lvui::native::BackendActivation{}")+") {}\n    };\n"+
               native_scope_parts + "    struct Scope final : " + base +
               " {\n" + scope + native_style_slots + "        explicit Scope(const lvui::native::BackendFactories& factories = {}) : "+base+"(factories)"+scope_constructor+" {}\n"+publisher+
               (native_initializers.empty() ? "" : "        bool values_initialized = false;\n") +
               (native_runtime ? "        std::weak_ptr<lvui::native::Bindings> native_lifetime;\n        bool active() const noexcept { auto owner = native_lifetime.lock(); if (!owner || !owner->available()) return false; " + std::string(nested ? "if (!this->parent) return false; if constexpr (requires { this->parent->active(); }) return this->parent->active(); " : "") + "return true; }\n        ii::core::Result<std::vector<lvui::native::ScopedBindingFailure>> binding_failures() const { if (!this->binding_diagnostics) return ii::core::fail(ii::core::ErrorCode::closed, \"View scope has closed\"); auto failures = this->binding_diagnostics->failures(); if (!failures) return failures; if (!active()) return ii::core::fail(ii::core::ErrorCode::closed, \"View scope has closed\"); return failures; }\n" : "") + "    };\n";
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
    struct WindowOutput final {
        std::string title, appearance, palette;
        double width=0, height=0, minimum_width=0, minimum_height=0;
        std::string opened, closed, close_requested;
        bool custom_chrome=false;
    };
    WindowOutput prepare_window(const View& root) {
        if(component_kinds(root.children).size()!=1)error(root.position,"ApplicationWindow requires exactly one child View after sequence expansion");
        const auto values=inputs(root,{"title","width","height","minimumWidth","minimumHeight","accent","backgroundColor","fontFamily","windowChrome","onOpened","onClosed","onCloseRequested","id","initiallyOpen"});
        if (!application_generation && (values.contains("id") || values.contains("initiallyOpen")))
            error(root.position, "Named Window scenes require an Application root");
        const bool scene_events=values.contains("onOpened")||values.contains("onClosed")||values.contains("onCloseRequested");
        if(scene_events&&!application_generation)error(root.position,"Window lifecycle events require an Application root");
        scene_lifecycle_runtime=scene_lifecycle_runtime||scene_events;
        const auto opened=action(values,"onOpened"),closed=action(values,"onClosed"),close_requested=close_decision(values);
        bool custom_chrome=false;
        if (auto found=values.find("windowChrome");found!=values.end()) {
            const auto& input=*found->second;
            if (native_expression(input.value)) error(input.position,"windowChrome requires a static .system or .custom value");
            const auto value=resolve(input.value,input.position);
            if(value.kind!=ValueKind::enumeration || (value.text!=".system" && value.text!=".custom"))
                error(input.position,"windowChrome requires a static .system or .custom value");
            custom_chrome=value.text==".custom";
        }
        window_close_runtime=window_close_runtime||close_requested!="{}";
        if(auto found=values.find("accent");found!=values.end()) {
            const auto& input=*found->second;
            if(native_expression(input.value)) {
                native_style_slots+="        ii::core::Observable<lvui::WindowAccent> window_accent{lvui::WindowAccent::blue};\n";
                active_window_accent="scope->window_accent";
            } else {
                const auto value=resolve(input.value,input.position);
                const auto name=value.kind==ValueKind::enumeration&&value.text.starts_with('.')?value.text.substr(1):
                                value.kind==ValueKind::string?value.text:std::string{};
                if(!parse_window_accent(name))error(input.position,"Unknown LVUI Window accent");
                window_accent_name=name;
            }
        }
        Inputs properties=values;
        properties.erase("onOpened"); properties.erase("onClosed"); properties.erase("onCloseRequested"); properties.erase("id"); properties.erase("initiallyOpen"); properties.erase("windowChrome");
        Inputs bootstrap=properties;
        for(auto it=bootstrap.begin();it!=bootstrap.end();)if(native_expression(it->second->value))it=bootstrap.erase(it);else ++it;
        auto width=numeric(bootstrap,"width",640,1,32767),height=numeric(bootstrap,"height",420,1,32767);
        auto minimum_width=numeric(bootstrap,"minimumWidth",320,1,32767),minimum_height=numeric(bootstrap,"minimumHeight",220,1,32767);
        if(values.contains("width")&&!bootstrap.contains("width"))width=std::max(width,minimum_width);
        if(values.contains("height")&&!bootstrap.contains("height"))height=std::max(height,minimum_height);
        if(values.contains("minimumWidth")&&!bootstrap.contains("minimumWidth"))minimum_width=std::min(minimum_width,width);
        if(values.contains("minimumHeight")&&!bootstrap.contains("minimumHeight"))minimum_height=std::min(minimum_height,height);
        if(width<minimum_width||height<minimum_height)error(root.position,"Window size must be at least its minimum size");
        emit_window_properties(properties);
        for(const auto& child:root.children)emit(child,"native_parent");
        return {text(bootstrap,"title","LVUI"),appearance_code(bootstrap,window_accent_name),accent_palette_code(),
                width,height,minimum_width,minimum_height,opened,closed,close_requested,custom_chrome};
    }
    enum class ScopedRowContext { none, hierarchy, list, list_item, table };
    std::string emit_scoped(const View& view, const std::string& parent, const Inputs* menu_parent,const Repetition* repetition=nullptr,bool menu_row=false,ScopedRowContext row_context=ScopedRowContext::none,bool tab_row=false,WindowOutput* window=nullptr) {
        const bool hierarchy_view_row=row_context==ScopedRowContext::hierarchy,list_view_row=row_context==ScopedRowContext::list;
        const bool list_item_view=row_context==ScopedRowContext::list_item,table_view=row_context==ScopedRowContext::table;
        const auto name = std::string(repetition?"RepeatedView_":"LocalView_") + std::to_string(next_scope++);
        native_runtime = true;
        needs_parent_scope = true;
        const auto outer_segmented_style=active_segmented_style;
        const auto outer_tabbar_registration=active_tabbar_registration;
        const auto outer_tabbar_section=active_tabbar_section,outer_tabbar_row=active_tabbar_row,outer_tabbar_capture=active_tabbar_capture;
        const bool captured_tab=!tab_row&&!active_tabbar_capture.empty();
        const bool borrowed_tab_row=!tab_row&&!active_tabbar_row.empty()&&!captured_tab;
        if(!tab_row&&!active_tabbar_row.empty())borrow_path(active_tabbar_row);
        if(tab_row){active_tabbar_registration="capture_tab";active_tabbar_row="scope->tab_row";active_tabbar_capture.clear();}
        if(captured_tab){active_tabbar_registration="capture_tab";active_tabbar_capture="scope->tab_pending";}
        if(active_tabbar_section.starts_with("scope->"))borrow_path(active_tabbar_section);
        const auto outer_window_accent=active_window_accent;
        const auto outer_menu=active_menu_registration,outer_divider=active_menu_divider_registration,outer_section=active_menu_section,outer_menu_row=active_menu_row;
        const auto outer_menu_capture=active_menu_capture;const bool captured_menu=!menu_row&&!active_menu_capture.empty();
        const bool borrowed_menu_row=!menu_row&&!active_menu_row.empty()&&!captured_menu;
        if(!menu_row&&!active_menu_row.empty())borrow_path(active_menu_row);
        if(menu_row){active_menu_registration="capture_menu_item";active_menu_divider_registration="capture_menu_divider";active_menu_row="scope->menu_row";active_menu_capture.clear();}
        if(captured_menu){active_menu_registration="capture_menu_item";active_menu_divider_registration="capture_menu_divider";active_menu_capture="scope->menu_pending";}
        if(active_menu_registration.starts_with("scope->"))borrow_path(active_menu_registration);
        if(active_menu_divider_registration.starts_with("scope->"))borrow_path(active_menu_divider_registration);
        if(active_menu_section.starts_with("scope->"))borrow_path(active_menu_section);
        if(!active_segmented_style.empty())borrow_path(active_segmented_style);
        if(active_tabbar_registration.starts_with("scope->"))borrow_path(active_tabbar_registration);
        if(!active_window_accent.empty())borrow_path(active_window_accent);
        auto outer_constants = constants;
        auto outer_reactive = reactive;
        auto outer_native_values = native_values;
        auto outer_writable = writable;
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
        for (auto& [key, access] : writable) borrow_path(access.target);
        std::string payload = "        std::shared_ptr<ParentScope> parent;\n", types;
        if(repetition){payload+="        std::shared_ptr<RowType> row;\n";repetition_symbols(*repetition,false);}
        if(hierarchy_view_row)payload+="        std::shared_ptr<const lvui::native::HierarchyRowContext> hierarchy_view_row;\n";
        if(list_view_row)payload+="        std::shared_ptr<const lvui::native::ListRowContext> list_view_row;\n";
        if(list_item_view)payload+="        std::shared_ptr<const detail_list_composite::Context> list_item_view;\n";
        if(table_view)payload+="        std::shared_ptr<const lvui::native::TableViewContext> table_view;\n";
        bool menu_group=false,captured_row=false;
        if(menu_row){const auto kinds=component_kinds({view});
            if(std::ranges::any_of(kinds,[](const auto kind){return kind!="MenuItem"&&kind!="ContextMenuItem"&&kind!="MenuDivider"&&kind!="ContextMenuDivider"&&kind!="if"&&kind!="for";}))
                error(view.position,"A repeated Menu row must expand to items, dividers, conditionals, or repetitions");
            const bool dynamic=std::ranges::any_of(kinds,[](const auto kind){return kind=="if"||kind=="for";});
            captured_row=dynamic&&!(kinds.size()==1&&kinds.front()=="if");
            menu_group=kinds.size()!=1||(dynamic&&kinds.front()=="for");
            if(captured_row)active_menu_capture="scope->menu_pending";}
        if(menu_row)payload+="        std::shared_ptr<lvui_generated::detail_menu::MenuRepeatedRows::Row> menu_row;\n";
        if(captured_menu||captured_row)payload+="        std::shared_ptr<lvui_generated::detail_menu::MenuCapture> menu_pending;\n";
        bool tab_group=false,captured_tab_row=false;
        if(tab_row){const auto kinds=component_kinds({view});
            if(std::ranges::any_of(kinds,[](const auto kind){return kind!="Tab"&&kind!="if"&&kind!="for";}))
                error(view.position,"A repeated TabBar row must expand to tabs, conditionals, or repetitions");
            const bool dynamic=std::ranges::any_of(kinds,[](const auto kind){return kind=="if"||kind=="for";});
            captured_tab_row=dynamic&&!(kinds.size()==1&&kinds.front()=="if");
            tab_group=kinds.size()!=1||(dynamic&&kinds.front()=="for");
            if(captured_tab_row)active_tabbar_capture="scope->tab_pending";}
        if(tab_row)payload+="        std::shared_ptr<lvui_generated::detail_tab::TabRepeatedRows::Row> tab_row;\n";
        if(captured_tab||captured_tab_row)payload+="        std::shared_ptr<lvui_generated::detail_tab::TabCapture> tab_pending;\n";
        declarations(view.imports, view.bindings, payload, types);
        auto child = view;
        child.imports.clear(); child.bindings.clear();
        if((repetition&&!menu_row&&!tab_row)||hierarchy_view_row||list_view_row||list_item_view||table_view)require_single_view(child,hierarchy_view_row?"A hierarchy row View":list_view_row?"A list row View":list_item_view?"A list item View":table_view?"A table cell View":"A repeated row");
        std::string group_setup;
        if(menu_group){
            if(next>=4096)error(view.position,"Native widget limit is 4096");
            const auto group_id=id_prefix+"@lvui/menu-group/"+std::to_string(next++);
            if(!ids.insert(group_id).second)error(view.position,"Duplicate menu group id");
            group_setup="            auto menu_group = widgets.vstack(native_parent, "+literal(group_id)+", {.alignment = ii::gui::StackAlignment::stretch});\n"
                        "            if (!menu_group) return std::unexpected(menu_group.error());\n"
                        "            if (auto collapsed = menu_group->set_collapse_when_empty(true); !collapsed) return collapsed;\n"
                        "            if (auto grouped = scope->menu_row->add_group(*menu_group); !grouped) return grouped;\n";
        }
        if(tab_group){
            if(next>=4096)error(view.position,"Native widget limit is 4096");
            const auto group_id=id_prefix+"@lvui/tab-group/"+std::to_string(next++);
            if(!ids.insert(group_id).second)error(view.position,"Duplicate tab group id");
            group_setup="            auto tab_group = widgets.hstack(native_parent, "+literal(group_id)+", {.alignment = ii::gui::StackAlignment::center});\n"
                        "            if (!tab_group) return std::unexpected(tab_group.error());\n"
                        "            if (auto collapsed = tab_group->set_collapse_when_empty(true); !collapsed) return collapsed;\n"
                        "            if (auto grouped = scope->tab_row->add_group(*tab_group); !grouped) return grouped;\n";
        }
        if(window) *window=prepare_window(child);
        else emit(child, menu_group?"*menu_group":tab_group?"*tab_group":"native_parent", menu_parent);
        emit_context_bindings("in the local component scope");
        const bool window_events=window&&(window->opened!="{}"||window->closed!="{}");
        const bool window_close=window&&window->close_requested!="{}";
        const std::string scene_scope_setup = scene_lifecycle_runtime ? (window_events ?
            "            scope->scene_effects = std::make_shared<lvui::native::SceneEffectGroup>();\n" :
            "            scope->scene_effects = scope->parent->scene_effects;\n") : "";
        const std::string window_lifecycle_setup = window_events ?
            "            auto scene_lifecycle = std::make_shared<lvui::native::SceneLifecycle>();\n"
            "            if (auto retiring = widgets.on_retire([scene_lifecycle, effects = scope->scene_effects, report = scope->parent->scene_failure_handler] { if (auto retired = effects->retire(); !retired && report) report(retired.error()); if (auto closed = scene_lifecycle->closed(); !closed && report) report(closed.error()); }); !retiring) return retiring;\n" : "";
        const std::string window_lifecycle_publish = window_events ?
            "            if (auto configured = scene_lifecycle->configure("+window->opened+", "+window->closed+"); !configured) return configured;\n"
            "            if (auto publishing = widgets.on_publish([scene_lifecycle, report = scope->parent->scene_failure_handler] { if (auto opened = scene_lifecycle->opened(); !opened && report) report(opened.error()); }); !publishing) return publishing;\n" : "";
        const std::string window_close_setup = window_close ?
            "            if (!scene_close_guard) return ii::core::fail(ii::core::ErrorCode::invalid_argument, \"Window close policy is missing\");\n"
            "            if (auto owned = widgets.retain(scene_close_guard); !owned) return owned;\n"
            "            if (auto retiring = widgets.on_retire([guard = scene_close_guard] { (void)guard->retire(); }); !retiring) return retiring;\n" : "";
        const std::string window_close_publish = window_close ?
            "            if (auto configured = scene_close_guard->configure("+window->close_requested+"); !configured) return configured;\n"
            "            if (auto publishing = widgets.on_publish([guard = scene_close_guard, report = scope->parent->scene_failure_handler] { if (auto opened = guard->publish(); !opened && report) report(opened.error()); }); !publishing) return publishing;\n" : "";
        std::string menu_collectors;
        if(captured_menu||captured_row)menu_collectors="            [[maybe_unused]] auto capture_menu_item = [pending = scope->menu_pending](lvui_generated::detail_menu::MenuItemPort item) -> ii::core::Result<void> { return pending->append(std::move(item)); };\n            [[maybe_unused]] auto capture_menu_divider = [pending = scope->menu_pending](ii::gui::DividerGroupPort divider) -> ii::core::Result<void> { auto item = lvui_generated::detail_menu::NativeMenu::divider_port(std::move(divider)); if (!item) return std::unexpected(item.error()); return pending->append(std::move(*item)); };\n";
        else if(menu_row||borrowed_menu_row){const auto row=menu_row?"scope->menu_row":active_menu_row;
            menu_collectors="            [[maybe_unused]] auto capture_menu_item = [row = "+row+"](lvui_generated::detail_menu::MenuItemPort item) -> ii::core::Result<void> { return row->append(std::move(item)); };\n            [[maybe_unused]] auto capture_menu_divider = [row = "+row+"](ii::gui::DividerGroupPort divider) -> ii::core::Result<void> { auto item = lvui_generated::detail_menu::NativeMenu::divider_port(std::move(divider)); if (!item) return std::unexpected(item.error()); return row->append(std::move(*item)); };\n";
        }
        std::string tab_collectors;
        if(captured_tab||captured_tab_row)tab_collectors="            [[maybe_unused]] auto capture_tab = [pending = scope->tab_pending](lvui_generated::detail_tab::TabGroupPort item) -> ii::core::Result<void> { return pending->append(std::move(item)); };\n";
        else if(tab_row||borrowed_tab_row){const auto row=tab_row?"scope->tab_row":active_tabbar_row;
            tab_collectors="            [[maybe_unused]] auto capture_tab = [row = "+row+"](lvui_generated::detail_tab::TabGroupPort item) -> ii::core::Result<void> { return row->append(std::move(item)); };\n";}
        nested_views += "    template<class ParentScope"+std::string(repetition?", class RowType":"")+"> struct " + name + " final {\n" + types + scope_declaration(payload, true) + value_initialization() +
            "        static ii::core::Result<void> build(ii::gui::WidgetBuilder& widgets, ii::gui::Widget native_parent, std::shared_ptr<ParentScope> parent"+std::string(repetition?", std::shared_ptr<RowType> row":"")+std::string(menu_row?", std::shared_ptr<lvui_generated::detail_menu::MenuRepeatedRows::Row> menu_row":"")+std::string(captured_menu?", std::shared_ptr<lvui_generated::detail_menu::MenuCapture> menu_pending":"")+std::string(hierarchy_view_row?", std::shared_ptr<const lvui::native::HierarchyRowContext> hierarchy_view_row":"")+std::string(list_view_row?", std::shared_ptr<const lvui::native::ListRowContext> list_view_row":"")+std::string(list_item_view?", std::shared_ptr<const detail_list_composite::Context> list_item_view":"")+std::string(table_view?", std::shared_ptr<const lvui::native::TableViewContext> table_view":"")+std::string(tab_row?", std::shared_ptr<lvui_generated::detail_tab::TabRepeatedRows::Row> tab_row":"")+std::string(captured_tab?", std::shared_ptr<lvui_generated::detail_tab::TabCapture> tab_pending":"")+std::string(window_close?", std::shared_ptr<lvui::native::WindowCloseGuard> scene_close_guard":"")+") {\n            auto scope = std::make_shared<Scope>(parent->backend_factories);\n            scope->parent = std::move(parent);\n"+
            std::string(("            scope->radio_groups = scope->parent->radio_groups;\n            scope->navigation_context = scope->parent->navigation_context;\n            scope->binding_diagnostics = scope->parent->binding_diagnostics;\n"))+
            std::string(scene_registry_runtime?"            scope->scene_registry = scope->parent->scene_registry;\n":"")+
            std::string(scoped_image_runtime?"            scope->image_services = scope->parent->image_services;\n":"")+
            std::string(repetition?"            scope->row = std::move(row);\n":"")+std::string(menu_row?"            scope->menu_row = std::move(menu_row);\n":"")+std::string(captured_menu?"            scope->menu_pending = std::move(menu_pending);\n":"")+std::string(captured_row?"            scope->menu_pending = std::make_shared<lvui_generated::detail_menu::MenuCapture>();\n            if (auto retiring = widgets.on_retire([weak = std::weak_ptr(scope->menu_pending)] { if (auto owner = weak.lock()) (void)owner->close(); }); !retiring) return retiring;\n":"")+std::string(hierarchy_view_row?"            scope->hierarchy_view_row = std::move(hierarchy_view_row);\n":"")+std::string(list_view_row?"            scope->list_view_row = std::move(list_view_row);\n":"")+std::string(list_item_view?"            scope->list_item_view = std::move(list_item_view);\n":"")+std::string(table_view?"            scope->table_view = std::move(table_view);\n":"")+std::string(tab_row?"            scope->tab_row = std::move(tab_row);\n":"")+std::string(captured_tab?"            scope->tab_pending = std::move(tab_pending);\n":"")+std::string(captured_tab_row?"            scope->tab_pending = std::make_shared<lvui_generated::detail_tab::TabCapture>();\n            if (auto retiring = widgets.on_retire([weak = std::weak_ptr(scope->tab_pending)] { if (auto owner = weak.lock()) (void)owner->close(); }); !retiring) return retiring;\n":"")+"            if (auto owned = widgets.retain(scope); !owned) return owned;\n" +
            scene_scope_setup + binding_owner(true, window_close_setup + window_lifecycle_setup) + "            scope->native_lifetime = native_bindings;\n            if (auto activated = lvui::native::attach_backend_activation(scope, widgets); !activated) return activated;\n" +
            (native_initializers.empty() ? "" : "            if (auto initialized = initialize_values(scope, native_bindings); !initialized) return initialized;\n") + window_close_publish + window_lifecycle_publish +
            group_setup + menu_collectors + tab_collectors + body + std::string(captured_row?"            if (auto published = scope->menu_pending->publish([weak = std::weak_ptr(scope->menu_row)](std::vector<lvui_generated::detail_menu::MenuItemPort> items) -> ii::core::Result<void> { auto row = weak.lock(); if (!row) return ii::core::fail(ii::core::ErrorCode::closed, \"Repeated menu row retired\"); return row->replace(std::move(items)); }); !published) return published;\n":"") + std::string(captured_tab_row?"            if (auto published = scope->tab_pending->publish([weak = std::weak_ptr(scope->tab_row)](std::vector<lvui_generated::detail_tab::TabGroupPort> items) -> ii::core::Result<void> { auto row = weak.lock(); if (!row) return ii::core::fail(ii::core::ErrorCode::closed, \"Repeated tab row retired\"); return row->replace(std::move(items)); }); !published) return published;\n":"") + "            return {};\n        }\n    };\n";
        constants = std::move(outer_constants); reactive = std::move(outer_reactive);
        native_values = std::move(outer_native_values); backends = std::move(outer_backends);
        writable = std::move(outer_writable);
        fixed_values = std::move(outer_fixed_values);
        backend_access = std::move(outer_access); binding_kinds = std::move(outer_kinds);
        backend_types = std::move(outer_backend_types);
        blueprint_types = std::move(outer_blueprints);
        view_values = std::move(outer_views);
        --lexical_depth;
        variable_types = std::move(outer_types); variable_sources = std::move(outer_sources);
        native_initializers = std::move(outer_initializers); native_scope_parts = std::move(outer_parts);
        native_style_slots=std::move(outer_style_slots);active_segmented_style=outer_segmented_style;
        active_tabbar_registration=outer_tabbar_registration;
        active_tabbar_section=outer_tabbar_section;active_tabbar_row=outer_tabbar_row;active_tabbar_capture=outer_tabbar_capture;
        active_window_accent=outer_window_accent;
        active_menu_registration=outer_menu;active_menu_divider_registration=outer_divider;active_menu_section=outer_section;active_menu_row=outer_menu_row;active_menu_capture=outer_menu_capture;
        native_scope_base = std::move(outer_base); native_scope_next = outer_scope_next;
        scope_constructor = std::move(outer_constructor);
        widget_variables = std::move(outer_variables); context_bindings = std::move(outer_context);
        storage_names = std::move(outer_storage); next_backend = outer_backend_next;
        body = std::move(outer_body);
        if(!repetition&&!hierarchy_view_row&&!list_view_row&&!list_item_view&&!table_view)body += "            if (auto local = " + name + "<Scope>::build(widgets, " + parent + ", scope"+(captured_menu?", "+outer_menu_capture:"")+(captured_tab?", "+outer_tabbar_capture:"")+(window_close?", scene_close_guard":"")+ "); !local) return local;\n";
        return name;
    }


    std::string collection_source(const Value& source,SourcePosition at,bool initial=false)const{
        return expression_code(source,at,initial);
    }
    void emit_repetition(const View& view,const std::string& parent,const Inputs* menu_parent){
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
        const bool menu_row=!active_menu_section.empty();
        const bool tab_row=!active_tabbar_section.empty();
        if(menu_row){
            const auto& child=view.children.front();
            const bool direct=child.component&&(child.component->spelling=="MenuItem"||child.component->spelling=="ContextMenuItem"||
               child.component->spelling=="MenuDivider"||child.component->spelling=="ContextMenuDivider");
            if((!child.condition&&!child.repetition&&child.custom_component.empty()&&!direct)||child.fragment)
                error(child.position,"A repeated Menu row requires items, dividers, or a conditional");
            for(const auto& [name,input]:values)if(name!="id"&&name!="first"&&name!="count")
                error(input->position,"A repeated Menu uses its parent's layout and accepts id, first and count only");
        }
        if(tab_row){
            const auto& child=view.children.front();
            const bool direct=child.component&&child.component->spelling=="Tab";
            if((!child.condition&&!child.repetition&&child.custom_component.empty()&&!direct)||child.fragment)
                error(child.position,"A repeated TabBar row requires tabs or a conditional");
            for(const auto& [name,input]:values)if(name!="id"&&name!="first"&&name!="count")
                error(input->position,"A repeated TabBar uses its parent's layout and accepts id, first and count only");
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
        const auto spacing=tab_row?(menu_parent&&menu_parent->contains("spacing")&&scoped_expression(menu_parent->at("spacing")->value)?
            "lvui::native::number(lvui::native::range("+expression_code(menu_parent->at("spacing")->value,menu_parent->at("spacing")->position)+", 0, 32767))":
            decimal(menu_parent?numeric(*menu_parent,"spacing",4,0,32767):4)):
            menu_row?(menu_parent&&menu_parent->contains("itemSpacing")&&scoped_expression(menu_parent->at("itemSpacing")->value)?
            "lvui::native::number(lvui::native::range("+expression_code(menu_parent->at("itemSpacing")->value,menu_parent->at("itemSpacing")->position)+", 0, 8192))":
            decimal(menu_parent?numeric(*menu_parent,"itemSpacing",2,0,8192):2)):
            (values.contains("spacing")&&scoped_expression(values.at("spacing")->value)?
            "lvui::native::number("+expression_code(values.at("spacing")->value,values.at("spacing")->position)+")":decimal(numeric(values,"spacing",0,0,32767)));
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
        const auto row_view=emit_scoped(view.children[0],"{}",(menu_row||tab_row)?menu_parent:nullptr,&repetition,menu_row,ScopedRowContext::none,tab_row);
        const auto row_ambient=std::move(ambient_reads.back().expressions);ambient_reads.pop_back();
        --factory_depth;
        ids=std::move(outer_ids);
        if(next>=4096)error(view.position,"Native widget limit is 4096");
        const auto index=next++;
        const auto variable="node_"+std::to_string(index);
        const auto id=text(values,"id","@lvui/repeat/"+std::to_string(index));
        if(id.empty()||!ids.insert(id).second)error(view.position,"Duplicate or empty repeated View id");
        widget_variables.emplace(id,variable);
        if(menu_row){
            const auto owner=active_menu_section.substr(0,active_menu_section.rfind("->register_section"));
            body+="            auto "+variable+" = widgets.content_host("+parent+", "+literal(id)+", {});\n            if (!"+variable+") return std::unexpected("+variable+".error());\n";
            if(!active_menu_capture.empty())body+="            if (auto collapsed = "+variable+"->set_collapse_when_empty(true); !collapsed) return collapsed;\n            auto menu_section_"+variable+" = "+active_menu_capture+"->slot();\n";
            else body+="            auto menu_section_"+variable+" = "+active_menu_section+"(*"+variable+");\n";
            body+="            if (!menu_section_"+variable+") return std::unexpected(menu_section_"+variable+".error());\n";
            body+="            { using Collection = lvui::native::KeyedRows<lvui::native::collection_item_t<decltype("+initial_source+")>>;\n              using MenuRows = lvui_generated::detail_menu::MenuRepeatedRows;\n              auto rows = std::make_shared<Collection>(); auto menu_rows = std::make_shared<MenuRows>(*menu_section_"+variable+");\n              if (auto owned = widgets.retain(rows); !owned) return owned;\n              if (auto owned = widgets.retain(menu_rows); !owned) return owned;\n              if (auto retiring = widgets.on_retire([weak = std::weak_ptr(rows), weak_menu = std::weak_ptr(menu_rows)] { if (auto owner = weak.lock()) (void)owner->close(); if (auto owner = weak_menu.lock()) (void)owner->close(); }); !retiring) return retiring;\n              auto target = *"+variable+"; auto parent_menu = *"+owner+";\n";
            body+="              auto binding = native_bindings->bind([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return lvui::native::collection_frame("+source+", [&](const auto& row_value, std::size_t row_index) { return "+key+"; }, {.first = "+first+", .count = "+count+", .spacing = "+spacing+"}); },\n";
            body+="                [weak_scope = std::weak_ptr(scope), weak_activation = std::weak_ptr(native_bindings), rows, menu_rows, target, parent_menu](const auto& frame) -> ii::core::Result<void> {\n                  auto scope = weak_scope.lock(); auto activation = weak_activation.lock();\n                  if (!scope || !activation || !activation->available() || !scope->active()) return ii::core::fail(ii::core::ErrorCode::closed, \"Repeated menu owner retired\");\n                  auto spacing_matches = [&]() -> ii::core::Result<void> { auto options = parent_menu.options(); if (!options) return std::unexpected(options.error()); if (options->configuration.item_spacing != frame.options.spacing) return ii::core::fail(ii::core::ErrorCode::cancelled, \"Repeated menu spacing was not accepted by its parent\"); return {}; };\n                  if (auto matched = spacing_matches(); !matched) return matched;\n                  return rows->apply(frame, [&](const auto& mounted) -> ii::core::Result<void> {\n                    std::vector<MenuRows::Mount> mounts; std::vector<ii::gui::KeyedContentItem> items; mounts.reserve(mounted.size()); items.reserve(mounted.size());\n                    for (const auto& item : mounted) {\n                      auto menu_row = menu_rows->row(item.key); if (!menu_row) return std::unexpected(menu_row.error()); mounts.push_back({item.key, *menu_row});\n                      items.push_back({item.key, [weak_scope, weak_activation, row = item.row, menu_row = *menu_row](ii::gui::WidgetBuilder& widgets) -> ii::core::Result<void> {\n                        auto scope = weak_scope.lock(); auto activation = weak_activation.lock();\n                        if (!scope || !activation || !activation->available() || !scope->active()) return ii::core::fail(ii::core::ErrorCode::closed, \"Repeated menu row owner retired\");\n                        return "+row_view+"<Scope, typename Collection::Row>::build(widgets, {}, scope, row, menu_row);\n                      }});\n                    }\n                    return target.set_keyed_content(std::move(items), {.layout = {.spacing = frame.options.spacing, .alignment = ii::gui::StackAlignment::stretch}}, [&]() -> ii::core::Result<void> {\n                      if (auto matched = spacing_matches(); !matched) return matched;\n                      return menu_rows->publish(std::move(mounts));\n                    });\n                  });\n                });\n              if (!binding) return std::unexpected(binding.error()); }\n";
            return;
        }
        if(tab_row){
            const auto owner=active_tabbar_section.substr(0,active_tabbar_section.rfind("->register_section"));
            body+="            auto "+variable+" = widgets.content_host("+parent+", "+literal(id)+", {});\n            if (!"+variable+") return std::unexpected("+variable+".error());\n";
            if(!active_tabbar_capture.empty())body+="            if (auto collapsed = "+variable+"->set_collapse_when_empty(true); !collapsed) return collapsed;\n            auto tab_section_"+variable+" = "+active_tabbar_capture+"->slot();\n";
            else body+="            auto tab_section_"+variable+" = "+active_tabbar_section+"(*"+variable+");\n";
            body+="            if (!tab_section_"+variable+") return std::unexpected(tab_section_"+variable+".error());\n";
            body+="            { using Collection = lvui::native::KeyedRows<lvui::native::collection_item_t<decltype("+initial_source+")>>;\n              using TabRows = lvui_generated::detail_tab::TabRepeatedRows;\n              auto rows = std::make_shared<Collection>(); auto tab_rows = std::make_shared<TabRows>(*tab_section_"+variable+");\n              if (auto owned = widgets.retain(rows); !owned) return owned;\n              if (auto owned = widgets.retain(tab_rows); !owned) return owned;\n              if (auto retiring = widgets.on_retire([weak = std::weak_ptr(rows), weak_tab = std::weak_ptr(tab_rows)] { if (auto owner = weak.lock()) (void)owner->close(); if (auto owner = weak_tab.lock()) (void)owner->close(); }); !retiring) return retiring;\n              auto target = *"+variable+"; auto parent_tab = *"+owner+";\n";
            body+="              auto binding = native_bindings->bind([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return lvui::native::collection_frame("+source+", [&](const auto& row_value, std::size_t row_index) { return "+key+"; }, {.first = "+first+", .count = "+count+", .spacing = "+spacing+"}); },\n";
            body+="                [weak_scope = std::weak_ptr(scope), weak_activation = std::weak_ptr(native_bindings), rows, tab_rows, target, parent_tab](const auto& frame) -> ii::core::Result<void> {\n                  auto scope = weak_scope.lock(); auto activation = weak_activation.lock();\n                  if (!scope || !activation || !activation->available() || !scope->active()) return ii::core::fail(ii::core::ErrorCode::closed, \"Repeated tab owner retired\");\n                  auto spacing_matches = [&]() -> ii::core::Result<void> { auto options = parent_tab.options(); if (!options) return std::unexpected(options.error()); if (options->spacing != frame.options.spacing) return ii::core::fail(ii::core::ErrorCode::cancelled, \"Repeated tab spacing was not accepted by its parent\"); return {}; };\n                  if (auto matched = spacing_matches(); !matched) return matched;\n                  return rows->apply(frame, [&](const auto& mounted) -> ii::core::Result<void> {\n                    std::vector<TabRows::Mount> mounts; std::vector<ii::gui::KeyedContentItem> items; mounts.reserve(mounted.size()); items.reserve(mounted.size());\n                    for (const auto& item : mounted) {\n                      auto tab_row = tab_rows->row(item.key); if (!tab_row) return std::unexpected(tab_row.error()); mounts.push_back({item.key, *tab_row});\n                      items.push_back({item.key, [weak_scope, weak_activation, row = item.row, tab_row = *tab_row](ii::gui::WidgetBuilder& widgets) -> ii::core::Result<void> {\n                        auto scope = weak_scope.lock(); auto activation = weak_activation.lock();\n                        if (!scope || !activation || !activation->available() || !scope->active()) return ii::core::fail(ii::core::ErrorCode::closed, \"Repeated tab row owner retired\");\n                        return "+row_view+"<Scope, typename Collection::Row>::build(widgets, {}, scope, row, tab_row);\n                      }});\n                    }\n                    return target.set_keyed_content(std::move(items), {.horizontal = true, .layout = {.spacing = frame.options.spacing, .alignment = ii::gui::StackAlignment::center}}, [&]() -> ii::core::Result<void> {\n                      if (auto matched = spacing_matches(); !matched) return matched;\n                      return tab_rows->publish(std::move(mounts));\n                    });\n                  });\n                });\n              if (!binding) return std::unexpected(binding.error()); }\n";
            return;
        }
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
            body+="            { auto target = *"+variable+"; auto scroll_target = *"+variable+"_viewport;\n              auto viewport = std::make_shared<lvui::native::VirtualViewportBinding<ii::gui::BasicVirtualViewport>>();\n              if (auto owned = widgets.retain(viewport); !owned) return owned;\n              if (auto retiring = widgets.on_retire([weak = std::weak_ptr(viewport)] { if (auto owner = weak.lock()) owner->close(); }); !retiring) return retiring;\n";
            body+="              auto geometry = native_bindings->bind([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return ii::gui::Size{"+width+", "+height+"}; }, [scroll_target](ii::gui::Size size) { return scroll_target.set_extent(size); });\n              if (!geometry) return std::unexpected(geometry.error());\n";
            body+="              auto flex = native_bindings->bind([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+flex+"; }, [scroll_target](double value) { return scroll_target.set_flex(value); });\n              if (!flex) return std::unexpected(flex.error());\n";
            body+="              using Collection = lvui::native::KeyedRows<lvui::native::collection_item_t<decltype("+initial_source+")>>;\n              auto rows = std::make_shared<Collection>();\n              if (auto owned = widgets.retain(rows); !owned) return owned;\n              if (auto retiring = widgets.on_retire([weak = std::weak_ptr(rows)] { if (auto owner = weak.lock()) (void)owner->close(); }); !retiring) return retiring;\n";
            body+="              using Planner = lvui::native::VirtualCollectionPlannerBinding<ii::gui::BasicVirtualCollectionPlanner>;\n              using Measurements = lvui::native::VirtualMeasurementsBinding<ii::gui::BasicVirtualMeasurements, lvui::native::collection_item_t<decltype("+initial_source+")>, Planner>;\n              auto measurements = std::make_shared<Measurements>();\n              if (auto owned = widgets.retain(measurements); !owned) return owned;\n              if (auto retiring = widgets.on_retire([weak = std::weak_ptr(measurements)] { if (auto owner = weak.lock()) owner->close(); }); !retiring) return retiring;\n";
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
                body+="              using Navigation = std::remove_reference_t<decltype("+access.object+")>;\n              using WidgetNavigation = lvui::widgets::CollectionWidgetNavigationBinding<ii::gui::BasicCollectionWidgetNavigation, lvui::native::collection_item_t<decltype("+initial_source+")>, Navigation, lvui::native::VirtualViewportBinding<ii::gui::BasicVirtualViewport>>;\n";
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
    std::string scoped_view_factory(const View& branch,std::string_view owner="Conditional",const Inputs* menu_parent=nullptr,bool menu_section=false,bool grouped_menu=false) {
        const bool connection_only=owner=="Conditional"&&!menu_section&&!grouped_menu&&component_kinds({branch}).empty();
        if(!grouped_menu&&!connection_only)require_single_view(branch,owner=="Conditional"?"A conditional branch":"A hierarchy count View");
        auto outer_body = std::exchange(body, {});
        auto outer_ids = std::exchange(ids, {});
        auto outer_variables = std::exchange(widget_variables, {});
        auto outer_context = std::exchange(context_bindings, {});
        const auto outer_menu=active_menu_registration,outer_divider=active_menu_divider_registration,outer_section=active_menu_section,outer_capture=active_menu_capture;
        if(menu_section){active_menu_registration="capture_menu_item";active_menu_divider_registration="capture_menu_divider";active_menu_capture="pending";}
        std::string group_setup;
        if(grouped_menu){
            if(next>=4096)error(branch.position,"Native widget limit is 4096");
            const auto group_id=id_prefix+"@lvui/menu-group/"+std::to_string(next++);
            if(!ids.insert(group_id).second)error(branch.position,"Duplicate menu group id");
            group_setup="            auto menu_group = widgets.vstack({}, "+literal(group_id)+", {.alignment = ii::gui::StackAlignment::stretch});\n"
                        "            if (!menu_group) return std::unexpected(menu_group.error());\n"
                        "            if (auto collapsed = menu_group->set_collapse_when_empty(true); !collapsed) return collapsed;\n"
                        "            if (auto grouped = pending->add_group(*menu_group); !grouped) return grouped;\n";
        }
        if(connection_only){
            if(next>=4096)error(branch.position,"Native widget limit is 4096");
            const auto group_id=id_prefix+"@lvui/query-group/"+std::to_string(next++);
            if(!ids.insert(group_id).second)error(branch.position,"Duplicate query group id");
            group_setup="            auto query_group = widgets.absolute_stack({}, "+literal(group_id)+", {});\n"
                        "            if (!query_group) return std::unexpected(query_group.error());\n"
                        "            if (auto hidden = query_group->set_visible(false); !hidden) return hidden;\n";
        }
        ++factory_depth;
        emit(branch, grouped_menu?"*menu_group":connection_only?"*query_group":"{}",menu_parent);
        emit_context_bindings("in conditional content");
        auto content = std::move(body);
        --factory_depth;
        active_menu_registration=outer_menu;active_menu_divider_registration=outer_divider;active_menu_section=outer_section;active_menu_capture=outer_capture;
        body = std::move(outer_body); ids = std::move(outer_ids);
        widget_variables = std::move(outer_variables); context_bindings = std::move(outer_context);
        return std::string("ii::gui::WidgetFactory{[weak_scope = std::weak_ptr(scope), weak_activation = std::weak_ptr(native_bindings)") + (menu_section?", pending":"") + "](ii::gui::WidgetBuilder& widgets) -> ii::core::Result<void> {\n            auto scope = weak_scope.lock(); auto activation = weak_activation.lock();\n            if (!scope || !activation || !activation->available() || !scope->active()) return ii::core::fail(ii::core::ErrorCode::closed, " +
               literal(std::string(owner)+" owner retired") + ");\n" +
               (menu_section?"            if (auto owned = widgets.retain(pending); !owned) return owned;\n            if (auto retiring = widgets.on_retire([weak = std::weak_ptr(pending)] { if (auto owner = weak.lock()) (void)owner->close(); }); !retiring) return retiring;\n":"") + group_setup +
               (menu_section?"            [[maybe_unused]] auto capture_menu_item = [pending](lvui_generated::detail_menu::MenuItemPort item) -> ii::core::Result<void> { return pending->append(std::move(item)); };\n            [[maybe_unused]] auto capture_menu_divider = [pending](ii::gui::DividerGroupPort divider) -> ii::core::Result<void> { auto item = lvui_generated::detail_menu::NativeMenu::divider_port(std::move(divider)); if (!item) return std::unexpected(item.error()); return pending->append(std::move(*item)); };\n":"") +
               binding_owner() + content + "            return {};\n        }}";
    }
    std::string scoped_tab_factory(const View& branch,const Inputs* tab_parent,bool grouped){
        if(!grouped)require_single_view(branch,"A conditional TabBar branch");
        auto outer_body=std::exchange(body,{});auto outer_ids=std::exchange(ids,{});
        auto outer_variables=std::exchange(widget_variables,{});auto outer_context=std::exchange(context_bindings,{});
        const auto outer_registration=active_tabbar_registration,outer_capture=active_tabbar_capture;
        active_tabbar_registration="capture_tab";active_tabbar_capture="pending";
        std::string group_setup;
        if(grouped){
            if(next>=4096)error(branch.position,"Native widget limit is 4096");
            const auto group_id=id_prefix+"@lvui/tab-group/"+std::to_string(next++);
            if(!ids.insert(group_id).second)error(branch.position,"Duplicate tab group id");
            group_setup="            auto tab_group = widgets.hstack({}, "+literal(group_id)+", {.alignment = ii::gui::StackAlignment::center});\n"
                        "            if (!tab_group) return std::unexpected(tab_group.error());\n"
                        "            if (auto collapsed = tab_group->set_collapse_when_empty(true); !collapsed) return collapsed;\n"
                        "            if (auto grouped = pending->add_group(*tab_group); !grouped) return grouped;\n";
        }
        ++factory_depth;emit(branch,grouped?"*tab_group":"{}",tab_parent);
        emit_context_bindings("in conditional content");
        auto content=std::move(body);--factory_depth;
        active_tabbar_registration=outer_registration;active_tabbar_capture=outer_capture;
        body=std::move(outer_body);ids=std::move(outer_ids);widget_variables=std::move(outer_variables);context_bindings=std::move(outer_context);
        return std::string("ii::gui::WidgetFactory{[weak_scope = std::weak_ptr(scope), weak_activation = std::weak_ptr(native_bindings), pending](ii::gui::WidgetBuilder& widgets) -> ii::core::Result<void> {\n"
            "            auto scope = weak_scope.lock(); auto activation = weak_activation.lock();\n"
            "            if (!scope || !activation || !activation->available() || !scope->active()) return ii::core::fail(ii::core::ErrorCode::closed, \"Conditional TabBar owner retired\");\n"
            "            if (auto owned = widgets.retain(pending); !owned) return owned;\n"
            "            if (auto retiring = widgets.on_retire([weak = std::weak_ptr(pending)] { if (auto owner = weak.lock()) (void)owner->close(); }); !retiring) return retiring;\n") + group_setup +
            "            [[maybe_unused]] auto capture_tab = [pending](lvui_generated::detail_tab::TabGroupPort item) -> ii::core::Result<void> { return pending->append(std::move(item)); };\n" +
            binding_owner()+content+"            return {};\n        }}";
    }
    std::string list_item_view_factory(const Inputs& values,std::string_view name) {
        auto slot=values.find(name);if(slot==values.end())return {};
        const auto& input=*slot->second;
        ComponentParameter parameter{.name=std::string(name),.type="View",.position=input.position};
        auto content=component_argument(parameter,input.value,input.position).view;
        auto scoped_content=std::make_shared<ViewDefinition>(*content);auto& env=scoped_content->environment;
        if(env.depth>lexical_depth+1)error(input.position,"A list item View escaped its lexical owner");
        for(auto depth=env.depth;depth<lexical_depth+1;++depth){
            for(auto& [key,access]:env.access){borrow_path(access.object);borrow_path(access.revision);borrow_path(access.dependency);}
            for(auto& [key,access]:env.reactive)borrow_path(access);
            for(auto& [key,access]:env.native)borrow_path(access);
        }
        const std::string item="listItem";env.depth=lexical_depth+1;
        env.constants.erase(item);env.reactive.erase(item);env.native.erase(item);env.fixed.erase(item);
        env.blueprints.erase(item);env.kinds.erase(item);env.views.erase(item);
        env.backends[item]=0;env.access[item]={"lvui::native::initial(*scope->list_item_view)",{},"*scope->list_item_view"};
        env.backend_types[item]={};content=std::move(scoped_content);
        const auto key="_lvui_list_item_view_"+std::to_string(next_argument++);
        view_values.emplace(key,std::move(content));
        View invocation;invocation.custom_component=key;invocation.position=input.position;
        const auto scoped=emit_scoped(invocation,"{}",nullptr,nullptr,false,ScopedRowContext::list_item);
        view_values.erase(key);native_runtime=true;needs_parent_scope=true;
        return "detail_list_composite::ViewFactory{[weak_scope = std::weak_ptr(scope), weak_activation = std::weak_ptr(native_bindings)](ii::gui::WidgetBuilder& widgets, std::shared_ptr<const detail_list_composite::Context> list_item_view) -> ii::core::Result<void> { auto scope = weak_scope.lock(); auto activation = weak_activation.lock(); if (!scope || !activation || !activation->available() || !scope->active()) return ii::core::fail(ii::core::ErrorCode::closed, \"List item View owner retired\"); return "+scoped+"<Scope>::build(widgets, {}, scope, std::move(list_item_view)); }}";
    }
    std::string list_item_view_arguments(const Inputs& values) {
        const auto footer=list_item_view_factory(values,"footerComponent");
        const auto preview=list_item_view_factory(values,"previewComponent");
        const auto leading=list_item_view_factory(values,"leadingComponent");
        const auto trailing=list_item_view_factory(values,"trailingComponent");
        if(footer.empty()&&preview.empty()&&leading.empty()&&trailing.empty())return {};
        std::string result=", detail_list_composite::Views{";bool first=true;
        const auto append=[&](std::string_view name,const std::string& value){if(value.empty())return;
            if(!first)result+=", ";first=false;result+="."+std::string(name)+" = "+value;};
        append("footer_context",footer);append("preview_context",preview);append("leading_context",leading);append("trailing_context",trailing);return result+"}";
    }
    std::string card_view_arguments(const Inputs& values,const View& view) {
        const auto factory=[&](std::string_view name)->std::string{
            const auto found=values.find(name);if(found==values.end())return {};
            const auto& input=*found->second;
            ComponentParameter parameter{.name=std::string(name),.type="View",.position=input.position};
            auto content=component_argument(parameter,input.value,input.position).view;
            const auto key="_lvui_card_view_"+std::to_string(next_argument++);
            view_values.emplace(key,std::move(content));
            View invocation;invocation.custom_component=key;invocation.position=input.position;
            const auto result=scoped_view_factory(invocation,"Card View");
            view_values.erase(key);return result;
        };
        const auto content=view.children.empty()?factory("contentComponent"):scoped_view_factory(view.children.front(),"Card content");
        const auto preview=factory("previewComponent"),icon=factory("iconComponent");
        if(content.empty()&&preview.empty()&&icon.empty())return {};
        std::string result=", lvui::native::NativeCardViewsValue<ii::gui::WidgetFactory>{";bool first=true;
        const auto append=[&](std::string_view name,const std::string& value){if(value.empty())return;
            if(!first)result+=", ";first=false;result+="."+std::string(name)+" = "+value;};
        append("content",content);append("preview",preview);append("icon",icon);return result+"}";
    }
    std::string item_view_argument(const Inputs& values,std::string_view owner,bool per_row,bool row_delegate=false,bool flat=false) {
        const auto slot_name=row_delegate?"itemDelegate":"countView",row_name=row_delegate?"itemRow":"countRow";
        const auto factory_type=flat?"ListRowViewFactoryValue<ii::gui::WidgetBuilder>":row_delegate?"HierarchyRowViewFactoryValue<ii::gui::WidgetBuilder>":"HierarchyCountViewFactoryValue<ii::gui::WidgetBuilder>";
        const auto row_type=flat?"ListRowContext":"HierarchyRowContext",row_field=flat?"list_view_row":"hierarchy_view_row";
        auto slot=values.find(slot_name),row=values.find(row_name);
        if(row!=values.end()&&(!per_row||slot==values.end()))error(row->second->position,std::string(row_name)+(flat?" requires a list row View":" requires a hierarchy View"));
        if(slot==values.end())return {};
        const auto& input=*slot->second;
        ComponentParameter parameter{.name=slot_name,.type="View",.position=input.position};
        auto content=component_argument(parameter,input.value,input.position).view;
        const auto key="_lvui_count_view_"+std::to_string(next_argument++);
        if(row!=values.end()){
            if(row->second->value.kind!=ValueKind::string)error(row->second->position,std::string(row_name)+" requires a literal row identifier");
            const auto name=text(values,row_name);
            if(!identifier(name))error(row->second->position,std::string(row_name)+" requires a valid identifier");
            auto scoped=std::make_shared<ViewDefinition>(*content);auto& env=scoped->environment;
            if(env.depth>lexical_depth+1)error(input.position,flat?"A list row View escaped its lexical owner":"A hierarchy row View escaped its lexical owner");
            for(auto depth=env.depth;depth<lexical_depth+1;++depth){
                for(auto& [key,access]:env.access){borrow_path(access.object);borrow_path(access.revision);borrow_path(access.dependency);}
                for(auto& [key,access]:env.reactive)borrow_path(access);
                for(auto& [key,access]:env.native)borrow_path(access);
            }
            env.depth=lexical_depth+1;env.constants.erase(name);env.reactive.erase(name);env.native.erase(name);
            env.fixed.erase(name);env.blueprints.erase(name);env.kinds.erase(name);env.views.erase(name);
            env.backends[name]=0;env.access[name]={"lvui::native::initial(*scope->"+std::string(row_field)+")",{},"*scope->"+std::string(row_field)};
            env.backend_types[name]={};content=std::move(scoped);
        }
        view_values.emplace(key,std::move(content));
        View invocation;invocation.custom_component=key;invocation.position=input.position;
        if(row!=values.end()){
            const auto scoped=emit_scoped(invocation,"{}",nullptr,nullptr,false,flat?ScopedRowContext::list:ScopedRowContext::hierarchy);
            view_values.erase(key);native_runtime=true;needs_parent_scope=true;
            return ", lvui::native::"+std::string(factory_type)+"{[weak_scope = std::weak_ptr(scope), weak_activation = std::weak_ptr(native_bindings)](ii::gui::WidgetBuilder& widgets, std::shared_ptr<const lvui::native::"+row_type+"> "+row_field+") -> ii::core::Result<void> { auto scope = weak_scope.lock(); auto activation = weak_activation.lock(); if (!scope || !activation || !activation->available() || !scope->active()) return ii::core::fail(ii::core::ErrorCode::closed, "+literal(std::string(owner)+" owner retired")+"); return "+scoped+"<Scope>::build(widgets, {}, scope, std::move("+row_field+")); }}";
        }
        const auto factory=scoped_view_factory(invocation,owner);
        view_values.erase(key);native_runtime=true;needs_parent_scope=true;
        if(!per_row)return ", "+factory;
        return ", lvui::native::"+std::string(factory_type)+"{[view = "+factory+"](ii::gui::WidgetBuilder& widgets, std::shared_ptr<const lvui::native::"+row_type+">) mutable { return view(widgets); }}";
    }
    std::string table_view_arguments(const Inputs& values,bool standalone_header=false) {
        const auto factory=[&](bool header,const Input* specific=nullptr,bool allow_absent=false)->std::string{
            const auto primary=header?"headerDelegate":"cellDelegate",alias=header?"headerCellDelegate":"delegate";
            const auto context_name=header?"headerContext":"cellContext";
            auto slot=values.find(primary),other=values.find(alias),context=values.find(context_name);
            if(slot!=values.end()&&other!=values.end())error(other->second->position,"Use one table View delegate alias");
            if(slot==values.end())slot=other;
            if(!specific&&context!=values.end()&&slot==values.end()&&!allow_absent)error(context->second->position,std::string(context_name)+" requires a View delegate");
            if(!specific&&slot==values.end())return {};
            const auto& input=specific?*specific:*slot->second;ComponentParameter parameter{.name=primary,.type="View",.position=input.position};
            auto content=component_argument(parameter,input.value,input.position).view;
            const auto key="_lvui_table_view_"+std::to_string(next_argument++);
            if(context!=values.end()){
                if(context->second->value.kind!=ValueKind::string)error(context->second->position,std::string(context_name)+" requires a literal identifier");
                const auto name=text(values,context_name);if(!identifier(name))error(context->second->position,std::string(context_name)+" requires a valid identifier");
                auto scoped=std::make_shared<ViewDefinition>(*content);auto& env=scoped->environment;
                if(env.depth>lexical_depth+1)error(input.position,"A table View escaped its lexical owner");
                for(auto depth=env.depth;depth<lexical_depth+1;++depth){
                    for(auto& [key,access]:env.access){borrow_path(access.object);borrow_path(access.revision);borrow_path(access.dependency);}
                    for(auto& [key,access]:env.reactive)borrow_path(access);
                    for(auto& [key,access]:env.native)borrow_path(access);
                }
                env.depth=lexical_depth+1;env.constants.erase(name);env.reactive.erase(name);env.native.erase(name);
                env.fixed.erase(name);env.blueprints.erase(name);env.kinds.erase(name);env.views.erase(name);
                env.backends[name]=0;env.access[name]={"lvui::native::initial(*scope->table_view)",{},"*scope->table_view"};
                env.backend_types[name]={};content=std::move(scoped);
            }
            view_values.emplace(key,std::move(content));View invocation;invocation.custom_component=key;invocation.position=input.position;
            if(context!=values.end()){
                const auto scoped=emit_scoped(invocation,"{}",nullptr,nullptr,false,ScopedRowContext::table);
                view_values.erase(key);native_runtime=true;needs_parent_scope=true;
                return "lvui::native::TableViewFactoryValue<ii::gui::WidgetBuilder>{[weak_scope = std::weak_ptr(scope), weak_activation = std::weak_ptr(native_bindings)](ii::gui::WidgetBuilder& widgets, std::shared_ptr<const lvui::native::TableViewContext> table_view) -> ii::core::Result<void> { auto scope = weak_scope.lock(); auto activation = weak_activation.lock(); if (!scope || !activation || !activation->available() || !scope->active()) return ii::core::fail(ii::core::ErrorCode::closed, \"Table View owner retired\"); return "+scoped+"<Scope>::build(widgets, {}, scope, std::move(table_view)); }}";
            }
            const auto view=scoped_view_factory(invocation,"Table View");view_values.erase(key);
            return "lvui::native::TableViewFactoryValue<ii::gui::WidgetBuilder>{[view = "+view+"](ii::gui::WidgetBuilder& widgets, std::shared_ptr<const lvui::native::TableViewContext>) mutable { return view(widgets); }}";
        };
        const bool has_columns=values.contains("columnViews");
        const auto header=factory(true,nullptr,has_columns),cell=standalone_header?std::string{}:factory(false,nullptr,has_columns);
        std::string headers,columns;std::set<std::string> keys;
        if(has_columns){
            const auto& list=values.at("columnViews")->value;
            if(list.kind!=ValueKind::list)error(list.position,"columnViews requires a literal list");
            for(const auto& item:list.elements){
                if(item.kind!=ValueKind::view || !item.view || !item.view->component ||
                   item.view->component->spelling!="TableColumn" || !item.view->children.empty() ||
                   !item.view->bindings.empty() || !item.view->imports.empty())
                    error(item.position,"Each column View requires a TableColumn declaration");
                const Value *key=nullptr,*head=nullptr,*body=nullptr;std::set<std::string> fields;
                for(const auto& field:item.view->inputs){
                    if(field.name.empty()||!fields.insert(field.name).second)
                        error(field.position,"Invalid or duplicate column View field");
                    if(field.name=="key")key=&field.value;
                    else if(field.name=="header")head=&field.value;
                    else if(field.name=="cell"&&!standalone_header)body=&field.value;
                    else error(field.position,"Unknown column View field '"+field.name+"'");
                }
                if(!key||key->kind!=ValueKind::string||key->text.empty()||key->text.size()>1024)
                    error(item.position,"A column View requires a nonempty literal key");
                if(!keys.insert(key->text).second)error(key->position,"Duplicate column View key");
                if(!head&&!body)error(item.position,"A column View requires a header or cell View");
                if(head){Input input{"header",*head,head->position};if(!headers.empty())headers+=", ";headers+="{"+literal(key->text)+", "+factory(true,&input,true)+"}";}
                if(body){Input input{"cell",*body,body->position};if(!columns.empty())columns+=", ";columns+="{"+literal(key->text)+", "+factory(false,&input,true)+"}";}
            }
        }
        if(values.contains("headerContext")&&header.empty()&&headers.empty())error(values.at("headerContext")->position,"headerContext requires a View delegate");
        if(!standalone_header&&values.contains("cellContext")&&cell.empty()&&columns.empty())error(values.at("cellContext")->position,"cellContext requires a View delegate");
        if(header.empty()&&cell.empty()&&headers.empty()&&columns.empty())return {};
        std::string result=standalone_header?", lvui::native::NativeTableViewsValue<ii::gui::WidgetBuilder>{":", {}, lvui::native::NativeTableViewsValue<ii::gui::WidgetBuilder>{";bool first=true;
        if(!header.empty()){result+=".header = "+header;first=false;}
        if(!cell.empty()){if(!first)result+=", ";result+=".cell = "+cell;first=false;}
        if(!headers.empty()){if(!first)result+=", ";result+=".headers = {"+headers+"}";first=false;}
        if(!columns.empty()){if(!first)result+=", ";result+=".columns = {"+columns+"}";}
        return result+"}";
    }
    void emit_menu_conditional(const View& view,const std::string& parent,const Inputs* menu_parent){
        native_runtime=true;needs_parent_scope=true;
        if(next>=4096)error(view.position,"Native widget limit is 4096");
        const auto variable="node_"+std::to_string(next++);
        const auto id=id_prefix+"@lvui/conditional/"+std::to_string(next);
        ids.insert(id);widget_variables.emplace(id,variable);
        std::vector<std::string> factories;std::size_t inspected=0;
        const auto choose=[&](const auto& self,const View& branch,std::size_t depth)->std::string{
            if(depth>=64||++inspected>16384)error(branch.position,"Conditional menu composition limit exceeded");
            if(branch.condition){
                if(branch.fragment||!branch.bindings.empty()||!branch.imports.empty())
                    error(branch.position,"A conditional Menu branch cannot introduce local declarations");
                if(branch.children.empty()||branch.children.size()>2)error(branch.position,"A conditional requires one or two View branches");
                const auto condition=scoped_expression(*branch.condition)?expression_code(*branch.condition,branch.position):[&]{
                    const auto value=resolve(*branch.condition,branch.position);
                    if(value.kind!=ValueKind::boolean)error(branch.position,"A conditional View requires Bool");return value.text;
                }();
                const auto yes=self(self,branch.children[0],depth+1);
                const auto no=branch.children.size()==2?self(self,branch.children[1],depth+1):"-1";
                return "(lvui::native::truth("+condition+") ? "+yes+" : "+no+")";
            }
            const auto kinds=component_kinds({branch});
            if(std::ranges::any_of(kinds,[](const auto kind){return kind!="MenuItem"&&kind!="ContextMenuItem"&&kind!="MenuDivider"&&kind!="ContextMenuDivider"&&kind!="if"&&kind!="for";}))
                error(branch.position,"A conditional Menu branch requires items or dividers");
            const bool grouped=kinds.size()!=1||std::ranges::any_of(kinds,[](const auto kind){return kind=="if"||kind=="for";});
            const auto index=factories.size();factories.push_back(scoped_view_factory(branch,"Conditional",menu_parent,true,grouped));return std::to_string(index);
        };
        const auto selection=choose(choose,view,0);
        std::string cases;
        for(std::size_t i=0;i<factories.size();++i)cases+="case "+std::to_string(i)+": factory = "+factories[i]+"; break;\n";
        const auto section="menu_section_"+variable;
        body+="            auto "+variable+" = widgets.content_host("+parent+", "+literal(id)+", {});\n"
              "            if (!"+variable+") return std::unexpected("+variable+".error());\n";
        if(!active_menu_capture.empty())body+="            if (auto collapsed = "+variable+"->set_collapse_when_empty(true); !collapsed) return collapsed;\n"
              "            auto "+section+" = "+active_menu_capture+"->slot();\n"
              "            if (!"+section+") return std::unexpected("+section+".error());\n";
        else if(active_menu_row.empty())body+="            auto "+section+" = "+active_menu_section+"(*"+variable+");\n"
              "            if (!"+section+") return std::unexpected("+section+".error());\n";
        else body+="            if (auto collapsed = "+variable+"->set_collapse_when_empty(true); !collapsed) return collapsed;\n"
              "            auto "+section+" = lvui_generated::detail_menu::MenuSectionPort{[row = "+active_menu_row+"](std::vector<lvui_generated::detail_menu::MenuItemPort> items) { return row->replace(std::move(items)); }};\n";
        body+="            { auto target = *"+variable+"; auto section = "+std::string(!active_menu_capture.empty()||active_menu_row.empty()?"*":"")+section+"; auto binding = native_bindings->bind([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+selection+"; },\n"
              "                [target, section, weak_scope = std::weak_ptr(scope), weak_activation = std::weak_ptr(native_bindings)](int selected) -> ii::core::Result<void> {\n"
              "                  auto scope = weak_scope.lock(); auto native_bindings = weak_activation.lock();\n"
              "                  if (!scope || !native_bindings || !native_bindings->available() || !scope->active()) return ii::core::fail(ii::core::ErrorCode::closed, \"Conditional menu owner retired\");\n"
              "                  auto pending = std::make_shared<lvui_generated::detail_menu::MenuCapture>(); ii::gui::WidgetFactory factory;\n"
              "                  switch (selected) { "+cases+" default: break; }\n"
              "                  return target.set_content(std::move(factory), [section, pending]() mutable -> ii::core::Result<void> { return pending->publish(section); });\n"
              "                });\n              if (!binding) return std::unexpected(binding.error()); }\n";
    }
    void emit_tab_conditional(const View& view,const std::string& parent,const Inputs* tab_parent){
        native_runtime=true;needs_parent_scope=true;
        if(next>=4096)error(view.position,"Native widget limit is 4096");
        const auto variable="node_"+std::to_string(next++);
        const auto id=id_prefix+"@lvui/conditional/"+std::to_string(next);
        ids.insert(id);widget_variables.emplace(id,variable);
        std::vector<std::string> factories;std::size_t inspected=0;
        const auto choose=[&](const auto& self,const View& branch,std::size_t depth)->std::string{
            if(depth>=64||++inspected>16384)error(branch.position,"Conditional TabBar composition limit exceeded");
            if(branch.condition){
                if(branch.fragment||!branch.bindings.empty()||!branch.imports.empty())
                    error(branch.position,"A conditional TabBar branch cannot introduce local declarations");
                if(branch.children.empty()||branch.children.size()>2)error(branch.position,"A conditional requires one or two View branches");
                const auto condition=scoped_expression(*branch.condition)?expression_code(*branch.condition,branch.position):[&]{
                    const auto value=resolve(*branch.condition,branch.position);
                    if(value.kind!=ValueKind::boolean)error(branch.position,"A conditional View requires Bool");return value.text;
                }();
                const auto yes=self(self,branch.children[0],depth+1);
                const auto no=branch.children.size()==2?self(self,branch.children[1],depth+1):"-1";
                return "(lvui::native::truth("+condition+") ? "+yes+" : "+no+")";
            }
            const auto kinds=component_kinds({branch});
            if(std::ranges::any_of(kinds,[](const auto kind){return kind!="Tab"&&kind!="if"&&kind!="for";}))
                error(branch.position,"A conditional TabBar branch requires tabs");
            const bool grouped=kinds.size()!=1||std::ranges::any_of(kinds,[](const auto kind){return kind=="if"||kind=="for";});
            const auto index=factories.size();factories.push_back(scoped_tab_factory(branch,tab_parent,grouped));return std::to_string(index);
        };
        const auto selection=choose(choose,view,0);
        std::string cases;for(std::size_t i=0;i<factories.size();++i)cases+="case "+std::to_string(i)+": factory = "+factories[i]+"; break;\n";
        const auto section="tab_section_"+variable;
        body+="            auto "+variable+" = widgets.content_host("+parent+", "+literal(id)+", {});\n"
              "            if (!"+variable+") return std::unexpected("+variable+".error());\n";
        if(!active_tabbar_capture.empty())body+="            if (auto collapsed = "+variable+"->set_collapse_when_empty(true); !collapsed) return collapsed;\n"
              "            auto "+section+" = "+active_tabbar_capture+"->slot();\n"
              "            if (!"+section+") return std::unexpected("+section+".error());\n";
        else if(active_tabbar_row.empty())body+="            auto "+section+" = "+active_tabbar_section+"(*"+variable+");\n"
              "            if (!"+section+") return std::unexpected("+section+".error());\n";
        else body+="            if (auto collapsed = "+variable+"->set_collapse_when_empty(true); !collapsed) return collapsed;\n"
              "            auto "+section+" = lvui_generated::detail_tab::TabSectionPort{[row = "+active_tabbar_row+"](std::vector<lvui_generated::detail_tab::TabGroupPort> items) { return row->replace(std::move(items)); }};\n";
        body+="            { auto target = *"+variable+"; auto section = "+std::string(!active_tabbar_capture.empty()||active_tabbar_row.empty()?"*":"")+section+"; auto binding = native_bindings->bind([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+selection+"; },\n"
              "                [target, section, weak_scope = std::weak_ptr(scope), weak_activation = std::weak_ptr(native_bindings)](int selected) -> ii::core::Result<void> {\n"
              "                  auto scope = weak_scope.lock(); auto native_bindings = weak_activation.lock();\n"
              "                  if (!scope || !native_bindings || !native_bindings->available() || !scope->active()) return ii::core::fail(ii::core::ErrorCode::closed, \"Conditional TabBar owner retired\");\n"
              "                  auto pending = std::make_shared<lvui_generated::detail_tab::TabCapture>(); ii::gui::WidgetFactory factory;\n"
              "                  switch (selected) { "+cases+" default: break; }\n"
              "                  return target.set_content(std::move(factory), [section, pending]() mutable -> ii::core::Result<void> { return pending->publish(section); });\n"
              "                });\n              if (!binding) return std::unexpected(binding.error()); }\n";
    }
    void emit_conditional(const View& view, const std::string& parent,const Inputs* menu_parent=nullptr) {
        if(!active_menu_section.empty()||!active_menu_row.empty()||!active_menu_capture.empty()){emit_menu_conditional(view,parent,menu_parent);return;}
        if(!active_tabbar_section.empty()||!active_tabbar_row.empty()||!active_tabbar_capture.empty()){emit_tab_conditional(view,parent,menu_parent);return;}
        native_runtime = true; needs_parent_scope = true;
        if (view.children.empty() || view.children.size() > 2) error(view.position, "A conditional requires one or two View branches");
        if (next >= 4096) error(view.position, "Native widget limit is 4096");
        const auto variable = "node_" + std::to_string(next++);
        const auto id = id_prefix + "@lvui/conditional/" + std::to_string(next);
        ids.insert(id); widget_variables.emplace(id, variable);
        const auto positive = scoped_view_factory(view.children[0]);
        const auto negative = view.children.size() == 2 ? scoped_view_factory(view.children[1]) : "ii::gui::WidgetFactory{}";
        const auto condition = scoped_expression(*view.condition) ? expression_code(*view.condition, view.position) : [&] {
            const auto value = resolve(*view.condition, view.position);
            if (value.kind != ValueKind::boolean) error(view.position, "A conditional View requires Bool");
            return value.text;
        }();
        body += "            auto " + variable + " = widgets.content_host(" + parent + ", " + literal(id) + ", {});\n            if (!" + variable + ") return std::unexpected(" + variable + ".error());\n"
                "            if (auto collapsed = " + variable + "->set_collapse_when_empty(true); !collapsed) return collapsed;\n";
        body += "            { auto target = *" + variable + "; auto binding = native_bindings->bind([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return lvui::native::truth(" + condition + "); }, [target, positive = " + positive + ", negative = " + negative + "](bool selected) -> ii::core::Result<void> { if (selected) return target.set_content(positive); if (negative) return target.set_content(negative); return target.clear_content(); });\n              if (!binding) return std::unexpected(binding.error()); }\n";
    }
    void emit_query_connection(const View& view){
        if(!view.children.empty())error(view.position,"QueryConnection cannot contain child Views");
        const auto values=inputs(view,{"source","query","value","onValue","capacity","deliveryBatch","overflow"});
        for(const auto name:{"source","query","onValue"})if(!values.contains(name))error(view.position,std::string("QueryConnection requires ")+name);
        const auto& source=values.at("source")->value;
        if(source.kind!=ValueKind::reference&&source.kind!=ValueKind::call)error(source.position,"QueryConnection source requires a scoped C++ stream");
        const auto dot=source.text.find('.');
        if(dot==source.text.npos||source.text.find('.',dot+1)!=source.text.npos||!identifier(source.text.substr(dot+1)))
            error(source.position,"QueryConnection source must be a direct public stream member");
        const auto owner=backend_access.find(source.text.substr(0,dot));
        if(owner==backend_access.end())error(source.position,"QueryConnection source requires a scoped C++ owner");
        if(!owner->second.dependency.empty())
            error(source.position,"QueryConnection source requires a stable owner; a replaceable collection item must remount its connection");
        if(source.kind==ValueKind::call&&(!source.elements.empty()||!method_query(source)))
            error(source.position,"QueryConnection stream accessor must take no arguments");
        const auto stream=owner->second.object+"."+source.text.substr(dot+1)+(source.kind==ValueKind::call?"()":"");
        const auto& handler=values.at("onValue")->value;
        std::string callback;
        if(auto named=values.find("value");named!=values.end()){
            if(named->second->value.kind!=ValueKind::string||!identifier(named->second->value.text))
                error(named->second->position,"QueryConnection value requires a literal identifier");
            if(handler.kind!=ValueKind::action)error(named->second->position,"A named query value requires an event block");
            const auto name=named->second->value.text;const auto saved=environment();
            constants.erase(name);reactive.erase(name);native_values.erase(name);fixed_values.erase(name);view_values.erase(name);writable.erase(name);
            backends[name]=0;backend_access[name]={"query_value",{}, {}};backend_types[name]={};blueprint_types.erase(name);binding_kinds.erase(name);
            std::string statements;try{statements=action_block_body(handler);}catch(...){environment(saved);throw;}
            environment(saved);
            callback="lvui::native::callback(scope, native_bindings, []([[maybe_unused]] const auto& scope, [[maybe_unused]] const auto& query_value) { "+statements+"})";
        }else callback=action(values,"onValue","const auto& query_value","query_value",true);
        const auto initial=expression_code(values.at("query")->value,values.at("query")->position,true);
        const auto expression=expression_code(values.at("query")->value,values.at("query")->position,false);
        const auto bounded=[&](std::string_view name){
            const auto value=numeric(values,name,64,1,65536);
            if(std::trunc(value)!=value)error(values.find(name)->second->position,std::string(name)+" must be an integer");
            return std::to_string(static_cast<std::size_t>(value));
        };
        const auto capacity=bounded("capacity"),batch=bounded("deliveryBatch");
        std::string overflow="reject";
        if(auto option=values.find("overflow");option!=values.end()){
            const auto selected=resolve(option->second->value,option->second->position);
            if(selected.kind!=ValueKind::enumeration||(selected.text!=".reject"&&selected.text!=".dropOldest"))
                error(option->second->position,"QueryConnection overflow must be .reject or .dropOldest");
            if(selected.text==".dropOldest")overflow="drop_oldest";
        }
        const auto suffix=std::to_string(next++),registry="query_registry_"+suffix,connection="query_connection_"+suffix,loop="query_loop_"+suffix;
        native_runtime=query_connection_runtime=true;needs_parent_scope=true;
        body+="            auto "+registry+" = scope->backend_factories.query_connections();\n"
              "            if (!"+registry+") return ii::core::fail(ii::core::ErrorCode::invalid_argument, \"QueryConnection requires a host query registry\");\n"
              "            auto "+loop+" = widgets.event_loop(); if (!"+loop+") return std::unexpected("+loop+".error());\n"
              "            if (!"+registry+"->uses("+loop+"->get())) return ii::core::fail(ii::core::ErrorCode::wrong_executor, \"QueryConnection requires its host event loop\");\n"
              "            auto "+connection+"_result = "+registry+"->connect(scope, "+stream+", "+initial+", "+callback+
              ", {.capacity = "+capacity+", .overflow = ii::runtime::OverflowPolicy::"+overflow+", .delivery_batch = "+batch+"});\n"
              "            if (!"+connection+"_result) return std::unexpected("+connection+"_result.error());\n"
              "            auto "+connection+" = *"+connection+"_result;\n"
              "            if (auto retiring = widgets.on_retire(["+registry+", "+connection+"] { (void)"+registry+"->retire("+connection+"); }); !retiring) { (void)"+registry+"->retire("+connection+"); return retiring; }\n"
              "            if (auto publishing = widgets.on_publish(["+connection+"] { "+connection+"->activate(); }); !publishing) return publishing;\n"
              "            { auto binding = native_bindings->bind([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+expression+"; }, ["+connection+"](const auto& selection) { return "+connection+"->select(selection); }); if (!binding) return std::unexpected(binding.error()); }\n";
    }
    void emit(const View& view, const std::string& parent,const Inputs* menu_parent=nullptr, bool figma_normalized=false) {
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
        if (view.condition) { emit_conditional(view, parent,menu_parent); return; }
        if (view.repetition) { emit_repetition(view,parent,menu_parent); return; }
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
        if (!figma_normalized && view.component && !view.fragment && detail::is_figma_component(view.component->spelling)) {
            auto canonical = view;
            if (detail::normalize_figma_component(canonical, [&](const Value& value) {
                    return native_expression(value) || constants.contains(value.text) || script_local(value.text);
                })) {
                emit(canonical, parent, menu_parent, true);
                return;
            }
        }
        if(view.fragment){
            if(!view.inputs.empty())error(view.position,"A View sequence has no root inputs");
            for(const auto& child:view.children)emit(child,parent,menu_parent);
            return;
        }
        const auto kind=view.component->spelling=="ListItemComposite"?std::string_view{"ListItem"}:
            view.component->spelling=="AbstractInputBar"?std::string_view{"InputField"}:view.component->spelling;
        if (kind == "Scene") error(view.position, "Scene requires an Application parent");
        if(kind=="QueryConnection"){emit_query_connection(view);return;}
        const bool stack=kind=="VStack" || kind=="HStack" || kind=="ZStack";
        const bool scroll=kind=="ScrollView";
        const bool icon_button=kind=="IconButton" || kind=="IconMenuButton";
        const bool dropdown=kind=="DropdownButton" || kind=="IconMenuButton" || kind=="LabelMenuButton";
        const bool button=kind=="PushButton" || kind=="LabelButton" || kind=="AbstractButton" || icon_button || dropdown || kind=="HelpButton" || kind=="ListItem";
        const bool material=kind=="MaterialSurface"||kind=="PanelMaterial"||kind=="WindowMaterial";
        const bool code_editor=kind=="CodeEditor",tooltip=kind=="Tooltip",focus_ring=kind=="FocusRing",window_noise=kind=="WindowNoiseTexture",wheel_guard=kind=="WheelScrollGuard";
        const bool mobile_tab=kind=="MobileTab"||kind=="MobileNavigationTab";
        const bool hierarchy_item=kind=="HierarchyItem",hierarchy_list=kind=="HierarchyList";
        const bool media_card=kind=="Card",avatar=kind=="Avatar",image_view=kind=="Image",native_view=kind=="NativeView";
        native_view_runtime|=native_view;
        if(native_view&&!view.children.empty())error(view.position,"NativeView content is built by its C++ source");
        const bool toolbar=kind=="ToolbarButton"||kind=="HierarchyToolbar"||kind=="ListToolbar"||kind=="ListFooter";
        const bool hierarchy_panel=(kind=="Hierarchy"),list_panel=(kind=="List"),card=kind=="AppCard",header=kind=="AppHeader",listener=kind=="EventListener",router=kind=="PageRouter",link=kind=="Link";
        const bool tabbar=kind=="TabBar",tab=kind=="Tab";
        const bool list_selector=kind=="ListItemSelector";
        const bool combo=kind=="ComboBox",stepper=kind=="Stepper",segmented=kind=="LabelSegmentedControl" || kind=="IconSegmentedControl";
        const bool menu=kind=="Menu" || kind=="ContextMenu",menu_item=kind=="MenuItem" || kind=="ContextMenuItem",divider=kind=="MenuDivider" || kind=="ContextMenuDivider";
        const auto submenu_owner=(menu)?active_submenu_registration:std::string{};
        const bool color_surface=kind=="ColorPickerSurface" || kind=="ColorField";
        const bool color_picker=kind=="ColorPicker";
        const bool color_button=kind=="ColorPickerButton";
        const bool popover=kind=="Popover";
        const bool modal=kind=="Modal",sheet=kind=="Sheet";
        const bool table_cell=kind=="TableCellItem",table_row=kind=="TableRow",table_header=kind=="TableHeader",table_view=kind=="Table";
        const bool alert=kind=="Alert",alert_button=kind=="AlertButton";
        const bool check=kind=="CheckBox" || kind=="RadioButton" || kind=="ToggleSwitch";
        Inputs values;
        if(list_selector)values=inputs(view,{"id","options","currentIndex","placeholder","enabled","visible","width","height","accessibilityName","onCurrentIndexChanged"});
        else if(native_view)values=inputs(view,{"id","source","width","height","enabled","clip"});
        else if(scroll)values=inputs(view,{"id","width","height","enabled","showIndicator","horizontal"});
        else if (stack)
            values = inputs(view, {"id", "spacing", "defaultSpacing", "padding", "leftPadding", "topPadding", "rightPadding", "bottomPadding",
                                   "alignment", "alignmentName", "intrinsicWidth", "measureHiddenChildren", "width", "height", "minimumWidth", "minimumHeight",
                                   "backgroundColor", "borderColor", "borderWidth", "cornerRadius"});
        else if(wheel_guard)values=inputs(view,{"id","targetId","width","height","enabled","consumeInside","onWheelRouted"});
        else if(window_noise)values=inputs(view,{"id","width","height","color","primaryColor","noiseStrength","blurRadius","grainStrength"});
        else if(focus_ring)values=inputs(view,{"id","width","height","radius","strokeWidth","color","active","motionEnabled","motion"});
        else if(tooltip)values=inputs(view,{"id","targetId","target","text","anchorX","anchorY","availableRect","preferredPlacement","automatic","scrollContent","motionEnabled","maximumWidth","maximumHeight","contentPadding","edgeMargin","cornerRadius","tailLength","tailWidth","delay","timeout","hideDelay","animationDuration","surfaceColor","borderColor","primaryColor","surfaceOpacity","backdrop","motion","accessibilityName","onOpened","onClosed"});
        else if((code_editor))values=inputs(view,{"id","model","editor","sourceRevision","text","fontFamily","accessibilityName","width","height","editorHeight","insetHorizontal","insetVertical","cornerRadius","fontPixelSize","textLineHeight","tabStopDistance","enabled","readOnly","textColor","backgroundColor","backgroundColorHover","selectionColor","focusColor","caretColor","onDocumentEdited","onSelectionChanged"});
        else if(link)values=inputs(view,{"id","text","href","to","params","router","navigator","targetComponent","replace","enabled","underline","tone","linkColor","hoverColor","pressedColor","disabledColor","horizontalPadding","verticalPadding","minimumHeight","cornerRadius","width","height","showFocusRing","accessibilityName","method","methods","onClick","onPressed","onReleased","onCanceled","onNavigated","onNavigationFailed"},true);
        else if(router){values=inputs(view,{"id","router","width","height"});if(!values.contains("router"))error(view.position,"PageRouter requires a scoped router member");}
        else if((hierarchy_item))values=inputs(view,{"id","text","label","itemKey","itemId","count","countView","enabled","activatable","selectable","selected","expanded","showChevron","hasChildItems","dragPreviewActive","indentLevel","width","itemWidth","height","rowHeight","indentStep","iconSize","chevronSize","baseLeftPadding","rowRightPadding","leadingSpacing","cornerRadius","countWidth","iconName","iconSource","iconRevision","accessibilityName","textColorNormal","textColorDisabled","rowBackgroundColorIdle","rowBackgroundColorHover","rowBackgroundColorPressed","rowBackgroundColorActive","rowBackgroundColorInactive","rowBackgroundColorDrag","focusColor","onClick","onExpansionChanged"},true);
        else if((hierarchy_list))values=inputs(view,{"id","controller","editor","modelRevision","viewRevision","width","height","enabled","editable","showIndicator","keyboardNavigationEnabled","overscan","generatedRowHeight","rowHeight","generatedIndentStep","indentStep","generatedIconSize","iconSize","generatedChevronSize","chevronSize","baseLeftPadding","rowRightPadding","leadingSpacing","cornerRadius","countWidth","countView","countRow","itemDelegate","itemRow","iconSource","iconRevision","textColorNormal","textColorDisabled","rowBackgroundColorIdle","rowBackgroundColorHover","rowBackgroundColorPressed","rowBackgroundColorActive","rowBackgroundColorInactive","rowBackgroundColorDrag","focusColor","onActiveChanged","onExpansionChanged","onItemMoved"});
        else if((kind=="ToolbarButton"))values=inputs(view,{"id","buttonId","iconName","icon","iconSource","iconRevision","accessibilityName","text","enabled","width","height","iconSize","leftPadding","rightPadding","spacing","cornerRadius","fontSize","tone","textColor","disabledTextColor","hoverColor","pressedColor","primaryColor","dangerColor","focusColor","onClick","onTriggered"},true);
        else if((kind=="ListToolbar"))values=inputs(view,{"id","icon1","icon2","icon3","iconSource","iconRevision","iconSize","buttonSize","horizontalPadding","verticalPadding","spacing","cornerRadius","backgroundColor","hoverColor","pressedColor","focusColor","interactive","enabled","width","height","onIconClicked"});
        else if((kind=="ListFooter"))values=inputs(view,{"id","button1","button2","button3","iconSource","iconRevision","horizontalPadding","verticalPadding","spacing","iconSize","buttonHeight","fontSize","cornerRadius","textColor","disabledTextColor","hoverColor","pressedColor","primaryColor","dangerColor","focusColor","interactive","enabled","width","height","onButtonClicked"});
        else if((kind=="HierarchyToolbar"))values=inputs(view,{"id","minimumToolbarWidth","horizontalPadding","verticalPadding","spacing","slotSize","iconSize","cornerRadius","fontSize","backgroundColor","backgroundOpacity","textColor","disabledTextColor","hoverColor","pressedColor","primaryColor","dangerColor","focusColor","distributeSpacing","enabled","width","height","buttonItems","activeButtonId","iconSource","iconRevision","onActiveChanged","onButtonTriggered"});
        else if(material)values=inputs(view,{"id","density","primaryColor","color","borderColor","backdropBaseColor","backdropSource","backdropBackground","tintOpacity","blurRadius","radius","innerHighlightOpacity","shadowRadius","shadowOffsetY","shadowOpacity","borderWidth","innerHighlightOffsetY","intenseOpacity","faintOpacity","blurEnabled","motionEnabled","enabled","backdrop","outlinePath","width","height","motion"});
        else if((media_card))values=inputs(view,{"id","type","size","detail","displayState","selected","selectable","enabled","title","filename","description","details","metadata","statusText","summary","footnote","actionText","domain","previewTitle","rows","progressLabel","progressText","progress","progressColor","statusColor","showDescription","showStatus","showMenu","showAction","menuAccessibleName","iconName","icon","iconSource","iconRevision","previewSource","imageLoader","contentComponent","previewComponent","iconComponent","width","height","padding","cornerRadius","backgroundColor","hoverColor","selectedColor","borderColor","selectedBorderColor","textColor","secondaryColor","disabledTextColor","progressTrackColor","focusColor","onClick","onDoubleClick","onDoubleClicked","onSelectedChanged","onMenuRequested","onActionTriggered"},true);
        else if(avatar)values=inputs(view,{"id","size","source","imageLoader","backgroundColor","width","height","enabled","accessibilityName"});
        else if(card)values=inputs(view,{"id","title","subtitle","enabled","width","height","padding","spacing","cornerRadius","shapeStyle","backgroundColor","titleColor","subtitleColor"});
        else if(header)values=inputs(view,{"id","title","subtitle","menuVisible","compact","enabled","contentHorizontalPadding","contentVerticalPadding","rowSpacing","actionSpacing","menuButtonPadding","width","height","backgroundColor","titleColor","subtitleColor","menuBackgroundColor","menuPressedBackgroundColor","onMenuClicked"});
        else if(listener)values=inputs(view,{"id","width","height","sizeToContent","onInput","onResized"});
        else if(list_panel)values=inputs(view,{"id","controller","model","editor","editable","reorderable","scrollable","modelRevision","viewRevision","width","height","rowHeight","overscan","showIndicator","keyboardNavigationEnabled","leftPadding","rightPadding","iconSize","spacing","cornerRadius","iconSource","iconRevision","itemDelegate","itemRow","textColor","descriptionColor","disabledTextColor","rowBackgroundColor","hoverColor","pressedColor","selectedRowColor","focusColor","toolbarHeight","toolbarVisible","toolbarIcon1","toolbarIcon2","toolbarIcon3","toolbarIconSize","toolbarInteractive","footerHeight","footerVisible","footerInteractive","footerButton1","footerButton2","footerButton3","backgroundColor","enabled","interactive","onItemActivated","onItemEdited","onItemSubmitted","onItemRejected","onItemMoved","onToolbarIconTriggered","onFooterButtonTriggered"});
        else if(hierarchy_panel)values=inputs(view,{"id","controller","editor","editable","modelRevision","viewRevision","width","height","rowHeight","overscan","showIndicator","keyboardNavigationEnabled","indentStep","iconSize","chevronSize","baseLeftPadding","rowRightPadding","leadingSpacing","cornerRadius","countWidth","countView","countRow","itemDelegate","itemRow","iconSource","iconRevision","textColorNormal","textColorDisabled","rowBackgroundColorIdle","rowBackgroundColorHover","rowBackgroundColorPressed","rowBackgroundColorActive","rowBackgroundColorInactive","rowBackgroundColorDrag","focusColor","toolbarHeight","toolbarVisible","toolbarItems","activeToolbarButtonId","footerHeight","footerVisible","footerInteractive","footerButton1","footerButton2","footerButton3","backgroundColor","enabled","onItemActivated","onExpansionChanged","onItemMoved","onToolbarActiveChanged","onToolbarTriggered","onFooterClicked"});
        else if((table_view))values=inputs(view,{"id","controller","model","editor","orderEditor","sorter","structureEditor","structureFactory","structureControlsVisible","addRowControlsVisible","addColumnControlsVisible","deleteContextMenuEnabled","selectOnContextClick","structureGutterWidth","structureGutterHeight","mergeEditor","editable","clipboardEnabled","sortingEnabled","columnResizeEnabled","rowResizeEnabled","columnResizeHandleWidth","rowResizeHandleHeight","modelRevision","headerColumnAppearance","headerDefaultLeadingIcon","headerDefaultTrailingIcon","headerIconSize","headerIconSpacing","headerIconSource","headerIconRevision","headerDelegate","headerCellDelegate","cellDelegate","delegate","headerContext","cellContext","columnViews","width","height","headerHeight","cellPadding","fallbackCellWidth","minColumnWidth","fontSize","enabled","selectionEnabled","keyboardNavigationEnabled","fitColumnsToViewport","showIndicator","accessibilityName","accessibilityText","backgroundColor","dividerColor","headerTextColor","cellTextColor","selectionColor","currentCellBorderColor","onSelectionChanged","onCellInputEdited","onCellInputSubmitted","onCellInputRejected","onRangeValuesChanged","onRowsSorted","onStructureChanged","onColumnResized","onRowResized","onMergeChanged"});
        else if((table_cell))values=inputs(view,{"id","text","width","height","cellHeight","contentSpacing","fontSize","dividerColor","backgroundColor","textColor","showDivider","clipContent","textStyle","inputable","enabled","selected","current","selectionColor","currentBorderColor","currentBorderWidth","valueType","valueValidator","accessibilityName","onInputEdited","onInputSubmitted","onInputRejected"});
        else if((table_row))values=inputs(view,{"id","cells","width","height","cellWidth","cellHeight","contentSpacing","fontSize","offset","inputable","enabled","showDivider","backgroundColor","dividerColor","textColor","selectionColor","currentBorderColor","onCellInputEdited","onCellInputSubmitted","onCellInputRejected"});
        else if((table_header))values=inputs(view,{"id","columns","width","rowHeight","cellHorizontalPadding","columnWidths","fallbackCellWidth","minColumnWidth","offset","fitColumnsToViewport","textColor","backgroundColor","focusColor","separatorHeight","separatorColor","interactive","enabled","accessibilityText","sort","columnAppearance","defaultLeadingIcon","defaultTrailingIcon","iconSize","iconSpacing","iconSource","iconRevision","onColumnClicked","cellDelegate","delegate","cellContext","columnViews"});
        else if(kind=="Spacer") values=inputs(view,{"id","minLength"});
        else if(image_view)values=inputs(view,{"id","source","imageLoader","width","height","accessibilityName","padding","cornerRadius","borderWidth","backgroundColor","borderColor","preserveAspect","fit","circular","allowEmpty","iconName","iconSize","iconSource","iconRevision"});
        else if(kind=="Label") values=inputs(view,{"id","text","fontSize","fontWeight","lineHeight","color","selectable","style","alignment","elideRight"});
        else if((dropdown)) values=inputs(view,{"id","text","enabled","tone","method","methods","onClick","onPressed","onReleased","onCanceled","iconMode","iconName","iconSource","iconRevision","iconSize","icon","width","height","horizontalPadding","leftPadding","rightPadding","spacing","cornerRadius","indicatorSize","fontSize","lineHeight","accessibilityName","primaryColor","hoverColor","pressedColor","textColor","disabledTextColor"},true);
        else if((kind=="HelpButton")) values=inputs(view,{"id","text","enabled","motionEnabled","enterKeyActivation","tone","method","methods","onClick","onPressed","onReleased","onCanceled","width","height","horizontalPadding","cornerRadius","fontSize","lineHeight","textOffsetY","accessibilityName","backgroundColor","hoverColor","pressedColor","textColor","disabledTextColor"},true);
        else if((kind=="ListItem")) values=inputs(view,{"id","type","size","label","text","description","detail","iconName","icon","iconSource","iconRevision","accessibilityName","selected","enabled","interactive","selectable","showDescription","showChevron","showTrailingIcon","showValue","value","valueWidth","showDate","showFolders","showTags","folderLabel1","folderLabel2","tagLabel1","tagLabel2","dateText","metadata1","metadata2","statusText","bookmarkIconName","durationText","mediaDetail","previewIconName","previewSource","imageLoader","previewSize","showPreview","previewComponent","footerComponent","leadingComponent","trailingComponent","trailingWidth","trailingHeight","showLeadingIcon","showBookmark","showMetadata","detailHorizontalPadding","sectionSpacing","contentSpacing","checked","toggleEnabled","selectionEnabled","showToggle","showSelection","primaryAction","secondaryAction","moreMenu","moreItems","showPrimaryAction","showSecondaryAction","showMoreMenu","actionWidth","slotPadding","slotSpacing","quantity","minimumQuantity","maximumQuantity","stepSize","quantityWidth","quantitySpacing","quantityLabel","optionLabel","modeLabel","scopeLabel","stepperEnabled","showStepper","stepperArrow","stepperTone","selector","selectorIndex","selectorWidth","selectorEnabled","showSelector","unitSelector","unitIndex","unitSelectorWidth","unitSelectorEnabled","showUnitSelector","segments","segmentIndex","segmentWidth","segmentsEnabled","showSegments","inputText1","inputPlaceholder1","inputText2","inputPlaceholder2","fieldLabel1","fieldLabel2","inputWidth","inputHeight","inputEnabled1","inputReadOnly1","showInput1","inputEnabled2","inputReadOnly2","showInput2","width","height","leftPadding","rightPadding","iconSize","spacing","cornerRadius","chevronSize","textColor","descriptionColor","disabledTextColor","backgroundColor","hoverColor","pressedColor","selectedColor","focusColor","onClick","onCheckedChanged","onEdited","onActionTriggered","onActionValuesTriggered","onActionEvent","onMenuItemTriggered","onMenuSelectionTriggered","onQuantityChanged","onSelectorIndexChanged","onUnitIndexChanged","onSegmentIndexChanged","onInputTextChanged","onInputSubmitted","onInputText2Changed","onInput2Submitted"},true);
        else if((button)) values=inputs(view,{"id","text","enabled","motionEnabled","enterKeyActivation","tone","checkable","checked","selectionStyle","method","methods","onClick","onPressed","onReleased","onCanceled","iconMode","iconName","iconSource","iconRevision","iconSize","icon","width","height","horizontalPadding","cornerRadius","iconSpacing","fontSize","lineHeight","textOffsetY","borderColor","borderWidth","primaryColor","hoverColor","pressedColor","normalColor","normalHoverColor","normalPressedColor","selectedColor","dangerColor","dangerHoverColor","dangerPressedColor","disabledBackgroundColor","textColor","disabledTextColor","accessibilityName"},true);
        else if((tab)) values=inputs(view,{"id","text","iconName","iconSource","iconRevision","badge","enabled","selected","tabStyle","iconSize","minimumWidth","height","accessibilityName","onClick"},true);
        else if((tabbar)) values=inputs(view,{"id","spacing","minimumTabWidth","currentIndex","autoSelect","accessibilityName","onCurrentIndexChanged"});
        else if(mobile_tab)values=inputs(view,{"id","text","iconName","iconSource","iconRevision","badge","badgeDot","badgeAccessibilityName","selected","enabled","showLabel","drawSelection","motionEnabled","platformStyle","width","height","focusColor","accessibilityName","onClick"},true);
        else if(kind=="MobileNavigationBar"||kind=="MobileTabBar") values=inputs(view,{"id","model","search","width","bottomSafeInset","currentIndex","autoSelect","enabled","platformStyle","materialEnabled","motionEnabled","accessibilityName","onActivated","onSearchRequested"});
        else if((combo)) values=inputs(view,{"id","text","enabled","tone","arrow","width","height","accessibilityName","leftPadding","rightPadding","indicatorSize","cornerRadius","fontSize","lineHeight","backgroundColor","hoverColor","pressedColor","disabledColor","textColor","disabledTextColor","focusColor","method","methods","onClick","onPressed","onReleased","onCanceled"},true);
        else if((stepper)) values=inputs(view,{"id","enabled","tone","arrow","cornerRadius","width","height","accessibilityName","primaryColor","hoverColor","pressedColor","disabledColor","textColor","disabledTextColor","onStepped","method","methods","onClick","onPressed","onReleased","onCanceled"},true);
        else if((segmented)) values=inputs(view,{"id","horizontalPadding","verticalPadding","spacing","borderWidth","cornerRadius","shapeStyle","forceBorderlessTone","backgroundColor","borderColor","enabled","orientation","method","methods"});
        else if((menu)) values=inputs(view,{"id","items","targetId","compactItems","itemWidth","itemSpacing","leftPadding","rightPadding","topPadding","bottomPadding","edgeMargin","cornerRadius","menuOpacity","selectedIndex","autoCloseOnTrigger","dismissOnGlobalPress","dismissOnGlobalContextRequest","showIconSlot","accessibilityName","baseColor","backgroundColor","borderColor","onItemTriggered","onOpened","onClosed","onCanceled"});
        else if((menu_item)) values=inputs(view,{"id","label","shortcut","key","keyPlaceholder","iconName","icon","iconSource","iconRevision","enabled","compact","state","selectionDirection","keyVisible","showIconSlot","showChevron","hasChildItems","expanded","itemWidth","itemHeight","iconSize","chevronSize","horizontalPadding","spacing","fontSize","lineHeight","accessibilityName","keepOpen","closeOnTrigger","onClick","onPressed","onReleased","onCanceled"},true);
        else if((divider)) values=inputs(view,{"id","axis","thickness","crossPadding","linePadding","lineLength","dividerColor"});
        else if((kind=="RadioButton")) values=inputs(view,{"id","text","enabled","available","checked","state","group","onToggled","accessibilityName","indicatorSize","dotSize","labelGap","motionEnabled","motion","transitionDuration","onColor","offColor","onColorDisabled","offColorDisabled","dotColor","dotColorDisabled","textColor","disabledTextColor"});
        else if((kind=="CheckBox")) values=inputs(view,{"id","text","enabled","checked","group","onToggled","accessibilityName","boxSize","framePadding","boxRadius","checkMarkStrokeWidth","labelGap","checkedColor","uncheckedColor","disabledCheckedColor","disabledUncheckedColor","checkColor","checkMarkColorDisabled","boxBorderWidthCheckedEnabled","boxBorderWidthUncheckedEnabled","boxBorderWidthCheckedDisabled","boxBorderWidthUncheckedDisabled","boxBorderColorCheckedEnabled","boxBorderColorUncheckedEnabled","boxBorderColorCheckedDisabled","boxBorderColorUncheckedDisabled","textColor","disabledTextColor","useFigmaCheckedAssets","innerShadowSoftColor","innerShadowStrongColor","shapeStyle","motionEnabled","motion","transitionDuration"});
        else if((kind=="ToggleSwitch")) values=inputs(view,{"id","text","enabled","checked","group","onToggled","accessibilityName","rightToLeft","motionEnabled","motion","transitionDuration","trackWidth","trackHeight","trackPadding","knobSize","trackCornerRadius","knobCornerRadius","labelGap","onColor","onColorHover","onColorPressed","offColor","offColorHover","offColorPressed","disabledTrackColor","knobFillColor","disabledKnobColor","textColor","disabledTextColor"});
        else if(check) values=inputs(view,{"id","text","enabled","checked","group","onToggled"});
        else if((kind=="InputField")) values=inputs(view,{"id","text","placeholder","enabled","readOnly","width","height","size","surface","onEdited","onSubmit","multiline","password","wrapEnabled","maximumLength","accessibilityName","clearButtonVisible","clearAccessibilityName","fontFamily","fontSize","lineHeight","horizontalPadding","verticalPadding","cornerRadius","backgroundColor","hoverColor","textColor","placeholderColor","selectionColor","caretColor","focusColor"});
        else if((kind=="TextEditor")) values=inputs(view,{"id","text","placeholder","enabled","readOnly","width","height","minimumHeight","onEdited","onSubmit","onContentHeightChanged","wrapEnabled","accessibilityName","fontFamily","fontSize","lineHeight","horizontalPadding","verticalPadding"});
        else if((kind=="SearchField")) values=inputs(view,{"id","text","placeholder","enabled","readOnly","width","surface","clearButtonVisible","accessibilityName","clearAccessibilityName","onEdited","onSubmit"});
        else if((kind=="Slider")) values=inputs(view,{"id","from","to","value","stepSize","width","enabled","active","type","showTicks","tickCount","segmentCount","showSymbol","symbolName","showMinMax","showLabels","showEndpointIcons","minimumLabel","maximumLabel","minimumIconName","maximumIconName","rightToLeft","size","thumbDiameter","trackHeight","rangeInset","height","accessibilityName","trackColor","fillColor","inactiveFillColor","thumbColor","focusColor","labelColor","onMoved"});
        else if(kind=="ProgressBar") values=inputs(view,{"id","minimumValue","maximumValue","endValue","startValue","currentValue","stateModel","minimumValueStateKey","maximumValueStateKey","startValueStateKey","currentValueStateKey","size","shapeStyle","width","height","largeHeight","regularHeight","cornerRadius","trackColor","fillColor","motionEnabled","motion","displayedStart","displayedProgress"});
        else if((color_surface)) values=inputs(view,{"id","model","modelRevision","field","width","height","markerDiameter","enabled","accessibilityName","onColorEdited"});
        else if((color_picker)) values=inputs(view,{"id","type","model","modelRevision","width","padding","enabled","showRecentColors","showActions","rememberOnAccept","recentColors","applyText","cancelText","accessibilityName","onColorEdited","onAccepted","onCanceled","onEyedropperRequested"});
        else if((color_button)) values=inputs(view,{"id","model","modelRevision","currentColor","buttonSize","padding","cornerRadius","enabled","showHueRing","circular","borderOpacity","accessibilityName","backgroundColor","hoverColor","pressedColor","borderColor","focusColor","onClick","onPressed","onReleased","onCanceled"},true);
        else if((popover)) values=inputs(view,{"id","width","height","padding","margins","cornerRadius","surfaceColor","surfaceOpacity","primaryColor","modal","dim","focus","clip","blurEnabled","motionEnabled","closeOnEscape","closeOnPressOutside","closeOnContextRequest","accessibilityName","onOpened","onClosed","onCanceled"});
        else if(sheet) values=inputs(view,{"id","open","title","description","presentation","detent","cornerRadius","showHeader","showDescription","showCloseButton","showGrabber","scrollContent","preferredWidth","preferredHeight","desktopMargin","contentPadding","topSafeInset","bottomSafeInset","dismissOnBackground","dismissOnEscape","dismissOnDrag","motionEnabled","motion","animationDuration","surfaceColor","backdropColor","accessibilityName","onOpened","onClosed"});
        else if((modal)) values=inputs(view,{"id","open","title","description","desc","message","iconName","iconSource","iconResolver","iconRevision","showIcon","iconSize","iconCornerRadius","iconBackgroundColor","iconBorderColor","buttonCount","primaryText","secondaryText","tertiaryText","primaryEnabled","secondaryEnabled","tertiaryEnabled","minWidth","maxWidth","preferredWidth","sidePadding","frameMinHeight","verticalOffset","frameCornerRadius","shapeStyle","frameColor","backdropColor","dismissOnBackground","dismissOnEscape","motionEnabled","motion","enterDuration","exitDuration","motionOvershoot","initialScale","contentPadding","sectionSpacing","actionSpacing","accessibilityName","onPrimaryClicked","onSecondaryClicked","onTertiaryClicked","onCanceled","onOpened","onClosed"});
        else if((alert_button)) values=inputs(view,{"id","text","enabled","motionEnabled","enterKeyActivation","tone","dialogStyle","width","height","horizontalPadding","cornerRadius","textColor","primaryColor","accessibilityName","method","methods","onClick","onPressed","onReleased","onCanceled"},true);
        else if((alert)) values=inputs(view,{"id","open","title","message","description","buttonCount","primaryText","secondaryText","tertiaryText","button1Text","button2Text","button3Text","button1Method","button2Method","button3Method","primaryEnabled","secondaryEnabled","tertiaryEnabled","secondaryDestructive","dismissOnBackground","closeOnEscape","minWidth","maxWidth","preferredWidth","sidePadding","cardCornerRadius","shapeStyle","backdropColor","cardBackgroundColor","glassEnabled","glassBlurRadius","shadowEnabled","motionEnabled","motion","enterDuration","exitDuration","motionOvershoot","initialScale","showIcon","appIconSource","imageSource","iconSource","iconName","iconResolver","iconRevision","iconFrameSize","primaryColor","accessibilityName","onPrimaryClicked","onSecondaryClicked","onTertiaryClicked","onDismissed","onOpened","onClosed"});
        else { error(view.position,"Component '"+std::string(kind)+"' is not supported by native AOT yet"); }
        const bool image_icon=image_view&&values.contains("iconName");
        if(image_view){
            if(!image_icon)for(const auto name:{"iconSize","iconSource","iconRevision"})
                if(values.contains(name))error(values.at(name)->position,"Icon input requires iconName");
            if(image_icon){
                for(const auto name:{"source","imageLoader","width","height","padding","cornerRadius","borderWidth","backgroundColor","borderColor","preserveAspect","fit","circular","allowEmpty"})
                    if(values.contains(name))error(values.at(name)->position,"Named icons do not accept Image source or appearance inputs");
                const auto* name=values.at("iconName");
                if(!native_expression(name->value)){
                    const auto value=text(values,"iconName");
                    if(value.size()>256||value.find_first_not_of(" \t\r\n")==value.npos)
                        error(name->position,"iconName must be nonempty and at most 256 bytes");
                }
            }
        }
        if(auto loader=values.find("imageLoader");loader!=values.end()){
            const auto source=(media_card||kind=="ListItem")?"previewSource":"source";
            if(!values.contains(source))error(loader->second->position,"imageLoader requires an image source");
            if(!native_expression(loader->second->value))error(loader->second->position,"imageLoader requires a native image resolver");
        }
        const bool scoped_image_source=!values.contains("imageLoader")&&
            (((image_view||avatar)&&values.contains("source"))||((media_card||kind=="ListItem")&&values.contains("previewSource")&&
                !(media_card&&(values.contains("previewComponent")||values.contains("contentComponent")||!view.children.empty()))));
        if(scoped_image_source){scoped_image_runtime=true;image_resolver_runtime=true;native_runtime=true;needs_parent_scope=true;}
        bool composite_list_item=view.component->spelling=="ListItemComposite";
        if((kind=="ListItem")){
            for(const auto name:{"dateText","metadata1","metadata2","statusText","bookmarkIconName","durationText","mediaDetail","previewIconName","previewSource","imageLoader","previewSize","showPreview","previewComponent","footerComponent","leadingComponent","trailingComponent","trailingWidth","trailingHeight","showLeadingIcon","showBookmark","showMetadata","detailHorizontalPadding","sectionSpacing","contentSpacing","checked","toggleEnabled","selectionEnabled","showToggle","showSelection","primaryAction","secondaryAction","moreMenu","moreItems","showPrimaryAction","showSecondaryAction","showMoreMenu","actionWidth","slotPadding","slotSpacing","quantity","minimumQuantity","maximumQuantity","stepSize","quantityWidth","quantitySpacing","quantityLabel","optionLabel","modeLabel","scopeLabel","stepperEnabled","showStepper","stepperArrow","stepperTone","selector","selectorIndex","selectorWidth","selectorEnabled","showSelector","unitSelector","unitIndex","unitSelectorWidth","unitSelectorEnabled","showUnitSelector","segments","segmentIndex","segmentWidth","segmentsEnabled","showSegments","inputText1","inputPlaceholder1","inputText2","inputPlaceholder2","fieldLabel1","fieldLabel2","inputWidth","inputHeight","inputEnabled1","inputReadOnly1","showInput1","inputEnabled2","inputReadOnly2","showInput2","onCheckedChanged","onEdited","onActionTriggered","onActionValuesTriggered","onActionEvent","onMenuItemTriggered","onMenuSelectionTriggered","onQuantityChanged","onSelectorIndexChanged","onUnitIndexChanged","onSegmentIndexChanged","onInputTextChanged","onInputSubmitted","onInputText2Changed","onInput2Submitted"})
                composite_list_item|=values.contains(name);
            for(const auto name:{"type","size"})if(auto found=values.find(name);found!=values.end()){
                if(native_expression(found->second->value))composite_list_item=true;
                else {const auto value=resolve(found->second->value,found->second->position);
                    composite_list_item|=value.kind==ValueKind::enumeration&&(value.text==".Toggle"||value.text==".toggle"||value.text==".Checkable"||value.text==".checkable"||value.text==".Action"||value.text==".action"||value.text==".ActionGroup"||value.text==".actionGroup"||value.text==".Stepper"||value.text==".stepper"||value.text==".Select"||value.text==".select"||value.text==".InlineEdit"||value.text==".inlineEdit"||value.text==".DetailActions"||value.text==".detailActions"||value.text==".DetailQuantity"||value.text==".detailQuantity"||value.text==".DetailSettings"||value.text==".detailSettings"||value.text==".Task"||value.text==".task"||value.text==".Resource"||value.text==".resource"||value.text==".Media"||value.text==".media"||value.text==".Form"||value.text==".form");}
            }
        }
        const auto children=component_kinds(view.children);
        if((kind=="HierarchyToolbar")&&!children.empty())error(view.position,"Independent HierarchyToolbar currently accepts buttonItems, not direct children");
        if(kind!="HierarchyToolbar" && kind!="AbstractButton" && !stack && !scroll && !wheel_guard && !link && !material && !media_card && !card && !header && !listener && !tabbar && !segmented && !menu && !menu_item && !popover && !modal && !sheet && !tooltip && !children.empty()) error(view.position,"This C++ control cannot accept children");
        if(kind=="AbstractButton"&&view.children.size()>1)error(view.position,"AbstractButton accepts one content View; wrap children in a stack");
        if(kind=="AbstractButton"&&!view.children.empty()&&(values.contains("text")||values.contains("icon")||values.contains("iconName")))
            error(view.position,"AbstractButton content cannot be combined with text or icon inputs");
        if(scroll&&(view.children.size()!=1||view.children.front().fragment||view.children.front().repetition))
            error(view.position,"ScrollView accepts one content View; wrap children in a stack");
        if(listener&&children.size()>1)error(view.position,"EventListener accepts one content View; wrap children in a stack");
        if(media_card){
            const bool content=values.contains("contentComponent")||!children.empty();
            if(!view.children.empty()&&children.size()!=1)error(view.position,"Card content requires one View; wrap children in a stack");
            if(!view.children.empty()&&values.contains("contentComponent"))error(view.position,"Use contentComponent or inline Card content, not both");
            if(content&&(values.contains("previewComponent")||values.contains("iconComponent")))error(view.position,"Card content cannot be combined with previewComponent or iconComponent");
        }
        if(modal&&children.size()>1)error(view.position,"Modal accepts one content View; wrap children in a stack");
        if(popover){unsigned content=0;for(const auto child:children)if(child!="Menu"&&child!="ContextMenu"&&child!="Popover")++content;if(content>1)error(view.position,"Popover accepts one content View; wrap children in a stack");}
        if(toolbar&&kind!="HierarchyToolbar"&&!children.empty())error(view.position,"This toolbar component has fixed native slots and accepts no child views");
        if(tabbar)for(const auto child:children)if(child!="Tab"&&child!="if"&&child!="for")error(view.position,"TabBar accepts Tab children only");
        if(menu)for(const auto child:children)if(child!="MenuItem"&&child!="ContextMenuItem"&&child!="MenuDivider"&&child!="ContextMenuDivider"&&child!="Menu"&&child!="ContextMenu"&&child!="if"&&child!="for")error(view.position,"Menu accepts menu items, dividers and owned popups only");
        if((menu))for(const auto& child:view.children){
            if(child.condition&&(child.fragment||!child.bindings.empty()||!child.imports.empty()))
                error(child.position,"A conditional Menu cannot introduce declarations on the condition itself");
            if(!child.condition&&!child.repetition){
                const auto kinds=component_kinds({child});
                if(std::ranges::any_of(kinds,[](const auto kind){return kind!="MenuItem"&&kind!="ContextMenuItem"&&kind!="MenuDivider"&&kind!="ContextMenuDivider"&&kind!="if"&&kind!="for";}))
                    error(child.position,"Independent Menu requires items, dividers, a conditional branch, or repeated rows");
            }
        }
        if(menu_item)for(const auto child:children)if(child!="Menu"&&child!="ContextMenu")error(view.position,"MenuItem children must be owned popups");
        if((menu_item)&&!view.children.empty()){
            if(view.children.size()!=1)error(view.position,"Independent MenuItem accepts one direct submenu");
            const auto& child=view.children.front();
            if(!child.component||(child.component->spelling!="Menu"&&child.component->spelling!="ContextMenu")||child.condition||child.repetition||child.fragment||!child.bindings.empty()||!child.imports.empty())
                error(child.position,"Independent submenu must be a direct, unconditional Menu");
            if(values.contains("hasChildItems")&&!native_expression(values.at("hasChildItems")->value)&&boolean(values,"hasChildItems",true)=="false")
                error(values.at("hasChildItems")->position,"A submenu requires hasChildItems");
        }
        if((menu_item)&&active_menu_registration.empty()&&(values.contains("keepOpen")||values.contains("closeOnTrigger")))
            error(view.position,"MenuItem close policy requires an independent Menu parent");
        if(next>=4096) error(view.position,"Native widget limit is 4096");
        const auto index=next++;
        const auto id=text(values,"id","@lvui/"+std::to_string(index));
        if(id.empty() || !ids.insert(id).second) error(view.position,"Duplicate widget id or empty id");
        const auto variable="node_"+std::to_string(index);
        widget_variables.emplace(id,variable);
        const auto api=("widgets.");
        const auto prefix=std::string(api);
        const auto args=parent+", "+literal(id);
        std::string call;
        if(tooltip){validate_owned_tooltip(values,view);owned_tooltip_runtime=true;native_runtime=true;needs_parent_scope=true;}
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
        else if(wheel_guard){
            if(!values.contains("targetId"))error(view.position,"WheelScrollGuard requires a targetId");
            if(native_expression(values.at("targetId")->value)||text(values,"targetId").empty())error(values.at("targetId")->position,"WheelScrollGuard targetId must be a static nonempty id");
            owned_wheel_guard_runtime=true;native_runtime=true;needs_parent_scope=true;
        }
        else if(window_noise){owned_window_noise_runtime=true;native_runtime=true;needs_parent_scope=true;}
        else if(focus_ring){owned_focus_ring_runtime=true;native_runtime=true;needs_parent_scope=true;}
        else if(material){owned_material_runtime=true;}
        else if(media_card){owned_media_card_runtime=true;native_runtime=true;needs_parent_scope=true;}
        else if(avatar){owned_avatar_runtime=true;native_runtime=true;needs_parent_scope=true;}
        else if((kind=="ListToolbar")){owned_list_toolbar_runtime=true;native_runtime=true;needs_parent_scope=true;}
        else if((kind=="ListFooter")){owned_list_footer_runtime=true;native_runtime=true;needs_parent_scope=true;}
        else if((kind=="HierarchyToolbar")){owned_hierarchy_toolbar_runtime=true;native_runtime=true;needs_parent_scope=true;}
        else if((kind=="ToolbarButton")){owned_toolbar_button_runtime=true;native_runtime=true;needs_parent_scope=true;}
        else if((kind=="ListItem")){if(composite_list_item)owned_list_item_composite_runtime=true;else owned_list_item_runtime=true;native_runtime=true;needs_parent_scope=true;}
        else if(list_selector){owned_list_item_selector_runtime=true;native_runtime=true;needs_parent_scope=true;}
        else if(router){
            owned_router_runtime=true;native_runtime=true;needs_parent_scope=true;
        } else
        if((link)){
            owned_link_runtime=true;native_runtime=true;needs_parent_scope=true;
        } else if(card){owned_card_runtime=true;native_runtime=true;needs_parent_scope=true;}
        else if(header){owned_header_runtime=true;native_runtime=true;needs_parent_scope=true;}
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
        else if((table_view)){owned_table_runtime=true;native_runtime=true;needs_parent_scope=true;}
        else if((table_row)){owned_table_row_runtime=true;native_runtime=true;needs_parent_scope=true;}
        else if((table_cell)){owned_table_cell_item_runtime=true;native_runtime=true;needs_parent_scope=true;}
        else if((table_header)){owned_table_header_runtime=true;native_runtime=true;needs_parent_scope=true;}
        else if((alert_button)){
            owned_alert_button_runtime=true;native_runtime=true;needs_parent_scope=true;
        }
        else if((alert)){
            owned_alert_runtime=true;native_runtime=true;needs_parent_scope=true;
        }
        else if((sheet)) {
            owned_sheet_runtime=true;native_runtime=true;needs_parent_scope=true;
        }
        else if((modal)) {
            owned_modal_runtime=true;native_runtime=true;needs_parent_scope=true;
        }
        else if((popover)) {
            owned_popover_runtime=true;native_runtime=true;needs_parent_scope=true;
        }
        else if((color_button)) {
            owned_color_button_runtime=true;native_runtime=true;needs_parent_scope=true;
        }
        else if((color_picker)) {
            owned_color_picker_runtime=true;owned_color_picker_default_runtime|=!values.contains("model");native_runtime=true;needs_parent_scope=true;
        }
        else if((color_surface)) {
            owned_color_surface_runtime=true;native_runtime=true;needs_parent_scope=true;
        }
        else if(scroll){
            const bool dynamic=std::ranges::any_of(values,[&](const auto& entry){
                return (entry.first=="width"||entry.first=="height"||entry.first=="enabled"||entry.first=="showIndicator"||entry.first=="horizontal")&&native_expression(entry.second->value);
            });
            if(dynamic){native_runtime=true;needs_parent_scope=true;}
            call=prefix+"scroll_view("+args+", "+scroll_view_options(values,true)+")";
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
        else if(image_view){if(image_icon){owned_icon_runtime=true;native_runtime=true;needs_parent_scope=true;}}
        else if(kind=="Label") {
            if(values.contains("alignment")||values.contains("elideRight")){
                call=prefix+"label("+args+", "+label_presentation_options(values,true,view.position)+")";
                native_runtime=true;needs_parent_scope=true;
            }else{
            const auto [role,size]=text_role(values);
            const auto font_size = numeric(values, "fontSize", size, 1, 256);
            const auto line_height = values.contains("fontSize")
                                         ? font_size
                                         : token_number(text_metric(role) + "-line-height", 1, 8192);
            const auto weight = token_number(
                "--lvui-theme-typography-weight-" +
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
            }
        } else if((dropdown)) {
            owned_dropdown_runtime=true;native_runtime=true;needs_parent_scope=true;
        } else if((button)) {
            owned_button_runtime=true;
            if(kind=="HelpButton")owned_help_runtime=true;
        } else if((tab)) {
            owned_tab_runtime=true;native_runtime=true;needs_parent_scope=true;
        } else if((tabbar)) {
            owned_tabbar_runtime=true;native_runtime=true;needs_parent_scope=true;
        } else if((combo)) {
            owned_combo_box_runtime=true;native_runtime=true;needs_parent_scope=true;
        } else if((stepper)) {
            owned_stepper_runtime=true;native_runtime=true;needs_parent_scope=true;
        } else if((segmented)) {
            owned_segmented_runtime=true;native_runtime=true;needs_parent_scope=true;
        } else if((menu)) {
            owned_menu_runtime=true;owned_menu_item_runtime=true;native_runtime=true;needs_parent_scope=true;
        } else if((menu_item)) {
            owned_menu_item_runtime=true;native_runtime=true;needs_parent_scope=true;
        } else if((divider)) {
            owned_divider_runtime=true;
        } else if((kind=="RadioButton")) {
            owned_radio_runtime=true;native_runtime=true;needs_parent_scope=true;
        } else if((kind=="CheckBox")) {
            owned_checkbox_runtime=true;
        } else if((kind=="ToggleSwitch")) {
            owned_toggle_runtime=true;
        } else if(check) {
            const auto method=kind=="CheckBox"?"check_box":kind=="RadioButton"?"radio_button":"toggle_switch";
            call=prefix+method+"("+args+", {.text = "+text_expression(values,"text")+", .enabled = "+boolean(values,"enabled",true)+
                ", .checked = "+boolean(values,"checked",false)+", .group = "+literal(text(values,"group"))+"}, "+action(values,"onToggled","bool value")+")";
        } else if(mobile_tab) {
            owned_mobile_tab_runtime=true;native_runtime=true;needs_parent_scope=true;
        } else if(kind=="MobileNavigationBar"||kind=="MobileTabBar") {
            if(!view.children.empty())error(view.position,"Mobile navigation accepts its typed model rather than child views");
            owned_mobile_navigation_runtime=true;native_runtime=true;needs_parent_scope=true;
        } else if((kind=="TextEditor")) {
            if(!view.children.empty())error(view.position,"TextEditor does not accept child views");
            owned_text_editor_runtime=true;native_runtime=true;needs_parent_scope=true;
        } else if((kind=="InputField")) {
            owned_input_field_runtime=true;native_runtime=true;needs_parent_scope=true;
        } else if((kind=="SearchField")) {
            owned_search_field_runtime=true;native_runtime=true;needs_parent_scope=true;
        } else if((kind=="Slider")) {
            owned_slider_runtime=true;native_runtime=true;needs_parent_scope=true;
        } else if((hierarchy_item)){
            owned_hierarchy_item_runtime=true;native_runtime=true;needs_parent_scope=true;
        } else if((hierarchy_list)){
            owned_hierarchy_list_runtime=true;native_runtime=true;needs_parent_scope=true;
        } else {
            owned_progress_runtime=true;
        }
        if((sheet)){
            const auto native="sheet_"+variable,derived="sheet_inputs_"+variable;
            const auto content=modal_content(view,"SheetContentFactory",false);
            const auto close_factory=owned_sheet_close_factory();
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="open"&&pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="ii::gui::NativeSheetCallbacks{.opened = "+action(values,"onOpened")+", .closed = "+action(values,"onClosed")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_sheet_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = ii::gui::NativeSheet::create(widgets, "+args+", "+derived+"->get(), "+callbacks+", "+content+", "+close_factory+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const ii::gui::SheetInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = ii::gui::NativeSheet::create(widgets, "+args+", "+owned_sheet_options(values,true)+", "+callbacks+", "+content+", "+close_factory+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if((alert)){
            const auto native="alert_"+variable,derived="alert_inputs_"+variable;
            const bool dynamic=(!active_window_accent.empty()&&!values.contains("primaryColor"))||std::ranges::any_of(values,[&](const auto& pair){return pair.first!="open"&&pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&pair.first!="button1Method"&&pair.first!="button2Method"&&pair.first!="button3Method"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto click=[&](std::string_view name){auto callback=action(values,name);return callback=="{}"?std::string("std::function<void()>{}"):callback;};
            const auto method=[&](std::string_view name){auto callback=button_method(values,name,"lvui::native::ButtonEventValue<ii::gui::Widget>");return callback=="{}"?std::string("std::function<void(const lvui::native::ButtonEventValue<ii::gui::Widget>&)>{}"):callback;};
            const auto callbacks="lvui::native::NativeAlertCallbacksValue<ii::gui::Widget>{.methods = std::array<std::function<void(const lvui::native::ButtonEventValue<ii::gui::Widget>&)>, 3>{"+method("button1Method")+", "+method("button2Method")+", "+method("button3Method")+"}, .clicked = std::array<std::function<void()>, 3>{"+click("onPrimaryClicked")+", "+click("onSecondaryClicked")+", "+click("onTertiaryClicked")+"}, .opened = "+action(values,"onOpened")+", .closed = "+action(values,"onClosed")+", .canceled = "+action(values,"onDismissed")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_alert_options(values,false,view.position)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::AlertBinding<ii::gui::BasicButton,ii::gui::BasicNativeButton,ii::gui::BasicAlertButton,ii::gui::BasicNativeAlertButton,ii::gui::BasicAlert,ii::gui::BasicNativeAlert,ii::gui::CachedSvgDecoder,ii::gui::Widget,ii::gui::AlertPresentation>::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::AlertInputValue<ii::gui::RasterImage>& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::AlertBinding<ii::gui::BasicButton,ii::gui::BasicNativeButton,ii::gui::BasicAlertButton,ii::gui::BasicNativeAlertButton,ii::gui::BasicAlert,ii::gui::BasicNativeAlert,ii::gui::CachedSvgDecoder,ii::gui::Widget,ii::gui::AlertPresentation>::create(widgets, "+args+", "+owned_alert_options(values,true,view.position)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if((alert_button)){
            const auto native="alert_button_"+variable,derived="alert_button_inputs_"+variable;
            const bool dynamic=(!active_window_accent.empty()&&!values.contains("primaryColor"))||std::ranges::any_of(values,[&](const auto& pair){return pair.first!="method"&&pair.first!="methods"&&pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeAlertButtonCallbacksValue<ii::gui::Widget>{.method = {}, .clicked = "+action(values)+", .pressed = "+action(values,"onPressed")+", .released = "+action(values,"onReleased")+", .canceled = "+action(values,"onCanceled")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_alert_button_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::AlertButtonBinding<ii::gui::BasicButton,ii::gui::BasicNativeButton,ii::gui::BasicAlertButton,ii::gui::BasicNativeAlertButton,ii::gui::CachedSvgDecoder,ii::gui::Widget>::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::AlertButtonConfiguration& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::AlertButtonBinding<ii::gui::BasicButton,ii::gui::BasicNativeButton,ii::gui::BasicAlertButton,ii::gui::BasicNativeAlertButton,ii::gui::CachedSvgDecoder,ii::gui::Widget>::create(widgets, "+args+", "+owned_alert_button_options(values,true)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
            emit_button_methods(values,native,"lvui::native::ButtonEventValue<ii::gui::Widget>");
        }
        else if((modal)){
            const auto native="modal_"+variable,derived="modal_inputs_"+variable;
            const auto content=modal_content(view);
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="open"&&pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto action_callback=[&](std::string_view name){auto callback=action(values,name);return callback=="{}"?std::string("std::function<void()>{}"):callback;};
            const auto callbacks="lvui::native::NativeModalCallbacks{.actions = std::array<std::function<void()>, 3>{"+action_callback("onPrimaryClicked")+", "+action_callback("onSecondaryClicked")+", "+action_callback("onTertiaryClicked")+"}, .opened = "+action(values,"onOpened")+", .closed = "+action(values,"onClosed")+", .canceled = "+action(values,"onCanceled")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_modal_options(values,false,view.position)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::ModalBinding<ii::gui::BasicButton,ii::gui::BasicNativeButton,ii::gui::BasicAlertButton,ii::gui::BasicModal,ii::gui::BasicNativeModal,ii::gui::CachedSvgDecoder,ii::gui::Widget,ii::gui::WidgetBuilder,ii::gui::ModalPresentation>::create(widgets, "+args+", "+derived+"->get(), "+callbacks+", "+content+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::ModalInputValue<ii::gui::RasterImage>& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::ModalBinding<ii::gui::BasicButton,ii::gui::BasicNativeButton,ii::gui::BasicAlertButton,ii::gui::BasicModal,ii::gui::BasicNativeModal,ii::gui::CachedSvgDecoder,ii::gui::Widget,ii::gui::WidgetBuilder,ii::gui::ModalPresentation>::create(widgets, "+args+", "+owned_modal_options(values,true,view.position)+", "+callbacks+", "+content+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if((color_picker)){
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
            const auto callbacks="lvui::native::NativeColorPickerCallbacksValue<ii::gui::WidgetColor>{.edited = "+action(values,"onColorEdited","ii::gui::WidgetColor value")+
                ", .accepted = "+action(values,"onAccepted","ii::gui::WidgetColor value")+", .canceled = "+action(values,"onCanceled")+
                ", .eyedropper_requested = "+action(values,"onEyedropperRequested")+"}";
            if(dynamic){
                const auto capture=owns_session?"[scope, "+session+"]":"[scope]";
                body+="            auto "+derived+"_result = native_bindings->derive("+capture+"([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_color_picker_options(values,false,view.position,session)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui_generated::detail_color_picker::NativeColorPicker::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::ColorPickerInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui_generated::detail_color_picker::NativeColorPicker::create(widgets, "+args+", "+owned_color_picker_options(values,true,view.position,session)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if((color_surface)){
            const auto native="color_surface_"+variable,derived="color_surface_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callback=action(values,"onColorEdited","ii::gui::WidgetColor value");
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_color_surface_options(values,false,view.position)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::ColorSurfaceBinding<ii::gui::BasicColorSurface, ii::gui::BasicNativeColorSurface>::create(widgets, "+args+", "+derived+"->get(), "+callback+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::ColorSurfaceInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::ColorSurfaceBinding<ii::gui::BasicColorSurface, ii::gui::BasicNativeColorSurface>::create(widgets, "+args+", "+owned_color_surface_options(values,true,view.position)+", "+callback+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if((color_button)){
            const auto native="color_button_"+variable,derived="color_button_inputs_"+variable;
            const bool dynamic=(!active_window_accent.empty()&&!values.contains("focusColor"))||std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeColorPickerButtonCallbacks{.clicked = "+action(values)+", .pressed = "+action(values,"onPressed")+", .released = "+action(values,"onReleased")+", .canceled = "+action(values,"onCanceled")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_color_button_options(values,false,view.position)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::ColorPickerButtonBinding<ii::gui::BasicColorPickerButton, ii::gui::BasicNativeColorPickerButton>::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::ColorPickerButtonInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::ColorPickerButtonBinding<ii::gui::BasicColorPickerButton, ii::gui::BasicNativeColorPickerButton>::create(widgets, "+args+", "+owned_color_button_options(values,true,view.position)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if((popover)){
            const auto native="popover_"+variable,derived="popover_inputs_"+variable;
            const bool dynamic=(!active_window_accent.empty()&&!values.contains("primaryColor"))||std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativePopoverCallbacks{.opened = "+action(values,"onOpened")+", .closed = "+action(values,"onClosed")+", .canceled = "+action(values,"onCanceled")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_popover_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::PopoverBinding<ii::gui::BasicPopover, ii::gui::BasicMaterial, ii::gui::BasicMaterialGradient, ii::gui::BasicNativePopover>::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::PopoverConfiguration& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::PopoverBinding<ii::gui::BasicPopover, ii::gui::BasicMaterial, ii::gui::BasicMaterialGradient, ii::gui::BasicNativePopover>::create(widgets, "+args+", "+owned_popover_options(values,true)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if((menu)){
            const auto native="menu_"+variable,derived="menu_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="items"&&pair.first!="targetId"&&pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeMenuCallbacks{.triggered = "+action(values,"onItemTriggered","int value")+", .opened = "+action(values,"onOpened")+", .closed = "+action(values,"onClosed")+", .canceled = "+action(values,"onCanceled")+"}";
            const bool context=kind=="ContextMenu";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_menu_options(values,false,context)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui_generated::detail_menu::NativeMenu::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::MenuInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui_generated::detail_menu::NativeMenu::create(widgets, "+args+", "+owned_menu_options(values,true,context)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
            const auto registration="menu_registration_"+variable,divider_registration="menu_divider_registration_"+variable,section_owner="menu_section_owner_"+variable;
            native_style_slots+="        lvui_generated::detail_menu::MenuRegistrationPort "+registration+";\n        std::function<ii::core::Result<void>(ii::gui::DividerGroupPort)> "+divider_registration+";\n        std::optional<lvui_generated::detail_menu::MenuSectionAccess> "+section_owner+";\n";
            body+="            scope->"+section_owner+" = "+native+"->weak_section_access();\n"
                  "            scope->"+registration+" = "+native+"->weak_registration_port();\n"
                  "            scope->"+divider_registration+" = [registration = scope->"+registration+"](ii::gui::DividerGroupPort divider) -> ii::core::Result<void> { auto item = lvui_generated::detail_menu::NativeMenu::divider_port(std::move(divider)); if (!item) return std::unexpected(item.error()); return registration(std::move(*item)); };\n";
        }
        else if((tabbar)){
            const auto native="tabbar_"+variable,derived="tabbar_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_tabbar_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::TabBarBinding<ii::gui::BasicTabBar, ii::gui::BasicNativeTabBar, ii::gui::Widget, ii::gui::WidgetSnapshot>::create(widgets, "+args+", "+derived+"->get(), "+bindable_action(values,"currentIndex","onCurrentIndexChanged","int")+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::TabBarConfiguration& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::TabBarBinding<ii::gui::BasicTabBar, ii::gui::BasicNativeTabBar, ii::gui::Widget, ii::gui::WidgetSnapshot>::create(widgets, "+args+", "+owned_tabbar_options(values,true)+", "+bindable_action(values,"currentIndex","onCurrentIndexChanged","int")+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
            const auto registration="tabbar_registration_"+variable,section_owner="tabbar_section_owner_"+variable;
            native_style_slots+="        lvui_generated::detail_tab::TabRegistrationPort "+registration+";\n        std::optional<lvui_generated::detail_tab::TabSectionAccess> "+section_owner+";\n";
            body+="            scope->"+registration+" = "+native+"->weak_registration_port();\n            scope->"+section_owner+" = "+native+"->weak_section_access();\n";
        }
        else if(list_panel){
            const auto native="list_panel_"+variable,derived="list_panel_inputs_"+variable;
            const auto row_argument=item_view_argument(values,"List row delegate",true,true,true);
            const bool owns_controller=!values.contains("controller");const auto controller="list_panel_controller_"+variable;
            if(owns_controller){
                const auto model="list_panel_model_"+variable,result="list_panel_controller_result_"+variable;
                if(values.contains("model"))body+="            auto "+result+" = lvui::ListController::create(lvui::native::widget_value<ii::gui::WidgetColor>("+scoped_member(values,"model")+"));\n";
                else body+="            auto "+model+" = lvui::ListMemory::create();\n            if (!"+model+") return ii::core::fail(ii::core::ErrorCode::invalid_argument, "+model+".error().message);\n"
                           "            auto "+result+" = lvui::ListController::create(*"+model+");\n";
                body+="            if (!"+result+") return ii::core::fail(ii::core::ErrorCode::invalid_argument, "+result+".error().message);\n"
                      "            auto "+controller+" = std::make_shared<lvui::ListController>(std::move(*"+result+"));\n";
            }
            const bool dynamic=!active_window_accent.empty()||std::ranges::any_of(values,[&](const auto& pair){return pair.first!="model"&&pair.first!="itemDelegate"&&pair.first!="itemRow"&&pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeListPanelCallbacks{.item_activated = "+action(values,"onItemActivated","const lvui::native::ListViewEvent& event","event.index, event.entry")+
                ", .item_edited = "+action(values,"onItemEdited","const lvui::native::ListEditEvent& event","event.index, event.entry, std::string(\"label\"), event.text")+
                ", .item_submitted = "+action(values,"onItemSubmitted","const lvui::native::ListEditEvent& event","event.index, event.entry, event.text")+
                ", .item_rejected = "+action(values,"onItemRejected","const lvui::native::ListEditEvent& event, const lvui::ListError& error","event.index, event.entry, event.text, error")+
                ", .item_moved = "+action(values,"onItemMoved","const lvui::ListDragProposal& event","event.request.key, event.from, event.to, event.request.before")+
                ", .toolbar_clicked = "+action(values,"onToolbarIconTriggered","std::size_t index, std::string icon","index, icon")+
                ", .footer_clicked = "+action(values,"onFooterButtonTriggered","std::size_t index, const lvui::ListFooterSlot& slot","index, slot")+"}";
            if(dynamic){
                const auto capture=owns_controller?"[scope, "+controller+"]":"[scope]";
                body+="            auto "+derived+"_result = native_bindings->derive("+capture+"([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_list_panel_options(values,false,controller)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui_generated::detail_list_panel::NativeListPanel::create(widgets, "+args+", "+derived+"->get(), "+callbacks+row_argument+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::ListPanelInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui_generated::detail_list_panel::NativeListPanel::create(widgets, "+args+", "+owned_list_panel_options(values,true,controller)+", "+callbacks+row_argument+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(hierarchy_panel){
            const auto native="hierarchy_panel_"+variable,derived="hierarchy_panel_inputs_"+variable;
            if(values.contains("countView")&&values.contains("itemDelegate"))error(view.position,"A hierarchy row delegate cannot also have a count View");
            const auto count_argument=item_view_argument(values,"Hierarchy panel count View",true);
            const auto row_argument=item_view_argument(values,"Hierarchy panel row delegate",true,true);
            const auto view_arguments=row_argument.empty()?count_argument:", {}"+row_argument;
            const bool owns_controller=!values.contains("controller");const auto controller="hierarchy_panel_controller_"+variable;
            if(owns_controller)body+="            auto "+controller+" = std::make_shared<lvui::HierarchyController>(std::make_shared<lvui::HierarchyMemory>());\n";
            const bool dynamic=!active_window_accent.empty()||std::ranges::any_of(values,[&](const auto& pair){return pair.first!="countView"&&pair.first!="countRow"&&pair.first!="itemDelegate"&&pair.first!="itemRow"&&pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
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
                    "            auto "+native+" = lvui_generated::detail_hierarchy_panel::NativeHierarchyPanel::create(widgets, "+args+", "+derived+"->get(), "+callbacks+view_arguments+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::HierarchyPanelInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui_generated::detail_hierarchy_panel::NativeHierarchyPanel::create(widgets, "+args+", "+owned_hierarchy_panel_options(values,true,controller)+", "+callbacks+view_arguments+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if((hierarchy_item)){
            const auto native="hierarchy_item_"+variable,derived="hierarchy_item_inputs_"+variable;
            const auto count_argument=item_view_argument(values,"Hierarchy count View",false);
            const bool dynamic=!active_window_accent.empty()||std::ranges::any_of(values,[&](const auto& pair){return pair.first!="countView"&&pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeHierarchyItemCallbacks{.activated = "+action(values,"onClick")+
                ", .expansion_changed = "+action(values,"onExpansionChanged","bool expanded","expanded")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_hierarchy_item_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui_generated::detail_hierarchy_item::NativeHierarchyItem::create(widgets, "+args+", "+derived+"->get(), "+callbacks+count_argument+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::HierarchyItemInputValue<ii::gui::RasterImage>& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui_generated::detail_hierarchy_item::NativeHierarchyItem::create(widgets, "+args+", "+owned_hierarchy_item_options(values,true)+", "+callbacks+count_argument+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if((hierarchy_list)){
            const auto native="hierarchy_list_"+variable,derived="hierarchy_list_inputs_"+variable;
            if(values.contains("countView")&&values.contains("itemDelegate"))error(view.position,"A hierarchy row delegate cannot also have a count View");
            const auto count_argument=item_view_argument(values,"Hierarchy list count View",true);
            const auto row_argument=item_view_argument(values,"Hierarchy list row delegate",true,true);
            const auto view_arguments=row_argument.empty()?count_argument:", {}"+row_argument;
            const bool owns_controller=!values.contains("controller");
            const auto controller="hierarchy_list_controller_"+variable;
            if(owns_controller)body+="            auto "+controller+" = std::make_shared<lvui::HierarchyController>(std::make_shared<lvui::HierarchyMemory>());\n";
            const bool dynamic=!active_window_accent.empty()||std::ranges::any_of(values,[&](const auto& pair){return pair.first!="countView"&&pair.first!="countRow"&&pair.first!="itemDelegate"&&pair.first!="itemRow"&&pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeHierarchyListCallbacks{.activated = "+action(values,"onActiveChanged","const lvui::native::HierarchyListEvent& event","event.key, event.id.value_or(-1), event.source_index")+
                ", .expansion_changed = "+action(values,"onExpansionChanged","std::string key, bool expanded","key, expanded")+
                ", .moved = "+action(values,"onItemMoved","const lvui::HierarchyMovePlan& plan","plan")+"}";
            if(dynamic){
                const auto capture=owns_controller?"[scope, "+controller+"]":"[scope]";
                body+="            auto "+derived+"_result = native_bindings->derive("+capture+"([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_hierarchy_list_options(values,false,controller)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui_generated::detail_hierarchy_list::NativeHierarchyList::create(widgets, "+args+", "+derived+"->get(), "+callbacks+view_arguments+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::HierarchyListInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui_generated::detail_hierarchy_list::NativeHierarchyList::create(widgets, "+args+", "+owned_hierarchy_list_options(values,true,controller)+", "+callbacks+view_arguments+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if((table_view)){
            const auto native="table_"+variable,derived="table_inputs_"+variable;
            const auto views=table_view_arguments(values);
            const bool owns_controller=!values.contains("controller")&&!values.contains("model");
            const auto controller="table_controller_"+variable;
            if(owns_controller)body+="            auto "+controller+" = std::make_shared<lvui::TableController>(std::make_shared<lvui::TableMemory>());\n";
            const bool dynamic=!active_window_accent.empty()||std::ranges::any_of(values,[&](const auto& pair){return pair.first!="headerDelegate"&&pair.first!="headerCellDelegate"&&pair.first!="cellDelegate"&&pair.first!="delegate"&&pair.first!="headerContext"&&pair.first!="cellContext"&&pair.first!="columnViews"&&pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeTableCallbacks{.selection_changed = "+action(values,"onSelectionChanged","std::optional<lvui::TableSelection> value")+
                ", .edited = "+action(values,"onCellInputEdited","const lvui::native::TableCellEvent& event","event.row_index, event.column_index, event.text")+
                ", .submitted = "+action(values,"onCellInputSubmitted","const lvui::native::TableCellEvent& event","event.row_index, event.column_index, event.text")+
                ", .rejected = "+action(values,"onCellInputRejected","const lvui::native::TableCellEvent& event, [[maybe_unused]] const lvui::TableError& error","event.row_index, event.column_index, event.text, event.value_type")+
                ", .range_values_changed = "+action(values,"onRangeValuesChanged","const lvui::TableRange& value","value")+
                ", .sorted = "+action(values,"onRowsSorted","std::size_t column, lvui::TableSortOrder order","column, order")+
                ", .structure_changed = "+action(values,"onStructureChanged","const lvui::TableStructureEdit& value","value")+
                ", .column_resized = "+action(values,"onColumnResized","std::size_t column, double width","column, width")+
                ", .row_resized = "+action(values,"onRowResized","std::size_t row, double height","row, height")+
                ", .merge_changed = "+action(values,"onMergeChanged","const lvui::TableMergeEdit& value","value")+"}";
            if(dynamic){
                const auto capture=owns_controller?"[scope, "+controller+"]":"[scope]";
                body+="            auto "+derived+"_result = native_bindings->derive("+capture+"([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_table_options(values,false,controller,view.position)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = detail_table::NativeTable::create(widgets, "+args+", "+derived+"->get(), "+callbacks+views+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::TableInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = detail_table::NativeTable::create(widgets, "+args+", "+owned_table_options(values,true,controller,view.position)+", "+callbacks+views+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if((kind=="HierarchyToolbar")){
            const auto native="hierarchy_toolbar_"+variable,derived="hierarchy_toolbar_inputs_"+variable;
            const bool dynamic=!active_window_accent.empty()||std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeHierarchyToolbarCallbacks{.active_changed = "+action(values,"onActiveChanged","const lvui::HierarchyToolbarSelection& value","value")+
                ", .triggered = "+action(values,"onButtonTriggered","const lvui::HierarchyToolbarSelection& value","value")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_hierarchy_toolbar_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui_generated::detail_hierarchy_toolbar::NativeHierarchyToolbar::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::HierarchyToolbarInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui_generated::detail_hierarchy_toolbar::NativeHierarchyToolbar::create(widgets, "+args+", "+owned_hierarchy_toolbar_options(values,true)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if((kind=="ListFooter")){
            const auto native="list_footer_"+variable,derived="list_footer_inputs_"+variable;
            const bool dynamic=!active_window_accent.empty()||std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeListFooterCallbacks{.clicked = "+action(values,"onButtonClicked","std::size_t index, const lvui::ListFooterSlot& slot","index, slot")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_list_footer_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::ListFooterBinding<ii::gui::BasicListFooter,ii::gui::BasicNativeListFooter,ii::gui::BasicToolbarButton,ii::gui::BasicNativeToolbarButton,ii::gui::CachedSvgDecoder>::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::ListFooterInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::ListFooterBinding<ii::gui::BasicListFooter,ii::gui::BasicNativeListFooter,ii::gui::BasicToolbarButton,ii::gui::BasicNativeToolbarButton,ii::gui::CachedSvgDecoder>::create(widgets, "+args+", "+owned_list_footer_options(values,true)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(list_selector){
            const auto native="list_selector_"+variable,derived="list_selector_inputs_"+variable;
            const bool dynamic=!active_window_accent.empty()||std::ranges::any_of(values,[&](const auto& pair){return pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto changed=bindable_action(values,"currentIndex","onCurrentIndexChanged","int");
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_selector_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui_generated::detail_selector::NativeListItemSelector::create(widgets, "+args+", "+derived+"->get(), "+changed+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::ListItemSelectorInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui_generated::detail_selector::NativeListItemSelector::create(widgets, "+args+", "+owned_selector_options(values,true)+", "+changed+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if((kind=="ListItem")&&composite_list_item){
            const auto native="list_item_composite_"+variable,derived="list_item_composite_inputs_"+variable;
            const bool dynamic=!active_window_accent.empty()||scoped_image_source||std::ranges::any_of(values,[&](const auto& pair){return pair.first!="footerComponent"&&pair.first!="previewComponent"&&pair.first!="leadingComponent"&&pair.first!="trailingComponent"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto views=list_item_view_arguments(values);
            const auto callbacks="lvui::native::NativeListItemCompositeCallbacks{.activated = "+action(values)+
                ", .checked_changed = "+action(values,"onCheckedChanged","bool value","value")+
                ", .edited = "+action(values,"onEdited","std::string field, const lvui::ListItemValue& value","field, value")+
                ", .action_triggered = "+action(values,"onActionTriggered","std::string name, const lvui::ListFooterSlot& value","name, value")+
                ", .quantity_changed = "+action(values,"onQuantityChanged","double value","value")+
                ", .selector_index_changed = "+action(values,"onSelectorIndexChanged","int value","value")+
                ", .input_text_changed = "+action(values,"onInputTextChanged","std::string value","value")+
                ", .input_submitted = "+action(values,"onInputSubmitted","std::string value","value")+
                ", .menu_item_triggered = "+action(values,"onMenuItemTriggered","int index, const lvui::ListItemOption& item","index, item")+
                ", .unit_index_changed = "+action(values,"onUnitIndexChanged","int value","value")+
                ", .segment_index_changed = "+action(values,"onSegmentIndexChanged","int value","value")+
                ", .input_text2_changed = "+action(values,"onInputText2Changed","std::string value","value")+
                ", .input_submitted2 = "+action(values,"onInput2Submitted","std::string value","value")+
                ", .action_values_triggered = "+action(values,"onActionValuesTriggered","std::string name, const lvui::ListItemActionValues& value","name, value")+
                ", .action_event = "+action(values,"onActionEvent","std::string name, const lvui::ListItemActionPayload& value","name, value")+
                ", .menu_selection_triggered = "+action(values,"onMenuSelectionTriggered","const lvui::ListItemMenuSelection& value","value")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_list_item_options(values,false,view.position,true)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = detail_list_composite::NativeListItemComposite::create(widgets, "+args+", "+derived+"->get(), "+callbacks+views+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const detail_list_composite::Input& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = detail_list_composite::NativeListItemComposite::create(widgets, "+args+", "+owned_list_item_options(values,true,view.position,true)+", "+callbacks+views+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if((kind=="ListItem")){
            const auto native="list_item_"+variable,derived="list_item_inputs_"+variable;
            const bool dynamic=!active_window_accent.empty()||std::ranges::any_of(values,[&](const auto& pair){return pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeListItemCallbacks{.activated = "+action(values)+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_list_item_options(values,false,view.position)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::ListItemBinding<ii::gui::BasicListItem, ii::gui::BasicListItemDrawing, ii::gui::BasicNativeListItem, ii::gui::CachedSvgDecoder, ii::gui::SrgbColorConverter, ii::gui::ListItemDrawingPort>::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::ListItemInputValue<ii::gui::RasterImage>& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::ListItemBinding<ii::gui::BasicListItem, ii::gui::BasicListItemDrawing, ii::gui::BasicNativeListItem, ii::gui::CachedSvgDecoder, ii::gui::SrgbColorConverter, ii::gui::ListItemDrawingPort>::create(widgets, "+args+", "+owned_list_item_options(values,true,view.position)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if((kind=="ToolbarButton")){
            const auto native="toolbar_button_"+variable,derived="toolbar_button_inputs_"+variable;
            const bool dynamic=(!active_window_accent.empty()&&(!values.contains("primaryColor")||!values.contains("pressedColor")||!values.contains("focusColor")))||std::ranges::any_of(values,[&](const auto& pair){return pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeToolbarButtonCallbacks{.clicked = "+action(values)+
                ", .triggered = "+action(values,"onTriggered","const lvui::ToolbarButtonId& value","value")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_toolbar_button_options(values,false,view.position)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::ToolbarButtonBinding<ii::gui::BasicToolbarButton,ii::gui::BasicNativeToolbarButton,ii::gui::CachedSvgDecoder>::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::ToolbarButtonInputValue<ii::gui::RasterImage>& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::ToolbarButtonBinding<ii::gui::BasicToolbarButton,ii::gui::BasicNativeToolbarButton,ii::gui::CachedSvgDecoder>::create(widgets, "+args+", "+owned_toolbar_button_options(values,true,view.position)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if((kind=="ListToolbar")){
            const auto native="list_toolbar_"+variable,derived="list_toolbar_inputs_"+variable;
            const auto sources="list_toolbar_sources_"+variable;
            body+="            auto "+sources+" = std::make_shared<IconSourceCache>();\n";
            const bool dynamic=!active_window_accent.empty()||std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="ii::gui::ListBarCallbacks{.clicked = "+action(values,"onIconClicked","std::size_t index, std::string source","index, source")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope, "+sources+"]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_list_toolbar_options(values,false,sources)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = ii::gui::ListToolbarView::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const ii::gui::ListBarInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = ii::gui::ListToolbarView::create(widgets, "+args+", "+owned_list_toolbar_options(values,true,sources)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if((segmented)){
            const auto native="segmented_"+variable,derived="segmented_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="method"&&pair.first!="methods"&&pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&native_expression(pair.second->value);});
            const bool icon=kind=="IconSegmentedControl";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_segmented_options(values,false,icon)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::SegmentedControlBinding<ii::gui::BasicSegmentedControl,ii::gui::BasicNativeSegmentedControl,lvui::native::ButtonMethodValue<lvui::native::ButtonEventValue<ii::gui::Widget>>,lvui::native::ButtonMethodRegistryValue<lvui::native::ButtonEventValue<ii::gui::Widget>>,lvui::native::ButtonMethodInvoker>::create(widgets, "+args+", "+derived+"->get());\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::SegmentedControlConfiguration& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::SegmentedControlBinding<ii::gui::BasicSegmentedControl,ii::gui::BasicNativeSegmentedControl,lvui::native::ButtonMethodValue<lvui::native::ButtonEventValue<ii::gui::Widget>>,lvui::native::ButtonMethodRegistryValue<lvui::native::ButtonEventValue<ii::gui::Widget>>,lvui::native::ButtonMethodInvoker>::create(widgets, "+args+", "+owned_segmented_options(values,true,icon)+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
            const auto style="segmented_style_"+variable;
            native_style_slots+="        lvui::native::SegmentedRegistrationPort "+style+";\n";
            body+="            scope->"+style+" = "+native+"->weak_tone_registration_port();\n";
            emit_button_methods(values,native,"lvui::native::ButtonEventValue<ii::gui::Widget>");
        }
        else if((combo)){
            const auto native="combo_box_"+variable,derived="combo_box_inputs_"+variable;
            const bool dynamic=!active_window_accent.empty()||std::ranges::any_of(values,[&](const auto& pair){return pair.first!="method"&&pair.first!="methods"&&pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeComboBoxCallbacks{.clicked = "+action(values)+", .pressed = "+action(values,"onPressed")+", .released = "+action(values,"onReleased")+", .canceled = "+action(values,"onCanceled")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_combo_box_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::ComboBoxBinding<ii::gui::BasicComboBox, ii::gui::BasicNativeComboBox, ii::gui::Widget>::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::ComboBoxConfiguration& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::ComboBoxBinding<ii::gui::BasicComboBox, ii::gui::BasicNativeComboBox, ii::gui::Widget>::create(widgets, "+args+", "+owned_combo_box_options(values,true)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
            emit_button_methods(values,native,"lvui::native::ButtonEventValue<ii::gui::Widget>");
        }
        else if((stepper)){
            const auto native="stepper_"+variable,derived="stepper_inputs_"+variable;
            const bool dynamic=!active_window_accent.empty()||std::ranges::any_of(values,[&](const auto& pair){return pair.first!="method"&&pair.first!="methods"&&pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeStepperCallbacks{.stepped = "+action(values,"onStepped","int value")+", .clicked = "+action(values)+", .pressed = "+action(values,"onPressed")+", .released = "+action(values,"onReleased")+", .canceled = "+action(values,"onCanceled")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_stepper_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::StepperBinding<ii::gui::BasicButton,ii::gui::BasicStepper,ii::gui::BasicNativeStepper,ii::gui::Widget>::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::StepperConfiguration& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::StepperBinding<ii::gui::BasicButton,ii::gui::BasicStepper,ii::gui::BasicNativeStepper,ii::gui::Widget>::create(widgets, "+args+", "+owned_stepper_options(values,true)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
            emit_button_methods(values,native,"lvui::native::ButtonEventValue<ii::gui::Widget>");
        }
        else if(mobile_tab){
            const auto native="mobile_tab_"+variable,derived="mobile_tab_inputs_"+variable;
            const bool dynamic=!active_window_accent.empty()||std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callback=action(values);
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_mobile_tab_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = detail_mobile_tab::NativeMobileTab::create(widgets, "+args+", "+derived+"->get(), "+callback+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::MobileTabInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = detail_mobile_tab::NativeMobileTab::create(widgets, "+args+", "+owned_mobile_tab_options(values,true)+", "+callback+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(kind=="MobileNavigationBar"||kind=="MobileTabBar"){
            const auto native="mobile_navigation_"+variable,derived="mobile_navigation_inputs_"+variable;
            const bool dynamic=!active_window_accent.empty()||std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeMobileNavigationCallbacks{.activated = "+action(values,"onActivated","int value")+", .search_requested = "+action(values,"onSearchRequested")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_mobile_navigation_options(values,false,kind=="MobileTabBar")+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui_generated::detail_mobile_navigation::NativeMobileNavigation::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::MobileNavigationConfiguration& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui_generated::detail_mobile_navigation::NativeMobileNavigation::create(widgets, "+args+", "+owned_mobile_navigation_options(values,true,kind=="MobileTabBar")+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if((kind=="TextEditor")){
            const auto native="text_editor_"+variable,derived="text_editor_inputs_"+variable;
            const bool dynamic=!active_window_accent.empty()||std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeTextEditorCallbacks{.edited = "+bindable_action(values,"text","onEdited","std::string")+", .submitted = "+action(values,"onSubmit")+", .content_height_changed = "+action(values,"onContentHeightChanged","double value")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_input_field_options(values,false,true)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui_generated::detail_text_editor::NativeTextEditor::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::TextEditorConfiguration& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui_generated::detail_text_editor::NativeTextEditor::create(widgets, "+args+", "+owned_input_field_options(values,true,true)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if((kind=="InputField")){
            const auto native="input_field_"+variable,derived="input_field_inputs_"+variable;
            const bool dynamic=!active_window_accent.empty()||std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeInputFieldCallbacks{.edited = "+bindable_action(values,"text","onEdited","std::string")+", .submitted = "+action(values,"onSubmit")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_input_field_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui_generated::detail_input::NativeInputField::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::InputFieldConfiguration& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui_generated::detail_input::NativeInputField::create(widgets, "+args+", "+owned_input_field_options(values,true)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if((kind=="SearchField")){
            const auto native="search_field_"+variable,derived="search_field_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeSearchFieldCallbacks{.edited = "+bindable_action(values,"text","onEdited","std::string")+", .submitted = "+action(values,"onSubmit")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_search_field_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui_generated::detail_search_field::NativeSearchField::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::SearchFieldConfiguration& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui_generated::detail_search_field::NativeSearchField::create(widgets, "+args+", "+owned_search_field_options(values,true)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if((kind=="Slider")){
            const auto native="slider_"+variable,derived="slider_inputs_"+variable;
            const bool dynamic=!active_window_accent.empty()||std::ranges::any_of(values,[&](const auto& pair){return pair.first!="onMoved"&&pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&native_expression(pair.second->value);});
            const auto callbacks=bindable_action(values,"value","onMoved","double");
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_slider_options(values,false,view.position)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::SliderViewBinding<ii::gui::BasicSlider,ii::gui::BasicNativeSlider,ii::gui::CachedSvgDecoder>::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::SliderConfiguration& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::SliderViewBinding<ii::gui::BasicSlider,ii::gui::BasicNativeSlider,ii::gui::CachedSvgDecoder>::create(widgets, "+args+", "+owned_slider_options(values,true,view.position)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(tooltip){
            const auto native="tooltip_"+variable,derived="tooltip_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){
                return pair.first!="text"&&pair.first!="targetId"&&pair.first!="target"&&pair.first!="visible"&&pair.first!="opacity"&&
                    pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);
            });
            const auto callbacks="lvui::TooltipCallbacks{.opened = "+action(values,"onOpened")+", .closed = "+action(values,"onClosed")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_tooltip_options(values,false)+"; });\n"
                      "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                      "            auto "+derived+" = *"+derived+"_result;\n"
                      "            auto "+native+" = detail_tooltip::NativeTooltip::create(widgets, "+args+", "+derived+"->get().configuration, "+derived+"->get().style, "+derived+"->get().target, "+callbacks+");\n"
                      "            if (!"+native+") return std::unexpected("+native+".error());\n"
                      "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                      "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const detail_tooltip::Input& value) { return component.set_options(value); });\n"
                      "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = detail_tooltip::NativeTooltip::create(widgets, "+args+", "+owned_tooltip_configuration(values,true)+", "+owned_tooltip_style(values,true)+", "+owned_tooltip_target(values,true)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
            body+="            { auto source = "+native+"->diagnostic_source(); if (!source) return std::unexpected(source.error());\n"
                  "              auto binding = native_bindings->bind([source = *source](lvui::native::ReadContext& context) { return context.read(*source); }, [](const lvui::native::TooltipDiagnostic& value) -> ii::core::Result<void> { if (value.failure) return std::unexpected(*value.failure); return {}; });\n"
                  "              if (!binding) return std::unexpected(binding.error()); }\n";
            if(auto item=values.find("target");item!=values.end())
                body+="            { auto binding = native_bindings->bind([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+
                    expression_code(item->second->value,item->second->position)+"; }, [component = *"+native+"](const ii::gui::Widget& value) { return component.set_target(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            if(values.contains("targetId"))context_bindings.push_back({native,text(values,"targetId"),values.at("targetId")->position,true,false,{},{},{}});
            if(values.contains("text")){
                const auto label="tooltip_text_"+variable,label_id=id+"/text";
                if(!ids.insert(label_id).second)error(view.position,"Tooltip text widget id conflicts with another widget");
                body+="            auto "+label+" = widgets.label("+native+"->content_root(), "+literal(label_id)+", {.text = "+text_expression(values,"text")+", .font_size = 13, .selectable = false});\n"
                      "            if (!"+label+") return std::unexpected("+label+".error());\n";
                if(native_expression(values.at("text")->value))native_binding(*values.at("text"),label,"[tooltip = *"+native+"](ii::gui::Widget widget, const auto& value) -> ii::core::Result<void> { if (auto changed = widget.set_text(lvui::native::text(value)); !changed) return changed; return tooltip.refresh(); }");
            }
        }
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
            body+="            auto "+native+" = lvui_generated::detail_code_editor::NativeCodeEditor::create(widgets, "+args+", std::move("+input+"), "+callbacks+");\n"
                  "            if (!"+native+") return std::unexpected("+native+".error());\n"
                  "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
            if(dynamic)body+="            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::CodeEditorAppearance& value) { return component.set_appearance(value); });\n"
                "              if (!binding) return std::unexpected(binding.error()); }\n";
            if(values.contains("sourceRevision")&&native_expression(values.at("sourceRevision")->value))
                native_binding(*values.at("sourceRevision"),variable,"[component = *"+native+"](ii::gui::Widget, const auto&) { return component.sync_source(); }");
        }
        else if(material){
            const auto native="material_"+variable,derived="material_inputs_"+variable;
            const bool dynamic=(kind!="WindowMaterial"&&!active_window_accent.empty()&&!values.contains("primaryColor"))||std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&pair.first!="backdropSource"&&pair.first!="backdropBackground"&&native_expression(pair.second->value);});
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_material_options(kind,values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::MaterialBinding<ii::gui::BasicMaterial, ii::gui::BasicMaterialGradient, ii::gui::BasicNativeMaterial, ii::gui::Widget>::create(widgets, "+args+", "+derived+"->get());\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::MaterialConfiguration& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::MaterialBinding<ii::gui::BasicMaterial, ii::gui::BasicMaterialGradient, ii::gui::BasicNativeMaterial, ii::gui::Widget>::create(widgets, "+args+", "+owned_material_options(kind,values,true)+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
            if(values.contains("backdropSource")||values.contains("backdropBackground")){
                ContextBinding binding;binding.menu=native;binding.position=view.position;binding.material=true;
                const auto source=[&](std::string_view name,std::string& id,std::string& expression){
                    const auto found=values.find(name);if(found==values.end())return;
                    binding.position=found->second->position;
                    if(native_expression(found->second->value))expression="lvui::native::widget_value<ii::gui::WidgetColor>("+expression_code(found->second->value,found->second->position)+")";
                    else {const auto value=text(values,name);if(!value.empty())id=id_prefix+value;}
                };
                source("backdropBackground",binding.background,binding.background_expression);
                source("backdropSource",binding.target,binding.content_expression);
                context_bindings.push_back(std::move(binding));
            }
        }
        else if((listener)){
            const auto native="listener_"+variable,derived="listener_inputs_"+variable;
            const auto callbacks="ii::gui::EventListenerCallbacks{.input = "+listener_input(values)+", .resized = "+action(values,"onResized","ii::gui::Rect value")+"}";
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return (pair.first=="width"||pair.first=="height")&&native_expression(pair.second->value);});
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_listener_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = ii::gui::NativeEventListener::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const ii::gui::EventListenerOptions& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = ii::gui::NativeEventListener::create(widgets, "+args+", "+owned_listener_options(values,true)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if((table_row)){
            const auto native="table_row_"+variable,derived="table_row_inputs_"+variable;
            const bool dynamic=!active_window_accent.empty()||std::ranges::any_of(values,[&](const auto& pair){return pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeTableRowCallbacks{.edited = "+action(values,"onCellInputEdited","std::size_t index, std::string value","index, value")+
                ", .submitted = "+action(values,"onCellInputSubmitted","std::size_t index, std::string value","index, value")+
                ", .rejected = "+action(values,"onCellInputRejected","std::size_t index, std::string value, std::string valueType","index, value, valueType")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_table_row_options(values,false,view.position)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui_generated::detail_table_row::NativeTableRow::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::TableRowInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui_generated::detail_table_row::NativeTableRow::create(widgets, "+args+", "+owned_table_row_options(values,true,view.position)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if((table_cell)){
            const auto native="table_cell_item_"+variable,derived="table_cell_item_inputs_"+variable;
            const bool dynamic=!active_window_accent.empty()||std::ranges::any_of(values,[&](const auto& pair){return pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeTableCellItemCallbacks{.edited = "+action(values,"onInputEdited","std::string value","value")+
                ", .submitted = "+action(values,"onInputSubmitted","std::string value","value")+
                ", .rejected = "+action(values,"onInputRejected","std::string value, std::string valueType","value, valueType")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_table_cell_item_options(values,false,view.position)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui_generated::detail_table_cell::NativeTableCellItem::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::TableCellItemInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui_generated::detail_table_cell::NativeTableCellItem::create(widgets, "+args+", "+owned_table_cell_item_options(values,true,view.position)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if((table_header)){
            const auto native="table_header_"+variable,derived="table_header_inputs_"+variable;
            Inputs header_views;
            if(auto it=values.find("cellDelegate");it!=values.end())header_views.emplace("headerDelegate",it->second);
            if(auto it=values.find("delegate");it!=values.end())header_views.emplace("headerCellDelegate",it->second);
            if(auto it=values.find("cellContext");it!=values.end())header_views.emplace("headerContext",it->second);
            if(auto it=values.find("columnViews");it!=values.end())header_views.emplace("columnViews",it->second);
            const auto views=table_view_arguments(header_views,true);
            const bool dynamic=!active_window_accent.empty()||std::ranges::any_of(values,[&](const auto& pair){return pair.first!="cellDelegate"&&pair.first!="delegate"&&pair.first!="cellContext"&&pair.first!="columnViews"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeTableHeaderCallbacks{.column_clicked = "+action(values,"onColumnClicked","std::size_t index, const lvui::TableColumn& column","index, column")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_table_header_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui_generated::detail_table_header::NativeTableHeader::create(widgets, "+args+", "+derived+"->get(), "+callbacks+views+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::TableHeaderInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui_generated::detail_table_header::NativeTableHeader::create(widgets, "+args+", "+owned_table_header_options(values,true)+", "+callbacks+views+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if((media_card)){
            const auto native="card_"+variable,derived="card_inputs_"+variable;
            const bool custom_content=values.contains("contentComponent")||!view.children.empty();
            const bool dynamic=scoped_image_source||std::ranges::any_of(values,[&](const auto& pair){return pair.first!="contentComponent"&&pair.first!="previewComponent"&&pair.first!="iconComponent"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto views=card_view_arguments(values,view);
            const auto callbacks="lvui::native::NativeCardCallbacks{.clicked = "+action(values)+
                ", .selected_changed = "+action(values,"onSelectedChanged","bool value","value")+
                ", .menu_requested = "+action(values,"onMenuRequested")+", .action_triggered = "+action(values,"onActionTriggered")+
                ", .double_clicked = "+action(values,values.contains("onDoubleClicked")?"onDoubleClicked":"onDoubleClick")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_media_card_options(values,false,view.position,custom_content)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::CardBinding<ii::gui::BasicCard, ii::gui::BasicNativeCard, ii::gui::CachedSvgDecoder, ii::gui::WidgetFactory>::create(widgets, "+args+", "+derived+"->get(), "+callbacks+views+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::CardInputValue<ii::gui::RasterImage>& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::CardBinding<ii::gui::BasicCard, ii::gui::BasicNativeCard, ii::gui::CachedSvgDecoder, ii::gui::WidgetFactory>::create(widgets, "+args+", "+owned_media_card_options(values,true,view.position,custom_content)+", "+callbacks+views+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(avatar){
            const auto native="avatar_"+variable,derived="avatar_inputs_"+variable;
            const bool dynamic=scoped_image_source||std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&native_expression(pair.second->value);});
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_avatar_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = ii::gui::NativeAvatar::create(widgets, "+args+", "+derived+"->get());\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const ii::gui::AvatarInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = ii::gui::NativeAvatar::create(widgets, "+args+", "+owned_avatar_options(values,true)+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if((router)){
            const auto native="router_"+variable;
            const auto metric=[&](std::string_view name,double fallback,bool initial){
                auto input=values.find(name);if(input!=values.end()&&native_expression(input->second->value))
                    return "lvui::native::range("+expression_code(input->second->value,input->second->position,initial)+", 1, 8192)";
                return decimal(numeric(values,name,fallback,1,8192));
            };
            body+="            auto "+native+" = lvui::native::PageRouterBinding<ii::gui::BasicNativePageRouter, ii::gui::WidgetBuilder>::create(widgets, "+args+", "+scoped_member(values,"router")+", scope->navigation_context, {"+metric("width",640,true)+", "+metric("height",400,true)+"});\n"
                  "            if (!"+native+") return std::unexpected("+native+".error());\n"
                  "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return (pair.first=="width"||pair.first=="height")&&native_expression(pair.second->value);});
            if(dynamic)body+="            { auto target = *"+variable+"; auto binding = native_bindings->bind([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return ii::gui::Size{"+metric("width",640,false)+", "+metric("height",400,false)+"}; }, [target](ii::gui::Size value) { return target.set_extent(value); });\n"
                              "              if (!binding) return std::unexpected(binding.error()); }\n";
        }
        else if((link)){
            const auto native="link_"+variable,derived="link_inputs_"+variable;
            const auto callbacks="lvui::native::LinkCallbacks{.method = "+button_method(values,"method","lvui::LinkEvent")+
                ", .methods = "+link_methods(values,"lvui::LinkEvent")+", .clicked = "+action(values)+
                ", .pressed = "+action(values,"onPressed")+", .released = "+action(values,"onReleased")+
                ", .canceled = "+action(values,"onCanceled")+", .navigated = "+action(values,"onNavigated","bool value")+
                ", .navigation_failed = "+action(values,"onNavigationFailed","const ii::core::Error& error","error")+"}";
            const bool dynamic=!active_window_accent.empty()||std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&pair.first!="method"&&pair.first!="methods"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_link_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::LinkBinding<ii::gui::BasicLink, ii::gui::BasicNativeLink>::create(widgets, "+args+", scope->navigation_context, "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::LinkConfiguration& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::LinkBinding<ii::gui::BasicLink, ii::gui::BasicNativeLink>::create(widgets, "+args+", scope->navigation_context, "+owned_link_options(values,true)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
            if(auto methods=values.find("methods");methods!=values.end()&&native_expression(methods->second->value)){
                const auto expression=expression_code(methods->second->value,methods->second->position);
                body+="            { auto binding = native_bindings->bind([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return std::make_shared<const std::vector<lvui::LinkMethod>>(lvui::native::widget_value<ii::gui::WidgetColor>("+expression+")); }, [component = *"+native+"](const auto& value) { return component.set_methods(*value); });\n"
                      "              if (!binding) return std::unexpected(binding.error()); }\n";
            }
        }
        else if((card)){
            const auto native="surface_"+variable,derived="card_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&native_expression(pair.second->value);});
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_card_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = ii::gui::NativeAppCard::create(widgets, "+args+", "+derived+"->get());\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const ii::gui::AppCardOptions& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = ii::gui::NativeAppCard::create(widgets, "+args+", "+owned_card_options(values,true)+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if((header)){
            const auto native="surface_"+variable,derived="header_inputs_"+variable;
            const bool dynamic=!active_window_accent.empty()||std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_header_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = ii::gui::NativeAppHeader::create(widgets, "+args+", "+derived+"->get(), "+action(values,"onMenuClicked")+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const ii::gui::AppHeaderOptions& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = ii::gui::NativeAppHeader::create(widgets, "+args+", "+owned_header_options(values,true)+", "+action(values,"onMenuClicked")+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(native_view){const auto native="native_view_"+variable;
            body+="            auto "+native+" = "+call+";\n            if (!"+native+") return std::unexpected("+native+".error());\n            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
            if(std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&native_expression(pair.second->value);}))
                body+="            { auto binding = native_bindings->bind([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+native_view_options(values,false)+"; }, [component = *"+native+"](const ii::gui::NativeViewOptions& value) { return component.set_options(value); });\n              if (!binding) return std::unexpected(binding.error()); }\n";
        }
        else if((menu_item)){
            const auto native="menu_item_"+variable,derived="menu_item_inputs_"+variable;
            const bool compact=kind=="ContextMenuItem";
            const bool dynamic=!active_window_accent.empty()||std::ranges::any_of(values,[&](const auto& pair){return pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeMenuItemCallbacks{.clicked = "+action(values)+", .pressed = "+action(values,"onPressed")+", .released = "+action(values,"onReleased")+", .canceled = "+action(values,"onCanceled")+"}";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_menu_item_options(values,false,compact,view.position)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::MenuItemBinding<ii::gui::BasicMenuItem, ii::gui::BasicNativeMenuItem, ii::gui::CachedSvgDecoder, ii::gui::Widget, ii::gui::WidgetSnapshot>::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::MenuItemInputValue<ii::gui::RasterImage>& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::MenuItemBinding<ii::gui::BasicMenuItem, ii::gui::BasicNativeMenuItem, ii::gui::CachedSvgDecoder, ii::gui::Widget, ii::gui::WidgetSnapshot>::create(widgets, "+args+", "+owned_menu_item_options(values,true,compact,view.position)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
            if(!active_menu_registration.empty())body+="            if (auto attached = "+active_menu_registration+"("+native+"->weak_group_port()); !attached) return attached;\n";
        }
        else if((tab)){
            const auto native="tab_"+variable,derived="tab_inputs_"+variable;
            const bool dynamic=!active_window_accent.empty()||std::ranges::any_of(values,[&](const auto& pair){return pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_tab_options(values,false,view.position)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::TabBinding<ii::gui::BasicTab, ii::gui::BasicNativeTab, ii::gui::CachedSvgDecoder, ii::gui::Widget, ii::gui::WidgetSnapshot>::create(widgets, "+args+", "+derived+"->get(), "+action(values)+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::TabInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::TabBinding<ii::gui::BasicTab, ii::gui::BasicNativeTab, ii::gui::CachedSvgDecoder, ii::gui::Widget, ii::gui::WidgetSnapshot>::create(widgets, "+args+", "+owned_tab_options(values,true,view.position)+", "+action(values)+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
            if(!active_tabbar_registration.empty())body+="            if (auto attached = "+active_tabbar_registration+"("+native+"->weak_group_port()); !attached) return attached;\n";
        }
        else if((dropdown)){
            const auto native="dropdown_"+variable,derived="dropdown_inputs_"+variable;
            const bool dynamic=!active_window_accent.empty()||std::ranges::any_of(values,[&](const auto& pair){return pair.first!="method"&&pair.first!="methods"&&pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callbacks="lvui::native::NativeDropdownCallbacks{.clicked = "+action(values)+", .pressed = "+action(values,"onPressed")+", .released = "+action(values,"onReleased")+", .canceled = "+action(values,"onCanceled")+"}";
            const bool icon_default=kind=="IconMenuButton";
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_dropdown_options(values,false,icon_default)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::DropdownBinding<ii::gui::BasicButton,ii::gui::BasicDropdownButton,ii::gui::BasicNativeDropdownButton,ii::gui::CachedSvgDecoder,ii::gui::Widget>::create(widgets, "+args+", "+derived+"->get(), "+callbacks+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::DropdownInputValue<ii::gui::RasterImage>& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::DropdownBinding<ii::gui::BasicButton,ii::gui::BasicDropdownButton,ii::gui::BasicNativeDropdownButton,ii::gui::CachedSvgDecoder,ii::gui::Widget>::create(widgets, "+args+", "+owned_dropdown_options(values,true,icon_default)+", "+callbacks+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
            if(!active_segmented_style.empty())
                body+="            if (auto attached = "+active_segmented_style+"("+native+"->weak_borderless_tone_port()); !attached) return attached;\n";
            emit_button_methods(values,native,"lvui::native::ButtonEventValue<ii::gui::Widget>");
        }
        else if((button)){
            const auto native="button_"+variable,derived="button_inputs_"+variable;
            const bool dynamic=(kind!="HelpButton"&&!active_window_accent.empty())||std::ranges::any_of(values,[&](const auto& pair){return pair.first!="method"&&pair.first!="methods"&&pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto button_options=[&](bool initial){return kind=="HelpButton"?owned_help_button_options(values,initial):owned_button_options(values,initial,icon_button,kind=="AbstractButton",view.position);};
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+button_options(false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::ButtonBinding<ii::gui::BasicButton,ii::gui::BasicNativeButton,ii::gui::CachedSvgDecoder,ii::gui::Widget>::create(widgets, "+args+", "+derived+"->get(), "+action(values)+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::native::ButtonInputValue<ii::gui::RasterImage>& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::ButtonBinding<ii::gui::BasicButton,ii::gui::BasicNativeButton,ii::gui::CachedSvgDecoder,ii::gui::Widget>::create(widgets, "+args+", "+button_options(true)+", "+action(values)+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
            if(values.contains("onPressed")||values.contains("onReleased")||values.contains("onCanceled")){
                const auto events="lvui::native::NativeButtonEvents{.pressed = "+action(values,"onPressed")+
                    ", .released = "+action(values,"onReleased")+", .canceled = "+action(values,"onCanceled")+"}";
                body+="            if (auto installed = "+native+"->set_events("+events+"); !installed) return std::unexpected(installed.error());\n";
            }
            if(!active_segmented_style.empty())
                body+="            if (auto attached = "+active_segmented_style+"("+native+"->weak_borderless_tone_port()); !attached) return attached;\n";
            emit_button_methods(values,native,"lvui::native::ButtonEventValue<ii::gui::Widget>");
        }
        else if((kind=="RadioButton")){
            const auto native="radio_"+variable,derived="radio_inputs_"+variable;
            // A group can deselect other members without emitting their
            // individual user callbacks. Keep its existing explicit command
            // path until the group publishes one coherent selection change.
            const auto on_toggled=values.contains("group")?action(values,"onToggled","bool value"):
                bindable_action(values,"checked","onToggled","bool");
            const bool dynamic=!active_window_accent.empty()||std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&pair.first!="onToggled"&&native_expression(pair.second->value);});
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_radio_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::RadioButtonBinding<ii::gui::BasicRadioButton, ii::gui::BasicNativeRadioButton>::create(widgets, "+args+", scope->radio_groups, "+derived+"->get(), "+on_toggled+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::RadioButtonConfiguration& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::RadioButtonBinding<ii::gui::BasicRadioButton, ii::gui::BasicNativeRadioButton>::create(widgets, "+args+", scope->radio_groups, "+owned_radio_options(values,true)+", "+on_toggled+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if((kind=="CheckBox")){
            const auto native="checkbox_"+variable,derived="checkbox_inputs_"+variable;
            const bool dynamic=!active_window_accent.empty()||std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&pair.first!="onToggled"&&native_expression(pair.second->value);});
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_checkbox_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::CheckBoxBinding<ii::gui::BasicCheckBox, ii::gui::BasicNativeCheckBox, ii::gui::CachedSvgDecoder>::create(widgets, "+args+", "+derived+"->get(), "+bindable_action(values,"checked","onToggled","bool")+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::CheckBoxConfiguration& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::CheckBoxBinding<ii::gui::BasicCheckBox, ii::gui::BasicNativeCheckBox, ii::gui::CachedSvgDecoder>::create(widgets, "+args+", "+owned_checkbox_options(values,true)+", "+bindable_action(values,"checked","onToggled","bool")+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if((kind=="ToggleSwitch")){
            const auto native="toggle_"+variable,derived="toggle_inputs_"+variable;
            const bool dynamic=!active_window_accent.empty()||std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("onToggled")&&native_expression(pair.second->value);});
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_toggle_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = lvui::native::ToggleSwitchBinding<ii::gui::BasicToggleSwitch, ii::gui::BasicNativeToggleSwitch>::create(widgets, "+args+", "+derived+"->get(), "+bindable_action(values,"checked","onToggled","bool")+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const lvui::ToggleSwitchConfiguration& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = lvui::native::ToggleSwitchBinding<ii::gui::BasicToggleSwitch, ii::gui::BasicNativeToggleSwitch>::create(widgets, "+args+", "+owned_toggle_options(values,true)+", "+bindable_action(values,"checked","onToggled","bool")+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(wheel_guard){
            const auto native="wheel_guard_"+variable,derived="wheel_guard_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="targetId"&&pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&!pair.first.starts_with("on")&&native_expression(pair.second->value);});
            const auto callback=action(values,"onWheelRouted","const ii::gui::WheelScrollObservation& value","value");
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_wheel_guard_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = ii::gui::NativeWheelScrollGuard::create(widgets, "+args+", "+derived+"->get(), "+callback+");\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const ii::gui::WheelScrollOptions& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = ii::gui::NativeWheelScrollGuard::create(widgets, "+args+", "+owned_wheel_guard_options(values,true)+", "+callback+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
            context_bindings.push_back({native,text(values,"targetId"),values.at("targetId")->position,false,false,{},{},{},true});
        }
        else if(window_noise){
            const auto native="window_noise_"+variable,derived="window_noise_inputs_"+variable;
            const bool dynamic=!active_window_accent.empty()||std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&native_expression(pair.second->value);});
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_window_noise_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = ii::gui::NativeWindowNoiseTexture::create(widgets, "+args+", "+derived+"->get());\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const ii::gui::WindowNoiseViewOptions& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = ii::gui::NativeWindowNoiseTexture::create(widgets, "+args+", "+owned_window_noise_options(values,true)+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(focus_ring){
            const auto native="focus_ring_"+variable,derived="focus_ring_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&native_expression(pair.second->value);});
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_focus_ring_options(values,false)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = ii::gui::NativeFocusRing::create(widgets, "+args+", "+derived+"->get());\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const ii::gui::FocusRingOptions& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = ii::gui::NativeFocusRing::create(widgets, "+args+", "+owned_focus_ring_options(values,true)+");\n"
                       "            if (!"+native+") return std::unexpected("+native+".error());\n"
                       "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if((divider)){
            const auto native="divider_"+variable,derived="divider_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&native_expression(pair.second->value);});
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_divider_options(values,false,kind=="ContextMenuDivider")+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = ii::gui::NativeDivider::create(widgets, "+args+", "+derived+"->get());\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const ii::gui::DividerConfiguration& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+native+" = ii::gui::NativeDivider::create(widgets, "+args+", "+owned_divider_options(values,true,kind=="ContextMenuDivider")+");\n"
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
        else if(image_view&&image_icon){
            const auto native="icon_"+variable;
            const auto derived="icon_inputs_"+variable;
            const auto sources="icon_sources_"+variable;
            body+="            auto "+sources+" = std::make_shared<IconSourceCache>();\n";
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){
                return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&native_expression(pair.second->value);
            });
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope, "+sources+"]([[maybe_unused]] lvui::native::ReadContext& context) { return "+owned_icon_options(values,false,sources)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = ii::gui::IconView::create(widgets, "+args+", "+derived+"->get());\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const ii::gui::IconViewInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            } else body+="            auto "+native+" = ii::gui::IconView::create(widgets, "+args+", "+owned_icon_options(values,true,sources)+");\n"
                          "            if (!"+native+") return std::unexpected("+native+".error());\n"
                          "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(image_view){
            owned_image_runtime=true;
            const auto native="image_"+variable;
            const auto derived="image_inputs_"+variable;
            const bool dynamic=scoped_image_source||std::ranges::any_of(values,[&](const auto& pair){
                return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&native_expression(pair.second->value);
            });
            if(dynamic){
                const auto options=owned_image_options(values,false);
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+options+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+native+" = ii::gui::NativeImage::create(widgets, "+args+", "+derived+"->get());\n"
                    "            if (!"+native+") return std::unexpected("+native+".error());\n"
                    "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const ii::gui::ImageInput& value) { return component.set_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            } else body+="            auto "+native+" = ii::gui::NativeImage::create(widgets, "+args+", "+owned_image_options(values,true)+");\n"
                          "            if (!"+native+") return std::unexpected("+native+".error());\n"
                          "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
        }
        else if(kind=="ProgressBar") {
            const auto native="progress_"+variable;
            const auto candidate="progress_candidate_"+variable;
            const auto derived="progress_inputs_"+variable;
            const bool dynamic=(!active_window_accent.empty()&&!values.contains("fillColor"))||values.contains("stateModel")||values.contains("motion")||std::ranges::any_of(values,[&](const auto& entry){
                constexpr std::string_view fields=" minimumValue maximumValue endValue startValue currentValue stateModel minimumValueStateKey maximumValueStateKey startValueStateKey currentValueStateKey size shapeStyle width height largeHeight regularHeight cornerRadius trackColor fillColor motionEnabled motion displayedStart displayedProgress ";
                return fields.find(" "+entry.first+" ")!=fields.npos && native_expression(entry.second->value);
            });
            const auto options=owned_progress_options(values,!dynamic);
            if(dynamic) body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+options+"; });\n"
                "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                "            auto "+derived+" = *"+derived+"_result; auto "+candidate+" = "+derived+"->get();\n";
            else body+="            auto "+candidate+" = "+options+";\n";
            body+="            auto "+native+" = ii::gui::ProgressIndicator::create(widgets, "+args+", "+candidate+", "+(dynamic?"true":"false")+");\n"
                  "            if (!"+native+") return std::unexpected("+native+".error());\n"
                  "            auto "+variable+" = ii::core::Result<ii::gui::Widget>{"+native+"->root()};\n";
            if(dynamic) body+="            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [component = *"+native+"](const ii::gui::ProgressConfiguration& value) { return component.set_configuration(value); });\n"
                "              if (!binding) return std::unexpected(binding.error()); }\n";
        }
        else if(kind=="Label"&&(values.contains("alignment")||values.contains("elideRight"))){
            const auto derived="label_inputs_"+variable;
            const bool dynamic=std::ranges::any_of(values,[&](const auto& pair){return pair.first!="visible"&&pair.first!="opacity"&&pair.first!="flex"&&native_expression(pair.second->value);});
            if(dynamic){
                body+="            auto "+derived+"_result = native_bindings->derive([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+label_presentation_options(values,false,view.position)+"; });\n"
                    "            if (!"+derived+"_result) return std::unexpected("+derived+"_result.error());\n"
                    "            auto "+derived+" = *"+derived+"_result;\n"
                    "            auto "+variable+" = widgets.label("+args+", "+derived+"->get());\n"
                    "            if (!"+variable+") return std::unexpected("+variable+".error());\n"
                    "            { auto binding = native_bindings->bind(["+derived+"]([[maybe_unused]] lvui::native::ReadContext& context) { return context.read(*"+derived+"); }, [target = *"+variable+"](const ii::gui::LabelOptions& value) { return target.set_label_options(value); });\n"
                    "              if (!binding) return std::unexpected(binding.error()); }\n";
            }else body+="            auto "+variable+" = "+call+";\n            if (!"+variable+") return std::unexpected("+variable+".error());\n";
        }
        else{body+="            auto "+variable+" = "+call+";\n            if (!"+variable+") return std::unexpected("+variable+".error());\n";if(!scroll)emit_bindings(values,variable,kind=="Label");}
        if(scroll&&std::ranges::any_of(values,[&](const auto& entry){
            return (entry.first=="width"||entry.first=="height"||entry.first=="enabled"||entry.first=="showIndicator"||entry.first=="horizontal")&&native_expression(entry.second->value);
        })) body+="            { auto target = *"+variable+"; auto binding = native_bindings->bind([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+scroll_view_options(values,false)+"; }, [target](const ii::gui::ScrollOptions& value) { return target.set_scroll_options(value); });\n              if (!binding) return std::unexpected(binding.error()); }\n";
        emit_common_layout_bindings(values,variable,stack,kind=="ToolbarButton"||(((kind=="ListItem"||media_card||table_header||table_cell||table_row)))||(((tab||menu_item||(divider&&!active_menu_divider_registration.empty())))));
        if(modal||sheet||alert)emit_popup_open(values,variable);
        if(stack) emit_stack_layout(values,variable,kind,view.position);
        if (stack)
            emit_stack_style(values, variable);
        if(menu && values.contains("targetId")) context_bindings.push_back({variable,text(values,"targetId"),values.at("targetId")->position,false,false,{},{},{}});
        if(menu && values.contains("items")) {
            if(!children.empty()) error(view.position,"Use menu items or typed children, not both");
            const auto& input=*values.at("items");auto items=resolve(input.value,input.position);
            if(items.kind!=ValueKind::list) error(input.position,"Static menu items must be a String list");
            if(items.elements.size()>4096-next) error(input.position,"Native widget limit is 4096");
            for(const auto& item:items.elements) {
                const auto value=resolve(item,input.position);if(value.kind!=ValueKind::string) error(input.position,"Static menu items must contain Strings");
                const auto child=next++;const auto id="@lvui/"+std::to_string(child);if(!ids.insert(id).second) error(input.position,"Duplicate widget id");
                const auto name="node_"+std::to_string(child);
                const auto item_input=[&](bool initial){return "([&] { lvui::native::MenuItemInputValue<ii::gui::RasterImage> value; value.configuration.label = "+literal(value.text)+
                    "; value.configuration.icon_name.clear(); value.configuration.has_children = false; value.configuration.key_visible = false; "
                    +menu_accent_setup(initial)+"return value; }())";};
                body+="            auto "+name+" = lvui::native::MenuItemBinding<ii::gui::BasicMenuItem, ii::gui::BasicNativeMenuItem, ii::gui::CachedSvgDecoder, ii::gui::Widget, ii::gui::WidgetSnapshot>::create(widgets, menu_"+variable+"->content_root(), "+literal(id)+", "+item_input(true)+");\n"
                      "            if (!"+name+") return std::unexpected("+name+".error());\n";
                if(!active_window_accent.empty())
                    body+="            { auto binding = native_bindings->bind([scope]([[maybe_unused]] lvui::native::ReadContext& context) { return "+item_input(false)+"; }, [component = *"+name+"](const lvui::native::MenuItemInputValue<ii::gui::RasterImage>& value) { return component.set_options(value); });\n"
                          "              if (!binding) return std::unexpected(binding.error()); }\n";
                body+="            if (auto attached = menu_"+variable+"->register_item("+name+"->weak_group_port()); !attached) return attached;\n";
            }
        }
        if((!modal&&!sheet)){
            const auto previous_style=active_segmented_style;
            const auto previous_tabbar=active_tabbar_registration;
            const auto previous_tabbar_section=active_tabbar_section,previous_tabbar_row=active_tabbar_row,previous_tabbar_capture=active_tabbar_capture;
            const auto previous_menu=active_menu_registration;
            const auto previous_menu_divider=active_menu_divider_registration;
            const auto previous_menu_section=active_menu_section;
            const auto previous_menu_row=active_menu_row;
            const auto previous_menu_capture=active_menu_capture;
            const auto previous_submenu=active_submenu_registration;
            if((segmented))active_segmented_style="scope->segmented_style_"+variable;
            if((tabbar)){active_tabbar_registration="scope->tabbar_registration_"+variable;
                active_tabbar_section="scope->tabbar_section_owner_"+variable+"->register_section";active_tabbar_row.clear();active_tabbar_capture.clear();}
            if((menu))active_menu_registration="scope->menu_registration_"+variable;
            if((menu))active_menu_divider_registration="scope->menu_divider_registration_"+variable;
            if((menu)){active_menu_section="scope->menu_section_owner_"+variable+"->register_section";active_menu_row.clear();active_menu_capture.clear();}
            if((menu))active_submenu_registration.clear();
            if((menu_item)&&!view.children.empty())active_submenu_registration="menu_item_"+variable+"->set_submenu";
            if(!media_card)for(const auto& child:view.children)
                emit(child,tooltip?"tooltip_"+variable+"->content_root()":(menu)?"menu_"+variable+"->content_root()":((card||header))?"surface_"+variable+(card?"->content_root()":"->actions_root()"):"*"+variable,(menu||tabbar)?&values:nullptr);
            active_segmented_style=previous_style;
            active_tabbar_registration=previous_tabbar;
            active_tabbar_section=previous_tabbar_section;active_tabbar_row=previous_tabbar_row;active_tabbar_capture=previous_tabbar_capture;
            active_menu_registration=previous_menu;
            active_menu_divider_registration=previous_menu_divider;
            active_menu_section=previous_menu_section;
            active_menu_row=previous_menu_row;
            active_menu_capture=previous_menu_capture;
            active_submenu_registration=previous_submenu;
        }
        if((tabbar))body+="            if (auto finished = tabbar_"+variable+"->finalize(); !finished) return finished;\n";
        if((menu))body+="            if (auto finished = menu_"+variable+"->finalize(); !finished) return finished;\n";
        if((menu)&&!submenu_owner.empty())body+="            if (auto attached = "+submenu_owner+"(menu_"+variable+"->weak_popup_port()); !attached) return attached;\n";
    }

public:
    Generator(const ViewDocument& value, const NativeCompileOptions& config) : document(value), options(config) {}
    void profile(StyleTokenOptions &output) {
        if (!document.imports().empty() || !document.root().children.empty() ||
            !document.root().inputs.empty())
            error({}, "A style profile contains only typed const declarations");
        for (const auto &binding : document.bindings()) {
            if (binding.mutable_value || (binding.type != "Color" && binding.type != "Metric") ||
                !identifier(binding.name))
                error(binding.position, "A style profile requires named Color/Metric const declarations");
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
            writable.erase(backend.name);
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
            writable.erase(binding.name);
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
                        if (parameter.type == "View" || parameter.type=="[View]" || parameter.type=="Color" || parameter.type=="Object" || parameter.type == "String" || parameter.type == "Bool" || parameter.type == "Int" ||
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
                error(binding.position, "Native const initializer must be a scalar constant");
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
                writable[binding.name] = {"scope->value_" + symbol, binding.type};
            }else constants.emplace(binding.name, std::move(value));
        }
    }
    void lower_compatibility_window(View& view,bool in_application) {
        if(!view.component)return;
        const auto name=view.component->spelling;
        if(name!="AppShell"&&name!="AppBootstrapWindow")return;
        bool explicit_initial=false;std::optional<bool> visible;SourcePosition visible_position=view.position;
        for(const auto& input:view.inputs)if(input.name=="initiallyOpen")explicit_initial=true;
        for(auto it=view.inputs.begin();it!=view.inputs.end();){
            if(it->name!="visible"){++it;continue;}
            if(visible)error(it->position,"Duplicate Window visibility");
            visible_position=it->position;
            if(native_expression(it->value))error(it->position,"Compatibility Window visible must be a static Bool");
            const auto value=resolve(it->value,it->position);
            if(value.kind!=ValueKind::boolean)error(it->position,"Compatibility Window visible must be a static Bool");
            visible=value.text=="true";it=view.inputs.erase(it);
        }
        if(visible&&explicit_initial)error(visible_position,"Use visible or initiallyOpen, not both");
        if(!visible&&!explicit_initial)visible=name=="AppBootstrapWindow";
        if(visible&&!*visible&&!in_application)error(visible_position,"An initially hidden Window requires a named secondary Application scene");
        if(in_application&&visible) view.inputs.push_back({"initiallyOpen",{ValueKind::boolean,*visible?"true":"false",{},visible_position},visible_position});
        view.component=find_keyword("ApplicationWindow");
    }
    std::string run() {
        if (!identifier(options.name)) error({}, "Invalid or reserved C++ identifier for generated view name");
        if (options.framework!="iiGUI") error({}, "Unsupported native framework; LVUI uses iiGUI");
        std::string scope;
        std::string type_imports;
        declarations(document.imports(), document.bindings(), scope, type_imports);
        auto root = document.root();
        for (std::size_t depth = 0; !root.component; ++depth) {
            if (root.condition || root.repetition) {
                // Both constructs already create a stable content host. Keep
                // that host as the root without introducing another layout.
                View window;
                window.component = find_keyword("Window");
                window.position = root.position;
                window.children.push_back(std::move(root));
                root = std::move(window);
                break;
            }
            if (root.fragment) {
                // A sequence needs one structural owner for its sibling roots.
                View container;
                container.component = find_keyword("VStack");
                container.position = root.position;
                // This stack is a structural owner, not an authored layout.
                container.inputs.push_back({"spacing", {ValueKind::number, "0", {}, root.position}, root.position});
                container.children.push_back(std::move(root));
                root = std::move(container);
                break;
            }
            if(has_materialized_inputs(root)){
                declarations({},root.bindings,scope,type_imports);root.bindings.clear();
            }
            if (depth == 64) error(root.position, "Maximum View value expansion depth is 64");
            auto found = view_values.find(root.custom_component);
            if (found == view_values.end()) error(root.position, "Unknown root View value");
            const auto definition = found->second;
            const auto supplied = instantiate_view(root, *definition);
            root = supplied;
        }
        lower_compatibility_window(root,false);
        const bool application_root = root.component->spelling == "Application";
        application_generation = application_root;
        std::vector<View> application_windows;
        std::string application_start, application_ready, application_stop, application_active, application_inactive,
                    application_foreground, application_background, application_files, application_arguments, application_close;
        if (application_root) {
            if (root.condition || root.repetition)
                error(root.position, "Application does not accept conditions or repetition");
            declarations(root.imports, root.bindings, scope, type_imports);
            if (root.children.empty())
                error(root.position, "Application requires at least one primary Window");
            application_windows = std::move(root.children);
            if (application_windows.size() > 4096) error(root.position, "Too many application Window scenes");
            for (std::size_t index = 0; index < application_windows.size(); ++index) {
                auto& scene = application_windows[index];
                // A Scene owns exactly one native window and its existing window-local
                // scope. Lower only at the application boundary; never admit Scene as a View.
                if (scene.component && scene.component->spelling == "Scene")
                    scene.component = find_keyword("Window");
                lower_compatibility_window(scene,true);
                if (!scene.component || scene.condition || scene.repetition || scene.fragment ||
                    (scene.component->spelling != "ApplicationWindow" && scene.component->spelling != "Window"))
                    error(scene.position, "Application requires unconditional Window or ApplicationWindow scenes");
                auto declaration = scene;
                declaration.imports.clear(); declaration.bindings.clear();
                const auto values = inputs(declaration, {"title","width","height","minimumWidth","minimumHeight","accent","backgroundColor","fontFamily","windowChrome","onOpened","onClosed","onCloseRequested","id","initiallyOpen"});
                std::string id;
                if (auto found = values.find("id"); found != values.end()) {
                    const auto value = resolve(found->second->value, found->second->position);
                    if (value.kind != ValueKind::string || value.text.empty() || value.text.size() > 1024 ||
                        value.text.find('\0') != value.text.npos || !ii::core::valid_utf8(value.text))
                        error(found->second->position, "Window id must be a nonempty static UTF-8 string of at most 1024 bytes");
                    id = value.text;
                    if (!application_scene_index.emplace(id, index).second)
                        error(found->second->position, "Duplicate Window id");
                }
                bool initial = true;
                if (auto found = values.find("initiallyOpen"); found != values.end()) {
                    const auto value = resolve(found->second->value, found->second->position);
                    if (value.kind != ValueKind::boolean)
                        error(found->second->position, "initiallyOpen requires a static Bool");
                    initial = value.text == "true";
                }
                if (!initial && (index == 0 || id.empty()))
                    error(scene.position, "Only a named secondary Window may start closed");
                if (!id.empty() && (index > 0 || application_windows.size() > 1)) scene_registry_runtime = true;
                application_scene_ids.push_back(std::move(id));
                application_scene_initial.push_back(initial);
            }
            if (application_scene_index.size() > 4096) error(root.position, "Too many named Window scenes");
            finish_scene_declarations();
            auto application_inputs = root;
            application_inputs.imports.clear(); application_inputs.bindings.clear();
            const auto events = inputs(application_inputs, {"onStart", "onReady", "onStop", "onActive", "onInactive", "onForeground", "onBackground", "onOpenFiles", "onLaunchArguments", "onCloseRequested"});
            application_start = action(events,"onStart");
            application_ready = action(events,"onReady");
            application_stop = action(events,"onStop");
            application_active = action(events,"onActive");
            application_inactive = action(events,"onInactive");
            application_foreground = action(events,"onForeground");
            application_background = action(events,"onBackground");
            application_files = action(events,"onOpenFiles", "const std::vector<std::string>& value");
            application_arguments = action(events,"onLaunchArguments", "const std::vector<std::string>& value");
            application_close = close_decision(events);
        }
        if (!application_root) finish_scene_declarations();
        if (!application_root && root.component->spelling != "ApplicationWindow" && root.component->spelling != "Window") {
            if (root.component->spelling == "Scene") error(root.position, "Scene requires an Application parent");
            View window;
            window.component = find_keyword("Window");
            window.position = root.position;
            window.children.push_back(std::move(root));
            root = std::move(window);
        }
        const bool multiple_scenes = application_windows.size() > 1;
        if (options.view_factory_only && multiple_scenes)
            error(root.position, "External view factories do not yet manage multiple scenes");
        WindowOutput window;
        std::vector<WindowOutput> scene_windows;
        std::vector<std::string> scene_bodies;
        if (application_root) {
            auto initial_body = std::exchange(body, {});
            for (std::size_t index=0; index<application_windows.size(); ++index) {
                ids.clear(); window_accent_name="blue"; active_window_accent.clear(); window_accent_derived.clear();
                window_input_setup.clear(); window_input_names.clear();
                window_appearance_name = multiple_scenes ? "appearance_scene_"+std::to_string(index) : "appearance";
                WindowOutput current;
                (void)emit_scoped(application_windows[index],"native_parent",nullptr,nullptr,false,ScopedRowContext::none,false,&current);
                scene_windows.push_back(std::move(current));
                scene_bodies.push_back(std::exchange(body, {}));
            }
            window=scene_windows.front();
            body=std::move(initial_body);
            if (multiple_scenes) {
                body += "            switch (scene_index) {\n";
                for (std::size_t index=0; index<scene_bodies.size(); ++index)
                    body += "            case "+std::to_string(index)+": {\n"+scene_bodies[index]+"                break; }\n";
                body += "            default: return ii::core::fail(ii::core::ErrorCode::out_of_range, \"Unknown application scene\");\n            }\n";
            } else body += scene_bodies.front();
        } else {
            declarations(root.imports, root.bindings, scope, type_imports);
            root.imports.clear(); root.bindings.clear();
            window=prepare_window(root);
        }
        const bool application_close_runtime = application_root && (application_close != "{}" || application_quit_runtime || multiple_scenes || window_close_runtime);
        const bool application_state_runtime = application_root && (application_active != "{}" || application_inactive != "{}" ||
                                                                    application_foreground != "{}" || application_background != "{}");
        if(scene_lifecycle_runtime)scope += "        std::function<void(const ii::core::Error&)> scene_failure_handler;\n";
        if(application_quit_runtime)scope += "        std::weak_ptr<lvui::native::ApplicationCommands> application_commands;\n";
        if (needs_parent_scope && scope.empty()) scope = "        // Lexical parent activation for locally owned child scopes.\n";
        if (window_runtime && scope.empty()) scope = "        // Native activation for Window properties.\n";
        if(popup_mount_validation)scope+="        std::vector<std::pair<std::weak_ptr<lvui::native::Bindings>, lvui::native::BindingId>> initial_popup_bindings;\n";
        emit_context_bindings("in the window scope");
        if (options.view_factory_only && (window_close_runtime || popup_mount_validation))
            error(root.position, "External view factory cannot discard window-close or popup host lifecycle");
        std::string result = "// Generated by lvui-compile. C++23 native widget construction; do not edit.\n#pragma once\n#include <iiGUI/NativeWidgets.h>\n#include <iiGUI/Bindings.h>\n";
        if (!options.view_factory_only) result += "#include <iiGUI/Host.h>\n";
        result += "#include <memory>\n#include <vector>\n#if !defined(IIGUI_TEXT_WEIGHT_RULES_API) || IIGUI_TEXT_WEIGHT_RULES_API < 1\n#error \"LVUI generated view requires iiGUI 0.5.0 or later\"\n#endif\n";
        if (application_root) result += "#include <iiGUI/Application.h>\n#include <LVUI/Scene.h>\n#include <array>\n#include <limits>\n#include <optional>\n#include <thread>\n#include <utility>\n#if !defined(IISDKCORE_APPLICATION_CONFIGURATION_TRANSACTION_API) || IISDKCORE_APPLICATION_CONFIGURATION_TRANSACTION_API < 1\n#error \"LVUI Application requires iiSDKCore atomic application configuration\"\n#endif\n#if !defined(IISDKCORE_APPLICATION_BEFORE_DRIVER_API) || IISDKCORE_APPLICATION_BEFORE_DRIVER_API < 1\n#error \"LVUI Application requires iiSDKCore before-driver attachment\"\n#endif\n";
        if (multiple_scenes) result += "#include <LVUI/SceneStartup.h>\n#include <LVUI/SceneWindowSet.h>\n#if !defined(IIGUI_HOST_FAILURE_HANDLER_API) || IIGUI_HOST_FAILURE_HANDLER_API < 1\n#error \"LVUI multiple windows require iiGUI host failure notifications\"\n#endif\n#if !defined(IIGUI_HOST_APPLICATION_DRIVER_API) || IIGUI_HOST_APPLICATION_DRIVER_API < 1\n#error \"LVUI multiple windows require an independent iiGUI application driver\"\n#endif\n";
        if (window_close_runtime) result += "#include <LVUI/WindowCloseGuard.h>\n";
        if (scene_registry_runtime) result += "#include <LVUI/SceneRegistry.h>\n#include <exception>\n#include <stdexcept>\n";
        if (application_quit_runtime) result += "#include <LVUI/ApplicationCommands.h>\n#include <exception>\n#include <stdexcept>\n";
        if (application_root && application_files != "{}") result += "#include <LVUI/ApplicationFileEvents.h>\n#include <exception>\n#include <stdexcept>\n";
        if (scene_lifecycle_runtime) result += "#include <LVUI/SceneLifecycle.h>\n#include <LVUI/SceneEffectGroup.h>\n#include <LVUI/NativeBindings.h>\n#if !defined(IIGUI_COMMITTED_PUBLICATION_API) || IIGUI_COMMITTED_PUBLICATION_API < 1\n#error \"Window lifecycle requires iiGUI committed window publication\"\n#endif\n#if !defined(LVUI_BINDINGS_RETIRE_EFFECTS_API) || LVUI_BINDINGS_RETIRE_EFFECTS_API < 1\n#error \"Window lifecycle requires LVUI presentation retirement\"\n#endif\n";
        if (application_root && (application_active!="{}" || application_inactive!="{}"))
            result += "#include <exception>\n#include <stdexcept>\n#if !defined(IIGUI_HOST_APPLICATION_ACTIVITY_API) || IIGUI_HOST_APPLICATION_ACTIVITY_API < 1\n#error \"LVUI Application activity requires iiGUI application activity support\"\n#endif\n";
        if (application_root && (application_foreground!="{}" || application_background!="{}"))
            result += "#include <exception>\n#include <stdexcept>\n#if !defined(IIGUI_HOST_APPLICATION_FOREGROUND_API) || IIGUI_HOST_APPLICATION_FOREGROUND_API < 1\n#error \"LVUI Application foreground events require iiGUI foreground support\"\n#endif\n";
        if(popup_open_runtime)result+="#if !defined(IIGUI_COMMITTED_PUBLICATION_API) || IIGUI_COMMITTED_PUBLICATION_API < 1\n#error \"LVUI popup bindings require committed popup publication from iiGUI\"\n#endif\n";
        if (native_view_runtime) result += "#include <iiGUI/NativeView.h>\n";
        if (owned_progress_runtime) result += "#include <LVUI/StyleTokens.h>\n"
            "#if !__has_include(<iiGUI/ProgressIndicator.h>)\n#error \"LVUI ProgressBar requires an iiGUI package with ProgressIndicator.h\"\n#endif\n"
            "#include <iiGUI/ProgressIndicator.h>\n";
        if (owned_button_runtime) result += "#include <LVUI/NativeButton.h>\n#include <iiGUI/NativeButtonView.h>\n#include <iiGUI/SvgImageCache.h>\n";
        if (owned_alert_button_runtime) result += "#include <LVUI/NativeAlertButton.h>\n#include <iiGUI/NativeAlertButtonView.h>\n#include <iiGUI/SvgImageCache.h>\n";
        if (owned_alert_runtime) result += "#include <LVUI/NativeAlert.h>\n#include <iiGUI/NativeAlertView.h>\n#include <iiGUI/AlertPresentation.h>\n#include <iiGUI/SvgImageCache.h>\n";
        if (owned_sheet_runtime) result += "#include <iiGUI/NativeSheetView.h>\n#include <iiGUI/NativeButtonView.h>\n"
            "#include <LVUI/Button.h>\n#include <LVUI/ButtonMotionDefaults.generated.h>\n";
        if (owned_toggle_runtime) result += "#include <LVUI/NativeToggleSwitch.h>\n#include <iiGUI/NativeToggleSwitchView.h>\n";
        if (owned_checkbox_runtime) result += "#include <LVUI/NativeCheckBox.h>\n#include <iiGUI/NativeCheckBoxView.h>\n#include <iiGUI/SvgImageCache.h>\n";
        if (owned_radio_runtime) result += "#include <LVUI/NativeRadioButton.h>\n#include <iiGUI/NativeRadioButtonView.h>\n";
        if (owned_material_runtime) result += "#include <LVUI/NativeMaterial.h>\n#include <iiGUI/NativeMaterialView.h>\n";
        if (owned_link_runtime) result += "#include <LVUI/NativeLink.h>\n#include <iiGUI/NativeLinkView.h>\n";
        if (owned_router_runtime) result += "#include <LVUI/NativePageRouter.h>\n#include <iiGUI/NativePageRouterView.h>\n";
        if (owned_card_runtime) result += "#include <iiGUI/NativeAppCardView.h>\n";
        if (owned_media_card_runtime) result += "#include <LVUI/NativeCard.h>\n#include <iiGUI/NativeCardView.h>\n#include <iiGUI/SvgImageCache.h>\n";
        if (owned_header_runtime) result += "#include <iiGUI/NativeAppHeaderView.h>\n";
        if (owned_listener_runtime) result += "#include <iiGUI/NativeEventListenerView.h>\n";
        if (owned_avatar_runtime) result += "#include <iiGUI/NativeAvatarView.h>\n#include <iiGUI/SvgImageCache.h>\n#include <LVUI/NativeAvatarAssets.h>\n#include <LVUI/NativeAuthoredIcons.h>\n";
        if (owned_tooltip_runtime) result += "#include <LVUI/NativeTooltip.h>\n#include <iiGUI/TooltipModel.h>\n#include <iiGUI/TooltipController.h>\n#include <iiGUI/TooltipOutline.h>\n#include <iiGUI/NativeMaterialView.h>\n#include <iiGUI/NativeTooltipPresentationView.h>\n#include <iiGUI/NativeTooltipTargetView.h>\n#include <iiGUI/TooltipScheduler.h>\n#include <iiGUI/NativeTooltipView.h>\n";
        if (owned_slider_runtime) result += "#include <LVUI/NativeSlider.h>\n#include <iiGUI/NativeSliderView.h>\n#include <iiGUI/SvgImageCache.h>\n";
        if (owned_input_field_runtime) result += "#include <LVUI/NativeInputField.h>\n#include <iiGUI/NativeInputFieldView.h>\n";
        if (owned_text_editor_runtime) result += "#include <LVUI/NativeTextEditor.h>\n#include <iiGUI/NativeTextEditorView.h>\n#include <iiGUI/NativeInputFieldView.h>\n";
        if (owned_mobile_navigation_runtime) result += "#include <LVUI/NativeMobileNavigation.h>\n#include <iiGUI/NativeMobileNavigationView.h>\n#include <iiGUI/MobileNavigationModel.h>\n#include <iiGUI/MobileNavigationMotion.h>\n#include <iiGUI/NativeMaterialView.h>\n#include <iiGUI/SvgImageCache.h>\n";
        if (owned_mobile_tab_runtime) result += "#include <LVUI/NativeMobileTab.h>\n#include <iiGUI/NativeMobileTabView.h>\n#include <iiGUI/SvgImageCache.h>\n";
        if (owned_search_field_runtime) result += "#include <LVUI/NativeSearchField.h>\n#include <iiGUI/NativeSearchFieldView.h>\n#include <iiGUI/SvgImageCache.h>\n";
        if (owned_code_editor_runtime) result += "#include <LVUI/NativeCodeEditor.h>\n#include <iiGUI/NativeCodeEditorView.h>\n";
        if (owned_stepper_runtime) result += "#include <LVUI/NativeStepper.h>\n#include <iiGUI/NativeStepperView.h>\n";
        if (owned_combo_box_runtime) result += "#include <LVUI/NativeComboBox.h>\n#include <iiGUI/NativeComboBoxView.h>\n";
        if (owned_segmented_runtime) result += "#include <LVUI/NativeSegmentedControl.h>\n#include <LVUI/NativeButtonMethods.h>\n#include <iiGUI/NativeSegmentedControlView.h>\n";
        if (owned_help_runtime) result += "#include <LVUI/HelpButton.h>\n";
        if (owned_dropdown_runtime) result += "#include <LVUI/NativeDropdownButton.h>\n#include <iiGUI/NativeDropdownButtonView.h>\n#include <iiGUI/SvgImageCache.h>\n";
        if (owned_tab_runtime) result += "#include <LVUI/NativeTab.h>\n#include <iiGUI/NativeTabView.h>\n#include <iiGUI/SvgImageCache.h>\n";
        if (owned_tabbar_runtime) result += "#include <LVUI/NativeTabBar.h>\n#include <iiGUI/NativeTabBarView.h>\n";
        if (owned_menu_runtime) result += "#include <LVUI/NativeMenu.h>\n#include <iiGUI/NativeMenuView.h>\n#include <iiGUI/NativeMenuItemView.h>\n#include <iiGUI/SvgImageCache.h>\n";
        if (owned_popover_runtime) result += "#include <LVUI/NativePopover.h>\n#include <iiGUI/NativePopoverView.h>\n";
        if (owned_modal_runtime) result += "#include <LVUI/NativeModal.h>\n#include <iiGUI/NativeModalView.h>\n#include <iiGUI/AlertButtonModel.h>\n#include <iiGUI/ModalPresentation.h>\n#include <iiGUI/SvgImageCache.h>\n";
        if (popup_open_runtime) result += "#include <LVUI/NativePopupOpen.h>\n";
        if (owned_color_button_runtime) result += "#include <LVUI/NativeColorPickerButton.h>\n#include <iiGUI/NativeColorPickerButtonView.h>\n";
        if (owned_color_surface_runtime) result += "#include <LVUI/NativeColorSurface.h>\n#include <iiGUI/NativeColorSurfaceView.h>\n";
        if (owned_color_picker_runtime) result += "#include <LVUI/NativeColorPicker.h>\n#include <iiGUI/NativeColorPickerView.h>\n#include <iiGUI/ColorPickerLayout.h>\n#include <iiGUI/NativeColorSurfaceView.h>\n#include <iiGUI/NativeColorChannelView.h>\n#include <iiGUI/NativeColorPickerValuesView.h>\n#include <iiGUI/ColorChannelSurface.h>\n#include <iiGUI/NativeInputFieldView.h>\n#include <iiGUI/NativeButtonView.h>\n#include <iiGUI/SvgImageCache.h>\n";
        if (owned_color_picker_default_runtime) result += "#include <LVUI/ColorSelectionSurface.h>\n";
        if (owned_hierarchy_item_runtime) result += "#include <LVUI/NativeHierarchyItem.h>\n#include <iiGUI/NativeHierarchyItemView.h>\n#include <iiGUI/HierarchyItemModel.h>\n#include <iiGUI/SvgImageCache.h>\n";
        if (owned_hierarchy_list_runtime) result += "#include <LVUI/NativeHierarchyList.h>\n#include <iiGUI/NativeHierarchyListView.h>\n#include <iiGUI/HierarchyViewport.h>\n#include <iiGUI/HierarchyDrag.h>\n#include <iiGUI/NativeHierarchyItemView.h>\n#include <iiGUI/HierarchyItemModel.h>\n#include <iiGUI/SvgImageCache.h>\n";
        if (owned_hierarchy_toolbar_runtime) result += "#include <LVUI/NativeHierarchyToolbar.h>\n#include <iiGUI/NativeHierarchyToolbarView.h>\n#include <iiGUI/HierarchyToolbarModel.h>\n#include <iiGUI/HierarchyToolbarLayout.h>\n#include <iiGUI/NativeToolbarButtonView.h>\n#include <iiGUI/SvgImageCache.h>\n";
        if (owned_hierarchy_panel_runtime) result += "#include <LVUI/NativeHierarchyPanel.h>\n#include <iiGUI/NativeHierarchyPanelView.h>\n#include <iiGUI/PanelLayout.h>\n#include <iiGUI/NativeHierarchyListView.h>\n#include <iiGUI/HierarchyViewport.h>\n#include <iiGUI/HierarchyDrag.h>\n#include <iiGUI/NativeHierarchyItemView.h>\n#include <iiGUI/HierarchyItemModel.h>\n#include <iiGUI/NativeHierarchyToolbarView.h>\n#include <iiGUI/HierarchyToolbarModel.h>\n#include <iiGUI/HierarchyToolbarLayout.h>\n#include <iiGUI/NativeToolbarButtonView.h>\n#include <iiGUI/NativeListFooterView.h>\n#include <iiGUI/SvgImageCache.h>\n";
        if (owned_list_panel_runtime) result += "#include <LVUI/NativeListPanel.h>\n#include <iiGUI/ListViewport.h>\n#include <iiGUI/ListDrag.h>\n#include <LVUI/NativeListView.h>\n#include <iiGUI/NativeListPanelView.h>\n#include <iiGUI/NativeListView.h>\n#include <iiGUI/ListItemDrawing.h>\n#include <iiGUI/SrgbColor.h>\n#include <iiGUI/NativeInputFieldView.h>\n#include <iiGUI/PanelLayout.h>\n#include <iiGUI/NativeListToolbarView.h>\n#include <iiGUI/NativeListFooterView.h>\n#include <iiGUI/SvgImageCache.h>\n";
        if (owned_list_item_runtime) result += "#include <LVUI/NativeListItem.h>\n#include <iiGUI/NativeListItemView.h>\n#include <iiGUI/SvgImageCache.h>\n";
        if (owned_list_item_composite_runtime) result += "#include <LVUI/NativeListItemComposite.h>\n#include <iiGUI/SrgbColor.h>\n#include <iiGUI/SvgImageCache.h>\n#include <iiGUI/NativeCheckBoxView.h>\n#include <iiGUI/NativeToggleSwitchView.h>\n#include <iiGUI/NativeListItemView.h>\n#include <iiGUI/NativeListFooterView.h>\n#include <iiGUI/NativeListItemSelectorView.h>\n#include <iiGUI/NativeComboBoxView.h>\n#include <iiGUI/NativeMenuView.h>\n#include <iiGUI/NativeMenuItemView.h>\n#include <iiGUI/ListItemSegmentsModel.h>\n#include <iiGUI/NativeListItemSegmentsView.h>\n#include <iiGUI/NativeInputFieldView.h>\n#include <iiGUI/NativeTextLayout.h>\n#include <iiGUI/NativeStepperView.h>\n#include <iiGUI/NativeListItemCompositeView.h>\n#include <iiGUI/ListItemDetailActionsLayout.h>\n#include <iiGUI/ListItemDetailControlsLayout.h>\n#include <iiGUI/ListItemTaskLayout.h>\n#include <iiGUI/ListItemPreviewRowLayout.h>\n#include <iiGUI/ListItemFormLayout.h>\n#include <iiGUI/ListItemQuantityLayout.h>\n#include <iiGUI/ListItemMenuModel.h>\n";
        if (owned_list_item_selector_runtime) result += "#include <LVUI/NativeListItemSelector.h>\n#include <iiGUI/NativeListItemSelectorView.h>\n#include <iiGUI/NativeComboBoxView.h>\n#include <iiGUI/NativeMenuView.h>\n#include <iiGUI/NativeMenuItemView.h>\n#include <iiGUI/SvgImageCache.h>\n";
        if (owned_list_toolbar_runtime) result += "#include <iiGUI/NativeListToolbarView.h>\n";
        if (owned_list_footer_runtime) result += "#include <LVUI/NativeListFooter.h>\n#include <iiGUI/NativeListFooterView.h>\n#include <iiGUI/SvgImageCache.h>\n";
        if (owned_toolbar_button_runtime) result += "#include <LVUI/NativeToolbarButton.h>\n#include <iiGUI/NativeToolbarButtonView.h>\n#include <iiGUI/SvgImageCache.h>\n";
        if (owned_table_runtime) result += "#include <LVUI/NativeTable.h>\n#include <iiGUI/NativeTableView.h>\n#include <iiGUI/TableViewport.h>\n#include <iiGUI/NativeInputFieldView.h>\n#include <iiGUI/NativeMenuView.h>\n#include <iiGUI/NativeMenuItemView.h>\n#include <iiGUI/TableHeaderPresentation.h>\n#include <iiGUI/TableViewGroup.h>\n#include <iiGUI/SvgImageCache.h>\n";
        if (owned_table_row_runtime) result += "#include <LVUI/NativeTableRow.h>\n#include <iiGUI/NativeTableRowView.h>\n#include <iiGUI/NativeTableCellItemView.h>\n#include <iiGUI/TableCellItemModel.h>\n#include <iiGUI/TableRowViewModel.h>\n";
        if (owned_table_cell_item_runtime) result += "#include <LVUI/NativeTableCellItem.h>\n#include <iiGUI/NativeTableCellItemView.h>\n#include <iiGUI/TableCellItemModel.h>\n";
        if (owned_table_header_runtime) result += "#include <LVUI/NativeTableHeader.h>\n#include <iiGUI/NativeTableHeaderView.h>\n#include <iiGUI/TableViewport.h>\n#include <iiGUI/TableHeaderModel.h>\n#include <iiGUI/TableHeaderPresentation.h>\n#include <iiGUI/TableViewGroup.h>\n#include <iiGUI/SvgImageCache.h>\n";
        if (owned_menu_item_runtime) result += "#include <LVUI/NativeMenuItem.h>\n#include <iiGUI/NativeMenuItemView.h>\n#include <iiGUI/SvgImageCache.h>\n";
        if (owned_divider_runtime) result += "#include <iiGUI/NativeDivider.h>\n";
        if (owned_focus_ring_runtime) result += "#include <iiGUI/NativeFocusRingView.h>\n";
        if (owned_window_noise_runtime) result += "#include <iiGUI/NativeWindowNoiseTextureView.h>\n";
        if (owned_wheel_guard_runtime) result += "#include <iiGUI/NativeWheelScrollGuard.h>\n";
        if (owned_image_runtime) result += "#include <iiGUI/NativeImageView.h>\n";
        if (owned_icon_runtime) result += "#include <iiGUI/NativeIconView.h>\n";
        if (owned_icon_runtime || owned_list_toolbar_runtime) result += "#include <LVUI/NativeAuthoredIcons.h>\n#include <iiGUI/SvgImageCache.h>\n";
        if (image_resolver_runtime) result += "#include <LVUI/NativeImageResolver.h>\n";
        if (native_runtime) result += "#include <LVUI/NativeValues.h>\n#include <LVUI/NativeBackendFactories.h>\n";
        if (localization_runtime) result += "#include <LVUI/NativeAppStrings.h>\n";
        if (component_input_runtime) result += "#include <LVUI/NativeComponentInputs.h>\n";
        if (object_input_runtime) result += "#include <LVUI/NativeObjectInputs.h>\n";
        if (method_runtime) result += "#include <LVUI/NativeMethods.h>\n";
        if (query_runtime) result += "#include <LVUI/NativeQueries.h>\n";
        if (query_connection_runtime) result += "#include <LVUI/NativeQueryConnections.h>\n";
        if (object_value_runtime) result += "#include <LVUI/NativeObjects.h>\n";
        if (inferred_runtime) result += "#include <LVUI/NativeInferredValues.h>\n";
        if (function_variable_runtime) result += "#include <LVUI/NativeFunctionVariable.h>\n";
        if (collection_runtime) result += "#include <LVUI/NativeCollection.h>\n#include <iiGUI/VirtualViewport.h>\n#include <iiGUI/VirtualCollectionPlanner.h>\n#include <iiGUI/VirtualMeasurements.h>\n#include <LVUI/NativeVirtualCollection.h>\n#include <LVUI/NativeMeasuredCollection.h>\n#include <LVUI/NativeCollectionWidgets.h>\n#include <iiGUI/CollectionWidgetNavigation.h>\n";
        if(panel_paint_runtime)result+="#include <iiGUI/PanelPainter.h>\n";
        if(widget_color_runtime)result+="#include <LVUI/NativeWidgetColors.h>\n";
        if(widget_layout_runtime)result+="#include <LVUI/NativeWidgetLayout.h>\n";
        if(window_runtime)result+="#include <LVUI/NativeWindowValues.h>\n";
        if(!window.palette.empty())result+="#include <array>\n#include <stdexcept>\n";
        for (const auto& include : options.includes) {
            if (include.empty() || include.front() == '/' || include.find("..") != include.npos ||
                !std::ranges::all_of(include, [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                    (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.' || c == '/'; }))
                error({}, "Invalid relative C++ include path");
            result += "#include \"" + include + "\"\n";
        }
        result+="\nnamespace ii::gui { class RadioGroups; }\nnamespace lvui { class NavigationContext; }\n";
        const auto options_value = [&](const WindowOutput& value) {
            return "ii::gui::HostOptions{.title = " + literal(value.title) +
                ", .size = {" + decimal(value.width) + ", " + decimal(value.height) + "}, .minimum_size = {" + decimal(value.minimum_width) + ", " + decimal(value.minimum_height) + "}" +
                (value.custom_chrome ? ", .chrome = ii::gui::WindowChrome::custom" : "") + "}";
        };
        result += "\nnamespace lvui_generated {\n";
        if (owned_list_item_composite_runtime) result +=
            "namespace detail_list_composite {\n"
            "using namespace lvui;\n"
            "using namespace lvui::native;\n"
            "using NativeInputField=InputFieldBinding<ii::gui::BasicNativeInputField,ii::gui::BasicTextInput,ii::gui::BasicTextFieldMaterial,ii::gui::NativeTextAffinity>;\n"
            "using NativeComboBox=ComboBoxBinding<ii::gui::BasicComboBox,ii::gui::BasicNativeComboBox,ii::gui::Widget>;\n"
            "using MenuItemPort=MenuItemPortValue<ii::gui::Widget,ii::gui::WidgetSnapshot>;\n"
            "using MenuSectionPort=MenuSectionPortValue<ii::gui::Widget,ii::gui::WidgetSnapshot>;\n"
            "using NativeMenuItem=MenuItemBinding<ii::gui::BasicMenuItem,ii::gui::BasicNativeMenuItem,ii::gui::CachedSvgDecoder,ii::gui::Widget,ii::gui::WidgetSnapshot>;\n"
            "using NativeMenu=MenuBinding<ii::gui::BasicMenu,ii::gui::BasicNativeMenu,ii::gui::Widget,ii::gui::WidgetSnapshot>;\n"
            "using ListItemSelector=ii::gui::BasicListItemSelector<ListItemSelectorModelPolicy>;\n"
            "using NativeListItemSelector=ListItemSelectorBinding<ii::gui::BasicListItemSelector,ii::gui::BasicNativeListItemSelector,NativeComboBox,NativeMenu,NativeMenuItem,ii::gui::RasterImage>;\n"
            "using SegmentButtonEvent=ButtonEventValue<ii::gui::Widget>;\n"
            "using SegmentButtonMethod=ButtonMethodValue<SegmentButtonEvent>;\n"
            "using SegmentButtonRegistry=ButtonMethodRegistryValue<SegmentButtonEvent>;\n"
            "using SegmentButton=ButtonBinding<ii::gui::BasicButton,ii::gui::BasicNativeButton,ii::gui::CachedSvgDecoder,ii::gui::Widget>;\n"
            "using SegmentGroup=SegmentedControlBinding<ii::gui::BasicSegmentedControl,ii::gui::BasicNativeSegmentedControl,SegmentButtonMethod,SegmentButtonRegistry,ButtonMethodInvoker>;\n"
            "using ListItemSegments=ii::gui::BasicListItemSegments<ListItemSegmentsModelPolicy>;\n"
            "using NativeListItemSegments=ListItemSegmentsBinding<ii::gui::BasicListItemSegments,ii::gui::BasicNativeListItemSegments,SegmentButton,SegmentGroup,ii::gui::RasterImage>;\n"
            "using ToolbarButtonInput=ToolbarButtonInputValue<ii::gui::RasterImage>;\n"
            "using NativeToolbarButton=ToolbarButtonBinding<ii::gui::BasicToolbarButton,ii::gui::BasicNativeToolbarButton,ii::gui::CachedSvgDecoder>;\n"
            "using NativeStepper=StepperBinding<ii::gui::BasicButton,ii::gui::BasicStepper,ii::gui::BasicNativeStepper,ii::gui::Widget>;\n"
            "using NativeListFooter=ListFooterBinding<ii::gui::BasicListFooter,ii::gui::BasicNativeListFooter,ii::gui::BasicToolbarButton,ii::gui::BasicNativeToolbarButton,ii::gui::CachedSvgDecoder>;\n"
            "using ListItemModel=ListItemModelBinding<ii::gui::BasicListItem>;\n"
            "using ListItemDrawing=ListItemDrawingBinding<ii::gui::BasicListItemDrawing,ListItemModel,ii::gui::SrgbColorConverter>;\n"
            "using NativeListItem=ListItemBinding<ii::gui::BasicListItem,ii::gui::BasicListItemDrawing,ii::gui::BasicNativeListItem,ii::gui::CachedSvgDecoder,ii::gui::SrgbColorConverter,ii::gui::ListItemDrawingPort>;\n"
            "using CompositeCheckBox=CheckBoxBinding<ii::gui::BasicCheckBox,ii::gui::BasicNativeCheckBox,ii::gui::CachedSvgDecoder>;\n"
            "using CompositeToggleSwitch=ToggleSwitchBinding<ii::gui::BasicToggleSwitch,ii::gui::BasicNativeToggleSwitch>;\n"
            "using IconCache=NativeIconCache<ii::gui::CachedSvgDecoder>;\n"
            "using ListItemDetailActions=lvui::ListItemDetailActionsBinding<ii::gui::BasicListItemDetailActions>;\n"
            "using ListItemDetailControls=lvui::ListItemDetailControlsBinding<ii::gui::BasicListItemDetailControls>;\n"
            "using ListItemTask=lvui::ListItemTaskBinding<ii::gui::BasicListItemTask>;\n"
            "using ListItemPreviewRow=lvui::ListItemPreviewRowBinding<ii::gui::BasicListItemPreviewRow>;\n"
            "using ListItemForm=lvui::ListItemFormBinding<ii::gui::BasicListItemForm>;\n"
            "using ListItemQuantity=lvui::ListItemQuantityBinding<ii::gui::BasicListItemQuantity>;\n"
            "using ListItemMenu=lvui::ListItemMenuBinding<ii::gui::BasicListItemMenu>;\n"
            "using Components=ii::gui::ListItemCompositeComponents<NativeInputField,NativeComboBox,NativeMenuItem,NativeMenu,ListItemSelector,NativeListItemSelector,ListItemSegments,NativeListItemSegments,NativeToolbarButton,NativeStepper,NativeListFooter,ListItemModel,ListItemDrawing,NativeListItem,CompositeCheckBox,CompositeToggleSwitch,MenuItemPort,MenuSectionPort,IconCache,ToolbarButtonInput,ListItemDetailActions,ListItemDetailControls,ListItemTask,ListItemPreviewRow,ListItemForm,ListItemQuantity,ListItemMenu>;\n"
            "using Input=lvui::native::ListItemCompositeInputValue<ii::gui::RasterImage>;\n"
            "using Context=lvui::native::ListItemContextValue<ii::gui::RasterImage>;\n"
            "using ViewFactory=lvui::native::ListItemContextViewFactoryValue<ii::gui::WidgetBuilder,ii::gui::RasterImage>;\n"
            "using Views=lvui::native::NativeListItemCompositeViewsValue<ii::gui::WidgetBuilder,ii::gui::WidgetFactory,ii::gui::RasterImage>;\n"
            "using NativeListItemComposite=lvui::native::ListItemCompositeBinding<ii::gui::BasicNativeListItemComposite,ii::gui::RasterImage,ii::gui::WidgetBuilder,ii::gui::WidgetFactory,Components>;\n"
            "} // namespace detail_list_composite\n"
            ;
        if (owned_table_runtime) result +=
            "namespace detail_table {\n"
            "using Field=lvui::native::InputFieldBinding<ii::gui::BasicNativeInputField,ii::gui::BasicTextInput,ii::gui::BasicTextFieldMaterial,ii::gui::NativeTextAffinity>;\n"
            "using Menu=lvui::native::MenuBinding<ii::gui::BasicMenu,ii::gui::BasicNativeMenu,ii::gui::Widget,ii::gui::WidgetSnapshot>;\n"
            "using Item=lvui::native::MenuItemBinding<ii::gui::BasicMenuItem,ii::gui::BasicNativeMenuItem,ii::gui::CachedSvgDecoder,ii::gui::Widget,ii::gui::WidgetSnapshot>;\n"
            "using Header=lvui::native::TableHeaderPresentationBinding<ii::gui::BasicTableHeaderPresentation,ii::gui::CachedSvgDecoder>;\n"
            "using Group=lvui::native::TableViewGroupBinding<ii::gui::BasicNativeTableViewGroup,ii::gui::WidgetBuilder,ii::gui::Rect>;\n"
            "using NativeTable=lvui::native::TableBinding<ii::gui::BasicNativeTable,Field,Menu,Item,Header,Group,ii::gui::WidgetBuilder,ii::gui::Rect,ii::gui::RasterImage,lvui::TableViewportBinding<ii::gui::BasicTableViewportSource>>;\n"
            "} // namespace detail_table\n";
        if (owned_tooltip_runtime) result +=
            "namespace detail_tooltip {\n"
            "using Model=lvui::TooltipModelBinding<ii::gui::BasicTooltip>;\n"
            "using Controller=lvui::TooltipControllerBinding<ii::gui::BasicTooltipController,Model>;\n"
            "using MaterialModel=ii::gui::BasicMaterial<lvui::MaterialModelPolicy>;\n"
            "using Material=lvui::native::MaterialBinding<ii::gui::BasicMaterial,ii::gui::BasicMaterialGradient,ii::gui::BasicNativeMaterial,ii::gui::Widget>;\n"
            "using Outline=lvui::TooltipOutlineBinding<ii::gui::BasicTooltipOutline>;\n"
            "using Style=lvui::native::TooltipPresentationStyleValue<ii::gui::MotionPreferences>;\n"
            "using Input=lvui::native::TooltipInputValue<ii::gui::MotionPreferences>;\n"
            "using Presentation=lvui::native::TooltipPresentationBinding<ii::gui::BasicNativeTooltipPresentation,ii::gui::MotionPreferences,MaterialModel,Material,Outline>;\n"
            "using Scheduler=ii::gui::TooltipScheduler;\n"
            "using Target=lvui::native::TooltipTargetBinding<ii::gui::BasicNativeTooltipTarget,ii::gui::MotionPreferences,Model,Controller,Presentation,Scheduler>;\n"
            "using NativeTooltip=lvui::native::TooltipBinding<ii::gui::BasicNativeTooltip,ii::gui::MotionPreferences,Model,Controller,Presentation,Scheduler,Target>;\n"
            "} // namespace detail_tooltip\n";
        if (owned_table_header_runtime) result +=
            "namespace detail_table_header {\n"
            "using Model=lvui::TableHeaderModelBinding<ii::gui::BasicTableHeader,lvui::TableViewportBinding<ii::gui::BasicTableViewportSource>>;\n"
            "using Presentation=lvui::native::TableHeaderPresentationBinding<ii::gui::BasicTableHeaderPresentation,ii::gui::CachedSvgDecoder>;\n"
            "using Group=lvui::native::TableViewGroupBinding<ii::gui::BasicNativeTableViewGroup,ii::gui::WidgetBuilder,ii::gui::Rect>;\n"
            "using NativeTableHeader=lvui::native::TableHeaderBinding<ii::gui::BasicNativeTableHeader,Model,Presentation,Group,ii::gui::WidgetBuilder,ii::gui::Rect,lvui::TableViewportBinding<ii::gui::BasicTableViewportSource>>;\n"
            "} // namespace detail_table_header\n";
        if (owned_code_editor_runtime) result +=
            "namespace detail_code_editor {\n"
            "using NativeCodeEditor=lvui::native::CodeEditorBinding<ii::gui::BasicNativeCodeEditor>;\n"
            "} // namespace detail_code_editor\n";
        if (owned_list_panel_runtime) result +=
            "namespace detail_list_panel {\n"
            "using ButtonModel=ii::gui::BasicToolbarButton<lvui::native::ToolbarButtonModelPolicy>;\n"
            "using FooterModel=ii::gui::BasicListFooter<lvui::native::ListFooterModelPolicy<ButtonModel>>;\n"
            "using Footer=lvui::native::ListFooterBinding<ii::gui::BasicListFooter,ii::gui::BasicNativeListFooter,ii::gui::BasicToolbarButton,ii::gui::BasicNativeToolbarButton,ii::gui::CachedSvgDecoder>;\n"
            "using ToolbarModel=ii::gui::BasicListToolbar<lvui::native::ListToolbarModelPolicy>;\n"
            "using Toolbar=lvui::native::ListToolbarBinding<ii::gui::BasicListToolbar,ii::gui::BasicNativeListToolbar,ii::gui::CachedSvgDecoder>;\n"
            "using Layout=lvui::PanelLayoutBinding<ii::gui::BasicPanelLayout,lvui::ListPanelConfiguration>;\n"
            "using Field=lvui::native::InputFieldBinding<ii::gui::BasicNativeInputField,ii::gui::BasicTextInput,ii::gui::BasicTextFieldMaterial,ii::gui::NativeTextAffinity>;\n"
            "using ItemModel=lvui::native::ListItemModelBinding<ii::gui::BasicListItem>;\n"
            "using Drawing=lvui::native::ListItemDrawingBinding<ii::gui::BasicListItemDrawing,ItemModel,ii::gui::SrgbColorConverter>;\n"
            "using List=lvui::native::ListViewBinding<ii::gui::BasicNativeListView,Drawing,Field,ii::gui::RasterImage,ii::gui::CachedSvgDecoder,ii::gui::WidgetBuilder,lvui::ListViewportBinding<ii::gui::BasicListViewportSource>,lvui::ListDragBinding<ii::gui::BasicListDrag,lvui::ListViewportBinding<ii::gui::BasicListViewportSource>>>;\n"
            "using NativeListPanel=lvui::native::ListPanelBinding<ii::gui::BasicNativeListPanel,Layout,ToolbarModel,FooterModel,List,Toolbar,Footer,Field,ii::gui::WidgetBuilder>;\n"
            "} // namespace detail_list_panel\n";
        if (owned_hierarchy_panel_runtime) result +=
            "namespace detail_hierarchy_panel {\n"
            "using ItemModel=lvui::HierarchyItemModelBinding<ii::gui::BasicHierarchyItem>;\n"
            "using Item=lvui::native::HierarchyItemBinding<ii::gui::BasicNativeHierarchyItem,ii::gui::BasicHierarchyItem,ii::gui::CachedSvgDecoder>;\n"
            "using List=lvui::native::HierarchyListBinding<ii::gui::BasicNativeHierarchyList,ItemModel,Item,ii::gui::RasterImage,ii::gui::CachedSvgDecoder,ii::gui::WidgetBuilder,lvui::HierarchyViewportBinding<ii::gui::BasicHierarchyViewportSource>,lvui::HierarchyDragBinding<ii::gui::BasicHierarchyDrag,lvui::HierarchyViewportBinding<ii::gui::BasicHierarchyViewportSource>>>;\n"
            "using ButtonModel=ii::gui::BasicToolbarButton<lvui::native::ToolbarButtonModelPolicy>;\n"
            "using Button=lvui::native::ToolbarButtonBinding<ii::gui::BasicToolbarButton,ii::gui::BasicNativeToolbarButton,ii::gui::CachedSvgDecoder>;\n"
            "using ToolbarModel=lvui::HierarchyToolbarModelBinding<ii::gui::BasicHierarchyToolbar>;\n"
            "using ToolbarLayout=lvui::native::HierarchyToolbarLayoutBinding<ii::gui::BasicHierarchyToolbarLayout,ButtonModel>;\n"
            "using Toolbar=lvui::native::HierarchyToolbarBinding<ii::gui::BasicNativeHierarchyToolbar,ToolbarModel,ToolbarLayout,ButtonModel,Button,ii::gui::RasterImage,ii::gui::CachedSvgDecoder>;\n"
            "using FooterModel=ii::gui::BasicListFooter<lvui::native::ListFooterModelPolicy<ButtonModel>>;\n"
            "using Footer=lvui::native::ListFooterBinding<ii::gui::BasicListFooter,ii::gui::BasicNativeListFooter,ii::gui::BasicToolbarButton,ii::gui::BasicNativeToolbarButton,ii::gui::CachedSvgDecoder>;\n"
            "using Layout=lvui::PanelLayoutBinding<ii::gui::BasicPanelLayout,lvui::HierarchyPanelConfiguration>;\n"
            "using NativeHierarchyPanel=lvui::native::HierarchyPanelBinding<ii::gui::BasicNativeHierarchyPanel,Layout,ToolbarLayout,FooterModel,List,Toolbar,Footer,ii::gui::WidgetBuilder>;\n"
            "} // namespace detail_hierarchy_panel\n";
        if (owned_hierarchy_list_runtime) result +=
            "namespace detail_hierarchy_list {\n"
            "using ItemModel=lvui::HierarchyItemModelBinding<ii::gui::BasicHierarchyItem>;\n"
            "using Item=lvui::native::HierarchyItemBinding<ii::gui::BasicNativeHierarchyItem,ii::gui::BasicHierarchyItem,ii::gui::CachedSvgDecoder>;\n"
            "using NativeHierarchyList=lvui::native::HierarchyListBinding<ii::gui::BasicNativeHierarchyList,ItemModel,Item,ii::gui::RasterImage,ii::gui::CachedSvgDecoder,ii::gui::WidgetBuilder,lvui::HierarchyViewportBinding<ii::gui::BasicHierarchyViewportSource>,lvui::HierarchyDragBinding<ii::gui::BasicHierarchyDrag,lvui::HierarchyViewportBinding<ii::gui::BasicHierarchyViewportSource>>>;\n"
            "} // namespace detail_hierarchy_list\n";
        if (owned_hierarchy_toolbar_runtime) result +=
            "namespace detail_hierarchy_toolbar {\n"
            "using Model=lvui::HierarchyToolbarModelBinding<ii::gui::BasicHierarchyToolbar>;\n"
            "using ButtonModel=ii::gui::BasicToolbarButton<lvui::native::ToolbarButtonModelPolicy>;\n"
            "using Button=lvui::native::ToolbarButtonBinding<ii::gui::BasicToolbarButton,ii::gui::BasicNativeToolbarButton,ii::gui::CachedSvgDecoder>;\n"
            "using Layout=lvui::native::HierarchyToolbarLayoutBinding<ii::gui::BasicHierarchyToolbarLayout,ButtonModel>;\n"
            "using NativeHierarchyToolbar=lvui::native::HierarchyToolbarBinding<ii::gui::BasicNativeHierarchyToolbar,Model,Layout,ButtonModel,Button,ii::gui::RasterImage,ii::gui::CachedSvgDecoder>;\n"
            "} // namespace detail_hierarchy_toolbar\n";
        if (owned_hierarchy_item_runtime) result +=
            "namespace detail_hierarchy_item {\n"
            "using NativeHierarchyItem=lvui::native::HierarchyItemBinding<ii::gui::BasicNativeHierarchyItem,ii::gui::BasicHierarchyItem,ii::gui::CachedSvgDecoder>;\n"
            "} // namespace detail_hierarchy_item\n";
        if (owned_mobile_tab_runtime) result +=
            "namespace detail_mobile_tab { using NativeMobileTab=lvui::native::MobileTabBinding<ii::gui::BasicNativeMobileTab,ii::gui::CachedSvgDecoder>; }\n";
        if (owned_mobile_navigation_runtime) result +=
            "namespace detail_mobile_navigation {\n"
            "using Model=lvui::MobileNavigationModelBinding<ii::gui::BasicMobileNavigationModel>;\n"
            "using Motion=lvui::MobileNavigationMotionBinding<ii::gui::BasicMobileNavigationMotion>;\n"
            "using Material=lvui::native::MaterialBinding<ii::gui::BasicMaterial,ii::gui::BasicMaterialGradient,ii::gui::BasicNativeMaterial,ii::gui::Widget>;\n"
            "using NativeMobileNavigation=lvui::native::MobileNavigationBinding<ii::gui::BasicNativeMobileNavigation,Model,Motion,Material,ii::gui::CachedSvgDecoder>;\n"
            "} // namespace detail_mobile_navigation\n";
        if (owned_color_picker_runtime) result +=
            "namespace detail_color_picker {\n"
            "using Field=lvui::native::InputFieldBinding<ii::gui::BasicNativeInputField,ii::gui::BasicTextInput,ii::gui::BasicTextFieldMaterial,ii::gui::NativeTextAffinity>;\n"
            "using Layout=lvui::ColorPickerLayoutBinding<ii::gui::BasicColorPickerLayout>;\n"
            "using Ramp=lvui::ColorChannelSurfaceBinding<ii::gui::BasicColorChannelSurface>;\n"
            "using Surface=lvui::native::ColorSurfaceBinding<ii::gui::BasicColorSurface,ii::gui::BasicNativeColorSurface>;\n"
            "using Channel=lvui::native::ColorChannelBinding<ii::gui::BasicNativeColorChannel,Ramp,Surface,Field>;\n"
            "using Values=lvui::native::ColorPickerValuesBinding<ii::gui::BasicNativeColorPickerValues,Field>;\n"
            "using Button=lvui::native::ButtonBinding<ii::gui::BasicButton,ii::gui::BasicNativeButton,ii::gui::CachedSvgDecoder,ii::gui::Widget>;\n"
            "using NativeColorPicker=lvui::native::ColorPickerBinding<ii::gui::BasicNativeColorPicker,Layout,Surface,Channel,Values,Button,ii::gui::RasterImage,ii::gui::WidgetColor>;\n"
            "} // namespace detail_color_picker\n";
        if (owned_table_row_runtime) result +=
            "namespace detail_table_row {\n"
            "using Field=lvui::native::InputFieldBinding<ii::gui::BasicNativeInputField,ii::gui::BasicTextInput,ii::gui::BasicTextFieldMaterial,ii::gui::NativeTextAffinity>;\n"
            "using CellModel=lvui::TableCellItemModelBinding<ii::gui::BasicTableCellItem>;\n"
            "using RowModel=lvui::TableRowViewModelBinding<ii::gui::BasicTableRowView,CellModel>;\n"
            "using Cell=lvui::native::TableCellItemBinding<ii::gui::BasicTableCellItem,ii::gui::BasicNativeTableCellItem,Field>;\n"
            "using NativeTableRow=lvui::native::TableRowBinding<ii::gui::BasicNativeTableRow,RowModel,Cell>;\n"
            "} // namespace detail_table_row\n";
        if (owned_table_cell_item_runtime) result +=
            "namespace detail_table_cell {\n"
            "using Field=lvui::native::InputFieldBinding<ii::gui::BasicNativeInputField,ii::gui::BasicTextInput,ii::gui::BasicTextFieldMaterial,ii::gui::NativeTextAffinity>;\n"
            "using NativeTableCellItem=lvui::native::TableCellItemBinding<ii::gui::BasicTableCellItem,ii::gui::BasicNativeTableCellItem,Field>;\n"
            "} // namespace detail_table_cell\n";
        if (owned_search_field_runtime) result +=
            "namespace detail_search_field {\n"
            "using Field=lvui::native::InputFieldBinding<ii::gui::BasicNativeInputField,ii::gui::BasicTextInput,ii::gui::BasicTextFieldMaterial,ii::gui::NativeTextAffinity>;\n"
            "using Material=ii::gui::BasicTextFieldMaterial<lvui::native::text_field_material::Policy>;\n"
            "using NativeSearchField=lvui::native::SearchFieldBinding<ii::gui::BasicNativeSearchField,Field,Material,ii::gui::CachedSvgDecoder,ii::gui::NativeTextAffinity>;\n"
            "} // namespace detail_search_field\n";
        if (owned_text_editor_runtime) result +=
            "namespace detail_text_editor {\n"
            "using Field=lvui::native::InputFieldBinding<ii::gui::BasicNativeInputField,ii::gui::BasicTextInput,ii::gui::BasicTextFieldMaterial,ii::gui::NativeTextAffinity>;\n"
            "using NativeTextEditor=lvui::native::TextEditorBinding<ii::gui::BasicNativeTextEditor,Field,ii::gui::NativeTextAffinity>;\n"
            "} // namespace detail_text_editor\n";
        if (owned_input_field_runtime) result +=
            "namespace detail_input {\n"
            "using NativeInputField=lvui::native::InputFieldBinding<ii::gui::BasicNativeInputField,ii::gui::BasicTextInput,ii::gui::BasicTextFieldMaterial,ii::gui::NativeTextAffinity>;\n"
            "} // namespace detail_input\n";
        if (owned_list_item_selector_runtime) result +=
            "namespace detail_selector {\n"
            "using Combo=lvui::native::ComboBoxBinding<ii::gui::BasicComboBox,ii::gui::BasicNativeComboBox,ii::gui::Widget>;\n"
            "using Menu=lvui::native::MenuBinding<ii::gui::BasicMenu,ii::gui::BasicNativeMenu,ii::gui::Widget,ii::gui::WidgetSnapshot>;\n"
            "using Item=lvui::native::MenuItemBinding<ii::gui::BasicMenuItem,ii::gui::BasicNativeMenuItem,ii::gui::CachedSvgDecoder,ii::gui::Widget,ii::gui::WidgetSnapshot>;\n"
            "using NativeListItemSelector=lvui::native::ListItemSelectorBinding<ii::gui::BasicListItemSelector,ii::gui::BasicNativeListItemSelector,Combo,Menu,Item,ii::gui::RasterImage>;\n"
            "} // namespace detail_selector\n";
        if (owned_tabbar_runtime) result +=
            "namespace detail_tab {\n"
            "using TabGroupPort=lvui::native::TabGroupPortValue<ii::gui::Widget,ii::gui::WidgetSnapshot>;\n"
            "using TabRegistrationPort=lvui::native::TabRegistrationPortValue<ii::gui::Widget,ii::gui::WidgetSnapshot>;\n"
            "using TabSectionPort=lvui::native::TabSectionPortValue<ii::gui::Widget,ii::gui::WidgetSnapshot>;\n"
            "using TabCapture=lvui::native::TabCaptureValue<ii::gui::Widget,ii::gui::WidgetSnapshot>;\n"
            "using TabRepeatedRows=lvui::native::TabRepeatedRowsValue<ii::gui::Widget,ii::gui::WidgetSnapshot>;\n"
            "using TabSectionAccess=lvui::native::TabSectionAccessValue<ii::gui::Widget,ii::gui::WidgetSnapshot>;\n"
            "} // namespace detail_tab\n";
        if (owned_menu_runtime) result +=
            "namespace detail_menu {\n"
            "using MenuItemPort=lvui::native::MenuItemPortValue<ii::gui::Widget,ii::gui::WidgetSnapshot>;\n"
            "using MenuSectionPort=lvui::native::MenuSectionPortValue<ii::gui::Widget,ii::gui::WidgetSnapshot>;\n"
            "using MenuSectionAccess=lvui::native::MenuSectionAccessValue<ii::gui::Widget,ii::gui::WidgetSnapshot>;\n"
            "using MenuRegistrationPort=lvui::native::MenuRegistrationPortValue<ii::gui::Widget,ii::gui::WidgetSnapshot>;\n"
            "using MenuCapture=lvui::native::MenuCaptureValue<ii::gui::Widget,ii::gui::WidgetSnapshot>;\n"
            "using MenuRepeatedRows=lvui::native::MenuRepeatedRowsValue<ii::gui::Widget,ii::gui::WidgetSnapshot>;\n"
            "using NativeMenu=lvui::native::MenuBinding<ii::gui::BasicMenu,ii::gui::BasicNativeMenu,ii::gui::Widget,ii::gui::WidgetSnapshot>;\n"
            "} // namespace detail_menu\n";
        result += "struct " + options.name + " final {\n";
        if (!options.view_factory_only) result += "    static ii::gui::HostOptions options() {\n        return " + options_value(window) + ";\n    }\n";
        else result += "    static ii::gui::WindowGeometry window_geometry() {\n        return {.title = " + literal(window.title) +
            ", .width = " + decimal(window.width) + ", .height = " + decimal(window.height) +
            ", .minimum_width = " + decimal(window.minimum_width) + ", .minimum_height = " + decimal(window.minimum_height) + "};\n    }\n";
        if (owned_icon_runtime || owned_list_toolbar_runtime) result += R"LVUI(    // Application-side composition of the LVUI asset source and iiGUI decoder.
    struct BuiltinThenDesignIconSource final : lvui::IconSource {
        lvui::IconResult<std::shared_ptr<const lvui::IconAsset>> resolve(std::string_view name) const override {
            auto builtin = lvui::builtin_icon_catalog();
            if (!builtin) return std::unexpected(builtin.error());
            auto asset = (*builtin)->resolve(name);
            if (asset || asset.error().code != lvui::IconErrorCode::not_found) return asset;
            return lvui::design_icon_catalog()->resolve(name);
        }
        std::uint64_t revision() const noexcept override { return 1; }
    };
    struct IconSourceAdapter final : ii::gui::ButtonIconSource {
        std::shared_ptr<const lvui::IconSource> source;
        explicit IconSourceAdapter(std::shared_ptr<const lvui::IconSource> value) : source(std::move(value)) {}
        std::uint64_t revision() const noexcept override { return source->revision(); }
        ii::core::Result<std::shared_ptr<const ii::gui::RasterImage>> resolve(std::string_view name, double size) const override {
            return lvui::native::icon_image_for_logical_size<ii::gui::CachedSvgDecoder>(source, name, size);
        }
    };
    struct IconSourceCache final {
        std::shared_ptr<const lvui::IconSource> source;
        std::shared_ptr<const ii::gui::ButtonIconSource> adapter;
        std::shared_ptr<const ii::gui::ButtonIconSource> get(std::shared_ptr<const lvui::IconSource> value, bool builtin_first = false) {
            if (!value && builtin_first) { static const auto fallback = std::make_shared<BuiltinThenDesignIconSource>(); value = fallback; }
            if (!value) value = lvui::design_icon_catalog();
            if (!adapter || source != value) { source = std::move(value); adapter = std::make_shared<IconSourceAdapter>(source); }
            return adapter;
        }
    };
)LVUI";
        if (owned_avatar_runtime) result += R"LVUI(    // App composition connects an LVUI design asset to iiGUI rasterization.
    static std::shared_ptr<const ii::gui::ImageRasterSource> avatar_placeholder_source() {
        static const auto source = ii::gui::make_avatar_placeholder_source(
            [](std::uint32_t pixels) -> ii::core::Result<std::shared_ptr<const ii::gui::RasterImage>> {
                auto catalog = lvui::native::avatar_placeholder_mask_catalog();
                if (!catalog) return std::unexpected(catalog.error());
                return lvui::native::icon_image<ii::gui::CachedSvgDecoder>(*catalog, "avatar-placeholder-mask", pixels);
            });
        return source;
    }
)LVUI";
        if (application_root) {
            result += "    static constexpr std::size_t scene_count = "+std::to_string(scene_windows.size())+";\n";
            result += "    static constexpr std::array<lvui::native::SceneDescriptor, scene_count> scenes{{\n";
            for (std::size_t index = 0; index < application_scene_ids.size(); ++index)
                result += "        {" + literal(application_scene_ids[index]) + ", " +
                          (application_scene_initial[index] ? "true" : "false") + "},\n";
            result += "    }};\n";
        }
        if (multiple_scenes) {
            if (scene_registry_runtime) {
                result += "    static constexpr std::array<bool, scene_count> initial_scenes{";
                for (std::size_t i = 0; i < application_scene_initial.size(); ++i)
                    result += std::string(i ? ", " : "") + (application_scene_initial[i] ? "true" : "false");
                result += "};\n";
            }
            result += "    static ii::core::Result<ii::gui::HostOptions> options_for_scene(std::size_t index) {\n        switch (index) {\n";
            for (std::size_t index=0; index<scene_windows.size(); ++index)
                result += "        case "+std::to_string(index)+": return "+options_value(scene_windows[index])+";\n";
            result += "        default: return ii::core::fail(ii::core::ErrorCode::out_of_range, \"Unknown application scene\");\n        }\n    }\n";
            bool palette_emitted=false;
            for (std::size_t index=0; index<scene_windows.size(); ++index) {
                auto appearance=scene_windows[index].appearance;
                const auto position=appearance.find("appearance()");
                if (position==std::string::npos) throw std::logic_error("Missing generated window appearance");
                appearance.replace(position,std::string("appearance()").size(),"appearance_scene_"+std::to_string(index)+"()");
                result += appearance;
                if (!palette_emitted && !scene_windows[index].palette.empty()) {
                    result += scene_windows[index].palette; palette_emitted=true;
                }
            }
            result += "    static ii::gui::WidgetAppearance appearance() { return appearance_scene_0(); }\n"
                "    static ii::core::Result<ii::gui::WidgetAppearance> appearance_for_scene(std::size_t index) {\n        switch (index) {\n";
            for (std::size_t index=0; index<scene_windows.size(); ++index)
                result += "        case "+std::to_string(index)+": return appearance_scene_"+std::to_string(index)+"();\n";
            result += "        default: return ii::core::fail(ii::core::ErrorCode::out_of_range, \"Unknown application scene\");\n        }\n    }\n";
        } else {
            result += window.appearance;
            result += window.palette;
        }
        result += type_imports;
        result += nested_views;
        if (!scope.empty()) result += scope_declaration(scope);
        result += value_initialization();
        if (application_root) {
            result += R"LVUI(    struct ApplicationOwner final : std::enable_shared_from_this<ApplicationOwner> {
        using Configure = std::function<void(Scope&)>;
        ApplicationOwner(Configure configure, lvui::native::BackendFactories factories)
            : configure_(std::move(configure)), factories_(std::move(factories)) {}
        ApplicationOwner(const ApplicationOwner&) = delete;
        ApplicationOwner& operator=(const ApplicationOwner&) = delete;
        ~ApplicationOwner() {
            if (scope_ || graph_) {
                auto result = retire();
                if (!result && (scope_ || graph_)) std::terminate();
            }
        }
        std::shared_ptr<Scope> scope() const {
            if (thread_ != std::this_thread::get_id() || phase_ != Phase::active) return {};
            return scope_;
        }
)LVUI";
            if(scene_registry_runtime) result += R"LVUI(        ii::core::Result<void> bind_scene_registry(const std::shared_ptr<lvui::native::SceneRegistry>& registry) {
            if (thread_ != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Application scope requires its owner thread");
            if (phase_ != Phase::created) return ii::core::fail(ii::core::ErrorCode::busy, "Application has already started");
            if (!registry) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Application scene registry is missing");
            scene_registry_ = registry;
            return {};
        }
)LVUI";
            if(application_close_runtime) result += R"LVUI(        ii::core::Result<void> bind_close_loop(ii::runtime::EventLoop& loop) {
            if (thread_ != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Application scope requires its owner thread");
            if (phase_ != Phase::created) return ii::core::fail(ii::core::ErrorCode::busy, "Application has already started");
            close_loop_ = &loop;
            return {};
        }
)LVUI";
            if(application_close_runtime && multiple_scenes) result += R"LVUI(        ii::core::Result<void> bind_close_failure_handler(std::function<void(const ii::core::Error&)> handler) {
            if (thread_ != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Application scope requires its owner thread");
            if (phase_ != Phase::created) return ii::core::fail(ii::core::ErrorCode::busy, "Application has already started");
            if (!handler) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Application close failure handler is missing");
            close_error_handler_ = std::move(handler);
            return {};
        }
)LVUI";
            if(application_close_runtime) {
                result += R"LVUI(        ii::core::Result<void> report_close_failure(const ii::core::Error& error) {
            if (thread_ != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Application scope requires its owner thread");
            if (!close_failure_) {
                close_failure_ = error;
)LVUI";
                if(multiple_scenes) result += "                try { auto handler = close_error_handler_; if (handler) handler(*close_failure_); } catch (...) {}\n";
                result += R"LVUI(            }
            if (close_loop_) close_loop_->request_stop();
            return {};
        }
)LVUI";
            }
            if(application_quit_runtime) result += R"LVUI(        ii::core::Result<void> bind_application_commands(const std::shared_ptr<lvui::native::ApplicationCommands>& commands) {
            if (thread_ != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Application scope requires its owner thread");
            if (phase_ != Phase::created) return ii::core::fail(ii::core::ErrorCode::busy, "Application has already started");
            if (!commands) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Application commands are missing");
            application_commands_ = commands;
            return {};
        }
)LVUI";
            if(scene_lifecycle_runtime) result += R"LVUI(        ii::core::Result<void> bind_scene_loop(ii::runtime::EventLoop& loop) {
            if (thread_ != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Application scope requires its owner thread");
            if (phase_ != Phase::created) return ii::core::fail(ii::core::ErrorCode::busy, "Application has already started");
            scene_loop_ = &loop;
            return {};
        }
        void report_scene_failure(const ii::core::Error& error) {
            if (!scene_failure_) scene_failure_ = error;
            if (scene_loop_) scene_loop_->request_stop();
        }
        ii::core::Result<void> scene_outcome() const {
            if (thread_ != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Application scope requires its owner thread");
            if (scene_failure_) return std::unexpected(*scene_failure_);
            return {};
        }
)LVUI";
            result += R"LVUI(
        ii::core::Result<void> activate() {
            if (thread_ != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Application scope requires its owner thread");
            if (phase_ != Phase::created) return ii::core::fail(ii::core::ErrorCode::busy, "Application scope has already started");
            phase_ = Phase::starting;
            invoking_ = true; Reset reset{invoking_};
            try {
                scope_ = std::make_shared<Scope>(factories_);
                scope_->binding_diagnostics = std::make_shared<lvui::native::BindingDiagnostics>();
                graph_ = std::make_shared<lvui::native::Bindings>();
                scope_->native_lifetime = graph_;
                auto diagnostics = scope_->binding_diagnostics->attach(graph_);
                if (!diagnostics) return std::unexpected(diagnostics.error());
)LVUI";
            if(scene_registry_runtime) result += "                scope_->scene_registry = scene_registry_;\n";
            if(application_quit_runtime) result += "                scope_->application_commands = application_commands_;\n";
            if(scene_lifecycle_runtime) result +=
                "                scope_->scene_failure_handler = [weak = std::weak_ptr(shared_from_this())](const ii::core::Error& error) { if (auto owner = weak.lock()) owner->report_scene_failure(error); };\n";
            if(scoped_image_runtime) result +=
                "                scope_->image_services = factories_.create<std::shared_ptr<lvui::native::ImageServices<ii::gui::RasterImage>>>(\"lvui::native::ImageServices\", \"image_services\", scope_->backend_activation);\n"
                "                if (!scope_->image_services) scope_->image_services = std::make_shared<lvui::native::ImageServices<ii::gui::RasterImage>>();\n";
            if(owned_radio_runtime) result += "                scope_->radio_groups = std::make_shared<ii::gui::RadioGroups>();\n";
            if(owned_link_runtime||owned_router_runtime) result += "                scope_->navigation_context = std::make_shared<lvui::NavigationContext>();\n";
            result += R"LVUI(                scope_->backend_activation.attach(scope_, [weak = std::weak_ptr(scope_)] {
                    if (auto scope = weak.lock()) scope->publish_native_changes();
                });
                if (configure_) configure_(*scope_);
)LVUI";
            if(!native_initializers.empty()) result += "                if (auto initialized = initialize_values(scope_, graph_); !initialized) return initialized;\n";
            if(application_files!="{}") result +=
                "                { auto scope = scope_; auto native_bindings = graph_; auto event = "+application_files+";\n"
                "                  file_events_ = std::make_unique<lvui::native::ApplicationFileEvents>([event = std::move(event)](const auto& paths) mutable -> ii::core::Result<void> { event(paths); return {}; }); }\n";
            if(application_close!="{}") result +=
                "                { auto scope = scope_; auto native_bindings = graph_; close_request_ = "+application_close+"; }\n";
            if(application_start!="{}") result +=
                "                { auto scope = scope_; auto native_bindings = graph_; auto event = "+application_start+"; event(); }\n";
            result += R"LVUI(                phase_ = Phase::active;
                return {};
            } catch (...) {
                return ii::core::fail(ii::core::ErrorCode::failed, "Application scope activation failed", std::current_exception());
            }
        }
)LVUI";
            if(application_arguments!="{}") result += R"LVUI(        ii::core::Result<void> notify_launch_arguments(const std::vector<std::string>& arguments) {
            if (thread_ != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Application scope requires its owner thread");
            if (invoking_) return ii::core::fail(ii::core::ErrorCode::busy, "Application lifecycle is active");
            if (phase_ != Phase::active) return ii::core::fail(ii::core::ErrorCode::closed, "Application scope is not active");
            if (arguments_attempted_ || ready_attempted_) return ii::core::fail(ii::core::ErrorCode::busy, "Launch arguments were already delivered");
            arguments_attempted_ = true;
            invoking_ = true; Reset reset{invoking_};
            try {
)LVUI" "                { auto scope = scope_; auto native_bindings = graph_; auto event = " + application_arguments + "; event(arguments); }\n" + R"LVUI(                return {};
            } catch (...) {
                arguments_failure_ = ii::core::Error{ii::core::ErrorCode::failed, "Application launch arguments callback failed", std::current_exception()};
                return std::unexpected(*arguments_failure_);
            }
        }
)LVUI";
            if(application_close_runtime) {
                result += R"LVUI(        ii::core::Result<bool> request_close() {
            if (thread_ != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Application scope requires its owner thread");
            if (phase_ == Phase::created || phase_ == Phase::closed) return true;
            if (invoking_ || phase_ != Phase::active) return false;
            if (close_failure_) return std::unexpected(*close_failure_);
)LVUI";
                if(scene_lifecycle_runtime) result += "            if (scene_failure_) return std::unexpected(*scene_failure_);\n";
                if(application_arguments!="{}") result += "            if (arguments_failure_) return std::unexpected(*arguments_failure_);\n";
                if(application_files!="{}") result += "            if (auto files = file_events_->outcome(); !files) return std::unexpected(files.error());\n";
                result += R"LVUI(            try {
                bool accepted = true;
                {
                    invoking_ = true; Reset reset{invoking_};
)LVUI";
                if(application_close!="{}") result += "                    accepted = close_request_();\n";
                result += R"LVUI(                }
)LVUI";
                if(scene_lifecycle_runtime) result += "                if (scene_failure_) return std::unexpected(*scene_failure_);\n";
                if(application_arguments!="{}") result += "                if (arguments_failure_) return std::unexpected(*arguments_failure_);\n";
                result += "                if (close_failure_) return std::unexpected(*close_failure_);\n";
                if(application_files!="{}") result += "                if (auto files = file_events_->outcome(); !files) return std::unexpected(files.error());\n";
                result += R"LVUI(                if (activity_failed_) return ii::core::fail(ii::core::ErrorCode::full, "Application activity queue is full");
                if (!accepted && ready_) {
)LVUI" + std::string(application_files!="{}" ? "                    auto delivered = drain_application_events();\n" : "                    auto delivered = deliver_activity();\n") + R"LVUI(                    if (!delivered) { (void)report_close_failure(delivered.error()); return std::unexpected(delivered.error()); }
                }
                return accepted;
            } catch (...) {
                ii::core::Error error{ii::core::ErrorCode::failed, "Application close request callback failed", std::current_exception()};
                (void)report_close_failure(error);
                return std::unexpected(std::move(error));
            }
        }
)LVUI";
            }
            result += R"LVUI(        ii::core::Result<void> notify_ready() {
            if (thread_ != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Application scope requires its owner thread");
            if (invoking_) return ii::core::fail(ii::core::ErrorCode::busy, "Application lifecycle is active");
            if (phase_ != Phase::active) return ii::core::fail(ii::core::ErrorCode::closed, "Application scope is not active");
)LVUI";
            if(scene_lifecycle_runtime) result += "            if (scene_failure_) return std::unexpected(*scene_failure_);\n";
            if(application_arguments!="{}") result += "            if (arguments_failure_) return std::unexpected(*arguments_failure_);\n            if (!arguments_attempted_) return ii::core::fail(ii::core::ErrorCode::closed, \"Application launch arguments have not been delivered\");\n";
            if(application_close_runtime) result += "            if (close_failure_) return std::unexpected(*close_failure_);\n";
            if(application_files!="{}") result += "            if (auto files = file_events_->outcome(); !files) return files;\n";
            result += R"LVUI(
            if (ready_attempted_) return ii::core::fail(ii::core::ErrorCode::busy, "Application readiness was already delivered");
            if (windows_ == 0) return ii::core::fail(ii::core::ErrorCode::closed, "Application has no active window");
            ready_attempted_ = true;
            try {
)LVUI";
            result += R"LVUI(                {
                    invoking_ = true; Reset reset{invoking_};
)LVUI";
            if(application_ready!="{}") result +=
                "                    { auto scope = scope_; auto native_bindings = graph_; auto event = "+application_ready+"; event(); }\n";
            result += R"LVUI(                }
                ready_ = true;
                if (auto initial = std::exchange(initial_activity_, std::nullopt))
                    if (auto queued = enqueue_activity(*initial); !queued) return queued;
                if (auto initial = std::exchange(initial_foreground_, std::nullopt))
                    if (auto queued = enqueue_state(true, *initial); !queued) return queued;
)LVUI" + std::string(application_files!="{}" ? "                return drain_application_events();\n" : "                return deliver_activity();\n") + R"LVUI(
            } catch (...) {
                return ii::core::fail(ii::core::ErrorCode::failed, "Application ready callback failed", std::current_exception());
            }
        }
        ii::core::Result<void> notify_activity(bool value) {
            if (thread_ != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Application scope requires its owner thread");
            if (phase_ != Phase::active || state_events_closed_ || activity_failed_ || (ready_attempted_ && !ready_ && !invoking_))
                return ii::core::fail(ii::core::ErrorCode::closed, "Application activity is closed");
            if (!ready_) { initial_activity_ = value; return {}; }
            if (auto queued = enqueue_activity(value); !queued) return queued;
            if (invoking_) return {};
)LVUI" + std::string(application_files!="{}" ? "            return drain_application_events();\n" : "            return deliver_activity();\n") + R"LVUI(
        }
        ii::core::Result<void> notify_foreground(bool value) {
            if (thread_ != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Application scope requires its owner thread");
            if (phase_ != Phase::active || state_events_closed_ || activity_failed_ || (ready_attempted_ && !ready_ && !invoking_))
                return ii::core::fail(ii::core::ErrorCode::closed, "Application foreground delivery is closed");
            if (!ready_) { initial_foreground_ = value; return {}; }
            if (auto queued = enqueue_state(true, value); !queued) return queued;
            if (invoking_) return {};
)LVUI" + std::string(application_files!="{}" ? "            return drain_application_events();\n" : "            return deliver_activity();\n") + R"LVUI(
        }
        ii::core::Result<void> cancel_state_events() {
            if (thread_ != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Application scope requires its owner thread");
            if (phase_ != Phase::active) return ii::core::fail(ii::core::ErrorCode::closed, "Application state delivery is closed");
            state_events_closed_ = true;
            initial_activity_.reset(); initial_foreground_.reset(); activity_size_ = 0;
            return {};
        }
        bool state_events_cancelled() const noexcept { return state_events_closed_ || phase_ == Phase::retiring || phase_ == Phase::closed; }
)LVUI";
            if(application_files!="{}") result += R"LVUI(        ii::core::Result<void> notify_files(std::vector<std::string> files) {
            if (thread_ != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Application scope requires its owner thread");
            if (phase_ != Phase::active || activity_failed_ || !file_events_ || (ready_attempted_ && !ready_ && !invoking_))
                return ii::core::fail(ii::core::ErrorCode::closed, "Application file delivery is closed");
            if (auto accepted = file_events_->submit(std::move(files)); !accepted) return accepted;
            if (!ready_ || invoking_) return {};
            return drain_application_events();
        }
        ii::core::Result<void> cancel_file_events() {
            if (thread_ != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Application scope requires its owner thread");
            if (phase_ != Phase::active || !file_events_) return ii::core::fail(ii::core::ErrorCode::closed, "Application file delivery is closed");
            if (auto cancelled = file_events_->cancel(); !cancelled) return cancelled;
            file_requests_cancelled_ = true;
            return {};
        }
        bool file_requests_cancelled() const noexcept { return file_requests_cancelled_ || phase_ == Phase::retiring || phase_ == Phase::closed; }
)LVUI";
            result += R"LVUI(        ii::core::Result<void> retire() {
            if (thread_ != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Application scope requires its owner thread");
            if (phase_ == Phase::closed) return {};
            if (phase_ == Phase::retiring || invoking_) return ii::core::fail(ii::core::ErrorCode::busy, "Application scope is changing lifecycle");
            if (windows_ != 0) return ii::core::fail(ii::core::ErrorCode::busy, "Application scope still has active windows");
            const bool started = phase_ == Phase::active;
            phase_ = Phase::retiring;
            state_events_closed_ = true; initial_activity_.reset(); initial_foreground_.reset(); activity_size_ = 0;
            invoking_ = true; Reset reset{invoking_};
            std::optional<ii::core::Error> first;
)LVUI";
            if(application_arguments!="{}") result += "            first = arguments_failure_;\n";
            if(scene_lifecycle_runtime) result += "            if (!first) first = scene_failure_;\n            scene_loop_ = nullptr;\n";
            if(application_close_runtime) result += "            if (!first) first = close_failure_;\n            close_loop_ = nullptr;\n";
            if(application_close_runtime && multiple_scenes) result += "            close_error_handler_ = {};\n";
            if(application_files!="{}") result += R"LVUI(            if (file_events_) {
                if (auto outcome = file_events_->outcome(); !outcome && !first) first = outcome.error();
                if (auto closed = file_events_->close(); !closed && !first) first = closed.error();
            }
)LVUI";
            if(application_stop!="{}") result +=
                "            try { if (started) { auto scope = scope_; auto native_bindings = graph_; auto event = "+application_stop+"; event(); } }\n"
                "            catch (...) { if (!first) first = ii::core::Error{ii::core::ErrorCode::failed, \"Application stop callback failed\", std::current_exception()}; }\n";
            else result += "            (void)started;\n";
            if(application_close!="{}") result += "            close_request_ = {};\n";
            result += R"LVUI(            try { if (graph_) if (auto result = graph_->close(); !result && !first) first = result.error(); }
            catch (...) { if (!first) first = ii::core::Error{ii::core::ErrorCode::failed, "Application bindings retirement failed", std::current_exception()}; }
            try { if (scope_) scope_->backend_activation.retire(scope_); }
            catch (...) { if (!first) first = ii::core::Error{ii::core::ErrorCode::failed, "Application backend retirement failed", std::current_exception()}; }
            try { if (scope_ && scope_->binding_diagnostics) if (auto result = scope_->binding_diagnostics->close(); !result && !first) first = result.error(); }
            catch (...) { if (!first) first = ii::core::Error{ii::core::ErrorCode::failed, "Application diagnostics retirement failed", std::current_exception()}; }
            graph_.reset(); scope_.reset(); phase_ = Phase::closed;
            if (first) return std::unexpected(std::move(*first));
            return {};
        }
        ii::core::Result<std::shared_ptr<void>> acquire_window() {
            if (thread_ != std::this_thread::get_id()) return ii::core::fail(ii::core::ErrorCode::wrong_executor, "Application scope requires its owner thread");
            if (phase_ != Phase::active) return ii::core::fail(ii::core::ErrorCode::closed, "Application scope is not active");
            if (windows_ == std::numeric_limits<std::size_t>::max()) return ii::core::fail(ii::core::ErrorCode::full, "Application window count exhausted");
            try {
                auto lease = std::make_shared<WindowLease>(shared_from_this());
                ++windows_;
                return std::static_pointer_cast<void>(std::move(lease));
            } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Application window lease failed", std::current_exception()); }
        }
    private:
        struct Reset final { bool& value; ~Reset() { value = false; } };
        static constexpr std::size_t activity_capacity = 64;
        struct StateTransition final { bool foreground = false, value = false; };
        ii::core::Result<void> enqueue_activity(bool value) {
            return enqueue_state(false, value);
        }
        ii::core::Result<void> enqueue_state(bool foreground, bool value) {
            auto previous = foreground ? delivered_foreground_ : delivered_activity_;
            for (std::size_t index = 0; index < activity_size_; ++index) {
                const auto& pending = activity_queue_[(activity_head_ + index) % activity_capacity];
                if (pending.foreground == foreground) previous = pending.value;
            }
            if (previous && *previous == value) return {};
            if (activity_size_ == activity_capacity) {
                activity_size_ = 0; activity_failed_ = true;
                return ii::core::fail(ii::core::ErrorCode::full, "Application activity queue is full");
            }
            activity_queue_[(activity_head_ + activity_size_) % activity_capacity] = {foreground, value};
            ++activity_size_; return {};
        }
        ii::core::Result<void> deliver_activity() {
            if (!ready_ || invoking_ || !activity_size_) return {};
            invoking_ = true; Reset reset{invoking_};
            for (std::size_t count = 0; count != activity_capacity && activity_size_; ++count) {
                const auto transition = activity_queue_[activity_head_];
                [[maybe_unused]] const bool value = transition.value;
                activity_head_ = (activity_head_ + 1) % activity_capacity; --activity_size_;
                if (transition.foreground) delivered_foreground_ = value;
                else delivered_activity_ = value;
                try {
)LVUI";
            if(application_active!="{}") result +=
                "                    if (!transition.foreground && value) { auto scope = scope_; auto native_bindings = graph_; auto event = "+application_active+"; event(); }\n";
            if(application_inactive!="{}") result +=
                "                    if (!transition.foreground && !value) { auto scope = scope_; auto native_bindings = graph_; auto event = "+application_inactive+"; event(); }\n";
            if(application_foreground!="{}") result +=
                "                    if (transition.foreground && value) { auto scope = scope_; auto native_bindings = graph_; auto event = "+application_foreground+"; event(); }\n";
            if(application_background!="{}") result +=
                "                    if (transition.foreground && !value) { auto scope = scope_; auto native_bindings = graph_; auto event = "+application_background+"; event(); }\n";
            result += R"LVUI(                } catch (...) {
                    activity_size_ = 0; activity_failed_ = true;
                    return ii::core::fail(ii::core::ErrorCode::failed, "Application activity callback failed", std::current_exception());
                }
                if (activity_failed_)
                    return ii::core::fail(ii::core::ErrorCode::full, "Application activity queue is full");
            }
            if (activity_size_) {
                activity_size_ = 0; activity_failed_ = true;
                return ii::core::fail(ii::core::ErrorCode::full, "Application activity reentry limit exceeded");
            }
            return {};
        }
)LVUI";
            if(application_files!="{}") result += R"LVUI(        ii::core::Result<void> drain_application_events() {
            if (!ready_ || invoking_ || pumping_events_) return {};
            pumping_events_ = true; Reset reset{pumping_events_};
            for (std::size_t count = 0; count < activity_capacity; ++count) {
                if (auto outcome = file_events_->outcome(); !outcome) return outcome;
                if (activity_size_) if (auto delivered = deliver_activity(); !delivered) return delivered;
                if (file_events_->pending()) {
                    invoking_ = true; Reset callback{invoking_};
                    if (auto delivered = file_events_->drain(); !delivered) return delivered;
                }
                if (activity_failed_) return ii::core::fail(ii::core::ErrorCode::full, "Application activity queue is full");
                if (!activity_size_ && !file_events_->pending()) return {};
            }
            activity_size_ = 0; activity_failed_ = true;
            return file_events_->abort({ii::core::ErrorCode::full, "Application event reentry limit exceeded"});
        }
)LVUI";
            result += R"LVUI(        struct WindowLease final {
            explicit WindowLease(std::shared_ptr<ApplicationOwner> value) : owner(std::move(value)) {}
            ~WindowLease() { owner->release_window(); }
            std::shared_ptr<ApplicationOwner> owner;
        };
        void release_window() noexcept {
            if (thread_ != std::this_thread::get_id() || !windows_) std::terminate();
            --windows_;
        }
        enum class Phase { created, starting, active, retiring, closed };
        Configure configure_;
        lvui::native::BackendFactories factories_;
        const std::thread::id thread_ = std::this_thread::get_id();
        Phase phase_ = Phase::created;
        bool ready_ = false, ready_attempted_ = false, invoking_ = false, activity_failed_ = false, state_events_closed_ = false;
        std::optional<bool> initial_activity_, delivered_activity_, initial_foreground_, delivered_foreground_;
        std::array<StateTransition,activity_capacity> activity_queue_{};
        std::size_t activity_head_ = 0, activity_size_ = 0;
        std::size_t windows_ = 0;
)LVUI";
            if(scene_lifecycle_runtime) result += "        std::optional<ii::core::Error> scene_failure_;\n        ii::runtime::EventLoop* scene_loop_ = nullptr;\n";
            if(application_arguments!="{}") result += "        bool arguments_attempted_ = false;\n        std::optional<ii::core::Error> arguments_failure_;\n";
            if(application_close_runtime) result += "        std::optional<ii::core::Error> close_failure_;\n        ii::runtime::EventLoop* close_loop_ = nullptr;\n";
            if(application_close_runtime && multiple_scenes) result += "        std::function<void(const ii::core::Error&)> close_error_handler_;\n";
            if(application_close!="{}") result += "        std::function<bool()> close_request_;\n";
            if(application_quit_runtime) result += "        std::weak_ptr<lvui::native::ApplicationCommands> application_commands_;\n";
            if(scene_registry_runtime) result += "        std::weak_ptr<lvui::native::SceneRegistry> scene_registry_;\n";
            if(application_files!="{}") result += "        std::unique_ptr<lvui::native::ApplicationFileEvents> file_events_;\n        bool pumping_events_ = false, file_requests_cancelled_ = false;\n";
            result += R"LVUI(
        std::shared_ptr<Scope> scope_;
        std::shared_ptr<lvui::native::Bindings> graph_;
    };
    static std::shared_ptr<ApplicationOwner> make_application_owner(
            const std::function<void(Scope&)>& configure = {}, const lvui::native::BackendFactories& factories = {}) {
        return std::make_shared<ApplicationOwner>(configure, factories);
    }
    static ii::runtime::ApplicationAttachment application_attachment(std::string name, std::shared_ptr<ApplicationOwner> owner) {
        return {std::move(name),
            [owner](ii::runtime::Application&) -> ii::core::Result<void> {
                if (!owner) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Application scope owner is missing");
                return owner->activate();
            },
            [owner]() -> ii::core::Result<void> { return owner ? owner->retire() : ii::core::Result<void>{}; }};
    }
)LVUI";
            if(application_close_runtime) result += R"LVUI(    static std::function<bool()> close_request_handler(const std::shared_ptr<ApplicationOwner>& application, std::function<void()> accepted) {
        return [weak = std::weak_ptr(application), accepted = std::move(accepted)] {
            auto owner = weak.lock();
            try {
                if (owner) {
                    auto decision = owner->request_close();
                    if (!decision) { (void)owner->report_close_failure(decision.error()); return false; }
                    if (!*decision) return false;
                }
                if (accepted) accepted();
                return true;
            } catch (...) {
                if (owner) (void)owner->report_close_failure({ii::core::ErrorCode::failed, "Application close handling failed", std::current_exception()});
                return false;
            }
        };
    }
)LVUI";
            if(window_close_runtime) result += R"LVUI(    static std::function<bool()> window_close_request_handler(
            const std::shared_ptr<ApplicationOwner>& application,
            std::shared_ptr<lvui::native::WindowCloseGuard> guard, std::function<void()> accepted) {
        auto closing = std::make_shared<bool>(false);
        return [weak = std::weak_ptr(application), guard = std::move(guard), accepted = std::move(accepted), closing,
                thread = std::this_thread::get_id()] {
            if (thread != std::this_thread::get_id()) return false;
            auto owner = weak.lock();
            if (*closing) return false;
            *closing = true;
            struct Reset final { bool& value; ~Reset() { value = false; } } reset{*closing};
            try {
                if (guard) {
                    auto decision = guard->request_close();
                    if (!decision) { if (owner) (void)owner->report_close_failure(decision.error()); return false; }
                    if (!*decision) return false;
                }
                if (owner) {
                    auto decision = owner->request_close();
                    if (!decision) { (void)owner->report_close_failure(decision.error()); return false; }
                    if (!*decision) return false;
                }
                if (accepted) accepted();
                return true;
            } catch (...) {
                if (owner) (void)owner->report_close_failure({ii::core::ErrorCode::failed, "Window close handling failed", std::current_exception()});
                return false;
            }
        };
    }
)LVUI";
            if(window_close_runtime && multiple_scenes) result += R"LVUI(    static std::function<bool()> all_windows_close_request_handler(
            const std::shared_ptr<ApplicationOwner>& application,
            std::shared_ptr<std::weak_ptr<lvui::native::SceneWindowSet>> windows, std::function<void()> accepted) {
        return [weak = std::weak_ptr(application), windows = std::move(windows), accepted = std::move(accepted)] {
            auto owner = weak.lock();
            try {
                auto scenes = windows ? windows->lock() : nullptr;
                if (!scenes) return false;
                auto decision = scenes->request_application_close([owner]() -> ii::core::Result<bool> {
                    return owner ? owner->request_close() : ii::core::Result<bool>{true};
                });
                if (!decision) { if (owner) (void)owner->report_close_failure(decision.error()); return false; }
                if (!*decision) return false;
                if (accepted) accepted();
                return true;
            } catch (...) {
                if (owner) (void)owner->report_close_failure({ii::core::ErrorCode::failed, "Application window close handling failed", std::current_exception()});
                return false;
            }
        };
    }
)LVUI";
            if(application_arguments!="{}") result += R"LVUI(    static ii::runtime::ApplicationAttachment launch_arguments_attachment(std::string name, std::shared_ptr<ApplicationOwner> owner) {
        return {std::move(name), [owner](ii::runtime::Application& application) -> ii::core::Result<void> {
            if (!owner) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Application scope owner is missing");
            try {
                const auto arguments = application.arguments();
                return owner->notify_launch_arguments(std::vector<std::string>(arguments.begin(), arguments.end()));
            } catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, "Application launch argument delivery failed", std::current_exception()); }
        }, []() -> ii::core::Result<void> { return {}; }};
    }
)LVUI";
            if(application_files!="{}") result += R"LVUI(    static std::function<void(std::vector<std::string>)> file_open_handler(const std::shared_ptr<ApplicationOwner>& application) {
        return [weak = std::weak_ptr(application)](std::vector<std::string> files) {
            if (auto owner = weak.lock()) if (auto delivered = owner->notify_files(std::move(files)); !delivered) {
                if (delivered.error().code == ii::core::ErrorCode::closed && owner->file_requests_cancelled()) return;
                if (delivered.error().cause) std::rethrow_exception(delivered.error().cause);
                throw std::runtime_error(delivered.error().message);
            }
        };
    }
)LVUI";
        }
        result += "    " + options.name + "() = default;\n    " + options.name + "(const " + options.name + "&) = delete;\n    " + options.name + "& operator=(const " + options.name + "&) = delete;\n";
        if (multiple_scenes) result += "    explicit "+options.name+"(std::size_t scene) : scene_index_(scene) {}\n";
        const auto scope_parameters=scope.empty()?std::string{}:", const std::function<void(Scope&)>& configure = {}, std::shared_ptr<Scope>* scope_owner = nullptr, const lvui::native::BackendFactories& factories = {}";
        const auto scope_arguments=scope.empty()?std::string{}:", configure, scope_owner, factories";
        result += "private:\n    static ii::core::Result<void> construct(ii::gui::WidgetBuilder& widgets, ii::gui::Widget native_parent"+scope_parameters+
                  std::string(application_root?", std::shared_ptr<ApplicationOwner> supplied = {}":"")+
                  std::string(multiple_scenes?", std::size_t scene_index = 0":"")+
                  std::string(window_close_runtime?", std::shared_ptr<lvui::native::WindowCloseGuard> scene_close_guard = {}":"")+") {\n";
        if (multiple_scenes) result += "            if (scene_index >= scene_count) return ii::core::fail(ii::core::ErrorCode::out_of_range, \"Unknown application scene\");\n";
        if (window_close_runtime) result += "            if (!scene_close_guard) scene_close_guard = std::make_shared<lvui::native::WindowCloseGuard>();\n";
        if(application_root) {
            result += R"LVUI(            const bool standalone = !supplied;
            auto application = supplied ? std::move(supplied) : make_application_owner(configure, factories);
            if (standalone) {
                if (auto activated = application->activate(); !activated) { (void)application->retire(); return activated; }
            }
            auto scope = application->scope();
            if (!scope) return ii::core::fail(ii::core::ErrorCode::closed, "Application scope is not active");
            if (auto owned = widgets.retain(application); !owned) { if (standalone) (void)application->retire(); return owned; }
            auto lease = application->acquire_window();
            if (!lease) { if (standalone) (void)application->retire(); return std::unexpected(lease.error()); }
            if (auto owned = widgets.retain(*lease); !owned) return owned;
)LVUI";
            if(popup_mount_validation)result += "            scope->initial_popup_bindings.clear();\n";
        } else {
            if (!scope.empty()) result += "            auto scope = std::make_shared<Scope>(factories);\n            if (auto owned = widgets.retain(scope); !owned) return owned;\n";
            if (scoped_image_runtime&&!scope.empty()) result +=
                "            scope->image_services = factories.create<std::shared_ptr<lvui::native::ImageServices<ii::gui::RasterImage>>>(\"lvui::native::ImageServices\", \"image_services\", scope->backend_activation);\n"
                "            if (!scope->image_services) scope->image_services = std::make_shared<lvui::native::ImageServices<ii::gui::RasterImage>>();\n";
            if(native_runtime&&!scope.empty())result += "            scope->binding_diagnostics = std::make_shared<lvui::native::BindingDiagnostics>();\n";
            if (owned_radio_runtime) result += "            scope->radio_groups = std::make_shared<ii::gui::RadioGroups>();\n";
            if (owned_link_runtime||owned_router_runtime) result += "            scope->navigation_context = std::make_shared<lvui::NavigationContext>();\n";
            result += binding_owner(!scope.empty());
            if (native_runtime && !scope.empty()) result += "            scope->native_lifetime = native_bindings;\n";
            if (!scope.empty()) result += "            if (auto activated = lvui::native::attach_backend_activation(scope, widgets); !activated) return activated;\n            if (configure) configure(*scope);\n";
            if (!native_initializers.empty()) result += "            if (auto initialized = initialize_values(scope, native_bindings); !initialized) return initialized;\n";
        }
        result += body;
        if(application_root) result += R"LVUI(            if (standalone) {
                if (auto retiring = widgets.on_retire([weak = std::weak_ptr(application)] { if (auto owner = weak.lock()) (void)owner->retire(); }); !retiring) return retiring;
            }
)LVUI";
        if (!scope.empty()) result += "            if (scope_owner) *scope_owner = scope;\n";
        result += "            return {};\n    }\npublic:\n    static ii::core::Result<void> build(ii::gui::WidgetBuilder& widgets, ii::gui::Widget native_parent = {}"+scope_parameters+") {\n";
        result+="        return construct(widgets, native_parent"+scope_arguments+");\n";
        result+="    }\n";
        if(application_root) result += R"LVUI(    static ii::core::Result<void> build_with_application(ii::gui::WidgetBuilder& widgets, ii::gui::Widget native_parent,
            std::shared_ptr<ApplicationOwner> application)LVUI" + std::string(window_close_runtime ? ", std::shared_ptr<lvui::native::WindowCloseGuard> scene_close_guard = {}" : "") + R"LVUI() {
        if (!application) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Application scope owner is missing");
        return construct(widgets, native_parent, {}, nullptr, {}, std::move(application))LVUI" + std::string(window_close_runtime && multiple_scenes ? ", 0" : "") + std::string(window_close_runtime ? ", std::move(scene_close_guard)" : "") + R"LVUI();
    }
)LVUI";
        if(multiple_scenes) result += R"LVUI(    static ii::core::Result<void> build_scene_with_application(ii::gui::WidgetBuilder& widgets, ii::gui::Widget native_parent,
            std::shared_ptr<ApplicationOwner> application, std::size_t scene_index)LVUI" + std::string(window_close_runtime ? ", std::shared_ptr<lvui::native::WindowCloseGuard> scene_close_guard = {}" : "") + R"LVUI() {
        if (!application) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Application scope owner is missing");
        return construct(widgets, native_parent, {}, nullptr, {}, std::move(application), scene_index)LVUI" + std::string(window_close_runtime ? ", std::move(scene_close_guard)" : "") + R"LVUI();
    }
)LVUI";
        if (options.view_factory_only) return result + "};\n} // namespace lvui_generated\n";
        result+="    static ii::core::Result<ii::gui::WidgetMount> mount(ii::gui::Host& host"+scope_parameters+
                std::string(application_root?", std::shared_ptr<ApplicationOwner> application = {}":"")+
                std::string(multiple_scenes?", std::size_t scene_index = 0":"")+
                std::string(window_close_runtime?", std::shared_ptr<lvui::native::WindowCloseGuard> scene_close_guard = {}":"")+") {\n";
        if(multiple_scenes) result += "        auto scene_appearance = appearance_for_scene(scene_index);\n        if (!scene_appearance) return std::unexpected(scene_appearance.error());\n";
        if(popup_mount_validation){
            result+="        std::shared_ptr<Scope> mounting_scope;\n"
                "        auto mounted = host.mount_widgets([&configure, &mounting_scope, &factories"+std::string(application_root?", &application":"")+std::string(multiple_scenes?", scene_index":"")+std::string(window_close_runtime?", &scene_close_guard":"")+"](ii::gui::WidgetBuilder& widgets) -> ii::core::Result<void> {\n"
                "            return construct(widgets, {}, configure, &mounting_scope, factories"+std::string(application_root?", application":"")+std::string(multiple_scenes?", scene_index":"")+std::string(window_close_runtime?", scene_close_guard":"")+");\n        }, "+std::string(multiple_scenes?"*scene_appearance":"appearance()")+");\n"
                "        if (scope_owner && mounting_scope) *scope_owner = mounting_scope;\n"
                "        if (!mounted) return mounted;\n"
                "        if (!mounting_scope || !mounting_scope->active()) { (void)host.unmount_widgets(*mounted); return ii::core::fail(ii::core::ErrorCode::closed, \"Popup activation retired during mount\"); }\n"
                "        for (const auto& [weak_graph, id] : mounting_scope->initial_popup_bindings) if (auto graph = weak_graph.lock()) {\n"
                "            auto failures = graph->failures();\n"
                "            if (!failures) { (void)host.unmount_widgets(*mounted); return std::unexpected(failures.error()); }\n"
                "            for (const auto& failure : *failures) if (failure.binding == id) {\n"
                "                (void)host.unmount_widgets(*mounted); return std::unexpected(failure.error);\n            }\n        }\n"
                "        return mounted;\n";
        }else result += "        return host.mount_widgets(" + std::string(scope.empty() ? "[]" : application_root?(multiple_scenes?(window_close_runtime?"[&configure, scope_owner, &factories, &application, scene_index, &scene_close_guard]":"[&configure, scope_owner, &factories, &application, scene_index]"):(window_close_runtime?"[&configure, scope_owner, &factories, &application, &scene_close_guard]":"[&configure, scope_owner, &factories, &application]")):"[&configure, scope_owner, &factories]") +
                  "(ii::gui::WidgetBuilder& widgets) -> ii::core::Result<void> {\n            return construct(widgets, {}" +
                  scope_arguments + std::string(application_root?", application":"") + std::string(multiple_scenes?", scene_index":"") + std::string(window_close_runtime?", scene_close_guard":"") + ");\n        }, "+std::string(multiple_scenes?"*scene_appearance":"appearance()")+");\n";
        result+="    }\n";
        const auto configuration=scope.empty() ? std::string{} : ", const std::function<void(Scope&)>& configure = {}, const lvui::native::BackendFactories& factories = {}";
        std::string accessors;
        if(native_runtime&&!scope.empty())accessors+="    ii::core::Result<std::vector<lvui::native::ScopedBindingFailure>> binding_failures() const { if (!scope_) return ii::core::fail(ii::core::ErrorCode::closed, \"View scope has closed\"); return scope_->binding_failures(); }\n";
        for(const auto& [name,type]:variable_types) {
            const auto& source=variable_sources.at(name);
            const auto guard=std::string("        auto current = native_widget_.snapshot();\n        if (!current) return std::unexpected(current.error());\n        if (!scope_ || !scope_->active()) return ii::core::fail(ii::core::ErrorCode::closed, \"View scope has closed\");\n");
            accessors+="    ii::core::Result<void> set_"+name+"("+type+" value) {\n"+guard+"        try { return lvui::native::assign_value("+source.target+", std::move(value)); }\n        catch (...) { return ii::core::fail(ii::core::ErrorCode::failed, \"Observable update rejected\", std::current_exception()); }\n    }\n";
            if(source.callable||source.inferred)accessors+="    template<class NativeFactory> ii::core::Result<void> set_"+name+"_native(NativeFactory factory) {\n"+guard+"        return "+(source.inferred?"lvui::native::inferred::replace("+source.target+", std::move(factory))":source.target+".replace_native(std::move(factory))")+";\n    }\n";
        }
        if (application_root) {
            std::string scene_mask = "std::vector<bool>{";
            for (std::size_t index = 0; index < application_scene_initial.size(); ++index)
                scene_mask += std::string(index ? ", " : "") + (application_scene_initial[index] ? "true" : "false");
            scene_mask += "}";
            if(multiple_scenes) result += R"LVUI(    static ii::core::Result<void> validate_scene_hosts(const std::vector<std::weak_ptr<ii::gui::Host>>& hosts, const std::vector<bool>& required = {}) {
        if (hosts.empty()) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Application requires a native scene");
        if (!required.empty() && required.size() != hosts.size()) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid scene readiness mask");
        for (std::size_t index = 0; index < hosts.size(); ++index) {
            if (!required.empty() && !required[index]) continue;
            const auto& weak = hosts[index];
            auto host = weak.lock();
            if (!host) return ii::core::fail(ii::core::ErrorCode::closed, "Application scene host expired");
            auto snapshot = host->snapshot();
            if (!snapshot) return std::unexpected(snapshot.error());
            if (snapshot->closed) return ii::core::fail(ii::core::ErrorCode::closed, "Application scene closed before readiness");
        }
        return {};
    }
    static std::function<void(const ii::core::Error&)> scene_failure_handler(
            std::shared_ptr<lvui::native::SceneStartup> startup, ii::runtime::EventLoop& event_loop) {
        return [weak = std::weak_ptr(startup), loop = &event_loop](const ii::core::Error& error) {
            if (auto state = weak.lock()) {
                (void)state->fail(error);
                if (state->failed()) loop->request_stop();
            }
        };
    }
    static ii::gui::HostCallbacks scene_callbacks(std::weak_ptr<ii::gui::Host> child,
            std::shared_ptr<lvui::native::SceneStartup> startup, ii::runtime::EventLoop& event_loop,
            std::size_t index, std::function<ii::core::Result<void>(ii::gui::Host&)> attach, bool initial = true) {
        auto report = scene_failure_handler(startup, event_loop);
        ii::gui::HostCallbacks callbacks;
        callbacks.opened = [child = std::move(child), startup, attach = std::move(attach), report, index, initial] {
            if (!startup || !(initial ? startup->accepting() : startup->accepts_runtime())) return;
            try {
                auto host = child.lock();
                if (!host) { report({ii::core::ErrorCode::closed, "Application scene host expired"}); return; }
                if (auto opened = attach(*host); !opened) { report(opened.error()); return; }
                if (initial && startup->accepting()) if (auto ready = startup->mounted(index); !ready) report(ready.error());
            } catch (...) { report({ii::core::ErrorCode::failed, "Application scene attachment failed", std::current_exception()}); }
        };
        callbacks.closed = [startup, report, index, initial] {
            if (initial && startup) if (auto closed = startup->closed(index); !closed && startup->failed()) report(closed.error());
        };
        return callbacks;
    }
    struct SceneReservation final {
        std::shared_ptr<lvui::native::SceneWindowSet> windows;
        lvui::native::SceneWindowSet::Ticket ticket;
        bool committed = false;
        ~SceneReservation() { if (!committed) (void)windows->abandon(ticket); }
    };
    static void bind_scene_window(ii::gui::HostCallbacks& callbacks,
            std::shared_ptr<lvui::native::SceneWindowSet> windows, lvui::native::SceneWindowSet::Ticket ticket,
            std::function<ii::core::Result<void>()> empty,
            std::function<void(const ii::core::Error&)> report) {
        callbacks.close_requested = [windows, ticket, report] {
            try {
                auto decision = windows->request_close(ticket);
                if (!decision) { report(decision.error()); return false; }
                return *decision;
            } catch (...) {
                report({ii::core::ErrorCode::failed, "Application window close decision failed", std::current_exception()});
                return false;
            }
        };
        auto prior = std::move(callbacks.closed);
        callbacks.closed = [windows, ticket, prior = std::move(prior), empty = std::move(empty), report,
                            notified = std::make_shared<bool>(false)] {
            if (std::exchange(*notified, true)) return;
            try {
                auto closed = windows->closed(ticket);
                if (!closed) { report(closed.error()); return; }
                if (prior) prior();
                if (*closed && empty) if (auto result = empty(); !result) report(result.error());
            } catch (...) {
                report({ii::core::ErrorCode::failed, "Application window close notification failed", std::current_exception()});
            }
        };
    }
    static std::function<ii::core::Result<bool>()> last_scene_close(
            const std::shared_ptr<ApplicationOwner>& application, ii::runtime::EventLoop& loop,
            std::shared_ptr<bool> approved, std::function<void()> cancel,
            std::function<ii::core::Result<void>()> close_driver) {
        return [weak = std::weak_ptr(application), sender = loop.sender(), approved = std::move(approved),
                cancel = std::move(cancel), close_driver = std::move(close_driver)]() -> ii::core::Result<bool> {
            auto owner = weak.lock();
            if (!owner) return ii::core::fail(ii::core::ErrorCode::closed, "Application scope owner is missing");
            if (*approved) return false;
            auto decision = owner->request_close();
            if (!decision) return std::unexpected(decision.error());
            if (!*decision) return false;
            auto submitted = sender.post([weak, approved, close_driver] {
                if (!*approved) return;
                try {
                    auto result = close_driver();
                    if (!result) {
                        *approved = false;
                        if (auto owner = weak.lock()) (void)owner->report_close_failure(result.error());
                    }
                } catch (...) {
                    *approved = false;
                    if (auto owner = weak.lock()) (void)owner->report_close_failure(
                        {ii::core::ErrorCode::failed, "Application driver close failed", std::current_exception()});
                }
            });
            if (!submitted) {
                const auto code = submitted.error == ii::runtime::SubmitError::full ? ii::core::ErrorCode::full :
                    submitted.error == ii::runtime::SubmitError::stopped ? ii::core::ErrorCode::closed :
                    submitted.error == ii::runtime::SubmitError::invalid ? ii::core::ErrorCode::invalid_argument : ii::core::ErrorCode::busy;
                return ii::core::fail(code, "Application driver close could not be queued");
            }
            *approved = true;
            try { if (cancel) cancel(); }
            catch (...) { *approved = false; return ii::core::fail(ii::core::ErrorCode::failed, "Application shutdown preparation failed", std::current_exception()); }
            return true;
        };
    }
)LVUI";
            result += "    static ii::core::Result<void> configure_application(ii::runtime::Application& app" + configuration + ") {\n"
                "        auto application = make_application_owner(configure, factories);\n";
            if(!multiple_scenes) result += "        auto view = std::make_shared<" + options.name + ">();\n";
            if(multiple_scenes) result += R"LVUI(        ii::gui::require_application_driver_abi_v1();
        if (!ii::gui::host_capabilities().secondary_windows || !ii::gui::supports_application_driver())
            return ii::core::fail(ii::core::ErrorCode::unsupported, "This native host does not support independent application windows");
        auto scene_hosts = std::make_shared<std::vector<std::weak_ptr<ii::gui::Host>>>(scene_count);
)LVUI" + std::string(scene_registry_runtime ? "        auto required_scenes = " + scene_mask + ";\n" : "") +
                "        auto startup = std::make_shared<lvui::native::SceneStartup>(" + std::string(scene_registry_runtime ? "required_scenes" : "scene_count") + ", [weak = std::weak_ptr(application), scene_hosts" + std::string(scene_registry_runtime ? ", required_scenes" : "") + "]() -> ii::core::Result<void> {\n"
                "            if (auto live = validate_scene_hosts(*scene_hosts" + std::string(scene_registry_runtime ? ", required_scenes" : "") + "); !live) return live;\n" + R"LVUI(
            auto owner = weak.lock();
            if (!owner) return ii::core::fail(ii::core::ErrorCode::closed, "Application scope owner is missing");
            return owner->notify_ready();
        });
)LVUI";
            result += "        return app.configure_atomically([&](ii::runtime::Application& target) -> ii::core::Result<void> {\n";
            if(scene_lifecycle_runtime) result += "            if (auto bound = application->bind_scene_loop(target.event_loop()); !bound) return bound;\n";
            if(application_close_runtime) result += "            if (auto bound = application->bind_close_loop(target.event_loop()); !bound) return bound;\n";
            if(application_close_runtime && multiple_scenes) result += "            if (auto bound = application->bind_close_failure_handler(scene_failure_handler(startup, target.event_loop())); !bound) return bound;\n";
            if(scene_registry_runtime) result += R"LVUI(            auto scene_registry = std::make_shared<lvui::native::SceneRegistry>(
                [sender = target.event_loop().sender()](lvui::native::SceneRegistry::Task task) -> ii::core::Result<void> {
                    auto submitted = sender.post(std::move(task));
                    if (submitted) return {};
                    switch (submitted.error) {
                    case ii::runtime::SubmitError::full: return ii::core::fail(ii::core::ErrorCode::full, "Application event queue is full");
                    case ii::runtime::SubmitError::stopped: return ii::core::fail(ii::core::ErrorCode::closed, "Application event loop is stopped");
                    case ii::runtime::SubmitError::invalid: return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid application event");
                    default: return ii::core::fail(ii::core::ErrorCode::busy, "Application event queue is overloaded");
                    }
                }, scene_failure_handler(startup, target.event_loop()));
            if (auto bound = application->bind_scene_registry(scene_registry); !bound) return bound;
)LVUI";
            if(application_quit_runtime) result += "            auto command_link = std::make_shared<std::weak_ptr<lvui::native::ApplicationCommands>>();\n";
            if(multiple_scenes) result += "            auto windows_link = std::make_shared<std::weak_ptr<lvui::native::SceneWindowSet>>();\n            auto close_approved = std::make_shared<bool>(false);\n";
            if(multiple_scenes || application_files!="{}" || application_close_runtime) result += "            ii::gui::HostCallbacks callbacks;\n";
            if(application_files!="{}") result += "            callbacks.files_opened = file_open_handler(application);\n";
            if(multiple_scenes || application_files!="{}" || application_close_runtime) {
                std::string captures;
                if (multiple_scenes) captures = "weak = std::weak_ptr(startup)";
                if (scene_registry_runtime) captures += ", registry = std::weak_ptr(scene_registry)";
                if (application_files!="{}") captures += (captures.empty() ? "" : ", ") + std::string("files = std::weak_ptr(application)");
                if (application_state_runtime) captures += (captures.empty() ? "" : ", ") + std::string("lifecycle = std::weak_ptr(application)");
                if (application_quit_runtime) captures += (captures.empty() ? "" : ", ") + std::string("commands = command_link");
                if (multiple_scenes) captures += ", windows = windows_link";
                result += multiple_scenes ? "            auto cancel_pending = [" + captures + "] {\n" :
                    window_close_runtime ? "            callbacks.close_requested = window_close_request_handler(application, view->window_close_guard(), [" + captures + "] {\n" :
                    application_close_runtime ? "            callbacks.close_requested = close_request_handler(application, [" + captures + "] {\n" :
                    "            callbacks.close_requested = [" + captures + "] {\n";
                if (application_quit_runtime) result += "                if (auto command = commands->lock()) (void)command->cancel();\n";
                if (application_files!="{}") result += "                if (auto owner = files.lock()) (void)owner->cancel_file_events();\n";
                if (application_state_runtime) result += "                if (auto owner = lifecycle.lock()) (void)owner->cancel_state_events();\n";
                if (scene_registry_runtime) result += "                if (auto scenes = registry.lock()) (void)scenes->cancel();\n";
                if (multiple_scenes) result += "                if (auto state = weak.lock()) (void)state->cancel();\n";
                if (multiple_scenes) result += R"LVUI(                if (auto state = windows->lock()) (void)state->cancel();
            };
            auto approve_shutdown = )LVUI" + std::string(window_close_runtime ? "all_windows_close_request_handler(application, windows_link, cancel_pending);\n" : "close_request_handler(application, cancel_pending);\n") + R"LVUI(
            callbacks.close_requested = [close_approved, approve_shutdown = std::move(approve_shutdown)]() mutable {
                if (std::exchange(*close_approved, false)) return true;
                return approve_shutdown();
            };
)LVUI";
                else result += application_close_runtime ? "            });\n" : "                return true;\n            };\n";
            }
            if(multiple_scenes) result += "            auto host = ii::gui::attach_application(target, options(), std::move(callbacks));\n";
            else if(application_files!="{}" || application_close_runtime) result += "            auto host = ii::gui::attach_application(target, options(), std::move(callbacks));\n";
            else result += "            auto host = ii::gui::attach_application(target, options());\n";
            result += "            if (!host) return std::unexpected(host.error());\n";
            if(multiple_scenes) result += "            if (auto configured = (*host)->set_application_driver(true); !configured) return configured;\n";
            if(multiple_scenes) result += R"LVUI(            auto driver_close = [weak = std::weak_ptr(*host)]() -> ii::core::Result<void> {
                auto driver = weak.lock();
                if (!driver) return ii::core::fail(ii::core::ErrorCode::closed, "Application driver is unavailable");
                return driver->request_close();
            };
            auto scene_windows = std::make_shared<lvui::native::SceneWindowSet>(scene_count,
                last_scene_close(application, target.event_loop(), close_approved, cancel_pending, driver_close));
            *windows_link = scene_windows;
            auto empty_scene = [weak = std::weak_ptr(startup), driver_close]() -> ii::core::Result<void> {
                auto state = weak.lock();
                if (!state || !state->accepts_runtime()) return {};
                return driver_close();
            };
)LVUI";
            if(application_quit_runtime) result += R"LVUI(            auto commands = std::make_shared<lvui::native::ApplicationCommands>(
                [sender = target.event_loop().sender()](lvui::native::ApplicationCommands::Task task) -> ii::core::Result<void> {
                    auto submitted = sender.post(std::move(task));
                    if (submitted) return {};
                    switch (submitted.error) {
                    case ii::runtime::SubmitError::full: return ii::core::fail(ii::core::ErrorCode::full, "Application event queue is full");
                    case ii::runtime::SubmitError::stopped: return ii::core::fail(ii::core::ErrorCode::closed, "Application event loop is stopped");
                    case ii::runtime::SubmitError::invalid: return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Invalid application command");
                    default: return ii::core::fail(ii::core::ErrorCode::busy, "Application event queue is overloaded");
                    }
                }, [weak = std::weak_ptr(*host)]() -> ii::core::Result<void> {
                    auto owner = weak.lock();
                    if (!owner) return ii::core::fail(ii::core::ErrorCode::closed, "Application host is unavailable");
                    return owner->request_close();
                }, [weak = std::weak_ptr(application)](const ii::core::Error& error) {
                    if (auto owner = weak.lock()) (void)owner->report_close_failure(error);
                });
            *command_link = commands;
            if (auto bound = application->bind_application_commands(commands); !bound) return bound;
)LVUI";
            if(scene_registry_runtime) {
                for (std::size_t index = 0; index < application_scene_ids.size(); ++index) {
                    if (application_scene_ids[index].empty()) continue;
                    const auto number = std::to_string(index);
                    result += "            if (auto declared = scene_registry->define(" + literal(application_scene_ids[index]) + R"LVUI(,
                [parent = *host, application, startup, scene_hosts, scene_windows, empty_scene, loop = &target.event_loop()](bool initial) -> ii::core::Result<lvui::native::SceneInstance> {
                    if (!startup->accepts_runtime()) return ii::core::fail(ii::core::ErrorCode::closed, "Application scene startup has ended");
)LVUI" + "                    auto scene_options = options_for_scene(" + number + ");\n" + R"LVUI(                    if (!scene_options) return std::unexpected(scene_options.error());
                    struct Generation final {
                        std::shared_ptr<ii::gui::Host> host;
)LVUI" + "                        std::shared_ptr<" + options.name + "> view;\n" + R"LVUI(                    };
                    auto generation = std::make_shared<Generation>();
                    generation->host = std::make_shared<ii::gui::Host>(std::move(*scene_options));
)LVUI" + "                    generation->view = std::make_shared<" + options.name + ">(" + number + ");\n" + R"LVUI(                    lvui::native::SceneInstance instance{
                        generation,
                        [generation]() -> ii::core::Result<bool> {
                            auto snapshot = generation->host->snapshot();
                            if (!snapshot) return std::unexpected(snapshot.error());
                            return snapshot->closed;
                        },
                        [generation]() -> ii::core::Result<void> { return generation->host->request_close(); }};
                    if (auto configured = generation->host->set_failure_handler(scene_failure_handler(startup, *loop)); !configured)
                        return std::unexpected(configured.error());
)LVUI" + "                    auto ticket = scene_windows->activate(" + number + ", true" + std::string(window_close_runtime ? ", generation->view->window_close_guard()" : "") + R"LVUI();
                    if (!ticket) return std::unexpected(ticket.error());
                    SceneReservation reservation{scene_windows, *ticket};
)LVUI" + "                    auto callbacks = scene_callbacks(generation->host, startup, *loop, " + number + R"LVUI(,
                        [view = generation->view, application](ii::gui::Host& host) { return view->attach_with_application(host, application); }, initial);
)LVUI" + "                    bind_scene_window(callbacks, scene_windows, *ticket, empty_scene, scene_failure_handler(startup, *loop));\n"
                    "                    (*scene_hosts)[" + number + R"LVUI(] = generation->host;
                    if (auto shown = generation->host->show(*parent, std::move(callbacks)); !shown) return std::unexpected(shown.error());
                    reservation.committed = true;
                    return instance;
                }); !declared) return declared;
)LVUI";
                }
            }
            if(application_active!="{}" || application_inactive!="{}") result +=
                "            if (auto configured = (*host)->set_application_activity_handler([weak = std::weak_ptr(application)](bool value) {\n"
                "                if (auto owner = weak.lock()) if (auto delivered = owner->notify_activity(value); !delivered) {\n"
                "                    if (delivered.error().code == ii::core::ErrorCode::closed && owner->state_events_cancelled()) return;\n"
                "                    if (delivered.error().cause) std::rethrow_exception(delivered.error().cause);\n"
                "                    throw std::runtime_error(delivered.error().message);\n"
                "                }\n"
                "            }); !configured) return configured;\n";
            if(application_foreground!="{}" || application_background!="{}") result +=
                "            if (auto configured = (*host)->set_application_foreground_handler([weak = std::weak_ptr(application)](bool value) {\n"
                "                if (auto owner = weak.lock()) if (auto delivered = owner->notify_foreground(value); !delivered) {\n"
                "                    if (delivered.error().code == ii::core::ErrorCode::closed && owner->state_events_cancelled()) return;\n"
                "                    if (delivered.error().cause) std::rethrow_exception(delivered.error().cause);\n"
                "                    throw std::runtime_error(delivered.error().message);\n"
                "                }\n"
                "            }); !configured) return configured;\n";
            result += "            if (auto attached = target.attach_before_driver(application_attachment(\"application\", application)); !attached) return attached;\n";
            if(application_quit_runtime) result += R"LVUI(            if (auto attached = target.attach_before_driver({"application-commands",
                [](ii::runtime::Application&) -> ii::core::Result<void> { return {}; },
                [commands]() -> ii::core::Result<void> { return commands->cancel(); }}); !attached) return attached;
)LVUI";
            if(application_arguments!="{}") result += "            if (auto attached = target.attach_before_driver(launch_arguments_attachment(\"launch-arguments\", application)); !attached) return attached;\n";
            if(scene_registry_runtime) result += R"LVUI(            if (auto attached = target.attach_before_driver({"scene-registry",
                [](ii::runtime::Application&) -> ii::core::Result<void> { return {}; },
                [scene_registry, startup]() -> ii::core::Result<void> { return startup->finish([&] { return scene_registry->shutdown(); }); }}); !attached) return attached;
)LVUI";
            std::string named_scene_branches;
            if (scene_registry_runtime) {
                for (std::size_t index = 0; index < application_scene_ids.size(); ++index) {
                    if (application_scene_ids[index].empty()) continue;
                    const auto number = std::to_string(index);
                    named_scene_branches += "                if (index == " + number + ") {\n"
                        "                    if (auto attached = target.attach({\"scene/" + number + "\",\n"
                        "                        [scene_registry](ii::runtime::Application&) -> ii::core::Result<void> { " +
                        std::string(application_scene_initial[index] ? "return scene_registry->open_initial(" + literal(application_scene_ids[index]) + ");" : "return {};") + " },\n"
                        "                        [startup]() -> ii::core::Result<void> { return startup->outcome(); }}); !attached) return attached;\n"
                        "                    continue;\n                }\n";
                }
            }
            if(multiple_scenes) result += R"LVUI(            for (std::size_t index = 0; index < scene_count; ++index) {
)LVUI" + named_scene_branches + R"LVUI(
                auto scene_options = options_for_scene(index);
                if (!scene_options) return std::unexpected(scene_options.error());
                auto child = std::make_shared<ii::gui::Host>(std::move(*scene_options));
                (*scene_hosts)[index] = child;
)LVUI" + std::string("                auto child_view = std::make_shared<")+options.name+R"LVUI(>(index);
                auto loop = &target.event_loop();
                if (auto attached = target.attach({"scene/" + std::to_string(index),
                    [parent = *host, child, child_view, application, startup, scene_windows, empty_scene, loop, index](ii::runtime::Application&) -> ii::core::Result<void> {
                        if (auto configured = child->set_failure_handler(scene_failure_handler(startup, *loop)); !configured) return configured;
                        auto ticket = scene_windows->activate(index)LVUI" + std::string(window_close_runtime ? ", false, child_view->window_close_guard()" : "") + R"LVUI();
                        if (!ticket) return std::unexpected(ticket.error());
                        SceneReservation reservation{scene_windows, *ticket};
                        auto callbacks = scene_callbacks(child, startup, *loop, index,
                            [child_view, application](ii::gui::Host& host) { return child_view->attach_with_application(host, application); });
                        bind_scene_window(callbacks, scene_windows, *ticket, empty_scene, scene_failure_handler(startup, *loop));
                        if (auto shown = child->show(*parent, std::move(callbacks)); !shown) return shown;
                        reservation.committed = true;
                        return {};
                    },
                    [child, child_view, application, startup]() -> ii::core::Result<void> {
                        return startup->finish([&] {
)LVUI" + std::string(scene_lifecycle_runtime ?
                            "                            auto detached = child_view->detach(*child); if (auto outcome = application->scene_outcome(); !outcome) return outcome; return detached;\n" :
                            "                            return child_view->detach(*child);\n") + R"LVUI(                        });
                    }}); !attached) return attached;
            }
            return {};
)LVUI";
            else result +=
                "            return target.attach({\"main\",\n"
                "                [host = *host, view, application](ii::runtime::Application&) -> ii::core::Result<void> { if (auto attached = view->attach_with_application(*host, application); !attached) return attached; return application->notify_ready(); },\n"
                + std::string(scene_lifecycle_runtime ?
                    "                [host = *host, view, application]() -> ii::core::Result<void> { auto detached = view->detach(*host); if (auto outcome = application->scene_outcome(); !outcome) return outcome; return detached; }});\n" :
                    "                [host = *host, view]() { return view->detach(*host); }});\n");
            result += "        });\n    }\n"
                "    ii::core::Result<void> attach_with_application(ii::gui::Host& host, std::shared_ptr<ApplicationOwner> application) {\n"
                "        auto current = native_widget_.snapshot();\n"
                "        if (current) return ii::core::fail(ii::core::ErrorCode::busy, \"Native C++ widget is already attached\");\n"
                "        if (current.error().code != ii::core::ErrorCode::closed) return std::unexpected(current.error());\n"
                "        if (!application) return ii::core::fail(ii::core::ErrorCode::invalid_argument, \"Application scope owner is missing\");\n"
                + std::string(window_close_runtime ? "        if (auto prepared = close_guard_->prepare(); !prepared) return prepared;\n" : "") +
                "        std::shared_ptr<Scope> owner;\n"
                "        auto created = mount(host, {}, &owner, {}, " + std::string(scene_lifecycle_runtime?"application":"std::move(application)") + std::string(multiple_scenes?", scene_index_":"") + std::string(window_close_runtime?", close_guard_":"") + ");\n"
                + std::string(scene_lifecycle_runtime ?
                    "        if (!created) { if (auto outcome = application->scene_outcome(); !outcome) return outcome; return std::unexpected(created.error()); }\n" :
                    "        if (!created) return std::unexpected(created.error());\n") +
                std::string(scene_lifecycle_runtime ? "        if (auto outcome = application->scene_outcome(); !outcome) { (void)host.unmount_widgets(*created); return outcome; }\n" : "") +
                "        native_widget_ = *created; scope_ = std::move(owner);\n"
                "        return {};\n    }\n";
        }
        result += "    ii::core::Result<void> attach(ii::gui::Host& host" + configuration + ") {\n"
            "        auto current = native_widget_.snapshot();\n"
            "        if (current) return ii::core::fail(ii::core::ErrorCode::busy, \"Native C++ widget is already attached\");\n"
            "        if (current.error().code != ii::core::ErrorCode::closed) return std::unexpected(current.error());\n"
            + std::string(window_close_runtime ? "        if (auto prepared = close_guard_->prepare(); !prepared) return prepared;\n" : "") +
            std::string(scope.empty() ? "" : "        std::shared_ptr<Scope> owner;\n") +
            "        auto created = mount(host" + std::string(scope.empty() ? "" : ", configure, &owner, factories") + std::string(window_close_runtime ? (multiple_scenes ? ", {}, scene_index_, close_guard_" : ", {}, close_guard_") : (multiple_scenes ? ", {}, scene_index_" : "")) + ");\n"
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
            + std::string(window_close_runtime ? "    std::shared_ptr<lvui::native::WindowCloseGuard> window_close_guard() const { return close_guard_; }\n" : "")
            + accessors + "private:\n    ii::gui::WidgetMount native_widget_;\n" + std::string(multiple_scenes?"    std::size_t scene_index_ = 0;\n":"") + std::string(window_close_runtime?"    std::shared_ptr<lvui::native::WindowCloseGuard> close_guard_ = std::make_shared<lvui::native::WindowCloseGuard>();\n":"") + std::string(scope.empty() ? "" : "    std::shared_ptr<Scope> scope_;\n") +
            "};\n} // namespace lvui_generated\n";
        return result;
    }
};
} // namespace
NativeCompileResult compile_native(std::string_view source, const NativeCompileOptions& options) {
    auto valid = default_style_tokens().validate(options.style_tokens);
    if (!valid)
        return std::unexpected(Diagnostic{0, 1, 1, valid.error().message});
    auto document = parse_view(source);
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
