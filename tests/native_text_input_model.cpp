#include <LVUI/TextInput.h>
#include <iiGUI/TextInputModel.h>
namespace lvui {using TextInput=TextInputBinding<ii::gui::BasicTextInput>;} 
#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(lvui::TextInputResult<T> result) {
    if (!result) throw std::runtime_error(result.error().message);
    return std::move(*result);
}

// A small deterministic boundary service for the model tests. It is deliberately
// separate from production Unicode segmentation; the host supplies that service.
class Boundary : public lvui::TextInputBoundary {
    std::vector<std::size_t> bytes_{0};
    std::vector<std::pair<std::size_t,std::size_t>> clusters_;
public:
    explicit Boundary(std::string_view text) {
        std::size_t byte = 0, unit = 0;
        while (byte < text.size()) {
            const auto lead = static_cast<unsigned char>(text[byte]);
            const std::size_t count = lead < 0x80 ? 1 : lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
            char32_t scalar = lead < 0x80 ? lead : lead & ((1u << (7 - count)) - 1);
            for (std::size_t i = 1; i < count; ++i)
                scalar = (scalar << 6) | (static_cast<unsigned char>(text[byte + i]) & 0x3f);
            const auto first = unit;
            if (scalar > 0xffff) { bytes_.push_back(invalid); ++unit; }
            byte += count; ++unit; bytes_.push_back(byte);
            if (scalar == 0x301 && !clusters_.empty()) clusters_.back().second = unit;
            else clusters_.push_back({first,unit});
        }
    }
    static constexpr std::size_t invalid = std::numeric_limits<std::size_t>::max();
    std::size_t length() const noexcept override { return bytes_.size() - 1; }
    std::optional<std::size_t> byte_offset(std::size_t unit) const override {
        if (unit >= bytes_.size() || bytes_[unit] == invalid) return std::nullopt;
        return bytes_[unit];
    }
    std::size_t previous_grapheme(std::size_t unit) const noexcept override {
        for (auto it = clusters_.rbegin(); it != clusters_.rend(); ++it)
            if (it->first < unit) return it->first;
        return 0;
    }
    std::size_t next_grapheme(std::size_t unit) const noexcept override {
        for (auto [first,last] : clusters_) if (last > unit) return last;
        return length();
    }
    std::size_t previous_word(std::size_t unit) const noexcept override { return previous_grapheme(unit); }
    std::size_t next_word(std::size_t unit) const noexcept override { return next_grapheme(unit); }
};
class InvalidWordBoundary final : public Boundary {
public:
    using Boundary::Boundary;
    std::optional<lvui::TextInputRange> word_range(std::size_t) const noexcept override { return lvui::TextInputRange{99,1}; }
};
class SplitWordBoundary final : public Boundary {
public:
    using Boundary::Boundary;
    std::optional<lvui::TextInputRange> word_range(std::size_t) const noexcept override { return lvui::TextInputRange{4,1}; }
};
class ShortBoundary final : public Boundary {
public:
    using Boundary::Boundary;
    std::size_t length() const noexcept override { return Boundary::length()-1; }
    std::optional<std::size_t> byte_offset(std::size_t unit) const override {
        return Boundary::byte_offset(unit==length()?Boundary::length():unit);
    }
};

lvui::TextInputResult<std::shared_ptr<const lvui::TextInputBoundary>> index(std::string_view text) {
    return std::shared_ptr<const lvui::TextInputBoundary>(std::make_shared<Boundary>(text));
}

void unicode_and_ranges() {
    auto text = take(lvui::TextInput::create(index, "A한😀e\xcc\x81Z"));
    require(text.length() == 7 && text.caret() == 7, "UTF-16 length or initial caret is wrong");
    require(!text.set_selection(3,3) && text.caret() == 7, "Selection split a surrogate pair");
    const auto unavailable=text.word_range(1);
    require(!unavailable && unavailable.error().code==lvui::TextInputErrorCode::unavailable && text.caret()==7,
            "An optional word boundary service changed editing state");
    require(!text.word_range(3), "Word lookup accepted a position inside a surrogate pair");
    auto invalid_factory=[](std::string_view value)->lvui::TextInputResult<std::shared_ptr<const lvui::TextInputBoundary>>{
        return std::shared_ptr<const lvui::TextInputBoundary>(std::make_shared<InvalidWordBoundary>(value));
    };
    auto invalid=take(lvui::TextInput::create(invalid_factory,"abc"));
    const auto invalid_range=invalid.word_range(1);
    require(!invalid_range && invalid_range.error().code==lvui::TextInputErrorCode::invalid_boundary && invalid.caret()==3,
            "A malformed word boundary was accepted or changed editing state");
    auto split_factory=[](std::string_view value)->lvui::TextInputResult<std::shared_ptr<const lvui::TextInputBoundary>>{
        return std::shared_ptr<const lvui::TextInputBoundary>(std::make_shared<SplitWordBoundary>(value));
    };
    auto split=take(lvui::TextInput::create(split_factory,"A한😀e\xcc\x81Z"));
    require(!split.word_range(4),"A word boundary split a combining grapheme");
    take(text.set_selection(2,4));
    require(take(text.read(text.selection())) == "😀", "UTF-16 selection read split text");
    require(!text.read({std::numeric_limits<std::size_t>::max(),1}), "Overflowing range was accepted");
    take(text.set_selection(7,7));
    take(text.move(-1,false));
    require(text.caret() == 6, "Caret did not move across the last character");
    take(text.move(-1,false));
    require(text.caret() == 4, "Caret split a combining grapheme");
    require(take(text.erase(-1)) && text.text() == "A한e\xcc\x81Z", "Backspace did not erase an entire emoji");
    take(text.set_selection(2,4));
    take(text.move(-1,false));
    require(text.caret() == 2 && text.anchor() == 2, "Arrow movement did not collapse selection");
    take(text.move(1,true));
    require(text.selection() == lvui::TextInputRange{2,2}, "Shift selection split a grapheme");
}

void composition() {
    auto text = take(lvui::TextInput::create(index, "A😀Z"));
    take(text.set_selection(1,3));
    take(text.set_composition("ㅎ"));
    require(text.text() == "AㅎZ" && text.marked() == lvui::TextInputRange{1,1} && text.caret() == 2,
            "Initial Korean composition did not replace the selection");
    take(text.set_composition("하"));
    take(text.set_composition("한"));
    require(text.text() == "A한Z" && text.marked() == lvui::TextInputRange{1,1},
            "Composition update duplicated marked text");
    const auto before = text.text();
    require(!text.set_composition("😀",std::nullopt,lvui::TextInputRange{1,0}) && text.text() == before,
            "Invalid composition selection changed text");
    require(take(text.cancel_composition()) && text.text() == "A😀Z" && text.selection() == lvui::TextInputRange{1,2},
            "Composition cancellation failed to restore the original selection and text");
    take(text.set_composition("하"));
    require(take(text.insert("한")) && text.text() == "A한Z" && !text.marked(),
            "Committing text did not replace marked text");
    take(text.set_composition("글"));
    require(take(text.commit_composition()) && text.text() == "A한글Z" && !text.marked(),
            "Unmarking failed to commit the composition");
    require(!take(text.commit_composition()), "Committing absent marked text reported a change");
}
void reversed_composition(){
    using namespace lvui;auto text=take(TextInput::create(index,"A😀Z"));take(text.set_selection(1,3));
    require(take(text.set_composition("한😀",{},TextInputRange{1,2},true))&&text.anchor()==4&&text.caret()==2,
            "Text input lost the reversed composition selection");
    require(!take(text.set_composition("한😀",{},TextInputRange{1,2},true)),"An unchanged reversed candidate changed state");
    require(!text.set_composition("한😀",{},TextInputRange{2,1},true)&&text.anchor()==4&&text.caret()==2,
            "Invalid reversed composition changed the editor");
}

void composition_region(){
    using namespace lvui;
    auto text=take(TextInput::create(index,"A😀Z"));
    take(text.set_selection(3,1));
    require(take(text.set_composition_region({1,2}))&&text.text()=="A😀Z"&&text.marked()==TextInputRange{1,2}&&
            text.anchor()==3&&text.caret()==1,"Marking existing text changed content or selection");
    const auto old_mark=text.marked();
    require(!text.set_composition_region({2,1})&&text.text()=="A😀Z"&&text.marked()==old_mark&&
            text.anchor()==3&&text.caret()==1,"An invalid region changed existing composition");
    require(take(text.set_composition_region({2,0}))&&!text.marked()&&text.anchor()==3&&text.caret()==1,
            "An empty region inside a surrogate failed to finish composition");
    take(text.set_composition_region({1,2}));
    take(text.set_composition("한"));take(text.set_selection(3,0));
    require(take(text.set_composition_region({0,2}))&&text.text()=="A한Z"&&text.committed_text()=="A한Z"&&
            text.marked()==TextInputRange{0,2}&&text.anchor()==3&&text.caret()==0,
            "Replacing a composition region lost the previous candidate or selection");
    take(text.insert("K"));
    require(text.text()=="KZ"&&take(text.undo())&&text.text()=="A한Z"&&text.anchor()==3&&text.caret()==0&&
            take(text.undo())&&text.text()=="A😀Z", "Composition-region changes lost undo history");
    auto bounded=take(TextInput::create(index,"A"));
    take(bounded.set_maximum_length(1,TextLengthUnit::unicode_scalars));take(bounded.set_composition("한글"));
    const auto before=bounded.text();const auto marked=bounded.marked();const auto anchor=bounded.anchor(),caret=bounded.caret();
    require(!bounded.set_composition_region({0,0})&&bounded.text()==before&&bounded.marked()==marked&&
            bounded.anchor()==anchor&&bounded.caret()==caret,
            "A clipping composition finish partially changed a field");
}

void transactions() {
    auto text = take(lvui::TextInput::create(index,"safe"));
    take(text.set_selection(1,3));
    const auto selection = text.selection();
    require(!text.insert(std::string("\xff",1)) && text.text() == "safe" && text.selection() == selection,
            "Invalid UTF-8 changed the document");
    require(!text.insert(std::string(lvui::TextInput::maximum_bytes + 1,'x')) && text.text() == "safe",
            "Oversized text changed the document");
    require(!text.insert("x",lvui::TextInputRange{100,1}) && text.text() == "safe",
            "Invalid replacement range changed the document");
    require(take(text.insert("OK")) && text.text() == "sOKe" && text.caret() == 3,
            "Selection replacement failed");
    require(!take(text.set_text("sOKe")) && text.caret() == 3, "Unchanged program text disturbed selection");
    take(text.set_text("new"));
    require(text.text() == "new" && text.caret() == 3, "Program text did not reset the caret");
    auto unavailable=[](std::string_view)->lvui::TextInputResult<std::shared_ptr<const lvui::TextInputBoundary>> {
        return std::unexpected(lvui::TextInputError{lvui::TextInputErrorCode::unavailable,"missing boundary service"});
    };
    require(!lvui::TextInput::create(unavailable,"x"), "A failed boundary service produced a field");
}

void history() {
    auto text = take(lvui::TextInput::create(index, "A😀Z"));
    take(text.set_selection(1,3));
    take(text.set_composition("ㅎ"));
    take(text.set_composition("하"));
    take(text.insert("한"));
    require(take(text.undo()) && text.text()=="A😀Z" && text.selection()==lvui::TextInputRange{1,2},
            "Undo did not restore the pre-composition text and selection");
    require(take(text.redo()) && text.text()=="A한Z" && text.caret()==2,
            "Redo did not restore the committed composition");
    take(text.insert("글"));
    require(take(text.undo()) && text.text()=="A한Z", "Undo did not restore the previous edit");
    take(text.insert("x"));
    require(!take(text.redo()) && text.text()=="A한xZ", "A new edit failed to clear redo history");
    take(text.set_composition("ㅎ"));
    require(take(text.undo()) && text.text()=="A한xZ" && !text.marked(),
            "Undo did not first cancel an uncommitted composition");
    require(take(text.undo()) && text.text()=="A한Z", "Canceling composition lost prior undo history");
    take(text.set_text("external"));
    require(!take(text.undo()) && !take(text.redo()), "External replacement retained stale history");
    text.set_history_enabled(false);
    take(text.insert("!"));
    require(!take(text.undo()), "Disabled history retained a sensitive edit");
    text.set_history_enabled(true);
    for (int i=0;i<100;++i) take(text.insert("a"));
    int count=0; while(take(text.undo())) ++count;
    require(count==64, "Input history did not honor its bounded entry count");

    auto reject = std::make_shared<bool>(false);
    auto fallible = [reject](std::string_view value) -> lvui::TextInputResult<std::shared_ptr<const lvui::TextInputBoundary>> {
        if (*reject) return std::unexpected(lvui::TextInputError{lvui::TextInputErrorCode::unavailable,"temporarily unavailable"});
        return index(value);
    };
    auto candidate = take(lvui::TextInput::create(fallible,"old"));
    take(candidate.insert("!"));
    *reject = true;
    require(!candidate.undo() && candidate.text()=="old!", "A failed history restore changed the text");
    *reject = false;
    require(take(candidate.undo()) && candidate.text()=="old", "A failed history restore consumed its entry");

    auto large = take(lvui::TextInput::create(index,std::string(lvui::TextInput::maximum_bytes,'a')));
    for (int i=0;i<40;++i) {
        take(large.set_selection(0,large.length()));
        take(large.insert(std::string(lvui::TextInput::maximum_bytes,i%2?'a':'b')));
    }
    count=0; while(take(large.undo())) ++count;
    require(count==32, "Input history did not honor its retained text byte budget");
}

void maximum_length() {
    using lvui::TextLengthUnit;
    auto text=take(lvui::TextInput::create(index,"A😀Z"));
    take(text.set_maximum_length(3,TextLengthUnit::utf16_code_units));
    require(text.text()=="A😀" && text.caret()==3,"Initial UTF-16 limit split or retained an extra scalar");
    take(text.set_selection(1,3));
    require(take(text.insert("한글W")) && text.text()=="A한글", "Replacing a selection did not reuse its capacity");
    require(!take(text.insert("X")) && text.text()=="A한글", "A full field accepted extra input");
    require(take(text.undo()) && text.text()=="A😀", "Undo lost the bounded replacement");
    require(take(text.redo()) && text.text()=="A한글", "Redo lost the bounded replacement");
    take(text.set_maximum_length(2,TextLengthUnit::utf16_code_units));
    require(text.text()=="A한" && !take(text.undo()) && !take(text.redo()),
            "Reducing the limit left overlong history");
    take(text.set_text("😀X"));
    require(text.text()=="😀", "External text was not truncated at a valid scalar boundary");
    take(text.set_maximum_length(1,TextLengthUnit::utf16_code_units));
    require(text.text().empty(), "UTF-16 limit split a supplementary scalar");

    take(text.set_maximum_length(3,TextLengthUnit::unicode_scalars));
    take(text.set_text("A😀ZQ"));
    require(text.text()=="A😀Z", "Scalar limit counted a supplementary scalar twice");
    take(text.set_selection(1,3));
    take(text.insert("한글"));
    require(text.text()=="A한Z", "Scalar limit failed when replacing a selection");
    require(take(text.undo()) && text.text()=="A😀Z", "Scalar-bounded undo failed");
    take(text.set_maximum_length(3,TextLengthUnit::utf16_code_units));
    take(text.set_text("A"));
    take(text.set_composition("😀한글"));
    require(text.text()=="A😀한글" && text.committed_text()=="A", "Preedit was truncated or committed prematurely");
    require(take(text.commit_composition()) && text.text()=="A😀", "Composition commit exceeded the UTF-16 limit");
    require(take(text.undo()) && text.text()=="A", "Undo failed after bounded composition commit");
    take(text.set_composition("한글"));
    take(text.set_maximum_length(1,TextLengthUnit::utf16_code_units));
    require(text.text()=="A" && !text.marked(), "Changing a limit failed to cancel preedit");
    take(text.set_maximum_length(std::nullopt,TextLengthUnit::unicode_scalars));
    take(text.insert("😀Z"));
    require(text.text()=="A😀Z", "Removing a limit did not restore capacity");
    take(text.set_maximum_length(0,TextLengthUnit::unicode_scalars));
    require(text.text().empty() && !take(text.insert("x")), "Zero limit did not keep the field empty");
    take(text.set_maximum_length(2,TextLengthUnit::unicode_scalars));
    take(text.set_text("A"));take(text.set_composition("한글"));
    require(take(text.insert("한글")) && text.text()=="A한", "Text-delivered IME commit was not limited");
    require(take(text.undo()) && text.text()=="A", "Text-delivered IME commit lost undo");
    take(text.set_composition("😀한"));
    take(text.insert("Q",lvui::TextInputRange{0,1}));
    require(text.text()=="😀한" && text.caret()==0, "Commit outside preedit violated the limit or caret range");
    auto poison=std::make_shared<bool>(false);
    auto unreliable=[poison](std::string_view value)->lvui::TextInputResult<std::shared_ptr<const lvui::TextInputBoundary>> {
        if(*poison&&value=="B😀한")return std::shared_ptr<const lvui::TextInputBoundary>(std::make_shared<ShortBoundary>(value));
        return index(value);
    };
    auto guarded=take(lvui::TextInput::create(unreliable,"AB"));
    take(guarded.set_maximum_length(3,TextLengthUnit::unicode_scalars));
    take(guarded.set_composition("😀한글"));const auto marked=guarded.marked();
    *poison=true;
    const auto invalid=guarded.insert("Q",lvui::TextInputRange{0,1});
    require(!invalid&&invalid.error().code==lvui::TextInputErrorCode::invalid_boundary&&
            guarded.text()=="AB😀한글"&&guarded.marked()==marked,
            "A malformed boundary after clipping changed the transaction");
    const auto before=text.text();
    require(!text.insert(std::string("\xff",1)) && text.text()==before,
            "Invalid UTF-8 changed a bounded input");
}
void composing_surrounding_deletion() {
    using namespace lvui;
    auto value=take(TextInput::create(index,"A😀xy한Z"));
    take(value.set_selection(5,4));take(value.set_composition_region({3,3}));
    require(!value.erase_surrounding({2,4})&&value.text()=="A😀xy한Z"&&value.marked()==TextInputRange{3,3},
            "A scalar-splitting composing deletion changed the field");
    require(take(value.erase_surrounding({1,5}))&&value.text()=="AyZ"&&value.committed_text()=="Axy한Z"&&
            value.marked()==TextInputRange{1,1}&&value.anchor()==2&&value.caret()==1,
            "Composing deletion did not preserve selected text and rebase the candidate");
    require(take(value.commit_composition())&&value.text()=="AyZ", "Shortened field candidate failed to commit");
    require(take(value.undo())&&value.text()=="Axy한Z"&&take(value.undo())&&value.text()=="A😀xy한Z",
            "Undo lost a composing or surrounding edit");
    require(take(value.redo())&&value.text()=="Axy한Z"&&take(value.redo())&&value.text()=="AyZ",
            "Redo lost a composing or surrounding edit");

    auto candidate=take(TextInput::create(index,"AxZ"));take(candidate.set_selection(2,2));
    take(candidate.set_composition_region({1,1}));take(candidate.erase_surrounding({1,1}));
    require(candidate.text()=="AZ"&&candidate.committed_text()=="AxZ"&&candidate.marked()==TextInputRange{1,0},
            "Deleting the complete candidate lost its empty composing state");
    require(take(candidate.undo())&&candidate.text()=="AxZ"&&!candidate.marked(),
            "Canceling the empty composition lost the original text");
}
void composing_surrounding_matrix() {
    using namespace lvui;const std::string original="abc";
    for(std::size_t begin=0;begin<=original.size();++begin)
    for(std::size_t end=begin;end<=std::min(original.size(),begin+2);++end)
    for(const std::string candidate:std::array<std::string,3>{"","x","yz"}){
        auto initial=take(TextInput::create(index,original));take(initial.set_selection(begin,end));
        take(initial.set_composition(candidate,TextInputRange{begin,end-begin}));
        const auto visible=original.substr(0,begin)+candidate+original.substr(end);const auto marked_end=begin+candidate.size();
        for(std::size_t left=0;left<=visible.size();++left)
        for(std::size_t right=left;right<=visible.size();++right)
        for(std::size_t first=0;first<=left;++first)
        for(std::size_t last=right;last<=visible.size();++last){
            auto value=initial;take(value.set_selection(right,left));
            const auto result=take(value.erase_surrounding({first,last-first}));
            const auto expected=visible.substr(0,first)+visible.substr(left,right-left)+visible.substr(last);
            std::vector<bool> removed(original.size(),false);
            for(std::size_t i=0;i<visible.size();++i)if((i>=first&&i<left)||(i>=right&&i<last)){
                if(i<begin)removed[i]=true;
                else if(i>=marked_end)removed[end+i-marked_end]=true;
            }
            std::string base;for(std::size_t i=0;i<original.size();++i)if(!removed[i])base+=original[i];
            const auto map=[&](std::size_t p){
                const auto a=p>first?std::min(p,left)-first:0;
                const auto b=p>right?std::min(p,last)-right:0;
                return p-a-b;
            };
            const auto marked=TextInputRange{map(begin),map(marked_end)-map(begin)};
            require(result==(first!=left||right!=last)&&value.text()==expected&&value.committed_text()==base&&
                    value.marked()==marked&&value.anchor()==first+(right-left)&&value.caret()==first,
                    "Composition and selection deletion matrix disagreed with the reference edit");
            auto canceled=value;take(canceled.cancel_composition());
            require(canceled.text()==base&&!canceled.marked(),"Cancel restored deleted surrounding text");
            take(value.commit_composition());
            require(value.text()==expected&&!value.marked(),"Finishing a matrix candidate changed its displayed text");
        }
    }
}
}
int main() { unicode_and_ranges(); composition(); reversed_composition(); composition_region(); composing_surrounding_deletion(); composing_surrounding_matrix(); transactions(); history(); maximum_length(); }
