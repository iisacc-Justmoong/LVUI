#include <LVUI/Compiler.h>

#include <stdexcept>
#include <string>

int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { VStack { ListItem(type: .Resource, previewSource: vm.preview, moreItems: [\"Duplicate\"], onMenuItemTriggered: vm.menu)\nListItem(type: .Media, durationText: vm.duration, mediaDetail: \"WAV\", segments: vm.versions, segmentIndex: vm.version, onSegmentIndexChanged: vm.changed) } }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto part:{"lvui::ListItemVariant::resource","lvui::ListItemVariant::media","value.preview", "value.duration_text", "value.media_detail",
                   "value.primary_action.text = \"Open\"","value.secondary_action.text = \"Reveal\"",
                   "value.primary_action.text = \"Play\"","value.secondary_action.text = \"Stop\"",
                   ".menu_item_triggered", ".segment_index_changed"})
        if(code->find(part)==code->npos)throw std::runtime_error(std::string("Preview item generation lost ")+part);
    for(auto source:{"Window { ListItem(type: .resource) }","Window { ListItem(type: .media) }"}){
        auto item=lvui::compile_native(source);if(!item||item->find("NativeListItemComposite::create")==item->npos)
            throw std::runtime_error("A preview item did not use the compound runtime");
    }
    for(auto source:{"Window { ListItem(type: .Resource, previewSource: 1) }",
                     "Window { ListItem(type: .Media, durationText: true) }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid preview input was accepted");
}
