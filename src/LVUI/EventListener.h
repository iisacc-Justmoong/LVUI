#pragma once
#include <cmath>

namespace lvui {
struct EventListenerConfiguration final {
    double width=640,height=480;
    friend bool operator==(const EventListenerConfiguration&,const EventListenerConfiguration&)=default;
};
// Event delivery belongs to an injected host; LVUI owns and validates the
// retained listener extent independently of any platform or widget toolkit.
class EventListener final {
    EventListenerConfiguration configuration_;
public:
    [[nodiscard]] const EventListenerConfiguration& configuration()const noexcept{return configuration_;}
    [[nodiscard]] static bool validate(const EventListenerConfiguration& value)noexcept {
        return std::isfinite(value.width)&&std::isfinite(value.height)&&value.width>=1&&value.height>=1&&value.width<=8192&&value.height<=8192;
    }
    bool set_configuration(EventListenerConfiguration value)noexcept {
        if(!validate(value))return false;configuration_=value;return true;
    }
};
} // namespace lvui
