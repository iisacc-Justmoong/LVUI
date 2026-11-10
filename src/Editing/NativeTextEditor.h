#pragma once
#include <LVUI/TextEditor.h>
#include <LVUI/NativeInputField.h>
namespace lvui::native {
using NativeTextEditorCallbacks=NativeInputFieldCallbacks;
template<class Affinity>using NativeTextEditorSnapshotValue=NativeInputFieldSnapshotValue<Affinity>;
template<class NativeField,class Affinity>struct TextEditorHostPolicy final {
 using Field=NativeField;using Configuration=TextEditorConfiguration;
 using Callbacks=NativeTextEditorCallbacks;using Clipboard=NativeInputFieldClipboard;
 using Snapshot=NativeTextEditorSnapshotValue<Affinity>;
 [[nodiscard]] static bool valid_configuration(const Configuration& value)noexcept{return valid_text_editor_configuration(value);}
};
template<template<class>class NativeHost,class NativeField,class Affinity>
using TextEditorBinding=NativeHost<TextEditorHostPolicy<NativeField,Affinity>>;
} // namespace lvui::native
