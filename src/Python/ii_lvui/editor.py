"""Explicit native bindings for app-owned menu and editor panel state.

Requests and results are dictionaries. The native C++ engine owns validation
and transition semantics; applications own their model, work and effects.
"""
from __future__ import annotations

import ctypes
from enum import IntEnum
import json
from pathlib import Path


class EditorFieldKind(IntEnum):
    TEXT = 0
    INTEGER = 1
    REAL = 2
    BOOLEAN = 3
    CHOICE = 4


class MenuEntryKind(IntEnum):
    ACTION = 0
    SUBMENU = 1
    DIVIDER = 2


class EditorPanelKind(IntEnum):
    MENU_PARAMETERS = 0
    PREVIEW = 1
    MASK_REFINEMENT = 2
    LAYER_CONTENT = 3
    FILTER_GALLERY = 4
    COLOR_PROFILE = 5
    SOFT_PROOF = 6
    TIMELINE = 7
    EXPORT_FORMAT = 8


class EditorPanelPhase(IntEnum):
    FRESH = 0
    EDITING = 1
    APPLYING = 2
    APPLIED = 3
    CANCELED = 4


class EditorPanelAction(IntEnum):
    BEGIN = 0
    EDIT = 1
    PREVIEW_READY = 2
    PREVIEW_FAILED = 3
    APPLY = 4
    APPLY_SUCCEEDED = 5
    APPLY_FAILED = 6
    CANCEL = 7


class EditorEffectKind(IntEnum):
    PREVIEW = 0
    COMMIT = 1
    DISCARD_PREVIEW = 2


class EditorError(ValueError):
    """A native validation or transition failure with its stable status code."""
    def __init__(self, message, code):
        super().__init__(message)
        self.code = code


class _MenuAction(ctypes.Structure):
    _fields_ = [(name, ctypes.c_char_p) for name in ('id', 'label', 'shortcut', 'icon_name', 'accessibility_name')] + [
        ('enabled', ctypes.c_int), ('visible', ctypes.c_int), ('checked', ctypes.c_int), ('kind', ctypes.c_int)]


class _Choice(ctypes.Structure):
    _fields_ = [('id', ctypes.c_char_p), ('label', ctypes.c_char_p)]


class _Value(ctypes.Structure):
    _fields_ = [('kind', ctypes.c_int), ('text', ctypes.c_char_p), ('integer', ctypes.c_int64),
                ('real', ctypes.c_double), ('boolean', ctypes.c_int)]


class _Field(ctypes.Structure):
    _fields_ = [('id', ctypes.c_char_p), ('label', ctypes.c_char_p), ('kind', ctypes.c_int), ('value', _Value),
                ('enabled', ctypes.c_int), ('visible', ctypes.c_int), ('required', ctypes.c_int),
                ('has_minimum', ctypes.c_int), ('has_maximum', ctypes.c_int),
                ('minimum', ctypes.c_double), ('maximum', ctypes.c_double),
                ('choices', ctypes.POINTER(_Choice)), ('choice_count', ctypes.c_size_t),
                ('validation_error', ctypes.c_char_p), ('has_integer_minimum', ctypes.c_int),
                ('has_integer_maximum', ctypes.c_int), ('integer_minimum', ctypes.c_int64),
                ('integer_maximum', ctypes.c_int64)]


class _Token(ctypes.Structure):
    _fields_ = [('session', ctypes.c_char_p), ('source_revision', ctypes.c_uint64), ('edit_revision', ctypes.c_uint64)]


class _State(ctypes.Structure):
    _fields_ = [('session', ctypes.c_char_p), ('source_revision', ctypes.c_uint64), ('edit_revision', ctypes.c_uint64),
                ('phase', ctypes.c_int), ('preview_enabled', ctypes.c_int), ('preview_pending', ctypes.c_int),
                ('preview_id', ctypes.c_char_p), ('error', ctypes.c_char_p),
                ('original_fields', ctypes.POINTER(_Field)), ('original_count', ctypes.c_size_t),
                ('fields', ctypes.POINTER(_Field)), ('field_count', ctypes.c_size_t)]


class _Event(ctypes.Structure):
    _fields_ = [('action', ctypes.c_int), ('field_id', ctypes.c_char_p), ('has_value', ctypes.c_int),
                ('value', _Value), ('token', _Token), ('preview_id', ctypes.c_char_p), ('error', ctypes.c_char_p)]


def load_editor_library(path):
    """Load the explicitly named native LVUI bridge and return its binding.

    The caller owns its path and binding lifetime. No search path or global
    library is consulted; C++ and Python use identical validation rules.
    """
    path = Path(path)
    if not path.is_absolute():
        raise ValueError('the native LVUI bridge path must be absolute')
    library = ctypes.CDLL(str(path))
    library.lvui_editor_menu_action_execute.argtypes = [ctypes.POINTER(_MenuAction)]
    library.lvui_editor_menu_action_execute.restype = ctypes.c_void_p
    library.lvui_editor_submenu_execute.argtypes = [ctypes.c_char_p, ctypes.POINTER(_MenuAction), ctypes.c_size_t]
    library.lvui_editor_submenu_execute.restype = ctypes.c_void_p
    library.lvui_editor_panel_execute.argtypes = [ctypes.c_int, ctypes.POINTER(_State), ctypes.POINTER(_Event)]
    library.lvui_editor_panel_execute.restype = ctypes.c_void_p
    library.lvui_editor_auto_form_execute.argtypes = [ctypes.POINTER(_Field), ctypes.c_size_t]
    library.lvui_editor_auto_form_execute.restype = ctypes.c_void_p
    library.lvui_editor_result_status.argtypes = [ctypes.c_void_p]
    library.lvui_editor_result_status.restype = ctypes.c_int
    library.lvui_editor_result_data.argtypes = [ctypes.c_void_p]
    library.lvui_editor_result_data.restype = ctypes.c_void_p
    library.lvui_editor_result_size.argtypes = [ctypes.c_void_p]
    library.lvui_editor_result_size.restype = ctypes.c_size_t
    library.lvui_editor_result_free.argtypes = [ctypes.c_void_p]
    library.lvui_editor_result_free.restype = None
    return library


def _result(library, handle):
    if not handle:
        raise MemoryError('the native editor result could not be allocated')
    try:
        status = library.lvui_editor_result_status(handle)
        pointer = library.lvui_editor_result_data(handle)
        length = library.lvui_editor_result_size(handle)
        if length and not pointer:
            raise RuntimeError('the native editor returned invalid data')
        content = ctypes.string_at(pointer, length).decode('utf-8') if length else ''
        if status:
            raise EditorError(content or 'editor operation failed', status)
        return json.loads(content)
    finally:
        library.lvui_editor_result_free(handle)


def _bool(value, name):
    if not isinstance(value, bool):
        raise TypeError(f'{name} must be a bool')
    return int(value)


def _uint(value, name):
    if isinstance(value, bool) or not isinstance(value, int) or not 0 <= value <= (1 << 64) - 1:
        raise ValueError(f'{name} must be an unsigned 64-bit integer')
    return value


def _int64(value, name):
    if isinstance(value, bool) or not isinstance(value, int) or not -(1 << 63) <= value < (1 << 63):
        raise ValueError(f'{name} must be a signed 64-bit integer')
    return value


class _Inputs:
    """Keep each borrowed C buffer alive until its explicit native call ends."""
    def __init__(self):
        self.buffers = []

    def text(self, value):
        if not isinstance(value, str) or '\0' in value:
            raise ValueError('editor text must be a string without NUL')
        encoded = value.encode('utf-8')
        self.buffers.append(encoded)
        return encoded

    def array(self, element, items):
        result = (element * len(items))(*items)
        self.buffers.append(result)
        return result

    def value(self, value):
        if isinstance(value, bool):
            return _Value(3, None, 0, 0.0, int(value))
        if isinstance(value, str):
            return _Value(0, self.text(value), 0, 0.0, 0)
        if isinstance(value, int):
            if not -(1 << 63) <= value < (1 << 63):
                raise ValueError('editor integer exceeds signed 64-bit range')
            return _Value(1, None, value, 0.0, 0)
        if isinstance(value, float):
            return _Value(2, None, 0, value, 0)
        raise TypeError('editor value must be a string, int, float, or bool')

    def menu(self, entry):
        return _MenuAction(*(self.text(entry.get(k, '')) for k in ('id', 'label', 'shortcut', 'icon_name', 'accessibility_name')),
                           _bool(entry.get('enabled', False), 'enabled'), _bool(entry.get('visible', True), 'visible'),
                           _bool(entry.get('checked', False), 'checked'), int(MenuEntryKind(entry.get('kind', 0))))

    def fields(self, fields):
        converted = []
        for f in fields:
            choices = self.array(_Choice, [_Choice(self.text(c['id']), self.text(c['label'])) for c in f.get('choices', [])])
            minimum, maximum = f.get('minimum'), f.get('maximum')
            integer_minimum, integer_maximum = f.get('integer_minimum'), f.get('integer_maximum')
            converted.append(_Field(self.text(f['id']), self.text(f['label']), int(EditorFieldKind(f['kind'])), self.value(f['value']),
                _bool(f.get('enabled', True), 'enabled'), _bool(f.get('visible', True), 'visible'),
                _bool(f.get('required', False), 'required'), int(minimum is not None), int(maximum is not None),
                0.0 if minimum is None else minimum, 0.0 if maximum is None else maximum, choices, len(choices),
                self.text(f.get('validation_error', '')), int(integer_minimum is not None), int(integer_maximum is not None),
                0 if integer_minimum is None else _int64(integer_minimum, 'integer_minimum'),
                0 if integer_maximum is None else _int64(integer_maximum, 'integer_maximum')))
        return self.array(_Field, converted)

    def token(self, t):
        return _Token(self.text(t.get('session', '')), _uint(t.get('source_revision', 0), 'source_revision'),
                      _uint(t.get('edit_revision', 0), 'edit_revision'))

    def state(self, state):
        original = self.fields(state.get('original_fields', []))
        fields = self.fields(state.get('fields', []))
        return _State(self.text(state['session']), _uint(state['source_revision'], 'source_revision'),
            _uint(state.get('edit_revision', 0), 'edit_revision'), int(EditorPanelPhase(state.get('phase', 0))),
            _bool(state.get('preview_enabled', False), 'preview_enabled'), _bool(state.get('preview_pending', False), 'preview_pending'),
            self.text(state.get('preview_id', '')), self.text(state.get('error', '')), original, len(original), fields, len(fields))

    def event(self, event):
        present = 'value' in event and event['value'] is not None
        return _Event(int(EditorPanelAction(event['action'])), self.text(event.get('field_id', '')), int(present),
            self.value(event['value']) if present else _Value(0, None, 0, 0.0, 0), self.token(event.get('token', {})),
            self.text(event.get('preview_id', '')), self.text(event.get('error', '')))


class EditorAutoFormBinding:
    """Project field schema to the same automatic-control data as C++."""
    def __init__(self, library):
        if library is None:
            raise TypeError('an explicit native editor library is required')
        self._library = library

    def execute(self, fields):
        """Return presentation snapshots; the host owns events and viewmodels."""
        inputs = _Inputs()
        values = inputs.fields(fields)
        return _result(self._library, self._library.lvui_editor_auto_form_execute(values, len(values)))


class MenuActionStateBinding:
    """Project the app's current command state into an LVUI menu item."""
    def __init__(self, library):
        if library is None:
            raise TypeError('an explicit native editor library is required')
        self._library = library

    def execute(self, request):
        """Return a projected menu item; the input and command stay app-owned."""
        inputs = _Inputs()
        action = inputs.menu(request)
        return _result(self._library, self._library.lvui_editor_menu_action_execute(ctypes.byref(action)))


class MenuSubmenuItemsBinding:
    """Project an ordered app-owned submenu, rejecting duplicate IDs."""
    def __init__(self, library):
        if library is None:
            raise TypeError('an explicit native editor library is required')
        self._library = library

    def execute(self, request):
        """Return ordered submenu items or raise EditorError without mutation."""
        inputs = _Inputs()
        actions = inputs.array(_MenuAction, [inputs.menu(item) for item in request['items']])
        return _result(self._library, self._library.lvui_editor_submenu_execute(inputs.text(request['parent_id']), actions, len(actions)))


class _Panel:
    """Return a new draft and explicit work effects; never invoke an owner."""
    _kind = None

    def __init__(self, library):
        if library is None:
            raise TypeError('an explicit native editor library is required')
        self._library = library

    def execute(self, request):
        """Return a new state and explicit effects for the app to orchestrate."""
        inputs = _Inputs()
        state = inputs.state(request['state'])
        event = inputs.event(request['event'])
        return _result(self._library, self._library.lvui_editor_panel_execute(int(self._kind), ctypes.byref(state), ctypes.byref(event)))


class MenuCommandParameterForm(_Panel):
    """Edit app-defined parameters for a menu command."""
    _kind = EditorPanelKind.MENU_PARAMETERS


class NonDestructivePreviewPanel(_Panel):
    """Request temporary previews and explicit apply or cancel effects."""
    _kind = EditorPanelKind.PREVIEW


class MaskRefinementControls(_Panel):
    """Edit the app's mask-refinement parameters."""
    _kind = EditorPanelKind.MASK_REFINEMENT


class LayerContentOptionsPanel(_Panel):
    """Edit options supplied by a layer-content owner."""
    _kind = EditorPanelKind.LAYER_CONTENT


class FilterGalleryPanel(_Panel):
    """Edit app-provided filter selections and their parameters."""
    _kind = EditorPanelKind.FILTER_GALLERY


class ColorProfilePicker(_Panel):
    """Edit the profile selection supplied by the app's color service."""
    _kind = EditorPanelKind.COLOR_PROFILE


class SoftProofSettingsPanel(_Panel):
    """Edit app-provided soft-proof settings."""
    _kind = EditorPanelKind.SOFT_PROOF


class TimelineEditOptionsPanel(_Panel):
    """Edit app-provided timeline operation settings."""
    _kind = EditorPanelKind.TIMELINE


class ExportFormatOptionsPanel(_Panel):
    """Edit app-provided export formats and options."""
    _kind = EditorPanelKind.EXPORT_FORMAT
