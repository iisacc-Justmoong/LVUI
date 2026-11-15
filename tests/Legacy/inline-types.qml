import LVRS 1.0 as LV
component Prompt: LV.TextEditor {
    required property var model
    readonly property alias input: field.editorItem
    property bool wide: width >= 1000
    onTextEdited: value => model.setText(value)
    function focusEditor() { }
}
