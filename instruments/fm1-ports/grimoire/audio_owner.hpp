#pragma once
#include "panel.hpp"

namespace grimoire {
// Instrument-local producer for PLATFORM P14's ClockSink contract. No GPIO,
// timer, ISR or device ownership here. All methods run on the single serialized
// producer; only the externally owned sink's tick() runs in its consumer.
template<class Sink> class AudioOwner {
    static_assert(Sink::block==128 && Sink::prime==256,"reviewed P14 geometry required");
    BoardSession session_;
    Sink& sink_;
    std::array<std::int16_t,Sink::block> pcm_{};
    bool started_=false;
    void flush() { started_=false;sink_.invalidate(); }
    void fail() { session_.disarm();flush(); }
public:
    explicit AudioOwner(Sink& sink):sink_(sink) { flush(); }
    AudioOwner(const AudioOwner&)=delete;
    AudioOwner& operator=(const AudioOwner&)=delete;
    bool armed() const { return session_.armed(); }
    PanelFrame frame() const { return session_.frame(); }
    int speed() const { return session_.speed(); }
    int direction() const { return session_.direction(); }
    bool editing() const { return session_.editing(); }
    bool arm() {
        if(sink_.faulted()){fail();return false;}
        if(armed()||!sink_.acknowledged())return false;
        if(session_.arm())flush();
        return true;
    }
    void disarm() { fail(); }
    // Service even if the sample queue is full or no input poll is due.
    void service(std::uint32_t now) {
        if(sink_.faulted()){fail();return;}
        if(session_.expire(now))flush();
    }
    void poll(bool valid,bool down,int x,int y,std::uint32_t now) {
        service(now);
        if(!sink_.acknowledged())return;
        if(session_.poll(valid,down,x,y,now))flush();
    }
    // At most one newly generated block per call, never retain a pre-flush
    // block for retry. Maintain <=384 queued codes, not a full 512-code ring.
    // Call at a bounded cadence; the sink, not this routine, clocks samples.
    bool fill(std::uint32_t now) {
        service(now);
        if(!armed()||!sink_.acknowledged()||session_.frame().engine.muted||sink_.queued()>Sink::prime)return false;
        if(session_.audio(now,pcm_.data(),pcm_.size())){flush();return false;}
        if(!sink_.push(pcm_.data())){fail();return false;}
        if(!started_&&sink_.queued()>=Sink::prime){
            if(!sink_.start()){fail();return false;}
            started_=true;
        }
        if(sink_.faulted()){fail();return false;}
        return true;
    }
};
}
