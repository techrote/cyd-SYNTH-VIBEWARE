#pragma once
#include <cstdint>
#include <algorithm>
namespace xenobio {
// Input producer only. Explicit arm followed by observed release; no touch
// event can restart synthesis while disarmed. All coordinates are landscape.
class TargetInput {
    bool armed_=false, await_=true, pressed_=false;
public:
    bool armed() const { return armed_; }
    template<class C> void disarm(C& c) {
        if (armed_ || pressed_) c.cancel();
        armed_=false; await_=true; pressed_=false;
    }
    template<class C> void arm(C& c) {
        if (armed_) return;
        c.cancel(); armed_=true; await_=true; pressed_=false;
    }
    template<class C> void poll(C& c, bool down, int x, int y) {
        if (!armed_) return;
        if (!down) {
            if (pressed_) c.up();
            pressed_=false; await_=false; return;
        }
        if (await_) return;
        if (x<0 || x>=320 || y<0 || y>=240 || (x>=280 && y>=220)) {
            disarm(c); return;
        }
        const bool accepted=pressed_ ? c.move(x,y) : c.down(x,y);
        pressed_=true;
        if (!accepted) disarm(c);
    }
};
inline int touch_axis(int raw, int extent) {
    return (std::clamp(raw,200,3900)-200)*(extent-1)/3700;
}
// Conservative half-scale signed PCM to unsigned DAC midpoint. The legacy
// ESP32 I2S DAC takes the high byte of each 16-bit sample.
inline uint16_t dac_word(int16_t pcm) {
    return uint16_t(32768+int32_t(pcm)/2);
}
}
