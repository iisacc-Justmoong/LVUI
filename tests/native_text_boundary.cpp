#include <LVUI/NativeTextBoundary.h>
#include <stdexcept>
#include <string>

namespace {
void require(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(lvui::TextInputResult<T> result) {
    if (!result) throw std::runtime_error(result.error().message);
    return std::move(*result);
}
}
int main() {
    const std::string family = "👨‍👩‍👧‍👦";
    auto text = take(lvui::TextInput::create(lvui::native::native_text_boundary,"한" + family + "e\xcc\x81Z"));
    require(text.length() == 15,"Native boundary returned an incorrect UTF-16 length");
    take(text.set_selection(12,12));
    require(take(text.erase(-1)) && text.text() == "한e\xcc\x81Z",
            "Native Unicode boundary split the family emoji grapheme");
    take(text.move(1,false));
    require(text.caret() == 3,"Native Unicode boundary split the combining character");
    take(text.set_selection(1,3));
    take(text.set_composition("ㅎ"));
    take(text.set_composition("하"));
    require(take(text.insert("한")) && text.text() == "한한Z",
            "Native Unicode boundary lost Korean composition replacement");
    require(!text.set_selection(10,10) && text.text() == "한한Z",
            "Native boundary accepted an out-of-range selection");
}
