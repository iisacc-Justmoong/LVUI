#pragma once
#include "SurroundingText.h"

#include <LVUI/CodeComposition.h>
#include <LVUI/CodeEditorController.h>
#include <LVUI/TextReplacementCursor.h>

#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>

namespace lvui {

struct CodeCompositionRegionChange final {
    bool updated=false;
    std::optional<CodeDocumentChange> committed;
};
struct CodeSurroundingDeleteChange final {
    bool updated=false;
    std::optional<CodeDocumentChange> committed;
};

namespace code_editor_session_detail {
// Private preparation view. It is rebound exactly once before the candidate
// can be observed, so the published projection always owns the real snapshot.
class PreparedView final : public CodeDocumentView {
    std::shared_ptr<const CodeDocumentView> view_;
public:
    explicit PreparedView(std::shared_ptr<const CodeDocumentView> view):view_(std::move(view)){}
    void bind(std::shared_ptr<const CodeDocumentView> view) noexcept {view_=std::move(view);}
    [[nodiscard]] std::uint64_t revision()const noexcept override{return view_->revision();}
    [[nodiscard]] std::size_t length()const noexcept override{return view_->length();}
    [[nodiscard]] std::size_t byte_size()const noexcept override{return view_->byte_size();}
    [[nodiscard]] std::size_t line_count()const noexcept override{return view_->line_count();}
    [[nodiscard]] CodeDocumentResult<std::size_t> line_start(std::size_t line)const override{return view_->line_start(line);}
    [[nodiscard]] CodeDocumentResult<std::string> read(CodeDocumentRange range,std::size_t maximum)const override{return view_->read(range,maximum);}
    [[nodiscard]] CodeDocumentResult<std::size_t> range_byte_size(CodeDocumentRange range)const override{return view_->range_byte_size(range);}
};
}

// A view-local input session. Candidate composition is isolated from the
// source until a revision-checked commit succeeds.
class CodeEditorSession final {
public:
    [[nodiscard]] static CodeDocumentResult<CodeEditorSession> create(std::shared_ptr<const CodeDocumentSource> source,
            std::shared_ptr<CodeDocumentEditor> editor = {}) {
        const bool editable = bool(editor);
        auto controller = CodeEditorController::create(std::move(source), std::move(editor));
        if (!controller) return std::unexpected(controller.error());
        return CodeEditorSession(std::move(*controller), editable);
    }
    [[nodiscard]] CodeDocumentResult<std::shared_ptr<const CodeDocumentView>> view() const {
        if (auto ready = thread(); !ready) return std::unexpected(ready.error());
        if (composition_) return composition_->view(); return controller_.snapshot();
    }
    [[nodiscard]] CodeDocumentResult<CodeSelection> selection() const {
        if (auto ready = thread(); !ready) return std::unexpected(ready.error());
        if (composition_) return CodeSelection{composition_->selection_anchor(),composition_->selection_caret()};
        return controller_.selection();
    }
    [[nodiscard]] CodeDocumentResult<std::optional<CodeDocumentRange>> marked() const {
        if (auto ready = thread(); !ready) return std::unexpected(ready.error());
        if (composition_) return std::optional<CodeDocumentRange>{composition_->marked()}; return std::optional<CodeDocumentRange>{};
    }
    [[nodiscard]] bool editable() const noexcept { return editable_; }
    [[nodiscard]] CodeDocumentResult<bool> set_selection(std::size_t anchor, std::size_t caret) {
        if (auto ready = ordinary(); !ready) return std::unexpected(ready.error()); return controller_.set_selection(anchor, caret);
    }
    [[nodiscard]] CodeDocumentResult<bool> set_input_selection(std::size_t anchor,std::size_t caret) {
        if(auto ready=mutable_state();!ready)return std::unexpected(ready.error());
        if(composition_)return composition_->set_input_selection(anchor,caret);
        return controller_.set_selection(anchor,caret);
    }
    [[nodiscard]] CodeDocumentResult<bool> move(CodeMove direction, bool extend = false) {
        if (auto ready = ordinary(); !ready) return std::unexpected(ready.error()); return controller_.move(direction, extend);
    }
    [[nodiscard]] CodeDocumentResult<bool> refresh() {
        if (auto ready = ordinary(); !ready) return std::unexpected(ready.error()); return controller_.refresh();
    }
    [[nodiscard]] CodeDocumentResult<std::optional<CodeDocumentChange>> erase(CodeErase direction) {
        if (auto ready = ordinary(); !ready) return std::unexpected(ready.error());
        busy_ = true; struct Reset final { bool& busy; ~Reset() { busy = false; } } reset{busy_};
        return controller_.erase(direction);
    }
    [[nodiscard]] CodeDocumentResult<bool> set_composition(std::string text,
            std::optional<CodeDocumentRange> replacement = {}, std::optional<CodeDocumentRange> selected = {},
            std::optional<std::int32_t> input_cursor = {},bool selection_reversed = false) {
        if (auto ready = mutable_state(); !ready) return std::unexpected(ready.error());
        if (!editable_) return fail(CodeDocumentErrorCode::invalid, "Code editor is read-only");
        if (text.empty()&&!input_cursor) return cancel_composition();
        if (text.size() > CodeComposition::maximum_bytes) return fail(CodeDocumentErrorCode::too_large, "Composition exceeds 64 KiB");
        if (!code_document_detail::valid_text(text)) return fail(CodeDocumentErrorCode::invalid, "Composition requires NUL-free UTF-8");
        std::size_t units = 0;
        for (std::size_t at = 0; at < text.size();) {
            const auto bytes = code_document_detail::scalar_bytes(static_cast<unsigned char>(text[at]));
            units += bytes == 4 ? 2 : 1; at += bytes;
        }
        const auto selection = selected.value_or(CodeDocumentRange{units, 0});
        if ((selection_reversed&&(!selection.length||input_cursor))||selection.location > units || selection.length > units - selection.location)
            return fail(CodeDocumentErrorCode::out_of_range, "Composition selection exceeds its text");
        auto base = controller_.snapshot(); if (!base) return std::unexpected(base.error());
        CodeDocumentRange original;
        if (composition_) {
            if (replacement && !same(*replacement, composition_->marked()))
                return fail(CodeDocumentErrorCode::invalid, "Composition replacement must cover its marked range");
            original = composition_->base_range();
            const auto anchor=composition_->selection_anchor(),caret=composition_->selection_caret();
            const auto first=selection.location,last=first+selection.length;
            if (!input_cursor&&composition_->text() == text && anchor>=original.location&&caret>=original.location&&
                anchor-original.location==(selection_reversed?last:first)&&caret-original.location==(selection_reversed?first:last))
                return false;
        } else {
            auto current = controller_.selection(); if (!current) return std::unexpected(current.error());
            original = replacement.value_or(current->range());
        }
        auto next = CodeComposition::create(std::move(*base), original, std::move(text), selection);
        if (!next) return std::unexpected(next.error());
        if(selection_reversed){
            const auto marked=next->marked();const auto first=marked.location+selection.location,last=first+selection.length;
            auto moved=next->set_input_selection(last,first);if(!moved)return std::unexpected(moved.error());
        }
        if(input_cursor){
            const auto marked=next->marked();const auto projection=next->view();
            const auto caret=text_replacement_cursor(marked.location,marked.location+marked.length,projection->length(),*input_cursor);
            auto moved=next->set_input_selection(caret,caret);if(!moved)return std::unexpected(moved.error());
            if(composition_&&composition_->text()==next->text()&&composition_->selection_anchor()==caret&&
               composition_->selection_caret()==caret)return false;
        }
        composition_ = std::move(*next); return true;
    }
    [[nodiscard]] CodeDocumentResult<bool> set_input_composition(std::string text,
            std::optional<CodeDocumentRange> replacement,std::int32_t cursor) {
        return set_composition(std::move(text),replacement,{},cursor);
    }
    [[nodiscard]] CodeDocumentResult<bool> cancel_composition() {
        if (auto ready = mutable_state(); !ready) return std::unexpected(ready.error());
        if (!composition_) return false; composition_.reset(); return true;
    }
    [[nodiscard]] CodeDocumentResult<std::optional<CodeDocumentChange>> commit_composition() {
        if (auto ready = mutable_state(); !ready) return std::unexpected(ready.error());
        busy_ = true; struct Reset final { bool& busy; ~Reset() { busy = false; } } reset{busy_};
        return commit_locked();
    }
    [[nodiscard]] CodeDocumentResult<CodeCompositionRegionChange> set_input_composition_region(CodeDocumentRange range){
        if(auto ready=mutable_state();!ready)return std::unexpected(ready.error());
        if(!editable_)return fail(CodeDocumentErrorCode::invalid,"Code editor is read-only");
        busy_=true;struct Reset final{bool& busy;~Reset(){busy=false;}}reset{busy_};
        auto visible=view();if(!visible)return std::unexpected(visible.error());
        if(range.location>(*visible)->length()||range.length>(*visible)->length()-range.location)
            return fail(CodeDocumentErrorCode::out_of_range,"Composition region exceeds the document");
        const auto previous=composition_?std::optional{composition_->marked()}:std::nullopt;
        if(!range.length){
            auto committed=commit_locked();if(!committed)return std::unexpected(committed.error());
            return CodeCompositionRegionChange{previous.has_value(),std::move(*committed)};
        }
        auto selected=selection();if(!selected)return std::unexpected(selected.error());
        auto text=(*visible)->read(range,CodeComposition::maximum_bytes);if(!text)return std::unexpected(text.error());
        auto prepared=std::make_shared<code_editor_session_detail::PreparedView>(*visible);
        auto next=CodeComposition::create(prepared,range,std::move(*text),{0,0});if(!next)return std::unexpected(next.error());
        if(next->marked().length!=range.length)return fail(CodeDocumentErrorCode::invalid,"Code source returned a partial composition region");
        auto moved=next->set_input_selection(selected->anchor,selected->caret);if(!moved)return std::unexpected(moved.error());
        auto committed=commit_locked();if(!committed)return std::unexpected(committed.error());
        auto latest=controller_.snapshot();if(!latest)return std::unexpected(latest.error());
        prepared->bind(std::move(*latest));
        const bool updated=!previous||!same(*previous,range)||
            (*committed&&(**committed).before_revision!=(**committed).revision);
        composition_=std::move(*next);
        return CodeCompositionRegionChange{updated,std::move(*committed)};
    }
    // Apply the portions outside the candidate to the source in one edit and
    // retain a separately shortened projection for the active composition.
    [[nodiscard]] CodeDocumentResult<CodeSurroundingDeleteChange> erase_input_surrounding(CodeDocumentRange range){
        using namespace surrounding_text_detail;
        if(auto ready=mutable_state();!ready)return std::unexpected(ready.error());
        if(!editable_||!composition_)return fail(CodeDocumentErrorCode::invalid,"Surrounding deletion requires an editable composition");
        busy_=true;struct Reset final{bool& busy;~Reset(){busy=false;}}reset{busy_};
        const auto view=composition_->view();const auto mark=composition_->marked(),original=composition_->base_range();
        const auto anchor=composition_->selection_anchor(),caret=composition_->selection_caret();
        const auto first=(std::min)(anchor,caret),last=(std::max)(anchor,caret);
        auto plan=Plan::create(view->length(),range.location,range.length,first,last-first);
        if(!plan)return fail(CodeDocumentErrorCode::out_of_range,"Surrounding deletion must contain the selection");
        const auto visible=plan->visible();
        for(std::size_t i=0;i<visible.count;++i){
            const auto item=visible.values[i];
            auto a=view->read({item.first,0},0);if(!a)return std::unexpected(a.error());
            auto b=view->read({item.last,0},0);if(!b)return std::unexpected(b.error());
        }
        if(plan->empty())return CodeSurroundingDeleteChange{};
        auto base=controller_.snapshot();if(!base)return std::unexpected(base.error());
        const auto base_ranges=plan->base(mark.location,mark.length,original.length);
        auto read_exact=[&](const CodeDocumentView& source,std::size_t begin,std::size_t end,std::size_t budget)->CodeDocumentResult<std::string>{
            auto text=source.read({begin,end-begin},budget);if(!text)return std::unexpected(text.error());
            if(!code_document_detail::valid_text(*text))return fail(CodeDocumentErrorCode::invalid,"Code source returned invalid UTF-8");
            std::size_t units=0;
            for(std::size_t at=0;at<text->size();){const auto bytes=code_document_detail::scalar_bytes(static_cast<unsigned char>((*text)[at]));units+=bytes==4?2:1;at+=bytes;}
            if(units!=end-begin||text->size()>budget)return fail(CodeDocumentErrorCode::invalid,"Code source returned a partial surrounding range");
            return std::move(*text);
        };
        std::string candidate;const auto marked_end=mark.location+mark.length;auto position=mark.location;
        auto append=[&](const CodeDocumentView& source,std::string& target,std::size_t begin,std::size_t end)->CodeDocumentResult<void>{
            if(begin==end)return {};
            auto part=read_exact(source,begin,end,CodeComposition::maximum_bytes-target.size());
            if(!part)return std::unexpected(part.error());target+=*part;return {};
        };
        for(std::size_t i=0;i<visible.count;++i){
            const auto item=visible.values[i];const auto start=(std::max)(item.first,mark.location),stop=(std::min)(item.last,marked_end);
            if(start>=stop)continue;
            if(auto part=append(*view,candidate,position,start);!part)return std::unexpected(part.error());position=stop;
        }
        if(auto part=append(*view,candidate,position,marked_end);!part)return std::unexpected(part.error());
        std::optional<CodeComposition> base_projection;std::string retained;CodeDocumentRange base_edit{};
        if(base_ranges.count){
            base_edit={base_ranges.values[0].first,base_ranges.values[base_ranges.count-1].last-base_ranges.values[0].first};
            for(std::size_t i=1;i<base_ranges.count;++i){
                if(auto part=append(**base,retained,base_ranges.values[i-1].last,base_ranges.values[i].first);!part)
                    return std::unexpected(part.error());
            }
            auto projected=CodeComposition::create(*base,base_edit,retained,{0,0});if(!projected)return std::unexpected(projected.error());
            base_projection=std::move(*projected);
        }
        auto prepared=std::make_shared<code_editor_session_detail::PreparedView>(base_projection?base_projection->view():*base);
        const auto new_original=CodeDocumentRange{base_ranges.map(original.location),original.length};
        auto next=CodeComposition::create(prepared,new_original,std::move(candidate),{0,0});if(!next)return std::unexpected(next.error());
        const auto new_marked_start=visible.map(mark.location),new_marked_end=visible.map(marked_end);
        if(next->marked().location!=new_marked_start||next->marked().length!=new_marked_end-new_marked_start)
            return fail(CodeDocumentErrorCode::invalid,"Surrounding composition projection is inconsistent");
        auto selected=next->set_input_selection(visible.map(anchor),visible.map(caret));if(!selected)return std::unexpected(selected.error());
        std::optional<CodeDocumentChange> committed;
        if(base_projection){
            auto old_selection=controller_.selection();if(!old_selection)return std::unexpected(old_selection.error());
            const CodeSelection adjusted{base_ranges.map(old_selection->anchor),base_ranges.map(old_selection->caret)};
            auto changed=controller_.replace_range_projected(base_edit,std::move(retained),adjusted,base_projection->view().get());
            if(!changed)return std::unexpected(changed.error());committed=std::move(*changed);
            auto latest=controller_.snapshot();if(!latest)return std::unexpected(latest.error());prepared->bind(std::move(*latest));
        }else{
            // A candidate-only edit has no source replace to perform the
            // revision check, so reject an already stale base explicitly.
            auto latest=controller_.source_->snapshot();if(!latest)return std::unexpected(latest.error());
            if(!*latest||(*latest)->revision()!=(*base)->revision())
                return fail(CodeDocumentErrorCode::stale,"Code composition source changed during surrounding deletion");
        }
        composition_=std::move(*next);return CodeSurroundingDeleteChange{true,std::move(committed)};
    }
    [[nodiscard]] CodeDocumentResult<CodeDocumentChange> insert(std::string text, std::optional<CodeDocumentRange> replacement = {}) {
        if (auto ready = mutable_state(); !ready) return std::unexpected(ready.error());
        if (!editable_) return fail(CodeDocumentErrorCode::invalid, "Code editor is read-only");
        CodeDocumentRange range;
        if (composition_) {
            if (replacement && !same(*replacement, composition_->marked()))
                return fail(CodeDocumentErrorCode::invalid, "Text replacement must cover the marked composition");
            range = composition_->base_range();
        } else {
            auto current = controller_.selection(); if (!current) return std::unexpected(current.error());
            range = replacement.value_or(current->range());
        }
        busy_ = true; struct Reset final { bool& busy; ~Reset() { busy = false; } } reset{busy_};
        auto changed = controller_.replace_range(range, std::move(text));
        if (!changed) return std::unexpected(changed.error()); composition_.reset(); return *changed;
    }
    [[nodiscard]] CodeDocumentResult<CodeDocumentChange> insert_input_text(std::string text,
            std::optional<CodeDocumentRange> replacement,std::int32_t cursor) {
        return insert_projected(std::move(text),replacement,cursor,{});
    }
    [[nodiscard]] CodeDocumentResult<CodeDocumentChange> insert_input_text_selection(std::string text,
            std::optional<CodeDocumentRange> replacement,CodeSelection selection) {
        return insert_projected(std::move(text),replacement,{},selection);
    }
private:
    [[nodiscard]] CodeDocumentResult<std::optional<CodeDocumentChange>> commit_locked(){
        if(!composition_)return std::optional<CodeDocumentChange>{};
        const CodeSelection selected{composition_->selection_anchor(),composition_->selection_caret()};
        const auto projection=composition_->view();
        auto changed=controller_.replace_range_projected(composition_->base_range(),composition_->text(),selected,projection.get());
        if(!changed)return std::unexpected(changed.error());
        composition_.reset();return std::optional<CodeDocumentChange>{*changed};
    }
    [[nodiscard]] CodeDocumentResult<CodeDocumentChange> insert_projected(std::string text,
            std::optional<CodeDocumentRange> replacement,std::optional<std::int32_t> cursor,
            std::optional<CodeSelection> selection) {
        if(auto ready=mutable_state();!ready)return std::unexpected(ready.error());
        if(!editable_)return fail(CodeDocumentErrorCode::invalid,"Code editor is read-only");
        if(text.size()>CodeComposition::maximum_bytes)return fail(CodeDocumentErrorCode::too_large,"Code input exceeds 64 KiB");
        if(!code_document_detail::valid_text(text))return fail(CodeDocumentErrorCode::invalid,"Code input requires NUL-free UTF-8");
        const auto units=[](std::string_view value){std::size_t count=0;for(std::size_t i=0;i<value.size();){
            const auto width=code_document_detail::scalar_bytes(static_cast<unsigned char>(value[i]));count+=width==4?2:1;i+=width;}
            return count;};
        const auto inserted_units=units(text);
        CodeDocumentRange range,target;
        if(composition_){
            const auto marked=composition_->marked();target=replacement.value_or(marked);range=composition_->base_range();
            if(!same(target,marked)){
                const auto visible=composition_->view();
                if(target.location>visible->length()||target.length>visible->length()-target.location)
                    return fail(CodeDocumentErrorCode::out_of_range,"Code input range exceeds the displayed document");
                auto first=visible->read({target.location,0},0);if(!first)return std::unexpected(first.error());
                auto last=visible->read({target.location+target.length,0},0);if(!last)return std::unexpected(last.error());
                const auto start=(std::min)(marked.location,target.location);
                const auto end=(std::max)(marked.location+marked.length,target.location+target.length);
                const auto read_exact=[&](CodeDocumentRange part,std::size_t limit)->CodeDocumentResult<std::string>{
                    auto value=visible->read(part,limit);if(!value)return std::unexpected(value.error());
                    if(value->size()>limit||!code_document_detail::valid_text(*value)||units(*value)!=part.length)
                        return fail(CodeDocumentErrorCode::invalid,"Code source returned partial projected text");
                    return value;
                };
                auto prefix=read_exact({start,target.location-start},CodeComposition::maximum_bytes-text.size());
                if(!prefix)return std::unexpected(prefix.error());
                auto suffix=read_exact({target.location+target.length,end-(target.location+target.length)},
                                       CodeComposition::maximum_bytes-text.size()-prefix->size());
                if(!suffix)return std::unexpected(suffix.error());
                range={start,range.location+range.length+(end-(marked.location+marked.length))-start};
                prefix->append(text);prefix->append(*suffix);text=std::move(*prefix);
            }
        }else{
            auto current=controller_.selection();if(!current)return std::unexpected(current.error());
            range=target=replacement.value_or(current->range());
        }
        auto base=controller_.snapshot();if(!base)return std::unexpected(base.error());
        auto candidate=CodeComposition::create(std::move(*base),range,text,{0,0});
        if(!candidate)return std::unexpected(candidate.error());
        const auto projection=candidate->view();
        if(cursor){
            if(inserted_units>(std::numeric_limits<std::size_t>::max)()-target.location)
                return fail(CodeDocumentErrorCode::too_large,"Code input cursor overflow");
            const auto caret=text_replacement_cursor(target.location,target.location+inserted_units,projection->length(),*cursor);
            selection=CodeSelection{caret,caret};
        }
        if(!selection)return fail(CodeDocumentErrorCode::invalid,"A projected text selection is required");
        auto anchor=projection->read({selection->anchor,0},0);if(!anchor)return std::unexpected(anchor.error());
        auto caret=projection->read({selection->caret,0},0);if(!caret)return std::unexpected(caret.error());
        busy_=true;struct Reset final{bool& busy;~Reset(){busy=false;}}reset{busy_};
        auto changed=controller_.replace_range_projected(range,std::move(text),*selection,projection.get());
        if(!changed)return std::unexpected(changed.error());composition_.reset();return *changed;
    }
    CodeEditorSession(CodeEditorController controller, bool editable) : controller_(std::move(controller)), editable_(editable) {}
    [[nodiscard]] static std::unexpected<CodeDocumentError> fail(CodeDocumentErrorCode code, std::string message) {
        return std::unexpected(CodeDocumentError{code, std::move(message)});
    }
    [[nodiscard]] static bool same(CodeDocumentRange a, CodeDocumentRange b) noexcept { return a.location == b.location && a.length == b.length; }
    [[nodiscard]] CodeDocumentResult<void> thread() const {
        if (owner_ != std::this_thread::get_id()) return fail(CodeDocumentErrorCode::wrong_thread, "Code input session requires its owner thread"); return {};
    }
    [[nodiscard]] CodeDocumentResult<void> mutable_state() const {
        if (auto ready = thread(); !ready) return ready;
        if (busy_) return fail(CodeDocumentErrorCode::busy, "A code input edit is committing"); return {};
    }
    [[nodiscard]] CodeDocumentResult<void> ordinary() const {
        if (auto ready = mutable_state(); !ready) return ready;
        if (composition_) return fail(CodeDocumentErrorCode::busy, "Commit or cancel the active composition first"); return {};
    }
    CodeEditorController controller_;
    std::optional<CodeComposition> composition_;
    const bool editable_;
    const std::thread::id owner_ = std::this_thread::get_id();
    bool busy_ = false;
};

} // namespace lvui
