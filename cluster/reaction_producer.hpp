#pragma once
#include "reaction_sound.hpp"

namespace cluster::spatial::sound {
// Source-only producer for the P14 ClockSink contract. The panel loop is the
// sole owner of this object; the sink's tick belongs to its sample clock.
// No timer, DAC, network, allocation, blocking wait or automatic rearm here.
template<class Sink> class Producer {
    static_assert(Sink::block==128 && Sink::prime==256,
                  "reviewed P14 block/prime contract required");
    Sink& sink_;
    Voice voice_{};
    std::array<int16_t,Sink::block> pcm_{};
    uint32_t word_=0, refreshed_=0;
    bool armed_=false, started_=false;
public:
    static constexpr uint32_t lease_ms=120;
    enum class Result { idle, queued, started, expired, fault };
    explicit Producer(Sink& sink):sink_(sink){}
    Producer(const Producer&)=delete;
    Producer& operator=(const Producer&)=delete;
    bool armed()const{return armed_;}
    bool arm(uint32_t word,uint32_t now){
        if(armed_||!sink_.acknowledged()||sink_.faulted()||sink_.queued()!=0)return false;
        voice_=Voice{};word_=word;refreshed_=now;armed_=true;started_=false;
        return true;
    }
    void mute(){
        if(!armed_)return;
        sink_.invalidate();armed_=false;started_=false;word_=0;voice_=Voice{};
    }
    // Called only when the panel has serviced its current presentation/control
    // lease. Repeated pumping alone must never refresh this deadline.
    bool refresh(uint32_t word,uint32_t now){
        if(!armed_)return false;
        if(sink_.faulted()||uint32_t(now-refreshed_)>=lease_ms){mute();return false;}
        word_=word;refreshed_=now;return true;
    }
    Result pump(uint32_t now){
        if(!armed_)return Result::idle;
        if(sink_.faulted()||!sink_.acknowledged()){mute();return Result::fault;}
        if(uint32_t(now-refreshed_)>=lease_ms){mute();return Result::expired;}
        // Maintain at most two blocks queued, and generate at most one per call.
        if(sink_.queued()>Sink::prime-Sink::block)return Result::idle;
        voice_.render(word_,pcm_.data(),pcm_.size());
        if(!sink_.push(pcm_.data())){mute();return Result::fault;}
        if(!started_&&sink_.queued()>=Sink::prime){
            if(!sink_.start()){mute();return Result::fault;}
            started_=true;return Result::started;
        }
        return Result::queued;
    }
};
}
