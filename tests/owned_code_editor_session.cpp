#include <LVUI/CodeEditorSession.h>

#include <array>
#include <memory>
#include <stdexcept>
#include <thread>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(lvui::CodeDocumentResult<T> value) {
    if (!value) throw std::runtime_error(value.error().message); return std::move(*value);
}
class ReentrantEditor final : public lvui::CodeDocumentEditor {
public:
    std::shared_ptr<lvui::CodeMemoryDocument> document; lvui::CodeEditorSession* session = nullptr; bool guarded = false;
    lvui::CodeDocumentResult<lvui::CodeDocumentChange> replace(const lvui::CodeDocumentEdit& edit) override {
        auto nested = session->set_composition("nested");
        auto selection = session->set_input_selection(0,0);
        auto region = session->set_input_composition_region({0,0});
        auto deletion = session->erase_input_surrounding({0,0});
        guarded = !nested && nested.error().code == lvui::CodeDocumentErrorCode::busy &&
            !selection && selection.error().code == lvui::CodeDocumentErrorCode::busy &&
            !region && region.error().code == lvui::CodeDocumentErrorCode::busy &&
            !deletion && deletion.error().code == lvui::CodeDocumentErrorCode::busy;
        return document->replace(edit);
    }
};
class ShortView final : public lvui::CodeDocumentView {
    std::shared_ptr<const lvui::CodeDocumentView> wrapped_;
public:
    explicit ShortView(std::shared_ptr<const lvui::CodeDocumentView> view):wrapped_(std::move(view)){}
    std::uint64_t revision()const noexcept override{return wrapped_->revision();}
    std::size_t length()const noexcept override{return wrapped_->length();}
    std::size_t byte_size()const noexcept override{return wrapped_->byte_size();}
    std::size_t line_count()const noexcept override{return wrapped_->line_count();}
    lvui::CodeDocumentResult<std::size_t> line_start(std::size_t line)const override{return wrapped_->line_start(line);}
    lvui::CodeDocumentResult<std::size_t> range_byte_size(lvui::CodeDocumentRange range)const override{return wrapped_->range_byte_size(range);}
    lvui::CodeDocumentResult<std::string> read(lvui::CodeDocumentRange range,std::size_t maximum)const override{
        if(range.length)return std::string{};return wrapped_->read(range,maximum);
    }
};
class ShortSource final : public lvui::CodeDocumentSource {
public:
    std::shared_ptr<lvui::CodeMemoryDocument> document;
    lvui::CodeDocumentResult<std::shared_ptr<const lvui::CodeDocumentView>> snapshot()const override{
        auto current=document->snapshot();if(!current)return std::unexpected(current.error());
        return std::shared_ptr<const lvui::CodeDocumentView>(std::make_shared<ShortView>(*current));
    }
};
void input_service_selection() {
    using namespace lvui;
    auto document=take(CodeMemoryDocument::create("A😀BC"));
    auto session=take(CodeEditorSession::create(document,document));
    require(take(session.set_selection(1,3))&&take(session.set_composition("한😀")),"Could not start selection fixture");
    require(take(session.set_input_selection(0,0))&&take(session.selection())==CodeSelection{0,0}&&
            take(session.marked()).has_value()&&take(document->snapshot())->revision()==0,
            "Selection outside marked text lost the composition");
    require(take(session.set_input_selection(5,1))&&take(session.selection())==CodeSelection{5,1},
            "Composition did not retain directional selection");
    require(!session.set_input_selection(3,3)&&!session.set_input_selection(7,7)&&
            take(session.selection())==CodeSelection{5,1}&&take(session.marked()).has_value(),
            "Invalid selection damaged the candidate or split an emoji");
    require(!session.set_selection(0,0),"Ordinary selection bypassed the active composition contract");
    auto change=take(session.commit_composition());
    require(change&&change->revision==1&&take(session.selection())==CodeSelection{5,1}&&
            take(take(session.view())->read({0,6},32))=="A한😀BC"&&!take(session.marked()),
            "Composition commit lost the text-service selection or its direction");
    require(take(session.set_input_selection(1,4))&&take(session.set_composition("😀")),"Could not start second candidate");
    require(take(session.set_input_selection(3,1))&&take(session.set_composition("😀",{},CodeDocumentRange{0,2}))&&
            take(session.selection())==CodeSelection{1,3},"Candidate update retained a stale reverse selection");
    require(take(session.set_input_selection(0,0)),"Could not move selection outside the candidate");
    (void)take(document->replace({.revision=1,.range={0,0},.text="external"}));
    auto stale=session.commit_composition();
    require(!stale&&stale.error().code==CodeDocumentErrorCode::stale&&take(session.selection())==CodeSelection{0,0}&&
            take(session.marked()).has_value()&&take(document->snapshot())->revision()==2,
            "A stale composition lost its text-service selection");
    require(take(session.cancel_composition())&&take(session.selection())==CodeSelection{1,4},
            "Canceling composition did not restore the original selection");
    auto reader=take(CodeEditorSession::create(document));
    require(take(reader.set_input_selection(1,0))&&take(reader.selection())==CodeSelection{1,0},
            "Read-only session rejected directional selection");
}
void input_service_cursor() {
    using namespace lvui;
    auto document=take(CodeMemoryDocument::create("A😀Z"));
    auto session=take(CodeEditorSession::create(document,document));
    require(take(session.set_selection(0,1)),"Could not set invalid-cursor fixture");
    auto invalid=session.set_input_composition("K",{},2);
    require(!invalid&&!take(session.marked())&&take(document->snapshot())->revision()==0&&
            take(session.selection())==CodeSelection{0,1},"A split cursor changed a code document");
    require(take(session.set_selection(1,3))&&take(session.set_input_composition("한",{},-1))&&
            take(session.selection())==CodeSelection{0,0},"A composition cursor did not move before its candidate");
    require(take(session.set_input_composition("하",{},2))&&take(session.selection())==CodeSelection{3,3},
            "A composition cursor did not move beyond its candidate");
    auto change=take(session.insert_input_text("B",{},2));
    require(change.revision==1&&take(session.selection())==CodeSelection{3,3}&&
            take(take(session.view())->read({0,3},16))=="ABZ"&&!take(session.marked()),
            "A committed code insertion lost its requested cursor");
    require(take(session.set_selection(0,1))&&take(session.set_input_composition("",{},1))&&
            take(take(session.view())->read({0,2},16))=="BZ"&&take(session.marked()).has_value(),
            "An empty input composition restored selected text");
    auto committed=take(session.commit_composition());
    require(committed&&committed->revision==2&&take(session.selection())==CodeSelection{0,0}&&
            take(take(session.view())->read({0,2},16))=="BZ","Empty code composition did not commit deletion");
}
void input_service_replacement_selection(){
    using namespace lvui;
    auto document=take(CodeMemoryDocument::create("A😀B한Z"));
    auto session=take(CodeEditorSession::create(document,document));
    require(take(session.set_selection(5,3)),"Could not select reverse deletion fixture");
    auto changed=take(session.insert_input_text_selection("B한",CodeDocumentRange{1,5},CodeSelection{3,1}));
    require(changed.revision==1&&take(session.selection())==CodeSelection{3,1}&&
            take(take(session.view())->read({0,3},16))=="AB한", "Replacement lost directional selection");
    auto next_document=take(CodeMemoryDocument::create("A😀Z"));
    auto next=take(CodeEditorSession::create(next_document,next_document));
    require(take(next.set_selection(0,1)),"Could not set invalid selection fixture");
    auto invalid=next.insert_input_text_selection("K",CodeDocumentRange{0,1},CodeSelection{2,2});
    require(!invalid&&take(next_document->snapshot())->revision()==0&&take(next.selection())==CodeSelection{0,1},
            "A scalar-splitting result selection partly committed an edit");
    auto large_document=take(CodeMemoryDocument::create("x"+std::string(100000,'q')+"z"));
    auto large=take(CodeEditorSession::create(large_document,large_document));
    require(take(large.set_selection(100001,1)),"Could not select the large document range");
    auto one_side=take(large.insert_input_text_selection("",CodeDocumentRange{0,1},CodeSelection{100000,0}));
    auto large_view=take(large.view());
    require(one_side.revision==1&&large_view->length()==100001&&take(large.selection())==CodeSelection{100000,0}&&
            take(large_view->read({0,1},4))=="q"&&take(large_view->read({100000,1},4))=="z",
            "One-sided deletion copied or lost a large selected range");
}
void input_service_replacement_during_composition(){
    using namespace lvui;const std::string original="abc";
    for(std::size_t begin=0;begin<=original.size();++begin)
    for(std::size_t end=begin;end<=original.size();++end)
    for(const std::string candidate:std::array<std::string,3>{"","x","yz"}){
        const auto visible=original.substr(0,begin)+candidate+original.substr(end);
        for(std::size_t first=0;first<=visible.size();++first)
        for(std::size_t last=first;last<=visible.size();++last)
        for(const std::string inserted:std::array<std::string,2>{"","K"}){
            auto document=take(CodeMemoryDocument::create(original));auto session=take(CodeEditorSession::create(document,document));
            take(session.set_selection(begin,end));take(session.set_input_composition(candidate,CodeDocumentRange{begin,end-begin},1));
            const auto result=take(session.insert_input_text(inserted,CodeDocumentRange{first,last-first},1));
            const auto expected=visible.substr(0,first)+inserted+visible.substr(last);const auto view=take(document->snapshot());
            require(!take(session.marked())&&take(session.selection())==CodeSelection{first+inserted.size(),first+inserted.size()}&&
                    view->length()==expected.size()&&take(view->read({0,expected.size()},32))==expected&&
                    result.revision==(expected==original?0:1),
                    "Projected replacement lost text, cursor, or its single revision");
        }
    }
    auto document=take(CodeMemoryDocument::create("A😀Z"));auto session=take(CodeEditorSession::create(document,document));
    take(session.set_selection(0,1));take(session.set_input_composition("한",{},1));
    auto invalid=session.insert_input_text("x",CodeDocumentRange{2,0},1);
    require(!invalid&&take(session.marked())->location==0&&take(document->snapshot())->revision()==0,
            "A replacement split an emoji or committed its candidate");
    invalid=session.insert_input_text("K",CodeDocumentRange{0,1},2);
    require(!invalid&&take(session.marked())->location==0&&take(document->snapshot())->revision()==0,
            "A cursor that splits an emoji partially committed a projected edit");
    invalid=session.insert_input_text_selection("x",CodeDocumentRange{0,0},CodeSelection{3,3});
    require(!invalid&&take(session.marked())->location==0&&take(document->snapshot())->revision()==0,
            "An invalid result selection partially committed a projected edit");
    (void)take(document->replace({.revision=0,.range={0,0},.text="external"}));
    auto stale=session.insert_input_text("x",CodeDocumentRange{0,1},1);
    require(!stale&&stale.error().code==CodeDocumentErrorCode::stale&&take(session.marked())->location==0&&
            take(document->snapshot())->revision()==1,"A stale projected replacement lost its candidate");

    auto reverse_document=take(CodeMemoryDocument::create("ABCDE"));auto reverse=take(CodeEditorSession::create(reverse_document,reverse_document));
    take(reverse.set_selection(1,2));take(reverse.set_input_composition("한글",{},1));
    auto selected=take(reverse.insert_input_text_selection("Q",CodeDocumentRange{4,1},CodeSelection{5,1}));
    require(selected.revision==1&&take(reverse.selection())==CodeSelection{5,1}&&
            take(take(reverse_document->snapshot())->read({0,6},32))=="A한글CQE",
            "Projected replacement did not preserve a directional result selection");
    for(const auto [offset,caret]:std::array<std::pair<std::int32_t,std::size_t>,4>{{{-1,3},{0,4},{1,5},{2,6}}}){
        auto source=take(CodeMemoryDocument::create("ABCDE"));auto positioned=take(CodeEditorSession::create(source,source));
        take(positioned.set_selection(1,2));take(positioned.set_input_composition("한글",{},1));
        take(positioned.insert_input_text("Q",CodeDocumentRange{4,1},offset));
        require(take(positioned.selection())==CodeSelection{caret,caret},
                "Projected replacement computed the cursor from the merged source range");
    }

    auto large_document=take(CodeMemoryDocument::create("A"+std::string(70000,'q')+"Z"));
    auto large=take(CodeEditorSession::create(large_document,large_document));
    take(large.set_selection(0,1));take(large.set_input_composition("한",{},1));
    auto too_large=large.insert_input_text("X",CodeDocumentRange{70001,1},1);
    require(!too_large&&too_large.error().code==CodeDocumentErrorCode::too_large&&
            take(large.marked())->location==0&&take(large_document->snapshot())->revision()==0,
            "An oversized preserved interval changed the code source");
    auto spanning=take(large.insert_input_text("X",CodeDocumentRange{0,70002},1));
    require(spanning.revision==1&&take(take(large_document->snapshot())->read({0,1},4))=="X",
            "Replacing a large selected range unnecessarily copied it");

    auto short_document=take(CodeMemoryDocument::create("abc"));auto short_source=std::make_shared<ShortSource>();
    short_source->document=short_document;auto short_session=take(CodeEditorSession::create(short_source,short_document));
    take(short_session.set_selection(0,1));take(short_session.set_input_composition("x",{},1));
    auto partial=short_session.insert_input_text("K",CodeDocumentRange{2,1},1);
    require(!partial&&take(short_session.marked())->location==0&&take(short_document->snapshot())->revision()==0,
            "A partial source read lost unchanged projected text");
}
void input_service_composition_region(){
    using namespace lvui;
    auto document=take(CodeMemoryDocument::create("A😀Z"));
    auto session=take(CodeEditorSession::create(document,document));take(session.set_selection(3,1));
    auto marked=take(session.set_input_composition_region({1,2}));
    require(marked.updated&&!marked.committed&&take(session.marked())->location==1&&take(session.marked())->length==2&&
            take(session.selection())==CodeSelection{3,1}&&take(document->snapshot())->revision()==0,
            "Marking existing code changed its text or selection");
    require(!session.set_input_composition_region({2,1})&&take(session.marked())->location==1&&
            take(session.selection())==CodeSelection{3,1}&&take(document->snapshot())->revision()==0,
            "A split region damaged the existing code candidate");
    auto empty=take(session.set_input_composition_region({2,0}));
    require(empty.updated&&!take(session.marked())&&take(session.selection())==CodeSelection{3,1},
            "An empty region inside a surrogate failed to finish code composition");
    take(session.set_input_composition_region({1,2}));
    take(session.set_input_composition("한",{},1));take(session.set_input_selection(3,0));
    auto retained=take(session.view());
    auto moved=take(session.set_input_composition_region({0,2}));
    require(moved.updated&&moved.committed&&moved.committed->revision==1&&take(session.selection())==CodeSelection{3,0}&&
            take(session.marked())->location==0&&take(session.marked())->length==2&&
            take(take(session.view())->read({0,3},16))=="A한Z"&&take(document->snapshot())->revision()==1,
            "Moving a region failed to commit the prior code composition");
    require(retained->revision()==0&&take(retained->read({0,3},16))=="A한Z",
            "Moving a region changed a retained composition snapshot");
    auto changed=take(session.insert_input_text("K",{},1));
    require(changed.revision==2&&take(take(session.view())->read({0,2},16))=="KZ"&&
            take(session.selection())==CodeSelection{1,1},"Newly marked code did not replace correctly");

    auto stale_document=take(CodeMemoryDocument::create("A😀Z"));
    auto stale=take(CodeEditorSession::create(stale_document,stale_document));
    take(stale.set_selection(1,3));take(stale.set_input_composition("한",{},1));take(stale.set_input_selection(3,0));
    (void)take(stale_document->replace({.revision=0,.range={0,0},.text="external"}));
    auto failed=stale.set_input_composition_region({0,2});
    require(!failed&&failed.error().code==CodeDocumentErrorCode::stale&&take(stale.marked())->location==1&&
            take(stale.selection())==CodeSelection{3,0}&&take(take(stale.view())->read({0,3},16))=="A한Z",
            "A stale region update lost the candidate or selection");
    auto large_document=take(CodeMemoryDocument::create(std::string(65537,'a')));
    auto large=take(CodeEditorSession::create(large_document,large_document));
    require(!large.set_input_composition_region({0,65537})&&take(large_document->snapshot())->revision()==0&&
            !take(large.marked()),"An oversized code region changed the source");
    auto broken_document=take(CodeMemoryDocument::create("short"));auto broken_source=std::make_shared<ShortSource>();
    broken_source->document=broken_document;auto broken=take(CodeEditorSession::create(broken_source,broken_document));
    require(!broken.set_input_composition_region({0,2})&&!take(broken.marked())&&take(broken_document->snapshot())->revision()==0,
            "A short source read changed the composing region");
}
void input_service_composing_deletion(){
    using namespace lvui;
    auto document=take(CodeMemoryDocument::create("A😀xy한Z"));
    auto session=take(CodeEditorSession::create(document,document));take(session.set_selection(5,4));
    take(session.set_input_composition_region({3,3}));auto old=take(session.view());
    require(!session.erase_input_surrounding({2,4})&&take(session.marked())->location==3&&take(session.marked())->length==3&&
            take(document->snapshot())->revision()==0,"A split deletion changed a code composition");
    auto result=take(session.erase_input_surrounding({1,5}));
    require(result.updated&&result.committed&&result.committed->revision==1&&take(session.marked())->location==1&&take(session.marked())->length==1&&
            take(session.selection())==CodeSelection{2,1}&&take(take(session.view())->read({0,3},16))=="AyZ"&&
            take(take(document->snapshot())->read({0,5},16))=="Axy한Z",
            "Composing deletion did not rebase the code candidate and source");
    require(take(old->read({0,7},32))=="A😀xy한Z"&&old->revision()==0,"The retained code view changed");
    auto committed=take(session.commit_composition());
    require(committed&&committed->revision==2&&take(take(document->snapshot())->read({0,3},16))=="AyZ",
            "The shortened code candidate did not commit");

    auto stale_document=take(CodeMemoryDocument::create("AxZ"));
    auto stale=take(CodeEditorSession::create(stale_document,stale_document));take(stale.set_selection(2,2));
    take(stale.set_input_composition_region({1,1}));
    (void)take(stale_document->replace({.revision=0,.range={0,0},.text="external"}));
    const auto failed_candidate=stale.erase_input_surrounding({1,1});
    require(!failed_candidate&&failed_candidate.error().code==CodeDocumentErrorCode::stale&&
            take(stale.marked())->location==1&&take(stale.marked())->length==1,
            "A stale candidate-only deletion changed its code composition");
    auto failed=stale.erase_input_surrounding({0,2});
    require(!failed&&failed.error().code==CodeDocumentErrorCode::stale&&take(stale.marked())->location==1&&take(stale.marked())->length==1&&
            take(take(stale.view())->read({0,3},16))=="AxZ"&&take(stale.selection())==CodeSelection{2,2},
            "A stale surrounding deletion lost its code candidate or selection");

    auto large_document=take(CodeMemoryDocument::create("x"+std::string(100000,'q')+"z"));
    auto large=take(CodeEditorSession::create(large_document,large_document));take(large.set_selection(100001,1));
    take(large.set_input_composition_region({50000,1}));
    const auto too_large=large.erase_input_surrounding({0,100002});
    require(!too_large&&take(large.selection())==CodeSelection{100001,1}&&
            take(large.marked())->location==50000&&take(large_document->snapshot())->revision()==0,
            "An oversized retained range changed a composing edit");
    auto one_side=take(large.erase_input_surrounding({0,100001}));
    require(one_side.updated&&one_side.committed&&take(large.selection())==CodeSelection{100000,0}&&
            take(large.marked())->location==49999&&take(large_document->snapshot())->length()==100001,
            "A one-sided composing deletion copied or lost a large selection");

    auto short_document=take(CodeMemoryDocument::create("AxZ"));auto short_source=std::make_shared<ShortSource>();
    short_source->document=short_document;auto short_session=take(CodeEditorSession::create(short_source,short_document));
    take(short_session.set_selection(2,2));take(short_session.set_input_composition("y",CodeDocumentRange{1,1},1));
    const auto short_result=short_session.erase_input_surrounding({0,3});
    require(!short_result&&take(short_document->snapshot())->revision()==0&&take(short_session.marked())->location==1&&
            take(short_session.selection())==CodeSelection{2,2},"A partial source read changed the composing edit");
}
void input_service_composing_deletion_matrix(){
    using namespace lvui;const std::string original="abc";
    for(std::size_t begin=0;begin<=original.size();++begin)
    for(std::size_t end=begin;end<=std::min(original.size(),begin+2);++end)
    for(const std::string candidate:std::array<std::string,3>{"","x","yz"}){
        const auto visible=original.substr(0,begin)+candidate+original.substr(end);const auto marked_end=begin+candidate.size();
        for(std::size_t left=0;left<=visible.size();++left)
        for(std::size_t right=left;right<=visible.size();++right)
        for(std::size_t first=0;first<=left;++first)
        for(std::size_t last=right;last<=visible.size();++last){
            auto document=take(CodeMemoryDocument::create(original));auto session=take(CodeEditorSession::create(document,document));
            take(session.set_selection(begin,end));take(session.set_input_composition(candidate,CodeDocumentRange{begin,end-begin},1));
            take(session.set_input_selection(right,left));
            auto changed=take(session.erase_input_surrounding({first,last-first}));
            const auto expected=visible.substr(0,first)+visible.substr(left,right-left)+visible.substr(last);
            std::vector<bool> removed(original.size(),false);
            for(std::size_t i=0;i<visible.size();++i)if((i>=first&&i<left)||(i>=right&&i<last)){
                if(i<begin)removed[i]=true;else if(i>=marked_end)removed[end+i-marked_end]=true;
            }
            std::string base;for(std::size_t i=0;i<original.size();++i)if(!removed[i])base+=original[i];
            const auto map=[&](std::size_t p){return p-(p>first?std::min(p,left)-first:0)-(p>right?std::min(p,last)-right:0);};
            const auto marked=take(session.marked());const auto source=take(document->snapshot());const auto displayed=take(session.view());
            require(changed.updated==(first!=left||right!=last)&&bool(changed.committed)==(base!=original)&&
                    source->revision()==(base!=original?1:0)&&take(source->read({0,base.size()},32))==base&&
                    take(displayed->read({0,expected.size()},32))==expected&&marked&&marked->location==map(begin)&&
                    marked->length==map(marked_end)-map(begin)&&take(session.selection())==CodeSelection{first+(right-left),first},
                    "Code composition deletion matrix disagreed with the reference edit");
            take(session.cancel_composition());auto canceled=take(session.view());
            require(take(canceled->read({0,base.size()},32))==base,"Cancel restored deleted code surroundings");
        }
    }
}
}
int main() {
    using namespace lvui;
    {
        auto source=take(CodeMemoryDocument::create("A😀Z"));auto active=take(CodeEditorSession::create(source,source));
        take(active.set_selection(1,3));
        require(take(active.set_composition("한😀",{},CodeDocumentRange{1,2},{},true))&&
                take(active.selection())==CodeSelection{4,2},"Code editor lost the reversed composition selection");
        require(!take(active.set_composition("한😀",{},CodeDocumentRange{1,2},{},true)),
                "An unchanged reversed code candidate changed state");
        require(!active.set_composition("한😀",{},CodeDocumentRange{2,1},{},true)&&
                take(active.selection())==CodeSelection{4,2},"Invalid reversed code selection changed state");
    }
    auto document = take(CodeMemoryDocument::create("A😀B"));
    auto readonly = take(CodeEditorSession::create(document));
    require(!readonly.set_composition("x") && !readonly.insert("x"), "Read-only session accepted editing");
    auto session = take(CodeEditorSession::create(document, document));
    require(take(session.set_selection(1, 3)) && take(session.set_composition("ㅎ")) &&
            take(session.view())->length() == 3 && take(take(session.view())->read({0, 3}, 16)) == "AㅎB" &&
            take(session.selection()) == CodeSelection{2, 2} && take(document->snapshot())->revision() == 0,
            "Composition did not preserve the source and project its selection");
    require(!session.set_selection(0, 0) && !session.move(CodeMove::next_scalar) &&
            !session.set_composition("x", CodeDocumentRange{0, 1}), "An unrelated action changed an active composition");
    require(take(session.set_composition("한", CodeDocumentRange{1, 1}, CodeDocumentRange{0, 1})) &&
            take(session.selection()) == CodeSelection{1, 2} && take(session.marked())->location == 1 &&
            take(document->snapshot())->revision() == 0, "Composition update did not replace the prior marked text");
    auto committed = take(session.commit_composition());
    require(committed && committed->revision == 1 && !take(session.marked()) &&
            take(take(session.view())->read({0, 3}, 16)) == "A한B", "Composition did not commit exactly once");
    require(!take(session.commit_composition()), "An absent composition committed again");
    require(!take(session.set_selection(1, 2)) && take(session.set_composition("😀")), "Could not start a second composition");
    (void)take(document->replace({.revision=1, .range={0, 0}, .text="external"}));
    auto stale = session.commit_composition();
    require(!stale && stale.error().code == CodeDocumentErrorCode::stale && take(session.marked()).has_value() &&
            take(document->snapshot())->revision() == 2, "A stale composition overwrote the source or discarded user input");
    require(take(session.cancel_composition()) && take(session.refresh()) && !take(session.marked()),
            "A cancelled stale composition did not allow refresh");
    require(take(session.set_composition("x")) && take(session.set_composition("")) && !take(session.marked()),
            "An empty marked string did not cancel composition");
    require(take(session.insert("!", CodeDocumentRange{0, 0})).revision == 3 && take(session.erase(CodeErase::previous_scalar)).has_value(),
            "Session text replacement or deletion did not reach its editor");
    bool wrong_thread = false; std::thread worker([&] {
        auto view = session.view(); auto selection=session.set_input_selection(0,0);
        wrong_thread = !view && view.error().code == CodeDocumentErrorCode::wrong_thread &&
            !selection && selection.error().code == CodeDocumentErrorCode::wrong_thread;
    });
    worker.join(); require(wrong_thread, "Session accepted a foreign-thread view request");
    auto editor = std::make_shared<ReentrantEditor>(); editor->document = document;
    auto guarded = take(CodeEditorSession::create(document, editor)); editor->session = &guarded;
    (void)take(guarded.insert("x")); require(editor->guarded, "A reentrant editor changed the composition during commit");
    input_service_selection();
    input_service_cursor();
    input_service_replacement_selection();
    input_service_replacement_during_composition();
    input_service_composition_region();
    input_service_composing_deletion();
    input_service_composing_deletion_matrix();
}
