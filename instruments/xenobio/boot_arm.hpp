#pragma once
#include <cstdint>
namespace xenobio {
// Input producer only. A button held at boot or through a disarm never arms.
// Arming requires an observed stable release, a new hold and a stable release.
enum class BootAction { None, Arm, Disarm };
class BootArm {
    enum class State { NeedHigh, HighDebounce, Ready, Held, ReleaseDebounce };
    State state_=State::NeedHigh;
    uint32_t since_=0;
public:
    static constexpr uint32_t debounce_ms=50, hold_ms=800;
    void reset() { state_=State::NeedHigh; }
    BootAction poll(uint32_t now, bool low, bool armed, bool healthy) {
        if (!healthy || armed) {
            reset();
            return armed && (!healthy || low) ? BootAction::Disarm : BootAction::None;
        }
        switch (state_) {
        case State::NeedHigh:
            if (!low) { since_=now; state_=State::HighDebounce; }
            break;
        case State::HighDebounce:
            if (low) reset();
            else if (uint32_t(now-since_)>=debounce_ms) state_=State::Ready;
            break;
        case State::Ready:
            if (low) { since_=now; state_=State::Held; }
            break;
        case State::Held:
            if (!low) {
                const bool longEnough=uint32_t(now-since_)>=hold_ms;
                since_=now;
                state_=longEnough ? State::ReleaseDebounce : State::HighDebounce;
            }
            break;
        case State::ReleaseDebounce:
            if (low) reset(); // fail closed on release bounce; retry deliberate hold
            else if (uint32_t(now-since_)>=debounce_ms) {
                reset(); return BootAction::Arm;
            }
            break;
        }
        return BootAction::None;
    }
};
}
