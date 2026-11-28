#pragma once
#include "cluster.hpp"
#include "reaction.hpp"

namespace cluster::spatial {
// Independent latest-state stream; never use this larger datagram on ESP-NOW.
// One full snapshot fits an ordinary 1500-byte IPv4 MTU without fragmentation.
constexpr std::size_t width=32,height=24,cells=width*height, prefix=20, bytes=prefix+cells;
constexpr uint32_t lease_ms=200;
using Pixels=std::array<uint8_t,cells>;
using Packet=std::array<uint8_t,bytes>;
inline uint32_t checksum(const uint8_t* p,std::size_t n,uint32_t h=2166136261u){
    for(std::size_t i=0;i<n;++i){h^=p[i];h*=16777619u;}return h;
}
inline uint32_t digest(const Pixels& p){return checksum(p.data(),p.size());}
class Field {
    Pixels a_{},b_{};
    uint32_t step_=0;
public:
    static constexpr uint8_t variant=0;
    const Pixels& pixels()const{return a_;}
    uint32_t sequence()const{return step_;}
    // Persistent integer excitable trails, toroidal advection + diffusion + decay.
    // Local/remote independent evolution; no request or ACK needed per snapshot.
    bool advance(){
        if(step_==std::numeric_limits<uint32_t>::max())return false;
        ++step_;
        const auto sx=std::size_t((step_/2u)%width);
        const auto sy=std::size_t((step_/3u)%height);
        for(std::size_t y=0;y<height;++y)for(std::size_t x=0;x<width;++x){
            const auto i=y*width+x;
            const unsigned c=a_[i],l=a_[y*width+(x+width-1)%width],
                r=a_[y*width+(x+1)%width],u=a_[((y+height-1)%height)*width+x],
                d=a_[((y+1)%height)*width+x];
            const unsigned v=(c*3u+l*3u+r+u+d)/9u;
            b_[i]=uint8_t(v>2u?v-2u:0u);
            if((x==sx||x==(sx+1)%width)&&(y==sy||y==(sy+1)%height))b_[i]=255;
        }
        a_.swap(b_);return true;
    }
};
#if defined(CLUSTER_SPATIAL_REACTION) && CLUSTER_SPATIAL_REACTION
using ActiveField=ReactionField;
#else
using ActiveField=Field;
#endif
template<class F> inline Packet encode(uint32_t epoch,const F& f){
    Packet p{};
    p[0]='S';p[1]='F';p[2]=1;p[3]=F::variant;
    put32(p.data()+4,epoch);put32(p.data()+8,f.sequence());
    put16(p.data()+12,uint16_t(width));put16(p.data()+14,uint16_t(height));
    for(std::size_t i=0;i<cells;++i)p[prefix+i]=f.pixels()[i];
    put32(p.data()+16,checksum(p.data()+prefix,cells,checksum(p.data(),16)));
    return p;
}
class Receiver {
    uint32_t epoch_,last_=0,at_=0;
    bool live_=false;
    uint8_t variant_;
    Pixels pixels_{};
public:
    explicit Receiver(uint32_t epoch,uint8_t variant=ActiveField::variant):epoch_(epoch),variant_(variant){}
    // Calls belong to one control/render loop. Caller must validate endpoint.
    // FNV is corruption detection, not authentication.
    bool accept(const uint8_t* p,std::size_t n,uint32_t now){
        if(!p||n!=bytes||!epoch_||p[0]!='S'||p[1]!='F'||p[2]!=1||variant_>1||p[3]!=variant_||
           get32(p+4)!=epoch_||get16(p+12)!=width||get16(p+14)!=height)return false;
        const auto seq=get32(p+8);
        if(!seq||seq<=last_||get32(p+16)!=checksum(p+prefix,cells,checksum(p,16)))return false;
        for(std::size_t i=0;i<cells;++i)pixels_[i]=p[prefix+i];
        last_=seq;at_=now;live_=true;return true;
    }
    bool fresh(uint32_t now){
        if(live_&&uint32_t(now-at_)>=lease_ms)live_=false;
        return live_;
    }
    void disconnect(){live_=false;} // retain high-water sequence on rejoin
    uint32_t sequence()const{return last_;}
    const Pixels& pixels()const{return pixels_;}
};
inline uint16_t color(uint8_t v){
    const unsigned r=unsigned(v)/8u,g=unsigned(v)*63u/255u,b=unsigned(v)*31u/255u;
    return uint16_t((r<<11)|(g<<5)|b);
}
// Local rendering, no remote RGB565 framebuffer or allocation.
inline void row(const Pixels& p,std::size_t y,uint16_t* out){
    if(y>=240||!out)return;
    for(std::size_t x=0;x<320;++x)out[x]=color(p[(y/10)*width+x/10]);
}
} // namespace cluster::spatial
