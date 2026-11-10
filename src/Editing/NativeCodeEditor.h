#pragma once
#include <LVUI/CodeEditorSession.h>
#include <LVUI/InputField.h>
#include <iiSDKCore/Core/Result/Result.h>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
namespace lvui::native {
struct CodeEditorAppearance final {
    std::string font_family, accessibility_name = "Code editor";
    double width = 400, height = 128, horizontal_padding = 10, vertical_padding = 8;
    double font_size = 13, line_height = 20, tab_stop = 80, corner_radius = 8;
    bool enabled = true;
    InputFieldPalette palette;
    friend bool operator==(const CodeEditorAppearance&, const CodeEditorAppearance&) = default;
};
template<class Color> [[nodiscard]] inline StyleColor code_editor_color(const Color& value) noexcept {
    return {value.red,value.green,value.blue,value.alpha};
}
struct CodeEditorInput final {
    std::shared_ptr<const CodeDocumentSource> source;
    std::shared_ptr<CodeDocumentEditor> editor;
    CodeEditorAppearance appearance;
};
struct NativeCodeEditorCallbacks final {
    std::function<void(const CodeDocumentChange&)> edited;
    std::function<void(CodeSelection)> selection_changed;
};
struct NativeCodeEditorClipboard final { std::function<ii::core::Result<void>(std::string)> write; };
struct NativeCodeEditorSnapshot final {
    std::uint64_t document_revision = 0;
    CodeSelection selection;
    std::optional<CodeDocumentRange> marked;
    double scroll_x = 0, scroll_y = 0;
    std::size_t first_column = 0;
};

struct CodeEditorHostPolicy final {
    using Input=CodeEditorInput;using Appearance=CodeEditorAppearance;using Callbacks=NativeCodeEditorCallbacks;
    using Clipboard=NativeCodeEditorClipboard;using Snapshot=NativeCodeEditorSnapshot;using Color=StyleColor;
    using Session=CodeEditorSession;using Selection=CodeSelection;using Move=CodeMove;using Erase=CodeErase;
    using ViewportLine=CodeViewportLine;using ViewportFrame=CodeViewportFrame;using ViewportPolicy=CodeViewportPolicy;using DocumentRange=CodeDocumentRange;
    using DocumentView=CodeDocumentView;using DocumentChange=CodeDocumentChange;using Error=CodeDocumentError;using ErrorCode=CodeDocumentErrorCode;using Limits=CodeDocumentLimits;
    template<class T>using Result=CodeDocumentResult<T>;
    [[nodiscard]] static auto error(ErrorCode code,std::string message){return code_document_detail::error(code,std::move(message));}
    [[nodiscard]] static bool valid_document_text(std::string_view value){return code_document_detail::valid_text(value);}
    [[nodiscard]] static auto scalar_bytes(unsigned char value){return code_document_detail::scalar_bytes(value);}
    [[nodiscard]] static bool valid_accessibility_text(std::string_view value){return lvui::valid_accessibility_text(value);}
};
template<template<class>class Host>using CodeEditorBinding=Host<CodeEditorHostPolicy>;
} // namespace lvui::native
