#pragma once
#include <LVUI/NativeIconCache.h>
#include <LVUI/NativeCheckBox.h>
#include <LVUI/NativeListItem.h>
#include <LVUI/NativeListFooter.h>
#include <LVUI/ListItemDetailActions.h>
#include <LVUI/ListItemDetailControls.h>
#include <LVUI/ListItemTask.h>
#include <LVUI/ListItemPreviewRow.h>
#include <LVUI/ListItemForm.h>
#include <LVUI/ListItemQuantity.h>
#include <LVUI/NativeListItemSelector.h>
#include <LVUI/NativeListItemSegments.h>
#include <LVUI/NativeInputField.h>
#include <LVUI/NativeStepper.h>
#include <LVUI/NativeToggleSwitch.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <algorithm>
#include <any>
#include <array>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>
namespace lvui::native {
[[nodiscard]] inline std::vector<ListItemOption> list_item_menu_options(std::vector<ListItemOption> value){return value;}
[[nodiscard]] inline std::vector<ListItemMenuEntry> list_item_menu_options(std::vector<ListItemMenuEntry> value){return value;}
[[nodiscard]] inline std::vector<ListItemOption> list_item_menu_options(const std::vector<std::string>& value){
    std::vector<ListItemOption> result;result.reserve(value.size());for(const auto& text:value)result.push_back({text,true});return result;
}
[[nodiscard]] inline std::string list_item_default_primary(ListItemVariant variant){
    return variant==ListItemVariant::task?"Done":variant==ListItemVariant::media?"Play":variant==ListItemVariant::form?"Save":
        variant==ListItemVariant::resource||variant==ListItemVariant::detail_actions?"Open":
        variant==ListItemVariant::detail_quantity||variant==ListItemVariant::detail_settings?"Apply":"Action";
}
[[nodiscard]] inline std::string list_item_default_secondary(ListItemVariant variant){
    return variant==ListItemVariant::task?"Assign":variant==ListItemVariant::resource?"Reveal":variant==ListItemVariant::form?"Cancel":
        variant==ListItemVariant::media?"Stop":variant==ListItemVariant::detail_actions?"Edit":
        variant==ListItemVariant::detail_settings?"Reset":"Secondary";
}
[[nodiscard]] inline std::string list_item_default_description(ListItemVariant variant){
    return variant==ListItemVariant::task||variant==ListItemVariant::resource||variant==ListItemVariant::media||variant==ListItemVariant::form||variant==ListItemVariant::detail_actions||
        variant==ListItemVariant::detail_quantity||variant==ListItemVariant::detail_settings?"Supporting text":"";
}

template<class Image>
struct ListItemCompositeInputValue final {
    ListItemInputValue<Image> row;
    std::optional<WindowAccentPalette> accent;
    std::optional<bool> checked;
    std::optional<double> quantity;
    ListItemSelectorConfiguration selector;
    std::optional<int> selector_index;
    ListItemSelectorConfiguration unit_selector{.placeholder="Units"};
    std::optional<int> unit_index,segment_index;
    std::vector<ListItemSegment> segments{{"Original",true,{}},{"Edited",true,{}}};
    std::optional<std::string> input_text,input_text2;
    std::string input_placeholder,input_placeholder2,field_label1="Field 1 label",field_label2="Field 2 label";
    std::string date_text="YYYY-MM-dd",metadata1="Metadata",metadata2="Status",status_text="Draft",bookmark_icon_name="bookmarksbookmark";
    std::string quantity_label="Quantity label",option_label="Apply to all",mode_label="Mode label",scope_label="Scope label";
    std::string duration_text="03:42",media_detail="WAV · 48 kHz",preview_icon_name="nodesfolder";
    std::shared_ptr<const Image> preview;
    // An unspecified size uses the default image size, or a custom view's natural size.
    std::optional<double> preview_size;
    bool show_preview=true;
    bool show_leading_icon=true,show_bookmark=true,show_metadata=true;
    double detail_horizontal_padding=12,detail_vertical_padding=12,detail_icon_size=18,detail_section_spacing=12;
    double minimum_quantity=0,maximum_quantity=std::numeric_limits<double>::infinity(),step_size=1,quantity_width=48,quantity_spacing=4;
    bool toggle_enabled=true,selection_enabled=true,show_toggle=true,show_selection=true;
    bool stepper_enabled=true,show_stepper=true;
    StepperArrow stepper_arrow=StepperArrow::up_down;
    ButtonTone stepper_tone=ButtonTone::primary;
    bool selector_enabled=true,show_selector=true;
    bool unit_selector_enabled=true,show_unit_selector=true,segments_enabled=true,show_segments=true;
    bool input_enabled=true,input_read_only=false,show_input=true,input2_enabled=true,input2_read_only=false,show_input2=true;
    bool show_primary_action=true,show_secondary_action=true,show_more_menu=true;
    ListFooterSlot primary_action{.text="Action"},secondary_action{.text="Secondary"},more_menu{.text="More"};
    std::vector<ListItemOption> more_items;
    std::optional<std::vector<ListItemMenuEntry>> menu_entries;
    double slot_padding=4,slot_spacing=8,action_width=72,selector_width=97,unit_selector_width=97,segment_width=56,input_width=140,input_height=30;
    // Absent dimensions follow the custom view's natural size.
    std::optional<double> leading_size,trailing_width,trailing_height;
    friend bool operator==(const ListItemCompositeInputValue&,const ListItemCompositeInputValue&)=default;
};
template<class Image> inline void assign_list_item_menu_options(ListItemCompositeInputValue<Image>& input,std::vector<ListItemOption> value){
    input.more_items=std::move(value);input.menu_entries.reset();
}
template<class Image> inline void assign_list_item_menu_options(ListItemCompositeInputValue<Image>& input,std::vector<ListItemMenuEntry> value){
    input.more_items.clear();input.menu_entries=std::move(value);
}
struct NativeListItemCompositeCallbacks final {
    std::function<void()> activated;
    std::function<void(bool)> checked_changed;
    std::function<void(std::string,const ListItemValue&)> edited;
    std::function<void(std::string,const ListFooterSlot&)> action_triggered;
    std::function<void(double)> quantity_changed;
    std::function<void(int)> selector_index_changed;
    std::function<void(std::string)> input_text_changed,input_submitted;
    std::function<void(int,const ListItemOption&)> menu_item_triggered;
    std::function<void(int)> unit_index_changed,segment_index_changed;
    std::function<void(std::string)> input_text2_changed,input_submitted2;
    std::function<void(std::string,const ListItemActionValues&)> action_values_triggered;
    std::function<void(std::string,const ListItemActionPayload&)> action_event;
    std::function<void(const ListItemMenuSelection&)> menu_selection_triggered;
};
template<class Image>struct ListItemContextPort;

// Commands own no widget generation: callbacks retain only the owner's weak state.
class ListItemContextCommands final {
public:
    [[nodiscard]] ii::core::Result<bool> edit_value(std::string field,ListItemValue value)const{return edit_(std::move(field),std::move(value));}
    [[nodiscard]] ii::core::Result<bool> step_quantity(int direction)const{return step_(direction);}
    [[nodiscard]] ii::core::Result<bool> trigger_action(std::string action,std::shared_ptr<const std::any> payload={})const{return trigger_(std::move(action),std::move(payload));}
private:
    template<class>friend struct ListItemContextPort;
    ListItemContextCommands(std::function<ii::core::Result<bool>(std::string,ListItemValue)> edit,
            std::function<ii::core::Result<bool>(int)> step,std::function<ii::core::Result<bool>(std::string,std::shared_ptr<const std::any>)> trigger)
        :edit_(std::move(edit)),step_(std::move(step)),trigger_(std::move(trigger)){}
    std::function<ii::core::Result<bool>(std::string,ListItemValue)> edit_;
    std::function<ii::core::Result<bool>(int)> step_;
    std::function<ii::core::Result<bool>(std::string,std::shared_ptr<const std::any>)> trigger_;
};
template<class Image>
struct ListItemContextSnapshotValue final {
    std::string label,description,inputText1,inputText2;
    ListItemVariant type=ListItemVariant::mini;
    bool enabled=true,visible=true,checked=false;
    double quantity=1;
    int selectorIndex=0,unitIndex=0,segmentIndex=0;
    ListItemCompositeInputValue<Image> options;
    ListItemActionValues values;
    std::shared_ptr<const ListItemContextCommands> commands;
    [[nodiscard]] std::function<void()> editValue(std::string field,ListItemValue value)const{
        std::weak_ptr<const ListItemContextCommands> weak=commands;
        return [weak,field=std::move(field),value=std::move(value)]{auto owner=weak.lock();
            if(!owner)throw std::logic_error("List item context is closed");require(owner->edit_value(field,value));};
    }
    [[nodiscard]] std::function<void()> stepQuantity(int direction)const{
        std::weak_ptr<const ListItemContextCommands> weak=commands;
        return [weak,direction]{auto owner=weak.lock();if(!owner)throw std::logic_error("List item context is closed");require(owner->step_quantity(direction));};
    }
    [[nodiscard]] std::function<void()> triggerAction(std::string action)const{
        return trigger_action(std::move(action),{});
    }
    template<class T> requires std::copy_constructible<std::decay_t<T>>
    [[nodiscard]] std::function<void()> triggerAction(std::string action,T&& value)const{
        using Value=std::decay_t<T>;
        if constexpr(std::is_convertible_v<T,std::shared_ptr<const std::any>>)
            return trigger_action(std::move(action),std::shared_ptr<const std::any>(std::forward<T>(value)));
        else if constexpr(std::is_convertible_v<T,std::string_view>){
            if constexpr(std::is_pointer_v<Value>)if(!value)throw std::invalid_argument("List item action payload string is null");
            return trigger_action(std::move(action),std::make_shared<const std::any>(std::string(std::string_view(value))));
        }
        else return trigger_action(std::move(action),std::make_shared<const std::any>(std::forward<T>(value)));
    }
    friend bool operator==(const ListItemContextSnapshotValue&,const ListItemContextSnapshotValue&)=default;
private:
    [[nodiscard]] std::function<void()> trigger_action(std::string action,std::shared_ptr<const std::any> payload)const{
        std::weak_ptr<const ListItemContextCommands> weak=commands;
        return [weak,action=std::move(action),payload=std::move(payload)]{auto owner=weak.lock();if(!owner)throw std::logic_error("List item context is closed");require(owner->trigger_action(action,payload));};
    }
    static void require(ii::core::Result<bool> result){if(!result){if(result.error().cause)std::rethrow_exception(result.error().cause);throw std::runtime_error(result.error().message);}}
};
template<class Image>
class ListItemContextValue final {
    using ListItemContextSnapshot=ListItemContextSnapshotValue<Image>;
public:
    [[nodiscard]] const ListItemContextSnapshot& get()const{check();return value_->get();}
    [[nodiscard]] std::uint64_t revision()const{check();return value_->revision();}
    [[nodiscard]] ii::core::ObservableSubscription observe(std::function<void(const ListItemContextSnapshot&,std::uint64_t)> callback,bool initial=false)const{
        check();return value_->observe(std::move(callback),initial);
    }
    [[nodiscard]] ii::core::ObservableRead read_dependency()const{check();return value_->read_dependency();}
    [[nodiscard]] ii::core::Result<bool> edit_value(std::string field,ListItemValue value)const{
        if(auto valid=usable();!valid)return std::unexpected(valid.error());return commands_->edit_value(std::move(field),std::move(value));
    }
    [[nodiscard]] ii::core::Result<bool> step_quantity(int direction)const{
        if(auto valid=usable();!valid)return std::unexpected(valid.error());return commands_->step_quantity(direction);
    }
    [[nodiscard]] ii::core::Result<bool> trigger_action(std::string action,std::shared_ptr<const std::any> payload={})const{
        if(auto valid=usable();!valid)return std::unexpected(valid.error());return commands_->trigger_action(std::move(action),std::move(payload));
    }
private:
    template<class>friend struct ListItemContextPort;
    ListItemContextValue(ListItemContextSnapshot initial,std::shared_ptr<const ListItemContextCommands> commands)
        :value_(std::in_place,std::move(initial)),commands_(std::move(commands)){}
    void check()const{if(owner_!=std::this_thread::get_id())throw std::logic_error("List item context requires its owner thread");
        if(!value_)throw std::logic_error("List item context is closed");}
    [[nodiscard]] ii::core::Result<void> usable()const{
        if(owner_!=std::this_thread::get_id())return ii::core::fail(ii::core::ErrorCode::wrong_executor,"List item context requires its owner thread");
        if(!value_||!commands_)return ii::core::fail(ii::core::ErrorCode::closed,"List item context is closed");return {};
    }
    void publish(ListItemContextSnapshot next){check();value_->set(std::move(next));}
    void close()noexcept{value_.reset();commands_.reset();}
    const std::thread::id owner_=std::this_thread::get_id();
    std::optional<ii::core::Observable<ListItemContextSnapshot>> value_;
    std::shared_ptr<const ListItemContextCommands> commands_;
};
template<class Image>
struct ListItemContextPort final {
    using Context=ListItemContextValue<Image>;using Snapshot=ListItemContextSnapshotValue<Image>;
    template<class E,class S,class T>[[nodiscard]] static std::shared_ptr<const ListItemContextCommands> make_commands(E edit,S step,T trigger){
        return std::shared_ptr<const ListItemContextCommands>(new ListItemContextCommands(std::move(edit),std::move(step),std::move(trigger)));
    }
    [[nodiscard]] static std::shared_ptr<Context> make(Snapshot value,std::shared_ptr<const ListItemContextCommands> commands){
        return std::shared_ptr<Context>(new Context(std::move(value),std::move(commands)));
    }
    static void publish(Context& context,Snapshot value){context.publish(std::move(value));}
    static void close(Context& context)noexcept{context.close();}
};
template<class Builder,class Image>using ListItemContextViewFactoryValue=std::function<ii::core::Result<void>(Builder&,std::shared_ptr<const ListItemContextValue<Image>>)>;
template<class Builder,class Factory,class Image>struct NativeListItemCompositeViewsValue final {
    Factory footer,preview,leading,trailing;
    ListItemContextViewFactoryValue<Builder,Image> footer_context,preview_context,leading_context,trailing_context;
};

template<class Image,class Builder,class Factory,class Components>
struct ListItemCompositeHostPolicy final {
    using Input=ListItemCompositeInputValue<Image>;using ListItemCompositeInput=Input;
    using ListItemInput=ListItemInputValue<Image>;using NativeListItemCompositeCallbacks=::lvui::native::NativeListItemCompositeCallbacks;
    using NativeListItemCompositeViews=NativeListItemCompositeViewsValue<Builder,Factory,Image>;
    using ListItemContext=ListItemContextValue<Image>;using ListItemContextSnapshot=ListItemContextSnapshotValue<Image>;
    using ListItemContextCommands=::lvui::native::ListItemContextCommands;using ContextPort=ListItemContextPort<Image>;
    using NativeInputField=typename Components::NativeInputField;
    using NativeComboBox=typename Components::NativeComboBox;
    using NativeMenuItem=typename Components::NativeMenuItem;
    using NativeMenu=typename Components::NativeMenu;
    using ListItemSelector=typename Components::ListItemSelector;
    using NativeListItemSelector=typename Components::NativeListItemSelector;
    using ListItemSegments=typename Components::ListItemSegments;
    using NativeListItemSegments=typename Components::NativeListItemSegments;
    using NativeToolbarButton=typename Components::NativeToolbarButton;
    using NativeStepper=typename Components::NativeStepper;
    using NativeListFooter=typename Components::NativeListFooter;
    using ListItemModel=typename Components::ListItemModel;
    using ListItemDrawing=typename Components::ListItemDrawing;
    using NativeListItem=typename Components::NativeListItem;
    using CompositeCheckBox=typename Components::CompositeCheckBox;
    using CompositeToggleSwitch=typename Components::CompositeToggleSwitch;
    using MenuItemPort=typename Components::MenuItemPort;
    using MenuSectionPort=typename Components::MenuSectionPort;
    using IconCache=typename Components::IconCache;
    using ToolbarButtonInput=typename Components::ToolbarButtonInput;
    using ListItemVariant=::lvui::ListItemVariant;
    using ListItemDetailActions=typename Components::ListItemDetailActions;
    using ListItemDetailActionsConfiguration=::lvui::ListItemDetailActionsConfiguration;
    using ListItemDetailControls=typename Components::ListItemDetailControls;
    using ListItemDetailControlsConfiguration=::lvui::ListItemDetailControlsConfiguration;
    using ListItemDetailControlVariant=::lvui::ListItemDetailControlVariant;
    using ListItemTask=typename Components::ListItemTask;
    using ListItemTaskConfiguration=::lvui::ListItemTaskConfiguration;
    using ListItemPreviewRow=typename Components::ListItemPreviewRow;
    using ListItemPreviewRowConfiguration=::lvui::ListItemPreviewRowConfiguration;
    using ListItemPreviewVariant=::lvui::ListItemPreviewVariant;
    using ListItemForm=typename Components::ListItemForm;
    using ListItemFormConfiguration=::lvui::ListItemFormConfiguration;
    using ListItemQuantity=typename Components::ListItemQuantity;
    using ListItemQuantityConfiguration=::lvui::ListItemQuantityConfiguration;
    using ListItemSize=::lvui::ListItemSize;
    using ListItemRect=::lvui::ListItemRect;
    using ListItemActionValues=::lvui::ListItemActionValues;
    using ListItemActionPayload=::lvui::ListItemActionPayload;
    using ListItemOption=::lvui::ListItemOption;
    using ListItemMenuEntry=::lvui::ListItemMenuEntry;
    using ListItemMenuSelection=::lvui::ListItemMenuSelection;
    using ListItemMenu=typename Components::ListItemMenu;
    using ListItemValue=::lvui::ListItemValue;
    using ButtonTone=::lvui::ButtonTone;
    using StepperArrow=::lvui::StepperArrow;
    using StepperConfiguration=::lvui::StepperConfiguration;
    using InputFieldConfiguration=::lvui::InputFieldConfiguration;
    using ToggleSwitchConfiguration=::lvui::ToggleSwitchConfiguration;using CheckBoxConfiguration=::lvui::CheckBoxConfiguration;
    using WindowAccentPalette=::lvui::WindowAccentPalette;using ListFooterSlot=::lvui::ListFooterSlot;
    using ListItemInputSubmission=::lvui::ListItemInputSubmission;using ListItemContextViewFactory=ListItemContextViewFactoryValue<Builder,Image>;
    using ListItemSelectorInput=::lvui::native::ListItemSelectorInput;
    using ListItemSegmentsInput=::lvui::native::ListItemSegmentsInput;
    using ListFooterInput=::lvui::native::ListFooterInput;
    using MenuInput=::lvui::native::MenuInput;
    using MenuItemInput=MenuItemInputValue<Image>;
    using MenuItemPresentation=::lvui::native::MenuItemPresentation;
    using MenuEntryState=::lvui::MenuEntryState;
    using MenuItemPalette=::lvui::MenuItemPalette;
    using IconSource=::lvui::IconSource;
    [[nodiscard]] static auto panel_surface_alt()noexcept{return ::lvui::figma_defaults::panel_surface_alt;}
    [[nodiscard]] static auto panel_surface_panel()noexcept{return ::lvui::figma_defaults::panel_surface_panel;}
    [[nodiscard]] static bool valid_accessibility_text(std::string_view value)noexcept{return ::lvui::valid_accessibility_text(value);}
    [[nodiscard]] static bool valid_window_accent_palette(const WindowAccentPalette& value)noexcept{return ::lvui::valid_window_accent_palette(value);}
    [[nodiscard]] static std::string native_display_text(std::string_view value){return ::lvui::native::native_display_text(value);}
};
template<template<class>class Host,class Image,class Builder,class Factory,class Components>
using ListItemCompositeBinding=Host<ListItemCompositeHostPolicy<Image,Builder,Factory,Components>>;
} // namespace lvui::native
