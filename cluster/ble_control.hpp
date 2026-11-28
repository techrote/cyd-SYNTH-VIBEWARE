#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <algorithm>
namespace cluster::ble {
constexpr char service[]="8f113f00-1b68-4e33-a19c-648ebf1d8e21";
constexpr char control[]="8f113f01-1b68-4e33-a19c-648ebf1d8e21";
constexpr char acknowledgement[]="8f113f02-1b68-4e33-a19c-648ebf1d8e21";
constexpr uint32_t lease_ms=250;
using Wire=std::array<uint8_t,16>;
using AckWire=std::array<uint8_t,12>;
struct Control { uint32_t epoch=0, seq=0; uint16_t x=0,y=0; };
inline void put32(uint8_t* b,uint32_t v){for(unsigned i=0;i<4;++i)b[i]=uint8_t(v>>(8*i));}
inline uint32_t get32(const uint8_t* b){uint32_t v=0;for(unsigned i=0;i<4;++i)v|=uint32_t(b[i])<<(8*i);return v;}
inline Wire encode(Control c){
    Wire b{{1,1,0,0}};put32(b.data()+4,c.epoch);put32(b.data()+8,c.seq);
    b[12]=uint8_t(c.x);b[13]=uint8_t(c.x>>8);b[14]=uint8_t(c.y);b[15]=uint8_t(c.y>>8);return b;
}
inline bool decode(const uint8_t* b,std::size_t n,Control& out){
    out={};if(!b||n!=16||b[0]!=1||b[1]!=1||b[2]||b[3])return false;
    Control c{get32(b+4),get32(b+8),uint16_t(unsigned(b[12])|(unsigned(b[13])<<8)),uint16_t(unsigned(b[14])|(unsigned(b[15])<<8))};
    if(!c.epoch||!c.seq)return false;
    out=c;return true;
}
inline AckWire ack(uint32_t epoch,uint32_t seq){
    AckWire b{{1,2,0,0}};put32(b.data()+4,epoch);put32(b.data()+8,seq);return b;
}
inline bool read_ack(const uint8_t* b,std::size_t n,uint32_t& epoch,uint32_t& seq){
    epoch=seq=0;if(!b||n!=12||b[0]!=1||b[1]!=2||b[2]||b[3]||!get32(b+4))return false;
    epoch=get32(b+4);seq=get32(b+8);return true;
}
struct XY {uint16_t x=32768,y=32768;};
struct Arrival { Control value{};uint32_t received=0,generation=0; };
// One main-loop owner; no allocation, remote gate or remote audio arm.
// Sequence numbers never wrap within an epoch. Reconnect preserves watermark.
class Modulator {
    uint32_t epoch_=0,last_=0,stamp_=0,generation_=0;
    XY remote_{};bool enabled_=false,active_=false;
public:
    explicit Modulator(uint32_t epoch=0):epoch_(epoch){}
    void mode(bool enabled,uint32_t generation){
        if(enabled!=enabled_||generation!=generation_)active_=false;
        enabled_=enabled;generation_=generation;
    }
    bool receive(const Arrival& a,uint32_t now){
        const auto& c=a.value;
        if(!enabled_||a.generation!=generation_||!epoch_||c.epoch!=epoch_||
           !c.seq||c.seq<=last_||uint32_t(now-a.received)>lease_ms)return false;
        last_=c.seq;stamp_=a.received;remote_={c.x,c.y};active_=true;return true;
    }
    bool fresh(uint32_t now)const{return enabled_&&active_&&uint32_t(now-stamp_)<=lease_ms;}
    XY effective(XY local,uint32_t now)const{
        if(!fresh(now))return local;
        return {uint16_t((uint32_t(local.x)+remote_.x)/2),uint16_t((uint32_t(local.y)+remote_.y)/2)};
    }
    uint32_t sequence()const{return last_;}
};
} // namespace cluster::ble
