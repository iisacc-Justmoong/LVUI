#include <LVUI/ListItemSegments.h>
#include <stdexcept>
int main(){lvui::ListItemSegmentsConfiguration c;if(c.options.size()!=2||c.selected_index!=0||c.segment_width!=56)
 throw std::runtime_error("Segment declaration defaults changed");}
