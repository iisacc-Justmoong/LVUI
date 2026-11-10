#pragma once
#include <LVUI/Tooltip.h>
#include <iiSDKCore/Core/Result/Result.h>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <thread>
#include <utility>

namespace lvui {
struct TooltipGeometryInput final {
    TooltipRect available;
    TooltipPoint anchor;
    TooltipSize content;
    std::optional<TooltipRect> anchor_bounds;
    friend bool operator==(const TooltipGeometryInput&,const TooltipGeometryInput&)=default;
};
struct TooltipSnapshot final {
    TooltipPhase phase=TooltipPhase::hidden;
    std::optional<TooltipLayout> layout;
    std::optional<std::int64_t> deadline;
    bool needs_reconciliation=false;
};
struct TooltipCallbacks final {std::function<void()> opened,closed;};

// A failed call leaves that operation unchanged or reports an uncertain host.
// Calls are synchronous on the owner thread; a scheduled wake must be delivered
// after the controller update returns.
// Reapplying an already committed presentation or deadline is idempotent.
class TooltipPresentationPort {
public:
    virtual ~TooltipPresentationPort()=default;
    [[nodiscard]] virtual ii::core::Result<void> present(std::optional<TooltipLayout>)=0;
    [[nodiscard]] virtual ii::core::Result<void> set_deadline(std::optional<std::int64_t>)=0;
    [[nodiscard]] virtual bool needs_reconciliation()const noexcept{return false;}
};

template<class NativeModel>struct TooltipControllerPolicy final {
    using Model=NativeModel;using Interaction=TooltipInteraction;using GeometryInput=TooltipGeometryInput;using Layout=TooltipLayout;
    using Port=TooltipPresentationPort;using Callbacks=TooltipCallbacks;using Transition=TooltipTransition;
    using Configuration=TooltipConfiguration;using Snapshot=TooltipSnapshot;
};
template<template<class>class Controller,class Model>using TooltipControllerBinding=Controller<TooltipControllerPolicy<Model>>;

} // namespace lvui
