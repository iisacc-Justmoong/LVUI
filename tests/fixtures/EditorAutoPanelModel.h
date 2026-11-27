#pragma once

#include <LVUI>
#include <stdexcept>

// Test-owned viewmodel: schema and event destination are supplied by the app.
// No platform, document or singleton is accessed while projecting the fields.
struct EditorAutoPanelModel final {
    std::shared_ptr<lvui::EditorAutoFormModel> autoModel = std::make_shared<lvui::EditorAutoFormModel>();
    std::vector<lvui::EditorField> schema{
        {"text", "Text field", lvui::EditorFieldKind::text, std::string{"Original"}},
        {"flag", "Boolean field", lvui::EditorFieldKind::boolean, true},
        {"amount", "Amount field", lvui::EditorFieldKind::real, 0.5, true, true, false, 0.0, 1.0},
        {"choice", "Choice field", lvui::EditorFieldKind::choice, std::string{"one"}, true, true, false,
            {}, {}, {{"one", "One"}, {"two", "Two"}}},
        {"integer", "Integer field", lvui::EditorFieldKind::integer, std::int64_t{9007199254740993LL}},
        {"pending", "Pending field", lvui::EditorFieldKind::choice, std::string{}, true, true, true,
            {}, {}, {{"available", "Available"}}},
        {"wide", "Wide field", lvui::EditorFieldKind::real, 0.0, true, true, false, -1.0e13, 1.0e13}
    };
    std::vector<lvui::EditorFieldEdit> edits;

    EditorAutoPanelModel() {
        auto rows = lvui::EditorAutoFormBinding{}.execute({schema,
            [this](lvui::EditorFieldEdit edit) { edits.push_back(std::move(edit)); }});
        if (!rows) throw std::runtime_error(rows.error().message);
        autoModel->fields.set(std::move(*rows));
    }
    void execute() {}
};
