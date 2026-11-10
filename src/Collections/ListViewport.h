#pragma once

#include <LVUI/List.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace lvui {

enum class ListReveal { contain, start, end, center };
struct ListViewportAnchor final { std::string key; std::size_t index=0; double within=0; std::uint64_t movement=0; };
struct ListViewportRequest final {
    double width=0,height=0,offset=0,row_height=20;
    std::size_t overscan=1;
    std::uint64_t movement=0;
    ListViewportAnchor anchor;
    std::string reveal_key;
    ListReveal reveal=ListReveal::contain;
};
struct ListViewportRow final { std::size_t index=0; double y=0,height=0; };
struct ListViewportFrame final {
    std::uint64_t source_revision=0;
    double content_height=0,offset=0;
    std::vector<ListViewportRow> rows;
    ListViewportAnchor anchor;
    std::string revealed_key;
};

// A viewport holds an immutable index; an application may keep a frame and
// its source snapshot while a newer source is being prepared elsewhere.
struct ListViewportPolicy final {
    using ListSnapshot=::lvui::ListSnapshot;
    using ListIndex=::lvui::ListIndex;
    using ListViewportFrame=::lvui::ListViewportFrame;
    using ListViewportRequest=::lvui::ListViewportRequest;
    using ListReveal=::lvui::ListReveal;
    using ListError=::lvui::ListError;
    using ListErrorCode=::lvui::ListErrorCode;
};
template<template<class>class Engine>using ListViewportBinding=Engine<ListViewportPolicy>;

} // namespace lvui
