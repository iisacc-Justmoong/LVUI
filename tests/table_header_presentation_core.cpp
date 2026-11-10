#include <LVUI/NativeTableHeaderPresentation.h>
#include <stdexcept>
struct Decoder{using Image=int;};
int main(){
    using Policy=lvui::native::TableHeaderPresentationPolicy<Decoder>;
    lvui::TableHeaderPresentation value;value.default_leading_icon="";
    if(!Policy::valid_appearance(value)||Policy::max_columns!=lvui::table_maximum_columns||Policy::display_text("Label",3)!="Lab")
        throw std::runtime_error("table header policy changed");
}
