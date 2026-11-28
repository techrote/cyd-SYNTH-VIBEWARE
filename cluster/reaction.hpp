#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace cluster::spatial {
// Persistent two-species reaction/diffusion, Q12 concentrations, periodic domain.
// A 64x48 internal lattice is reduced to the existing 32x24 indexed wire image.
// All arithmetic is bounded int32; no allocation, floating point or peer timing.
class ReactionField {
    static constexpr std::size_t w=64,h=48,n=w*h;
    static constexpr int32_t one=4096;
    using Plane=std::array<uint16_t,n>;
    std::array<Plane,2> u_{},v_{};
    std::array<uint8_t,32*24> pixels_{};
    uint32_t step_=0;
    unsigned bank_=0,sub_=0;
    std::size_t row_=0;
    bool busy_=false;
    static uint16_t clamp(int32_t x){return uint16_t(x<0?0:(x>one?one:x));}
    static int32_t lap(const Plane& p,std::size_t x,std::size_t y){
        return int32_t(p[y*w+(x+w-1)%w])+p[y*w+(x+1)%w]+p[((y+h-1)%h)*w+x]+p[((y+1)%h)*w+x]-4*int32_t(p[y*w+x]);
    }
    void inject(std::size_t cx,std::size_t cy){
        for(std::size_t dy=0;dy<5;++dy)for(std::size_t dx=0;dx<5;++dx){
            const auto i=((cy+dy)%h)*w+(cx+dx)%w;
            u_[bank_][i]=2048;v_[bank_][i]=1536;
        }
    }
    void project(){
        const auto& v=v_[bank_];
        for(std::size_t y=0;y<24;++y)for(std::size_t x=0;x<32;++x){
            const auto i=2*y*w+2*x;
            const unsigned sum=unsigned(v[i])+v[i+1]+v[i+w]+v[i+w+1];
            const unsigned intensity=sum/32u;
            pixels_[y*32+x]=uint8_t(intensity>255u?255u:intensity);
        }
    }
public:
    static constexpr uint8_t variant=1;
    static constexpr unsigned substeps=4;
    static constexpr std::size_t lattice_cells=n;
    static constexpr std::size_t slice_rows=8;
    static constexpr unsigned slices_per_tick=substeps*unsigned(h/slice_rows)+1;
    ReactionField(){u_[0].fill(uint16_t(one));inject(15,11);inject(43,30);project();}
    const auto& pixels()const{return pixels_;}
    uint32_t sequence()const{return step_;}
    bool busy()const{return busy_;}
    // Start once; a paused tick may resume without changing the published image.
    bool begin(){
        if(busy_||step_==std::numeric_limits<uint32_t>::max())return false;
        const auto next=step_+1u;
        if(next%17u==0)inject((next/17u*13u)%w,(next/17u*7u)%h);
        busy_=true;sub_=0;row_=0;return true;
    }
    // Single owner only. At most eight lattice rows OR one projection per call.
    // Bank selection is O(1); no whole-plane swap/copy at substep boundaries.
    // Returns true only when a complete new snapshot and sequence are published.
    bool slice(){
        if(!busy_)return false;
        if(sub_==substeps){project();++step_;busy_=false;return true;}
        const auto& up=u_[bank_];const auto& vp=v_[bank_];
        auto& nu=u_[bank_^1u];auto& nv=v_[bank_^1u];
        for(std::size_t y=row_;y<row_+slice_rows;++y)for(std::size_t x=0;x<w;++x){
            const auto i=y*w+x;
            const int32_t u=up[i],v=vp[i],reaction=((u*v)/one*v)/one;
            nu[i]=clamp(u+(640*lap(up,x,y))/one-reaction+150*(one-u)/one);
            nv[i]=clamp(v+(320*lap(vp,x,y))/one+reaction-395*v/one);
        }
        row_+=slice_rows;
        if(row_==h){bank_^=1u;row_=0;++sub_;}
        return false;
    }
    // Synchronous API remains available for native baselines and offline tools.
    // Do not call on a partially advanced tick: finish it via slice() instead.
    bool advance(){
        if(!begin())return false;
        while(!slice()){}
        return true;
    }
    bool bounded()const{
        for(unsigned b=0;b<2;++b)for(std::size_t i=0;i<n;++i)if(u_[b][i]>one||v_[b][i]>one)return false;
        return true;
    }
};
} // namespace cluster::spatial
