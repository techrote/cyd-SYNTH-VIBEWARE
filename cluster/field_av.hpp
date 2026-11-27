#pragma once
#include "link.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstddef>

namespace cluster::av {
constexpr unsigned width=320,height=240,rate=24000;
inline uint16_t rgb(unsigned r,unsigned g,unsigned b) {
    return uint16_t(((r>>3)<<11)|((g>>2)<<5)|(b>>3));
}
// A pure strip/row renderer: no frame buffer, allocation or peripheral access.
// Interpolation is in the actual returned density grid; a cross marks its job XY.
inline void row(const Grid& grid,const Job& job,unsigned y,uint16_t* dst) {
    if(y>=height) return;
    const unsigned gy=y*7*256/(height-1), iy=gy>>8, fy=gy&255;
    const unsigned next_y=std::min(iy+1,7u);
    const int marker_x=int(uint32_t(job.x)*(width-1)/65535u);
    const int marker_y=int(uint32_t(job.y)*(height-1)/65535u);
    for(unsigned x=0;x<width;++x) {
        const unsigned gx=x*7*256/(width-1), ix=gx>>8,fx=gx&255;
        const unsigned nx=std::min(ix+1,7u);
        const unsigned a=grid[iy*8+ix]*(256-fx)+grid[iy*8+nx]*fx;
        const unsigned b=grid[next_y*8+ix]*(256-fx)+grid[next_y*8+nx]*fx;
        const unsigned n=(a*(256-fy)+b*fy)>>16;
        const unsigned v=std::min(255u,n*1024u/std::max(1u,unsigned(job.particles)));
        const unsigned r=std::min(255u,8u+v*v/255u);
        const unsigned g=std::min(255u,12u+v*3/4);
        const unsigned blue=std::min(255u,22u+v);
        const int dx=int(x)-marker_x,dy=int(y)-marker_y;
        dst[x]=(dx>=-7&&dx<=7&&dy>=-7&&dy<=7&&(dx==0||dy==0))?rgb(255,205,90):rgb(r,g,blue);
    }
}
// Four spectral bands from pairs of columns, no fabricated modulation.
// A single word is the complete cross-thread audio control snapshot.
inline uint32_t controls(const Grid& grid,bool gate) {
    if(!gate) return 0;
    std::array<uint32_t,4> sums{}; uint32_t total=0;
    for(unsigned i=0;i<64;++i) {sums[(i%8)/2]+=grid[i];total+=grid[i];}
    if(!total) return 0;
    uint32_t word=0;
    for(unsigned i=0;i<4;++i) word|=((sums[i]*255u+total/2)/total)<<(8*i);
    return word;
}
class Voice {
    std::array<uint32_t,4> phase_{};
    std::array<int32_t,4> level_{};
public:
    void render(uint32_t word,int16_t* out,std::size_t count) {
        constexpr std::array<uint32_t,4> hz{{110,165,220,330}};
        for(std::size_t n=0;n<count;++n) {
            int32_t mix=0;
            for(unsigned i=0;i<4;++i) {
                const int32_t target=int32_t((word>>(i*8))&255u)*256;
                const int32_t delta=target-level_[i];
                level_[i]+=delta/64+(delta%64!=0?(delta>0?1:-1):0);
                phase_[i]+=uint32_t((uint64_t(hz[i])<<32)/rate);
                const int32_t p=int32_t(phase_[i]>>16);
                const int32_t tri=p<32768?p*2-32768:98303-p*2;
                mix+=tri*(level_[i]>>8)/1020;
            }
            out[n]=int16_t(mix); // worst case four full bands <=32768; actual controls sum ~255.
        }
    }
};
// Single control-loop owner. Audio sees only controls(); display only completed
// snapshots. Local fallback is the same kernel and runs off the audio thread.
class Field {
    Link link_;
    Job next_{1,512,64,32768,32768},active_{},shown_{};
    Grid grid_{};
    uint32_t last_=0;
    bool started_=false,remote_=false,gate_=false;
    Completion completion_=Completion::none;
    void complete(Completion c) {
        if(c==Completion::none) return;
        grid_=link_.grid(); shown_=active_; completion_=c;
    }
public:
    explicit Field(uint32_t epoch=91):link_(epoch) {}
    void xy(uint16_t x,uint16_t y) {next_.x=x;next_.y=y;}
    void gate(bool on) {gate_=on;}
    void remote(bool on) {if(!on&&remote_)link_.disconnect();remote_=on;}
    bool remote_enabled() const {return remote_;}
    bool ready() const {return link_.ready();}
    bool pending() const {return link_.pending();}
    const Grid& grid() const {return grid_;}
    const Job& shown() const {return shown_;}
    Completion last_completion() const {return completion_;}
    const LinkStats& stats() const {return link_.stats;}
    uint32_t sound() const {return controls(grid_,gate_);}
    void receive(const Frame& f,uint32_t now) {if(remote_)complete(link_.receive(f,now));}
    template<class Send> void tick(uint32_t now,Send& send) {
        auto route=[&](const Frame& f){return remote_&&send(f);};
        complete(link_.tick(now,route));
        if(!link_.pending()&&(!started_||uint32_t(now-last_)>=100u)) {
            active_=next_;
            if(link_.start(active_,now,60,route)) {
                started_=true;last_=now;++next_.seed;
                // immediate local completion if no negotiated peer.
                complete(link_.tick(now,route));
            }
        }
    }
};
} // namespace cluster::av
