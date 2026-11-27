"""Python access to LVUI's native, Qt-free ahead-of-time compiler."""
from __future__ import annotations

import ctypes
import os
from pathlib import Path
import sys
import threading
from .editor import (EditorError, EditorFieldKind, MenuEntryKind, EditorPanelKind, EditorPanelPhase, EditorPanelAction, EditorEffectKind,
                     load_editor_library, EditorAutoFormBinding, MenuActionStateBinding, MenuSubmenuItemsBinding, MenuCommandParameterForm,
                     NonDestructivePreviewPanel, MaskRefinementControls, LayerContentOptionsPanel, FilterGalleryPanel,
                     ColorProfilePicker, SoftProofSettingsPanel, TimelineEditOptionsPanel, ExportFormatOptionsPanel)

__all__ = ['CompileError', 'compile_native', 'compile_style_header', 'EditorError', 'EditorFieldKind', 'MenuEntryKind', 'EditorPanelKind',
           'EditorPanelPhase', 'EditorPanelAction', 'EditorEffectKind', 'load_editor_library', 'EditorAutoFormBinding', 'MenuActionStateBinding',
           'MenuSubmenuItemsBinding', 'MenuCommandParameterForm', 'NonDestructivePreviewPanel', 'MaskRefinementControls',
           'LayerContentOptionsPanel', 'FilterGalleryPanel', 'ColorProfilePicker', 'SoftProofSettingsPanel',
           'TimelineEditOptionsPanel', 'ExportFormatOptionsPanel']
_SOURCE_LIMIT = 1024 * 1024
_ARGUMENT_LIMIT = 4096
_INCLUDE_LIMIT = 256
_library = None
_library_lock = threading.Lock()


class CompileError(ValueError):
    """A diagnostic returned by the LVUI compiler, with source coordinates."""
    def __init__(self, message: str, *, stage: str, offset: int, line: int, column: int):
        super().__init__(message)
        self.stage = stage
        self.offset = offset
        self.line = line
        self.column = column


class _Bytes(ctypes.Structure):
    _fields_ = [('data', ctypes.c_char_p), ('size', ctypes.c_size_t)]


def _encode(value: str, label: str, limit: int, *, allow_nul: bool = False) -> bytes:
    if not isinstance(value, str):
        raise TypeError(f'{label} must be a string')
    encoded = value.encode('utf-8')
    if len(encoded) > limit:
        raise ValueError(f'{label} is too large')
    if not allow_nul and b'\0' in encoded:
        raise ValueError(f'{label} must not contain NUL')
    return encoded


def _bytes(value: bytes) -> _Bytes:
    return _Bytes(value, len(value))


def _configure_compiler(lib):
    """Configure one caller-supplied bridge without changing its ownership."""
    lib.lvui_compiler_abi_version.argtypes = []
    lib.lvui_compiler_abi_version.restype = ctypes.c_int
    if lib.lvui_compiler_abi_version() != 1:
        raise RuntimeError('unsupported compiler bridge ABI')
    lib.lvui_compiler_compile.argtypes = [_Bytes, _Bytes, ctypes.POINTER(_Bytes), ctypes.c_size_t, ctypes.POINTER(_Bytes)]
    lib.lvui_compiler_compile.restype = ctypes.c_void_p
    editor_compile = getattr(lib, 'lvui_compiler_compile_editor_panels', None)
    if editor_compile is not None:
        editor_compile.argtypes = lib.lvui_compiler_compile.argtypes
        editor_compile.restype = ctypes.c_void_p
    lib.lvui_compiler_styles.argtypes = [_Bytes, _Bytes]
    lib.lvui_compiler_styles.restype = ctypes.c_void_p
    for function in ('status', 'stage'):
        entry = getattr(lib, 'lvui_compiler_result_' + function)
        entry.argtypes = [ctypes.c_void_p]
        entry.restype = ctypes.c_int
    for function in ('offset', 'line', 'column', 'size'):
        entry = getattr(lib, 'lvui_compiler_result_' + function)
        entry.argtypes = [ctypes.c_void_p]
        entry.restype = ctypes.c_size_t
    lib.lvui_compiler_result_data.argtypes = [ctypes.c_void_p]
    lib.lvui_compiler_result_data.restype = ctypes.c_void_p
    lib.lvui_compiler_result_free.argtypes = [ctypes.c_void_p]
    lib.lvui_compiler_result_free.restype = None
    return lib


def _native():
    global _library
    if _library is not None:
        return _library
    with _library_lock:
        if _library is not None:
            return _library
        name = ('LVUICompilerBridge.dll' if os.name == 'nt' else
                'libLVUICompilerBridge.dylib' if sys.platform == 'darwin' else 'libLVUICompilerBridge.so')
        explicit = os.environ.get('LVUI_COMPILER_LIBRARY')
        if explicit:
            candidates = [Path(explicit)]
        else:
            candidates = []
            for parent in Path(__file__).resolve().parents:
                candidates.extend((parent / 'lib' / name, parent / 'bin' / name))
        failures = []
        for path in candidates:
            if not path.is_file():
                continue
            try:
                lib = _configure_compiler(ctypes.CDLL(str(path)))
            except (OSError, AttributeError, RuntimeError) as error:
                failures.append(f'{path}: {error}')
                continue
            _library = lib
            return lib
        detail = '; '.join(failures) if failures else 'library was not found'
        raise RuntimeError(f'LVUI compiler bridge is unavailable: {detail}')


def _result(lib, handle) -> str:
    if not handle:
        raise MemoryError('LVUI could not allocate a compiler result')
    try:
        status = lib.lvui_compiler_result_status(handle)
        length = lib.lvui_compiler_result_size(handle)
        pointer = lib.lvui_compiler_result_data(handle)
        if length and not pointer:
            raise RuntimeError('LVUI returned an invalid compiler result')
        text = ctypes.string_at(pointer, length).decode('utf-8') if length else ''
        if status == 0:
            return text
        if status == 1:
            stage_number = lib.lvui_compiler_result_stage(handle)
            stage = {0: 'source', 1: 'style', 2: 'header'}.get(stage_number, 'unknown')
            raise CompileError(text, stage=stage, offset=lib.lvui_compiler_result_offset(handle),
                               line=lib.lvui_compiler_result_line(handle),
                               column=lib.lvui_compiler_result_column(handle))
        if status == 2:
            raise ValueError(text or 'Invalid LVUI compiler arguments')
        raise RuntimeError(text or 'LVUI compiler failed')
    finally:
        lib.lvui_compiler_result_free(handle)


def compile_native(source: str, *, name: str = 'MainView', includes=(),
                   style_profile: str | None = None, editor_panels: bool = False, library=None) -> str:
    """Compile LVUI to C++23; editor panels require an explicit native binding."""
    if not isinstance(editor_panels, bool):
        raise TypeError('editor_panels must be a bool')
    if editor_panels and library is None:
        raise TypeError('editor panels require an explicit native compiler library')
    source_bytes = _encode(source, 'source', _SOURCE_LIMIT, allow_nul=True)
    name_bytes = _encode(name, 'name', _ARGUMENT_LIMIT)
    if isinstance(includes, (str, bytes)):
        raise TypeError('includes must be an iterable of strings')
    items = []
    for item in includes:
        if len(items) >= _INCLUDE_LIMIT:
            raise ValueError('too many includes')
        items.append(_encode(item, 'include', _ARGUMENT_LIMIT))
    include_array = (_Bytes * len(items))(*(_bytes(item) for item in items)) if items else None
    profile_bytes = None if style_profile is None else _encode(style_profile, 'style_profile', _SOURCE_LIMIT, allow_nul=True)
    profile = None if profile_bytes is None else _bytes(profile_bytes)
    lib = _native() if library is None else _configure_compiler(library)
    compiler = getattr(lib, 'lvui_compiler_compile_editor_panels', None) if editor_panels else lib.lvui_compiler_compile
    if compiler is None:
        raise RuntimeError('the native LVUI compiler does not support editor panels')
    return _result(lib, compiler(_bytes(source_bytes), _bytes(name_bytes), include_array,
                                len(items), None if profile is None else ctypes.byref(profile)))


def compile_style_header(profile: str, *, namespace: str = 'lvui_style') -> str:
    """Compile an LVUI style profile into a standalone C++ header."""
    profile_bytes = _encode(profile, 'profile', _SOURCE_LIMIT, allow_nul=True)
    namespace_bytes = _encode(namespace, 'namespace', _ARGUMENT_LIMIT)
    lib = _native()
    return _result(lib, lib.lvui_compiler_styles(_bytes(profile_bytes), _bytes(namespace_bytes)))
