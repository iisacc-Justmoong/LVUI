#pragma once
#include <LVUI/Document.h>
namespace lvui::native {
// App composition supplies this source contract to the selected layout host.
struct DocumentLayoutPolicy final {
    using Document=lvui::Document;using Node=lvui::Node;using Color=lvui::Color;using Length=lvui::Length;
    using Unit=lvui::Unit;using Direction=lvui::Direction;using Alignment=lvui::Alignment;using Kind=lvui::Kind;
};
} // namespace lvui::native
