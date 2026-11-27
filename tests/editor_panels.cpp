#include <LVUI>

#include <cstdlib>
#include <iostream>
#include <limits>

using namespace lvui;

namespace {
void require(bool condition, const char* message) {
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}

template<class Panel>
void panel_accepts_application_schema(EditorPanelKind kind, const EditorPanelRequest& request) {
    auto result = Panel{}.execute(request);
    require(result && result->kind == kind && result->state.original_fields == request.state.fields &&
            result->state.fields == request.state.fields && result->effects.empty(),
            "panel did not initialize the app-supplied schema independently");
}
}

int main() {
    MenuActionState action{"open", "Open", "Ctrl+O", {}, "Open document", false, true, true};
    auto projected = MenuActionStateBinding{}.execute(action);
    require(projected && projected->key == "open" && projected->label == "Open" && !projected->enabled &&
            projected->visible && projected->state == MenuEntryState::selected, "menu action state was lost");
    MenuSubmenuItemsRequest submenu{"recent", {action, {"other", "Other", {}, {}, {}, true, false, false}}};
    auto items = MenuSubmenuItemsBinding{}.execute(submenu);
    require(items && items->size() == 2 && (*items)[0].key == "open" && (*items)[1].key == "other" &&
            !(*items)[1].visible, "submenu did not preserve app order or state");
    submenu.items.push_back(action);
    require(!MenuSubmenuItemsBinding{}.execute(submenu), "duplicate submenu identity accepted");
    auto nested = action; nested.id = "more"; nested.kind = MenuEntryKind::submenu;
    auto divider = action; divider.id = "separator"; divider.label.clear(); divider.kind = MenuEntryKind::divider;
    auto nested_items = MenuSubmenuItemsBinding{}.execute({"root", {nested, divider}});
    require(nested_items && (*nested_items)[0].kind == MenuEntryKind::submenu && (*nested_items)[1].kind == MenuEntryKind::divider,
            "nested submenu or divider kind was lost");

    EditorField amount{"amount", "Amount", EditorFieldKind::real, 0.5, true, true, true, 0.0, 1.0, {}};
    EditorField mode{"mode", "Mode", EditorFieldKind::choice, std::string{"normal"}, true, true, true, {}, {},
                     {{"normal", "Normal"}, {"other", "Other"}}};
    EditorField flag{"flag", "Option", EditorFieldKind::boolean, true};
    EditorField locked{"locked", "Locked", EditorFieldKind::text, std::string{"keep"}, false};
    std::vector<EditorFieldEdit> inputs;
    EditorField integer{"count", "Count", EditorFieldKind::integer, std::int64_t{9007199254740993LL}};
    auto automatic = EditorAutoFormBinding{}.execute({{amount, mode, flag, locked, integer},
        [&inputs](EditorFieldEdit input) { inputs.push_back(std::move(input)); }});
    require(automatic && automatic->size() == 5, "schema did not produce an automatic field model");
    require((*automatic)[0]->number && (*automatic)[0]->bounded && (*automatic)[0]->minimum == 0 &&
            (*automatic)[0]->maximum == 1 && (*automatic)[1]->choice &&
            (*automatic)[1]->options == std::vector<std::string>({"Normal", "Other"}) &&
            (*automatic)[1]->selected == 0 && (*automatic)[2]->boolean && !(*automatic)[3]->enabled &&
            (*automatic)[4]->text == "9007199254740993", "automatic controls lost schema constraints or precision");
    (*automatic)[4]->textEdited("9007199254740992");
    require(inputs.back().value && std::get<std::int64_t>(*inputs.back().value) == 9007199254740992LL,
            "integer control lost a 64-bit edit");
    (*automatic)[4]->textEdited("not a number");
    require(!inputs.back().value && inputs.back().raw_text == "not a number",
            "invalid numeric input was silently converted");
    (*automatic)[1]->choiceEdited(1);
    require(inputs.back().field_id == "mode" && std::get<std::string>(*inputs.back().value) == "other",
            "choice control returned a label rather than its stable ID");
    auto prior_inputs = inputs.size();
    (*automatic)[1]->choiceEdited(-1);
    require(inputs.size() == prior_inputs, "out-of-range choice created an edit");
    EditorField unselected{"pending", "Pending", EditorFieldKind::choice, std::string{}, true, true, true,
                           {}, {}, {{"first", "First"}}};
    auto pending = EditorAutoFormBinding{}.execute({{unselected},
        [&inputs](EditorFieldEdit input) { inputs.push_back(std::move(input)); }});
    require(pending && (*pending)[0]->selected == 0 && (*pending)[0]->options.size() == 2 &&
            (*pending)[0]->options[0].empty(), "unselected choice has no valid native selector state");
    (*pending)[0]->choiceEdited(1);
    require(std::get<std::string>(*inputs.back().value) == "first", "unselected choice lost its option mapping");
    unselected.required = false;
    unselected.value = std::string{"first"};
    auto optional = EditorAutoFormBinding{}.execute({{unselected},
        [&inputs](EditorFieldEdit input) { inputs.push_back(std::move(input)); }});
    require(optional && (*optional)[0]->selected == 1, "optional choice lost its selected index");
    (*optional)[0]->choiceEdited(0);
    require(inputs.back().value && std::get<std::string>(*inputs.back().value).empty(),
            "optional choice could not be cleared");
    auto wide = amount;
    wide.minimum = -1.0e13; wide.maximum = 1.0e13;
    auto wide_rows = EditorAutoFormBinding{}.execute({{wide}, {}});
    require(wide_rows && !(*wide_rows)[0]->bounded,
            "a range unsupported by the native slider was not represented by numeric input");
    auto too_many = unselected;
    too_many.choices.clear(); too_many.value = std::string{};
    for (int i = 0; i < 401; ++i) too_many.choices.push_back({std::to_string(i), "Option"});
    require(!EditorAutoFormBinding{}.execute({{too_many}, {}}),
            "a choice list exceeding the native selector limit was silently accepted");
    auto duplicate_fields = std::vector<EditorField>{amount, amount};
    require(!EditorAutoFormBinding{}.execute({duplicate_fields, {}}), "automatic field model accepted duplicate IDs");
    EditorPanelState state; state.session = "activation-1"; state.source_revision = 9;
    state.fields = {amount, mode, flag, locked}; state.preview_enabled = true;
    EditorPanelRequest begin{state, {EditorPanelAction::begin}};
    auto zero_revision = begin; zero_revision.state.source_revision = 0;
    require(NonDestructivePreviewPanel{}.execute(zero_revision).has_value(),
            "application-owned zero revision was treated as missing");
    auto large_integer = state;
    large_integer.fields = {{"large", "Large", EditorFieldKind::integer, std::int64_t{9007199254740993LL},
                             true, true, true, {}, 9007199254740992.0}};
    require(!NonDestructivePreviewPanel{}.execute({large_integer, {EditorPanelAction::begin}}),
            "integer precision loss accepted an out-of-range value");
    large_integer.fields[0].maximum.reset();
    large_integer.fields[0].integer_maximum = 9007199254740993LL;
    require(NonDestructivePreviewPanel{}.execute({large_integer, {EditorPanelAction::begin}}).has_value(),
            "an exact 64-bit integer boundary was not preserved");
    panel_accepts_application_schema<MenuCommandParameterForm>(EditorPanelKind::menu_parameters, begin);
    panel_accepts_application_schema<NonDestructivePreviewPanel>(EditorPanelKind::preview, begin);
    panel_accepts_application_schema<MaskRefinementControls>(EditorPanelKind::mask_refinement, begin);
    panel_accepts_application_schema<LayerContentOptionsPanel>(EditorPanelKind::layer_content, begin);
    panel_accepts_application_schema<FilterGalleryPanel>(EditorPanelKind::filter_gallery, begin);
    panel_accepts_application_schema<ColorProfilePicker>(EditorPanelKind::color_profile, begin);
    panel_accepts_application_schema<SoftProofSettingsPanel>(EditorPanelKind::soft_proof, begin);
    panel_accepts_application_schema<TimelineEditOptionsPanel>(EditorPanelKind::timeline, begin);
    panel_accepts_application_schema<ExportFormatOptionsPanel>(EditorPanelKind::export_format, begin);

    auto opened = NonDestructivePreviewPanel{}.execute(begin);
    require(opened.has_value(), "valid editor session could not begin");
    auto preview_required = begin; preview_required.state.preview_enabled = false;
    auto forced_preview = NonDestructivePreviewPanel{}.execute(preview_required);
    require(forced_preview && forced_preview->state.preview_enabled,
            "non-destructive preview panel allowed preview to be disabled");
    EditorPanelRequest edit{opened->state, {EditorPanelAction::edit, "amount", EditorValue{0.75}}};
    auto changed = NonDestructivePreviewPanel{}.execute(edit);
    require(changed && changed->state.edit_revision == 1 && changed->effects.size() == 1 &&
            changed->effects[0].kind == EditorEffectKind::preview && changed->effects[0].token.edit_revision == 1 &&
            std::get<double>(changed->state.fields[0].value) == 0.75 &&
            std::get<double>(changed->state.original_fields[0].value) == 0.5,
            "edit failed to emit an isolated preview request");
    auto invalid = edit; invalid.event.value = EditorValue{2.0};
    require(!NonDestructivePreviewPanel{}.execute(invalid) && std::get<double>(edit.state.fields[0].value) == 0.5,
            "invalid range mutated the original editor state");
    invalid = edit; invalid.event.field_id = "mode"; invalid.event.value = EditorValue{std::string{"missing"}};
    require(!NonDestructivePreviewPanel{}.execute(invalid), "unknown app-provided choice accepted");
    invalid = edit; invalid.event.field_id = "locked"; invalid.event.value = EditorValue{std::string{"changed"}};
    require(!NonDestructivePreviewPanel{}.execute(invalid), "disabled field accepted an edit");
    invalid = edit; invalid.event.value = EditorValue{std::numeric_limits<double>::quiet_NaN()};
    require(!NonDestructivePreviewPanel{}.execute(invalid), "non-finite numeric value accepted");
    auto stale = changed->effects[0].token; stale.edit_revision = 0;
    EditorPanelRequest result{changed->state, {EditorPanelAction::preview_ready, {}, {}, stale, "preview-old"}};
    require(!NonDestructivePreviewPanel{}.execute(result), "stale preview replaced a newer edit");
    result.event.token = changed->effects[0].token; result.event.preview_id = "preview-1";
    auto preview = NonDestructivePreviewPanel{}.execute(result);
    require(preview && preview->state.preview_id == "preview-1" && preview->effects.empty(), "current preview was not accepted");
    auto next_edit = NonDestructivePreviewPanel{}.execute({preview->state,
        {EditorPanelAction::edit, "amount", EditorValue{0.9}}});
    require(next_edit && next_edit->effects.size() == 1, "second preview request was not emitted");
    auto failed_preview = NonDestructivePreviewPanel{}.execute({next_edit->state,
        {EditorPanelAction::preview_failed, {}, {}, next_edit->effects[0].token, {}, "preview failed"}});
    require(failed_preview && failed_preview->state.preview_id.empty() && failed_preview->effects.size() == 1 &&
            failed_preview->effects[0].kind == EditorEffectKind::discard_preview &&
            failed_preview->effects[0].preview_id == "preview-1", "failed preview kept a stale result visible");
    auto replaced_preview = NonDestructivePreviewPanel{}.execute({next_edit->state,
        {EditorPanelAction::preview_ready, {}, {}, next_edit->effects[0].token, "preview-2"}});
    require(replaced_preview && replaced_preview->state.preview_id == "preview-2" && replaced_preview->effects.size() == 1 &&
            replaced_preview->effects[0].kind == EditorEffectKind::discard_preview &&
            replaced_preview->effects[0].preview_id == "preview-1", "replaced preview was not retired");
    EditorPanelRequest cancel{preview->state, {EditorPanelAction::cancel}};
    auto canceled = NonDestructivePreviewPanel{}.execute(cancel);
    require(canceled && canceled->state.phase == EditorPanelPhase::canceled &&
            canceled->state.fields == canceled->state.original_fields && canceled->effects.size() == 1 &&
            canceled->effects[0].kind == EditorEffectKind::discard_preview,
            "cancel did not restore the draft and request preview disposal");
    EditorPanelRequest apply{preview->state, {EditorPanelAction::apply}};
    auto applying = NonDestructivePreviewPanel{}.execute(apply);
    require(applying && applying->state.phase == EditorPanelPhase::applying && applying->effects.size() == 1 &&
            applying->effects[0].kind == EditorEffectKind::commit, "apply did not request an explicit commit");
    require(!NonDestructivePreviewPanel{}.execute({applying->state, {EditorPanelAction::cancel}}),
            "an in-flight commit was silently canceled");
    auto wrong = applying->effects[0].token; wrong.session = "retired";
    require(!NonDestructivePreviewPanel{}.execute({applying->state, {EditorPanelAction::apply_succeeded, {}, {}, wrong}}),
            "another activation completed the current commit");
    auto complete = NonDestructivePreviewPanel{}.execute({applying->state,
        {EditorPanelAction::apply_succeeded, {}, {}, applying->effects[0].token}});
    require(complete && complete->state.phase == EditorPanelPhase::applied && complete->effects.size() == 1 &&
            complete->effects[0].kind == EditorEffectKind::discard_preview, "committed preview was not retired");
    auto failed = NonDestructivePreviewPanel{}.execute({applying->state,
        {EditorPanelAction::apply_failed, {}, {}, applying->effects[0].token, {}, "failure"}});
    require(failed && failed->state.phase == EditorPanelPhase::editing && failed->state.error == "failure" &&
            failed->state.fields == preview->state.fields, "failed application lost the draft or appeared complete");

    // Applying a newer draft while its preview is pending must not leave the
    // previous draft's image visible if the commit later fails.
    auto pending_apply = NonDestructivePreviewPanel{}.execute({next_edit->state, {EditorPanelAction::apply}});
    require(pending_apply && pending_apply->state.preview_id.empty() && pending_apply->effects.size() == 2 &&
            pending_apply->effects[0].kind == EditorEffectKind::discard_preview &&
            pending_apply->effects[0].preview_id == "preview-1" &&
            pending_apply->effects[1].kind == EditorEffectKind::commit,
            "applying a pending preview retained the previous draft's image");
    auto pending_failure = NonDestructivePreviewPanel{}.execute({pending_apply->state,
        {EditorPanelAction::apply_failed, {}, {}, pending_apply->effects[1].token, {}, "failure"}});
    require(pending_failure && pending_failure->state.phase == EditorPanelPhase::editing &&
            pending_failure->state.preview_pending && pending_failure->effects.size() == 1 &&
            pending_failure->effects[0].kind == EditorEffectKind::preview &&
            pending_failure->effects[0].fields == next_edit->state.fields,
            "failed application did not request the current draft's preview again");
    return 0;
}
