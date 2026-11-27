"""Python uses the same native LVUI editor-view compiler as C++ consumers."""
from pathlib import Path
import os
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src" / "Python"))
import ii_lvui

assert ii_lvui._library is None
try:
    ii_lvui.compile_native('Window { Label() }', editor_panels=True)
except TypeError:
    pass
else:
    raise AssertionError('editor panel compilation accepted an implicit native library')
library = ii_lvui.load_editor_library(Path(os.environ["LVUI_COMPILER_LIBRARY"]).resolve())
source = '''import vm AppModel
Window {
    ColorProfilePicker(title: vm.title, fields: [EditorTextControl(label: vm.label,
        value: vm.value, enabled: vm.enabled, onEdited: vm.edited)], preview: [],
        applyLabel: vm.applyLabel, cancelLabel: vm.cancelLabel, applyEnabled: vm.enabled,
        onApply: vm.apply, onCancel: vm.cancel)
}'''
result = ii_lvui.compile_native(source, editor_panels=True, library=library)
assert "NativeInputField" in result
for view in (
    'EditorTextControl(label: vm.label, value: vm.value, enabled: vm.enabled, onEdited: vm.edited)',
    'EditorToggleControl(label: vm.label, checked: vm.checked, enabled: vm.enabled, onToggled: vm.toggled)',
    'EditorRangeControl(label: vm.label, value: vm.value, minimum: vm.minimum, maximum: vm.maximum, '
    'enabled: vm.enabled, onMoved: vm.moved)',
):
    control = ii_lvui.compile_native('import vm Backend\nWindow { ' + view + ' }', editor_panels=True, library=library)
    assert "accessibility_name" in control, "the app-provided field label did not name the input for accessibility"
try:
    ii_lvui.compile_native("Window {\n  MissingEditorView()\n}", editor_panels=True, library=library)
except ii_lvui.CompileError as error:
    assert error.line == 2
else:
    raise AssertionError("invalid user source was accepted")
assert ii_lvui._library is None

# Python callers pass an explicit native binding; each transition uses the
# same C++ state machine as a native application and owns no global UI state.
action = {"id": "open", "label": "Open", "enabled": False, "visible": True, "checked": True}
bound = ii_lvui.MenuActionStateBinding(library).execute(action)
assert bound["id"] == "open" and not bound["enabled"] and bound["checked"]
submenu = ii_lvui.MenuSubmenuItemsBinding(library).execute({"parent_id": "recent", "items": [action]})
assert len(submenu) == 1 and submenu[0]["id"] == "open"
nested = dict(action, id="nested", kind=ii_lvui.MenuEntryKind.SUBMENU)
divider = dict(action, id="divider", label="", kind=ii_lvui.MenuEntryKind.DIVIDER)
items = ii_lvui.MenuSubmenuItemsBinding(library).execute({"parent_id": "recent", "items": [nested, divider]})
assert items[0]["kind"] == ii_lvui.MenuEntryKind.SUBMENU and items[1]["kind"] == ii_lvui.MenuEntryKind.DIVIDER
try:
    ii_lvui.MenuSubmenuItemsBinding(library).execute({"parent_id": "recent", "items": [action, action]})
except ii_lvui.EditorError:
    pass
else:
    raise AssertionError("duplicate submenu item accepted")

state = {"session": "python-activation", "source_revision": 4, "preview_enabled": True,
         "fields": [{"id": "level", "label": "Level", "kind": ii_lvui.EditorFieldKind.REAL,
                     "value": 0.5, "minimum": 0.0, "maximum": 1.0}]}
automatic = ii_lvui.EditorAutoFormBinding(library).execute(state["fields"])
assert automatic[0]["id"] == "level" and automatic[0]["bounded"] and automatic[0]["amount"] == 0.5
pending = ii_lvui.EditorAutoFormBinding(library).execute([{
    "id": "pending", "label": "Pending", "kind": ii_lvui.EditorFieldKind.CHOICE, "value": "", "required": True,
    "choices": [{"id": "first", "label": "First"}],
}])
assert pending[0]["selected"] == 0 and pending[0]["options"] == ["", "First"]
auto_source = '''import vm AppModel
Window { EditorAutoFields(model: vm.autoModel) }'''
assert "NativeListItemSelector" in ii_lvui.compile_native(auto_source, editor_panels=True, library=library)
for panel_type, kind in (
    (ii_lvui.MenuCommandParameterForm, ii_lvui.EditorPanelKind.MENU_PARAMETERS),
    (ii_lvui.NonDestructivePreviewPanel, ii_lvui.EditorPanelKind.PREVIEW),
    (ii_lvui.MaskRefinementControls, ii_lvui.EditorPanelKind.MASK_REFINEMENT),
    (ii_lvui.LayerContentOptionsPanel, ii_lvui.EditorPanelKind.LAYER_CONTENT),
    (ii_lvui.FilterGalleryPanel, ii_lvui.EditorPanelKind.FILTER_GALLERY),
    (ii_lvui.ColorProfilePicker, ii_lvui.EditorPanelKind.COLOR_PROFILE),
    (ii_lvui.SoftProofSettingsPanel, ii_lvui.EditorPanelKind.SOFT_PROOF),
    (ii_lvui.TimelineEditOptionsPanel, ii_lvui.EditorPanelKind.TIMELINE),
    (ii_lvui.ExportFormatOptionsPanel, ii_lvui.EditorPanelKind.EXPORT_FORMAT),
):
    assert panel_type(library).execute({"state": state, "event": {"action": ii_lvui.EditorPanelAction.BEGIN}})["kind"] == kind
panel = ii_lvui.NonDestructivePreviewPanel(library)
opened = panel.execute({"state": state, "event": {"action": ii_lvui.EditorPanelAction.BEGIN}})
edited = panel.execute({"state": opened["state"], "event": {"action": ii_lvui.EditorPanelAction.EDIT,
                        "field_id": "level", "value": 0.75}})
assert edited["state"]["original_fields"][0]["value"] == 0.5
assert edited["effects"][0]["kind"] == ii_lvui.EditorEffectKind.PREVIEW
integral_real = panel.execute({"state": opened["state"], "event": {"action": ii_lvui.EditorPanelAction.EDIT,
                               "field_id": "level", "value": 1.0}})
assert isinstance(integral_real["state"]["fields"][0]["value"], float)
panel.execute({"state": integral_real["state"], "event": {"action": ii_lvui.EditorPanelAction.CANCEL}})
token = edited["effects"][0]["token"]
completed = panel.execute({"state": edited["state"], "event": {"action": ii_lvui.EditorPanelAction.PREVIEW_READY,
                           "token": token, "preview_id": "preview"}})
next_edit = panel.execute({"state": completed["state"], "event": {"action": ii_lvui.EditorPanelAction.EDIT,
                           "field_id": "level", "value": 0.8}})
preview_failure = panel.execute({"state": next_edit["state"], "event": {"action": ii_lvui.EditorPanelAction.PREVIEW_FAILED,
                                "token": next_edit["effects"][0]["token"], "error": "preview failed"}})
assert preview_failure["state"]["preview_id"] == "" and preview_failure["effects"][0]["preview_id"] == "preview"
pending_apply = panel.execute({"state": next_edit["state"], "event": {"action": ii_lvui.EditorPanelAction.APPLY}})
assert [effect["kind"] for effect in pending_apply["effects"]] == [
    ii_lvui.EditorEffectKind.DISCARD_PREVIEW, ii_lvui.EditorEffectKind.COMMIT]
failed_apply = panel.execute({"state": pending_apply["state"], "event": {"action": ii_lvui.EditorPanelAction.APPLY_FAILED,
                             "token": pending_apply["effects"][1]["token"], "error": "commit failed"}})
assert failed_apply["state"]["preview_pending"] and failed_apply["effects"][0]["kind"] == ii_lvui.EditorEffectKind.PREVIEW
canceled = panel.execute({"state": completed["state"], "event": {"action": ii_lvui.EditorPanelAction.CANCEL}})
assert canceled["state"]["fields"][0]["value"] == 0.5
assert canceled["effects"][0]["kind"] == ii_lvui.EditorEffectKind.DISCARD_PREVIEW
large_state = {"session": "exact-int", "source_revision": 1,
               "fields": [{"id": "frame", "label": "Frame", "kind": ii_lvui.EditorFieldKind.INTEGER,
                           "value": 9007199254740993, "integer_maximum": 9007199254740993}]}
large = panel.execute({"state": large_state, "event": {"action": ii_lvui.EditorPanelAction.BEGIN}})
assert large["state"]["fields"][0]["integer_maximum"] == 9007199254740993
try:
    panel.execute({"state": opened["state"], "event": {"action": ii_lvui.EditorPanelAction.EDIT,
                   "field_id": "level", "value": 2.0}})
except ii_lvui.EditorError:
    pass
else:
    raise AssertionError("out-of-range value accepted")
