#pragma once
#include <LVUI/NativeInputField.h>
#include <iiGUI/NativeInputFieldView.h>
namespace lvui::native {
using NativeInputField=InputFieldBinding<ii::gui::BasicNativeInputField,ii::gui::BasicTextInput,ii::gui::BasicTextFieldMaterial,ii::gui::NativeTextAffinity>;
using NativeInputFieldSnapshot=NativeInputFieldSnapshotValue<ii::gui::NativeTextAffinity>;
using TextInput=lvui::TextInputBinding<ii::gui::BasicTextInput>;
}
