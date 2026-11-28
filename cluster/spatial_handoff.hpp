#pragma once
#include <array>
#include <cstdint>
#include <cstddef>

namespace cluster::spatial {
// Presentation-only blend. The selected simulation source changes immediately;
// no stale packet is re-admitted and no internal simulation phase is restored.
class Handoff {
public:
    using Image=std::array<uint8_t,768>;
    static constexpr uint32_t fade_ms=300;
private:
    Image shown_{},origin_{};
    uint32_t started_=0;
    bool initialized_=false,remote_=false,fading_=false;
public:
    const Image& pixels()const{return shown_;}
    bool remote()const{return remote_;}
    bool fading()const{return fading_;}
    void select(const Image& target,bool remote,uint32_t now,bool immediate=false){
        if(!initialized_||immediate){
            initialized_=true;remote_=remote;fading_=false;shown_=target;return;
        }
        if(remote_!=remote){origin_=shown_;started_=now;remote_=remote;fading_=true;}
        const uint32_t elapsed=now-started_;
        if(!fading_||elapsed>=fade_ms){shown_=target;fading_=false;return;}
        for(std::size_t i=0;i<shown_.size();++i){
            shown_[i]=uint8_t((uint32_t(origin_[i])*(fade_ms-elapsed)+uint32_t(target[i])*elapsed+fade_ms/2)/fade_ms);
        }
    }
};
} // namespace cluster::spatial
