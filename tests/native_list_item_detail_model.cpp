#include <LVUI/NativeListItemDrawing.h>
#include <iiGUI/ListItemModel.h>
using ListItemModel=lvui::native::ListItemModelBinding<ii::gui::BasicListItem>;
#include <stdexcept>
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
int main(){ListItemModel row;auto c=row.configuration();c.variant=lvui::ListItemVariant::detail;c.label="Files";c.description="No files";
    c.detail_date="Today";c.detail_folder1="Work";c.detail_tag1="Art";check(row.set_configuration(c),"valid details rejected");
    check(row.intrinsic_size().height==98,"metadata did not reserve separate rows");auto g=*row.geometry(194,98);
    check(g.detail_date.height>0&&g.detail_folders.height>0&&g.detail_tags.height>0&&g.description.y+g.description.height<=g.detail_date.y,"detail rows overlap");
    c.show_detail_date=c.show_detail_folders=c.show_detail_tags=false;check(row.set_configuration(c)&&row.intrinsic_size().height==44,"hidden details reserved space");
    g=*row.geometry(194,44);check(g.detail_date.height==0&&g.detail_folders.height==0&&g.detail_tags.height==0,"hidden detail still displayed");
    c.detail_date=std::string("\xc0\x80",2);check(!row.set_configuration(c),"invalid metadata accepted");}
