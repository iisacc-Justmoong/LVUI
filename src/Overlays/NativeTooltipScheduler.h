#pragma once
namespace lvui::native {
// The application supplies its scheduler implementation when composing a view.
template<class Scheduler>using TooltipSchedulerBinding=Scheduler;
} // namespace lvui::native
