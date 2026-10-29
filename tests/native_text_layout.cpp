#include <LVUI/NativeTextLayout.h>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
void require(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> value) {
    if (!value) throw std::runtime_error(value.error().message);
    return std::move(*value);
}
}
int main() {
    using namespace lvui::native;
    auto layout=take(NativeTextLayout::create("abc\n한😀\ne\xcc\x81Z"));
    require(layout.lines().size()==3 && layout.length()==11,"Paragraph boundaries or UTF-16 length are wrong");
    const auto first=take(layout.caret(0)), korean=take(layout.caret(4));
    require(first.y==0 && korean.y>first.y && korean.height>0,"Caret is not in the correct paragraph");
    const auto emoji_start=take(layout.caret(5)), emoji_end=take(layout.caret(7));
    require(take(layout.hit_test(emoji_start.x,emoji_start.y+1))==5 &&
            take(layout.hit_test(emoji_end.x,emoji_end.y+1))==7,"Shaped hit testing did not preserve emoji boundaries");
    require(!layout.caret(6),"Caret split a surrogate pair");
    const auto spans=take(layout.selection({2,7}));
    require(spans.size()>=3,"Selection did not cover the newline and following paragraphs");
    auto masked=take(NativeTextLayout::create("A😀e\xcc\x81Z",{},true));
    require(masked.display_text(0)=="••••••" && masked.length()==6,"Password display did not mask UTF-16 positions");
    const auto before=take(masked.caret(1)),after=take(masked.caret(3));
    const auto hit=take(masked.hit_test((before.x+after.x)*.5,before.y+1));
    require(hit==1 || hit==3,"Password hit testing split an emoji");
    require(!masked.selection({2,1}),"Password selection accepted a split surrogate");
    auto empty=take(NativeTextLayout::create("\n"));
    require(empty.lines().size()==2 && take(empty.caret(1)).y>0,"Trailing empty paragraph disappeared");
    require(!NativeTextLayout::create(std::string(4097,'\n')),"Excessive paragraph count was accepted");
    require(!layout.hit_test(std::numeric_limits<double>::infinity(),0),"Non-finite hit position was accepted");
}
