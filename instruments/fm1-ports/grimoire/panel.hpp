#pragma once
#include "strip.hpp"
#include "touch.hpp"

namespace grimoire {
// Immutable panel state; the audio owner captures it between PCM blocks.
struct PanelFrame {
    Snapshot engine;
    std::array<std::uint16_t,6> buttons;
};
inline PanelFrame capture_panel(const Engine& e,const Touch& t) {
    PanelFrame f{e.snapshot(),{}};
    for(int i=0;i<6;++i) f.buttons[i]=t.toolbar_pixel((2*i+1)*width/12,pad_end+1);
    return f;
}
class PanelRaster {
    PanelFrame frame_{};
    std::array<std::uint16_t,width*8> cache_{};
    int cached_=-1;
    bool begun_=false;
public:
    void begin(const PanelFrame& frame) { frame_=frame;cached_=-1;begun_=true; }
    // Contiguous logical display rows, up to eight; cache and destination must
    // not overlap. Source geometry is identical to M3 desktop projection.
    bool render(int first,int count,std::uint16_t* dst) {
        if(!begun_||!dst||first<0||first>=height||count<1||count>8||count>height-first)return false;
        for(int y=first;y<first+count;++y) {
            auto* line=dst+(y-first)*width;
            const int sy=source_y(y);
            if(sy<0) {
                for(int x=0;x<width;++x) {
                    const int g=x*6/width;
                    line[x]=(x==0||x*6/width!=(x-1)*6/width||y==pad_end)?0x0842:frame_.buttons[g];
                }
                continue;
            }
            const int base=sy/8*8;
            if(base!=cached_) {
                if(!render_strip(frame_.engine,base,8,cache_.data()))return false;
                cached_=base;
            }
            std::copy_n(cache_.data()+(sy-base)*width,width,line);
        }
        return true;
    }
};
// Serialized board-side session. No queue or DAC driver is implied. A future
// backend must call these methods from one owner and flush on every true result.
class BoardSession {
    Engine engine_;
    Touch touch_{engine_};
    bool armed_=false,requireRelease_=true,seen_=false;
    std::uint32_t last_=0;
    bool invalidate() { requireRelease_=true;seen_=false;return touch_.cancel(); }
public:
    BoardSession() { engine_.panic(); }
    BoardSession(const BoardSession&)=delete;
    BoardSession& operator=(const BoardSession&)=delete;
    bool armed() const { return armed_; }
    bool arm() { armed_=true;return invalidate(); }
    bool disarm() { armed_=false;return invalidate(); }
    // valid describes acquisition health, not pressure. A normal no-contact
    // observation releases; failed acquisition or stale polling cancels orbit too.
    bool poll(bool valid,bool down,int x,int y,std::uint32_t now) {
        if(!armed_)return false;
        if(!valid||(down&&(x<0||x>=width||y<0||y>=height)))return invalidate();
        bool flush=false;
        if(seen_&&std::uint32_t(now-last_)>=100)flush=invalidate();
        last_=now;seen_=true;
        if(!down){touch_.up();requireRelease_=false;return flush;}
        if(requireRelease_)return flush;
        const bool command=touch_.contact()?touch_.move(x,y):touch_.down(x,y);
        return flush||command;
    }
    bool expire(std::uint32_t now) {
        if(armed_&&seen_&&std::uint32_t(now-last_)>=100)return invalidate();
        return false;
    }
    // Backend is responsible for delivery/discard of this block and for honoring
    // flush before any submission; here the core is already digitally muted.
    bool audio(std::uint32_t now,std::int16_t* out,std::size_t n) {
        const bool flush=expire(now);engine_.process(out,n);return flush;
    }
    PanelFrame frame() const { return capture_panel(engine_,touch_); }
    int speed() const { return touch_.speed(); }
    int direction() const { return touch_.direction(); }
    bool editing() const { return touch_.editing(); }
};
// Candidate raw landscape calibration from the existing compile-qualified
// Ecophon profile. Actual per-board calibration remains a physical gate.
inline int raw_axis(int raw,int extent) { return (std::clamp(raw,200,3900)-200)*(extent-1)/3700; }
}
