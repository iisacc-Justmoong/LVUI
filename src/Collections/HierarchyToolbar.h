#pragma once

#include <LVUI/ToolbarButton.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace lvui {

enum class HierarchyToolbarErrorCode { invalid, not_found, wrong_thread, exhausted };
struct HierarchyToolbarError final { HierarchyToolbarErrorCode code; std::string message; };
template<class T> using HierarchyToolbarResult = std::expected<T, HierarchyToolbarError>;

class HierarchyToolbarCommand {
public:
    virtual ~HierarchyToolbarCommand() = default;
    virtual void invoke(const ToolbarButtonId&) const = 0;
};
struct HierarchyToolbarItem final {
    std::optional<ToolbarButtonId> id;
    std::string icon_name, text, accessibility_name;
    bool enabled = true, visible = true, selected = false;
    std::shared_ptr<const HierarchyToolbarCommand> command;
    friend bool operator==(const HierarchyToolbarItem&, const HierarchyToolbarItem&) = default;
};
struct HierarchyToolbarSelection final {
    ToolbarButtonId id;
    std::size_t index = 0;
    friend bool operator==(const HierarchyToolbarSelection&, const HierarchyToolbarSelection&) = default;
};
struct HierarchyToolbarSnapshot final {
    std::vector<HierarchyToolbarItem> items;
    std::vector<ToolbarButtonId> ids;
    std::optional<HierarchyToolbarSelection> active;
    std::uint64_t revision = 0;
};
struct HierarchyToolbarActivation final {
    HierarchyToolbarSelection selection;
    bool changed = false;
    std::shared_ptr<const HierarchyToolbarCommand> command;
};
enum class HierarchyToolbarDirection { previous, next, first, last };

// A toolbar is view state; selecting a tool never edits the application's
// document. Items, selection and activation are independent of the GUI host.
struct HierarchyToolbarModelPolicy final {
 using Item=HierarchyToolbarItem;using Id=ToolbarButtonId;using Snapshot=HierarchyToolbarSnapshot;using Selection=HierarchyToolbarSelection;
 using Activation=HierarchyToolbarActivation;using Direction=HierarchyToolbarDirection;using Error=HierarchyToolbarError;using ErrorCode=HierarchyToolbarErrorCode;
 template<class T>using Result=HierarchyToolbarResult<T>;
 [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return valid_accessibility_text(value);}
};
template<template<class>class Model>using HierarchyToolbarModelBinding=Model<HierarchyToolbarModelPolicy>;

} // namespace lvui
