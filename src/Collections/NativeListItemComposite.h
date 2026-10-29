#pragma once

#include <LVUI/NativeCheckBox.h>
#include <LVUI/NativeListItem.h>
#include <LVUI/NativeListFooter.h>
#include <LVUI/ListItemQuantity.h>
#include <LVUI/NativeListItemSelector.h>
#include <LVUI/NativeInputField.h>
#include <LVUI/NativeStepper.h>
#include <LVUI/NativeToggleSwitch.h>

#include <iiGUI/NativeWidgets.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <iomanip>
#include <limits>
#include <locale>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace lvui::native {

struct ListItemCompositeInput final {
    ListItemInput row;
    std::optional<bool> checked;
    std::optional<double> quantity;
    ListItemSelectorConfiguration selector;
    std::optional<int> selector_index;
    std::optional<std::string> input_text;
    std::string input_placeholder;
    double minimum_quantity=0,maximum_quantity=std::numeric_limits<double>::infinity(),step_size=1,quantity_width=48,quantity_spacing=4;
    bool toggle_enabled=true,selection_enabled=true,show_toggle=true,show_selection=true;
    bool stepper_enabled=true,show_stepper=true;
    bool selector_enabled=true,show_selector=true;
    bool input_enabled=true,input_read_only=false,show_input=true;
    bool show_primary_action=true,show_secondary_action=true,show_more_menu=true;
    ListFooterSlot primary_action{.text="Action"},secondary_action{.text="Secondary"},more_menu{.text="More"};
    double slot_padding=4,slot_spacing=8,action_width=72,selector_width=97,input_width=140,input_height=30;
    friend bool operator==(const ListItemCompositeInput&,const ListItemCompositeInput&)=default;
};
struct NativeListItemCompositeCallbacks final {
    std::function<void()> activated;
    std::function<void(bool)> checked_changed;
    std::function<void(std::string,const ListItemValue&)> edited;
    std::function<void(std::string,const ListFooterSlot&)> action_triggered;
    std::function<void(double)> quantity_changed;
    std::function<void(int)> selector_index_changed;
    std::function<void(std::string)> input_text_changed,input_submitted;
};

// Compound behavior is assembled from LVUI's own retained controls; the row
// model remains independently usable by virtualized lists.
class NativeListItemComposite final {
    struct State final {
        ListItemCompositeInput input;
        std::optional<NativeListItem> row;
        std::optional<NativeToggleSwitch> toggle;
        std::optional<NativeCheckBox> checkbox;
        std::optional<NativeListFooter> actions;
        std::optional<NativeStepper> stepper;
        std::optional<NativeListItemSelector> selector;
        std::optional<NativeInputField> input_field;
        std::optional<InputFieldConfiguration> applied_input;
        ListItemQuantity quantity;
        ii::gui::Widget quantity_label;
        ii::gui::Widget root;
        NativeListItemCompositeCallbacks callbacks;
        ii::gui::Size extent;
        std::optional<ii::core::Error> failure;
        const std::thread::id owner=std::this_thread::get_id();
        std::uint64_t revision=0;
        bool checked=false,live=false;
        int selector_index=0;
        std::string input_text;
    };
    explicit NativeListItemComposite(std::shared_ptr<State> state):state_(std::move(state)){}
    [[nodiscard]] static bool has_toggle(const ListItemCompositeInput& input)noexcept{
        return input.row.configuration.variant==ListItemVariant::toggle&&input.show_toggle;
    }
    [[nodiscard]] static bool has_checkbox(const ListItemCompositeInput& input)noexcept{
        return input.row.configuration.variant==ListItemVariant::checkable&&input.show_selection;
    }
    [[nodiscard]] static bool has_stepper(const ListItemCompositeInput& input)noexcept{
        return input.row.configuration.variant==ListItemVariant::stepper&&input.show_stepper;
    }
    [[nodiscard]] static bool has_quantity(const ListItemCompositeInput& input)noexcept{
        return input.row.configuration.variant==ListItemVariant::stepper;
    }
    [[nodiscard]] static bool has_selector(const ListItemCompositeInput& input)noexcept{
        return input.row.configuration.variant==ListItemVariant::select&&input.show_selector;
    }
    [[nodiscard]] static bool has_input(const ListItemCompositeInput& input)noexcept{
        return input.row.configuration.variant==ListItemVariant::inline_edit&&input.show_input;
    }
    [[nodiscard]] static bool has_action(const ListItemCompositeInput& input,std::size_t slot)noexcept{
        const auto type=input.row.configuration.variant;
        if(slot==0)return (type==ListItemVariant::action||type==ListItemVariant::action_group||type==ListItemVariant::inline_edit)&&input.show_primary_action&&input.primary_action.visible;
        if(slot==1)return type==ListItemVariant::action_group&&input.show_secondary_action&&input.secondary_action.visible;
        return type==ListItemVariant::action_group&&input.show_more_menu&&input.more_menu.visible;
    }
    [[nodiscard]] static double actions_width(const ListItemCompositeInput& input)noexcept{
        const auto count=unsigned(has_action(input,0))+unsigned(has_action(input,1))+unsigned(has_action(input,2));
        return std::min(8192.,double(count)*input.action_width+double(count?count-1:0)*input.slot_spacing);
    }
    [[nodiscard]] static double inline_width(const ListItemCompositeInput& input)noexcept{
        const auto action=actions_width(input);return (has_input(input)?input.input_width:0)+action+(has_input(input)&&action>0?input.slot_spacing:0);
    }
    [[nodiscard]] static bool default_checked(const ListItemCompositeInput& input)noexcept{
        return input.row.configuration.variant==ListItemVariant::toggle;
    }
    [[nodiscard]] static bool valid(const ListItemCompositeInput& input)noexcept{
        return ListItem::validate(input.row.configuration)&&input.row.icon_revision>=0&&
            !(input.row.icon&&!input.row.configuration.icon_name.empty())&&std::isfinite(input.slot_padding)&&
            input.slot_padding>=0&&input.slot_padding<=8192&&std::isfinite(input.slot_spacing)&&input.slot_spacing>=0&&input.slot_spacing<=8192&&
            std::isfinite(input.action_width)&&input.action_width>=1&&input.action_width<=8192&&
            std::isfinite(input.minimum_quantity)&&
            (std::isfinite(input.maximum_quantity)||input.maximum_quantity==std::numeric_limits<double>::infinity())&&
            input.minimum_quantity<=input.maximum_quantity&&std::isfinite(input.step_size)&&input.step_size>0&&
            std::isfinite(input.quantity_width)&&input.quantity_width>=0&&input.quantity_width<=8192&&
            std::isfinite(input.quantity_spacing)&&input.quantity_spacing>=0&&input.quantity_spacing<=8192&&
            std::isfinite(input.selector_width)&&input.selector_width>=19&&input.selector_width<=8176&&ListItemSelector::validate(input.selector)&&
            std::isfinite(input.input_width)&&input.input_width>=1&&input.input_width<=8192&&std::isfinite(input.input_height)&&input.input_height>=1&&input.input_height<=8192&&
            valid_accessibility_text(input.input_placeholder)&&(!input.input_text||valid_accessibility_text(*input.input_text))&&
            (!input.selector_index||(*input.selector_index>=0&&(input.selector.options.empty()?*input.selector_index==0:
                std::size_t(*input.selector_index)<input.selector.options.size())))&&
            (!input.quantity||(std::isfinite(*input.quantity)&&*input.quantity>=input.minimum_quantity&&*input.quantity<=input.maximum_quantity))&&
            valid_accessibility_text(input.primary_action.text)&&valid_accessibility_text(input.primary_action.accessibility_name)&&
            valid_accessibility_text(input.secondary_action.text)&&valid_accessibility_text(input.secondary_action.accessibility_name)&&
            valid_accessibility_text(input.more_menu.text)&&valid_accessibility_text(input.more_menu.accessibility_name)&&
            input.primary_action.icon_name.size()<=256&&valid_accessibility_text(input.primary_action.icon_name)&&
            input.secondary_action.icon_name.size()<=256&&valid_accessibility_text(input.secondary_action.icon_name)&&
            input.more_menu.icon_name.size()<=256&&valid_accessibility_text(input.more_menu.icon_name);
    }
    [[nodiscard]] static ListItemInput row_input(const State& state){
        auto value=state.input.row;const auto reserve=has_toggle(state.input)?38.:has_checkbox(state.input)?18.:
            has_quantity(state.input)?state.input.quantity_width+(has_stepper(state.input)?18.+state.input.quantity_spacing:0.):
            has_selector(state.input)?state.input.selector_width:
            state.input.row.configuration.variant==ListItemVariant::inline_edit?inline_width(state.input):actions_width(state.input);
        if(reserve>0)value.configuration.right_padding=std::min(8192.,value.configuration.right_padding+reserve+state.input.slot_padding+state.input.slot_spacing);
        return value;
    }
    [[nodiscard]] static ToggleSwitchConfiguration toggle_input(const State& state){
        ToggleSwitchConfiguration value;value.checked=state.checked;value.enabled=state.input.row.configuration.enabled&&state.input.toggle_enabled;
        value.accessibility_name=native_display_text(state.input.row.configuration.accessibility_name.empty()?state.input.row.configuration.label:state.input.row.configuration.accessibility_name);
        return value;
    }
    [[nodiscard]] static CheckBoxConfiguration checkbox_input(const State& state){
        CheckBoxConfiguration value;value.checked=state.checked;value.enabled=state.input.row.configuration.enabled&&state.input.selection_enabled;
        value.accessibility_name=native_display_text(state.input.row.configuration.accessibility_name.empty()?state.input.row.configuration.label:state.input.row.configuration.accessibility_name);
        return value;
    }
    [[nodiscard]] static ListItemQuantityConfiguration quantity_input(const ListItemCompositeInput& input,double value)noexcept{
        return {.value=value,.minimum=input.minimum_quantity,.maximum=input.maximum_quantity,.step=input.step_size,
                .enabled=input.row.configuration.enabled&&input.stepper_enabled};
    }
    [[nodiscard]] static StepperConfiguration stepper_input(const State& state){
        StepperConfiguration value;value.enabled=state.input.row.configuration.enabled&&state.input.stepper_enabled;
        value.accessibility_name=native_display_text(state.input.row.configuration.accessibility_name.empty()?state.input.row.configuration.label:state.input.row.configuration.accessibility_name);
        return value;
    }
    [[nodiscard]] static ii::gui::LabelOptions quantity_label_input(const State& state){
        std::ostringstream stream;stream.imbue(std::locale::classic());stream<<std::setprecision(12)<<state.quantity.configuration().value;
        const auto& palette=state.input.row.configuration.palette;
        const auto ink=state.input.row.configuration.enabled&&state.input.stepper_enabled?palette.text:palette.disabled_text;
        return {.text=stream.str(),.font_size=13,.selectable=false,.role=ii::gui::TextRole::body,
                .color=ii::gui::WidgetColor{ink.red,ink.green,ink.blue,ink.alpha},.alignment=ii::gui::TextAlignment::right,
                .line_height=16,.font_weight=500u,.elide_right=true};
    }
    [[nodiscard]] static ListItemSelectorInput selector_input(const State& state){
        ListItemSelectorInput value;value.configuration=state.input.selector;value.configuration.selected_index=state.selector_index;
        value.configuration.enabled=value.configuration.enabled&&state.input.selector_enabled&&state.input.row.configuration.enabled;
        value.width=state.input.selector_width;value.visible=has_selector(state.input)&&state.input.row.configuration.visible;
        value.accessibility_name=native_display_text(state.input.row.configuration.accessibility_name.empty()?state.input.row.configuration.label:state.input.row.configuration.accessibility_name);
        return value;
    }
    [[nodiscard]] static InputFieldConfiguration input_field_input(const State& state){
        InputFieldConfiguration value;value.text=state.input_text;value.placeholder=state.input.input_placeholder;
        value.width=state.input.input_width;value.height=state.input.input_height;
        value.enabled=state.input.row.configuration.enabled&&state.input.input_enabled;value.read_only=state.input.input_read_only;
        value.accessibility_name=native_display_text(state.input.row.configuration.accessibility_name.empty()?state.input.row.configuration.label:state.input.row.configuration.accessibility_name);
        return value;
    }
    [[nodiscard]] static ListFooterInput actions_input(const State& state){
        ListFooterInput value;auto& c=value.configuration;c.slots={state.input.primary_action,state.input.secondary_action,state.input.more_menu};
        for(std::size_t i=0;i<c.slots.size();++i)c.slots[i].visible=has_action(state.input,i);
        c.width=actions_width(state.input);c.height=22;c.horizontal_padding=0;c.vertical_padding=0;c.spacing=state.input.slot_spacing;
        c.enabled=state.input.row.configuration.enabled;value.icon_source=state.input.row.icon_source;value.icon_revision=state.input.row.icon_revision;return value;
    }
    [[nodiscard]] static ii::core::Result<void> frames(const std::shared_ptr<State>& state){
        if(!state->row||!state->toggle||!state->checkbox||!state->actions||!state->stepper||!state->selector||!state->input_field)return ii::core::fail(ii::core::ErrorCode::closed,"List item controls are unavailable");
        const auto width=state->extent.width,height=state->extent.height;
        if(!std::isfinite(width)||!std::isfinite(height)||width<0||height<0||width>8192||height>8192)
            return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid list item extent");
        if(auto placed=state->row->root().set_frame({0,0,float(width),float(height)});!placed)return placed;
        const auto right=std::max(0.,width-std::min(state->input.slot_padding,width));
        const auto place=[&](ii::gui::Widget widget,bool shown,double preferred_width,double preferred_height)->ii::core::Result<void>{
            const auto w=std::min(preferred_width,right),h=std::min(preferred_height,height);shown=shown&&w>0&&h>0;
            if(auto visible=widget.set_visible(shown);!visible)return visible;
            if(shown)return widget.set_frame({float(right-w),float((height-h)/2),float(w),float(h)});return {};
        };
        if(auto result=place(state->toggle->root(),has_toggle(state->input),38,22);!result)return result;
        if(auto result=place(state->checkbox->root(),has_checkbox(state->input),18,18);!result)return result;
        if(auto result=place(state->actions->root(),actions_width(state->input)>0,actions_width(state->input),22);!result)return result;
        if(auto result=place(state->stepper->root(),has_stepper(state->input),18,18);!result)return result;
        const auto step_width=std::min(18.,right),label_right=std::max(0.,right-(has_stepper(state->input)?step_width+state->input.quantity_spacing:0.));
        const auto label_width=std::min(state->input.quantity_width,label_right),label_height=std::min(16.,height);
        const auto label_shown=has_quantity(state->input)&&label_width>0&&label_height>0;
        if(auto visible=state->quantity_label.set_visible(label_shown);!visible)return visible;
        if(label_shown)if(auto frame=state->quantity_label.set_frame({float(label_right-label_width),float((height-label_height)/2),float(label_width),float(label_height)});!frame)return frame;
        if(auto result=place(state->selector->root(),has_selector(state->input)&&state->input.row.configuration.visible,state->input.selector_width,20);!result)return result;
        const auto actions=actions_width(state->input);const auto input_right=std::max(0.,right-(actions>0?actions+state->input.slot_spacing:0.));
        const auto input_width=std::min(state->input.input_width,input_right),input_height=std::min(state->input.input_height,height);
        const auto input_shown=has_input(state->input)&&state->input.row.configuration.visible&&input_width>0&&input_height>0;
        if(auto visible=state->input_field->root().set_visible(input_shown);!visible)return visible;
        if(input_shown)return state->input_field->root().set_frame({float(input_right-input_width),float((height-input_height)/2),float(input_width),float(input_height)});
        return {};
    }
    [[nodiscard]] static ii::core::Result<bool> apply(const std::shared_ptr<State>& state,std::uint64_t revision){
        const auto step=[&](auto&& operation)->ii::core::Result<bool>{if(state->revision!=revision)return false;auto result=operation();
            if(!result)return std::unexpected(result.error());return state->revision==revision;};
        ListItem model;if(!model.set_configuration(state->input.row.configuration))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid list item configuration");
        const auto size=model.intrinsic_size();
        auto result=step([&]{return state->root.set_extent({size.width,size.height});});if(!result||!*result)return result;
        result=step([&]{return state->root.set_visible(state->input.row.configuration.visible);});if(!result||!*result)return result;
        result=step([&]{return state->row->set_options(row_input(*state));});if(!result||!*result)return result;
        result=step([&]{return state->toggle->set_options(toggle_input(*state));});if(!result||!*result)return result;
        result=step([&]{return state->checkbox->set_options(checkbox_input(*state));});if(!result||!*result)return result;
        result=step([&]{return state->actions->set_options(actions_input(*state));});if(!result||!*result)return result;
        result=step([&]{return state->stepper->set_options(stepper_input(*state));});if(!result||!*result)return result;
        result=step([&]{return state->quantity_label.set_label_options(quantity_label_input(*state));});if(!result||!*result)return result;
        result=step([&]{return state->selector->set_options(selector_input(*state));});if(!result||!*result)return result;
        result=step([&]()->ii::core::Result<void>{auto options=input_field_input(*state);
            if(state->applied_input&&*state->applied_input==options)return {};
            auto changed=state->input_field->set_options(options);if(changed&&state->revision==revision)state->applied_input=std::move(options);return changed;});
        if(!result||!*result)return result;
        return step([&]{return frames(state);});
    }
    [[nodiscard]] static ii::core::Result<void> update(const std::shared_ptr<State>& state,ListItemCompositeInput input){
        if(state->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"List item requires its owner thread");
        try{
            if(auto live=state->root.snapshot();!live)return std::unexpected(live.error());if(!valid(input))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid list item composition");
            if(input==state->input)return {};
            auto echo=state->input;echo.checked=input.checked;echo.quantity=input.quantity;echo.selector_index=input.selector_index;echo.selector.selected_index=input.selector.selected_index;echo.input_text=input.input_text;
            const bool checked_echo=input.checked==state->input.checked||(input.checked&&*input.checked==state->checked);
            const bool quantity_echo=input.quantity==state->input.quantity||(input.quantity&&*input.quantity==state->quantity.configuration().value);
            const bool index_echo=input.selector_index==state->input.selector_index||(input.selector_index&&*input.selector_index==state->selector_index);
            const bool config_echo=input.selector.selected_index==state->input.selector.selected_index||input.selector.selected_index==state->selector_index;
            const bool text_echo=input.input_text==state->input.input_text||(input.input_text&&*input.input_text==state->input_text);
            if(echo==input&&checked_echo&&quantity_echo&&index_echo&&config_echo&&text_echo){state->input=std::move(input);return {};}
            if(state->revision==std::numeric_limits<std::uint64_t>::max())return ii::core::fail(ii::core::ErrorCode::out_of_range,"List item revision exhausted");
            const auto previous=state->input;const auto old_checked=state->checked;const auto old_extent=state->extent;const auto old_quantity=state->quantity;const auto old_index=state->selector_index;const auto old_text=state->input_text;
            auto next_checked=state->checked;
            if(input.checked!=previous.checked&&input.checked)next_checked=*input.checked;
            else if(input.row.configuration.variant!=previous.row.configuration.variant&&!input.checked)next_checked=default_checked(input);
            auto quantity_value=state->quantity.configuration().value;
            if(input.quantity!=previous.quantity&&input.quantity)quantity_value=*input.quantity;
            else quantity_value=std::clamp(quantity_value,input.minimum_quantity,input.maximum_quantity);
            auto next_quantity=state->quantity;(void)next_quantity.set_configuration(quantity_input(input,quantity_value));
            auto next_index=state->selector_index;
            if(input.selector_index!=previous.selector_index&&input.selector_index)next_index=*input.selector_index;
            else if(input.selector.options!=previous.selector.options||input.selector.selected_index!=previous.selector.selected_index)
                next_index=input.selector_index.value_or(input.selector.selected_index);
            auto next_text=state->input_text;if(input.input_text!=previous.input_text&&input.input_text)next_text=*input.input_text;
            const auto revision=++state->revision;state->input=std::move(input);state->checked=next_checked;state->quantity=next_quantity;state->selector_index=next_index;state->input_text=std::move(next_text);
            ListItem model;(void)model.set_configuration(state->input.row.configuration);const auto size=model.intrinsic_size();state->extent={size.width,size.height};
            auto result=apply(state,revision);
            if(!result){if(state->revision==revision){state->input=previous;state->checked=old_checked;state->quantity=old_quantity;state->selector_index=old_index;state->input_text=old_text;state->extent=old_extent;(void)apply(state,revision);}return std::unexpected(result.error());}
            state->failure.reset();return {};
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"List item update failed",std::current_exception());}
    }
    static void activated(const std::shared_ptr<State>& state)noexcept{
        try{if(state->live&&state->callbacks.activated){auto callback=state->callbacks.activated;callback();}}
        catch(...){try{state->failure=ii::core::Error{ii::core::ErrorCode::failed,"List item activation failed",std::current_exception()};}catch(...) {}}
    }
    static void triggered(const std::shared_ptr<State>& state,std::size_t index,const ListFooterSlot& slot)noexcept{
        try{if(!state->live||index>2||!has_action(state->input,index)||!state->input.row.configuration.enabled)return;
            if(state->callbacks.action_triggered){auto callback=state->callbacks.action_triggered;callback(index==0?"primary":index==1?"secondary":"more",slot);}}
        catch(...){try{state->failure=ii::core::Error{ii::core::ErrorCode::failed,"List item action failed",std::current_exception()};}catch(...) {}}
    }
    static void changed(const std::shared_ptr<State>& state,bool value)noexcept{
        try{
            if(!state->live||!state->input.row.configuration.enabled||!state->input.row.configuration.visible)return;
            if((has_toggle(state->input)&&!state->input.toggle_enabled)||(has_checkbox(state->input)&&!state->input.selection_enabled))return;
            if(!has_toggle(state->input)&&!has_checkbox(state->input))return;
            if(state->revision==std::numeric_limits<std::uint64_t>::max())throw std::out_of_range("List item revision exhausted");
            const auto old=state->checked;const auto revision=++state->revision;state->checked=value;auto result=apply(state,revision);
            if(!result){if(state->revision==revision){state->checked=old;(void)apply(state,revision);}state->failure=result.error();return;}
            if(!*result||!state->live||state->revision!=revision)return;
            if(state->callbacks.checked_changed){auto callback=state->callbacks.checked_changed;callback(value);}
            if(state->live&&state->revision==revision&&state->callbacks.edited){auto callback=state->callbacks.edited;const ListItemValue edit=value;callback("checked",edit);}
        }catch(...){try{state->failure=ii::core::Error{ii::core::ErrorCode::failed,"List item value change failed",std::current_exception()};}catch(...) {}}
    }
    static void stepped(const std::shared_ptr<State>& state,int direction)noexcept{
        try{
            if(!state->live||!state->input.row.configuration.visible||!has_stepper(state->input))return;
            auto next=state->quantity;const auto value=next.step(direction);if(!value)return;
            if(state->revision==std::numeric_limits<std::uint64_t>::max())throw std::out_of_range("List item revision exhausted");
            const auto old=state->quantity;const auto revision=++state->revision;state->quantity=next;auto result=apply(state,revision);
            if(!result){if(state->revision==revision){state->quantity=old;(void)apply(state,revision);}state->failure=result.error();return;}
            if(!*result||!state->live||state->revision!=revision)return;
            if(state->callbacks.quantity_changed){auto callback=state->callbacks.quantity_changed;callback(*value);}
            if(state->live&&state->revision==revision&&state->callbacks.edited){auto callback=state->callbacks.edited;const ListItemValue edit=*value;callback("quantity",edit);}
        }catch(...){try{state->failure=ii::core::Error{ii::core::ErrorCode::failed,"List item quantity change failed",std::current_exception()};}catch(...) {}}
    }
    static void selected(const std::shared_ptr<State>& state,int value)noexcept{
        try{
            if(!state->live||!state->input.row.configuration.visible||!state->input.row.configuration.enabled||!state->input.selector_enabled||
               !state->input.selector.enabled||!has_selector(state->input)||state->selector_index==value)return;
            if(state->revision==std::numeric_limits<std::uint64_t>::max())throw std::out_of_range("List item revision exhausted");
            const auto old=state->selector_index;const auto revision=++state->revision;state->selector_index=value;auto result=apply(state,revision);
            if(!result){if(state->revision==revision){state->selector_index=old;(void)apply(state,revision);}state->failure=result.error();return;}
            if(!*result||!state->live||state->revision!=revision)return;
            if(state->callbacks.selector_index_changed){auto callback=state->callbacks.selector_index_changed;callback(value);}
            if(state->live&&state->revision==revision&&state->callbacks.edited){auto callback=state->callbacks.edited;const ListItemValue edit=std::int64_t(value);callback("selectorIndex",edit);}
        }catch(...){try{state->failure=ii::core::Error{ii::core::ErrorCode::failed,"List item selection failed",std::current_exception()};}catch(...) {}}
    }
    static void text_changed(const std::shared_ptr<State>& state,std::string value)noexcept{
        try{
            if(!state->live||!state->input.row.configuration.visible||!state->input.row.configuration.enabled||!state->input.input_enabled||
               state->input.input_read_only||!has_input(state->input)||state->input_text==value)return;
            if(state->revision==std::numeric_limits<std::uint64_t>::max())throw std::out_of_range("List item revision exhausted");
            const auto old=state->input_text;const auto revision=++state->revision;state->input_text=std::move(value);auto result=apply(state,revision);
            if(!result){if(state->revision==revision){state->input_text=old;(void)state->input_field->set_text(old);(void)apply(state,revision);}state->failure=result.error();return;}
            if(!*result||!state->live||state->revision!=revision)return;
            if(state->callbacks.input_text_changed){auto callback=state->callbacks.input_text_changed;callback(state->input_text);}
            if(state->live&&state->revision==revision&&state->callbacks.edited){auto callback=state->callbacks.edited;const ListItemValue edit=state->input_text;callback("inputText1",edit);}
        }catch(...){try{state->failure=ii::core::Error{ii::core::ErrorCode::failed,"List item text edit failed",std::current_exception()};}catch(...) {}}
    }
    static void text_submitted(const std::shared_ptr<State>& state)noexcept{
        try{
            if(!state->live||!state->input.row.configuration.visible||!state->input.row.configuration.enabled||!state->input.input_enabled||
               !has_input(state->input)||!state->input_field)return;
            auto snapshot=state->input_field->snapshot();if(!snapshot){state->failure=snapshot.error();return;}if(snapshot->marked)return;
            if(state->callbacks.input_submitted){auto callback=state->callbacks.input_submitted;callback(snapshot->text);}
        }catch(...){try{state->failure=ii::core::Error{ii::core::ErrorCode::failed,"List item text submission failed",std::current_exception()};}catch(...) {}}
    }
public:
    [[nodiscard]] static ii::core::Result<NativeListItemComposite> create(ii::gui::WidgetBuilder& builder,ii::gui::Widget parent,std::string_view id,
            ListItemCompositeInput input={},NativeListItemCompositeCallbacks callbacks={}){
        try{
            if(!valid(input))return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Invalid list item composition");
            auto state=std::make_shared<State>();state->input=std::move(input);state->callbacks=std::move(callbacks);
            state->checked=state->input.checked.value_or(default_checked(state->input));ListItem model;(void)model.set_configuration(state->input.row.configuration);
            state->selector_index=state->input.selector_index.value_or(state->input.selector.selected_index);
            state->input_text=state->input.input_text.value_or("Value");
            const auto initial_quantity=state->input.quantity.value_or(std::clamp(1.,state->input.minimum_quantity,state->input.maximum_quantity));
            (void)state->quantity.set_configuration(quantity_input(state->input,initial_quantity));
            const auto size=model.intrinsic_size();state->extent={size.width,size.height};std::weak_ptr<State> weak=state;
            const auto name=[&](std::string_view suffix){return std::string(id)+"."+std::string(suffix);};
            auto root=builder.absolute_stack(parent,id,state->extent);if(!root)return std::unexpected(root.error());state->root=*root;
            auto row=NativeListItem::create(builder,*root,name("row"),row_input(*state),{.activated=[weak]{if(auto state=weak.lock())activated(state);}});
            if(!row)return std::unexpected(row.error());state->row=*row;
            auto toggle=NativeToggleSwitch::create(builder,*root,name("toggle"),toggle_input(*state),[weak](bool value){if(auto state=weak.lock())changed(state,value);});
            if(!toggle)return std::unexpected(toggle.error());state->toggle=*toggle;
            auto check=NativeCheckBox::create(builder,*root,name("checkbox"),checkbox_input(*state),[weak](bool value){if(auto state=weak.lock())changed(state,value);});
            if(!check)return std::unexpected(check.error());state->checkbox=*check;
            auto actions=NativeListFooter::create(builder,*root,name("actions"),actions_input(*state),
                {.clicked=[weak](std::size_t index,const ListFooterSlot& slot){if(auto state=weak.lock())triggered(state,index,slot);}});
            if(!actions)return std::unexpected(actions.error());state->actions=*actions;
            auto stepper=NativeStepper::create(builder,*root,name("stepper"),stepper_input(*state),
                {.stepped=[weak](int direction){if(auto state=weak.lock())stepped(state,direction);}});
            if(!stepper)return std::unexpected(stepper.error());state->stepper=*stepper;
            auto quantity_label=builder.label(*root,name("quantity"),quantity_label_input(*state));
            if(!quantity_label)return std::unexpected(quantity_label.error());state->quantity_label=*quantity_label;
            auto selector=NativeListItemSelector::create(builder,*root,name("selector"),selector_input(*state),
                [weak](int index){if(auto state=weak.lock())selected(state,index);});
            if(!selector)return std::unexpected(selector.error());state->selector=*selector;
            auto field_options=input_field_input(*state);auto input_field=NativeInputField::create(builder,*root,name("input1"),field_options,{
                .edited=[weak](std::string text){if(auto state=weak.lock())text_changed(state,std::move(text));},
                .submitted=[weak]{if(auto state=weak.lock())text_submitted(state);}});
            if(!input_field)return std::unexpected(input_field.error());state->input_field=*input_field;state->applied_input=std::move(field_options);
            if(auto clipped=root->set_clip_content(true);!clipped)return std::unexpected(clipped.error());
            if(auto handler=root->set_layout_handler([weak](ii::gui::Rect bounds){if(auto state=weak.lock()){
                if(state->extent.width==bounds.width&&state->extent.height==bounds.height)return;state->extent={bounds.width,bounds.height};
                if(auto result=frames(state);!result)state->failure=result.error();
            }});!handler)return std::unexpected(handler.error());
            auto applied=apply(state,state->revision);if(!applied)return std::unexpected(applied.error());
            if(auto retained=builder.retain(state);!retained)return std::unexpected(retained.error());
            if(auto published=builder.on_publish([weak]{if(auto state=weak.lock())state->live=true;});!published)return std::unexpected(published.error());
            if(auto retired=builder.on_retire([weak]{if(auto state=weak.lock())state->live=false;});!retired)return std::unexpected(retired.error());
            return NativeListItemComposite(std::move(state));
        }catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"List item composition failed",std::current_exception());}
    }
    [[nodiscard]] ii::gui::Widget root()const{return state_->root;}
    [[nodiscard]] ii::gui::Widget row()const{return state_->row->root();}
    [[nodiscard]] ii::gui::Widget toggle()const{return state_->toggle->root();}
    [[nodiscard]] ii::gui::Widget checkbox()const{return state_->checkbox->root();}
    [[nodiscard]] ii::gui::Widget stepper()const{return state_->stepper->root();}
    [[nodiscard]] ii::core::Result<NativeListItemSelector> selector()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"List item requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());return *state_->selector;
    }
    [[nodiscard]] ii::core::Result<int> selector_index()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"List item requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());return state_->selector_index;
    }
    [[nodiscard]] ii::core::Result<NativeInputField> input_field()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"List item requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());return *state_->input_field;
    }
    [[nodiscard]] ii::core::Result<std::string> input_text()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"List item requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());return state_->input_text;
    }
    [[nodiscard]] ii::core::Result<double> quantity()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"List item requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());return state_->quantity.configuration().value;
    }
    [[nodiscard]] ii::core::Result<ii::gui::Widget> action(std::size_t index)const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"List item requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());return state_->actions->slot(index);
    }
    [[nodiscard]] ii::core::Result<ListItemCompositeInput> options()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"List item requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());return state_->input;
    }
    [[nodiscard]] ii::core::Result<bool> checked()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"List item requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());return state_->checked;
    }
    [[nodiscard]] ii::core::Result<void> set_options(ListItemCompositeInput value)const{return update(state_,std::move(value));}
    [[nodiscard]] ii::core::Result<std::optional<ii::core::Error>> failure()const{
        if(state_->owner!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"List item requires its owner thread");
        if(auto live=state_->root.snapshot();!live)return std::unexpected(live.error());
        if(state_->failure)return state_->failure;
        if(state_->actions){auto child=state_->actions->failure();if(!child)return std::unexpected(child.error());if(*child)return *child;}
        if(state_->selector){auto child=state_->selector->failure();if(!child)return std::unexpected(child.error());return *child;}
        return std::optional<ii::core::Error>{};
    }
private:
    std::shared_ptr<State> state_;
};

} // namespace lvui::native
