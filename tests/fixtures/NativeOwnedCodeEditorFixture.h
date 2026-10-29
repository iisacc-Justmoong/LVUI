#pragma once
#include <LVUI/CodeDocument.h>
#include <LVUI/CodeEditorController.h>
#include <iiSDKCore/Core/Property/Observable.h>

#include <cstdint>
#include <memory>

struct NativeOwnedCodeEditorModel final {
    std::shared_ptr<lvui::CodeMemoryDocument> document = lvui::CodeMemoryDocument::create("alpha").value();
    ii::core::Observable<std::int64_t> revision{0};
    ii::core::Observable<double> width{240};
    int edits = 0, selections = 0; bool external_during_edit = false;
    void changed(const lvui::CodeDocumentChange& value) {
        ++edits; width.set(230); auto current = value.revision;
        if (external_during_edit) current = document->replace({.revision=current,.range={0,0},.text="?"}).value().revision;
        revision.set(static_cast<std::int64_t>(current));
    }
    void selected(lvui::CodeSelection) { ++selections; }
};
