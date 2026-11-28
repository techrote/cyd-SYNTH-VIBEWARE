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
    Plane u_{},v_{},next_u_{},next_v_{};
    std::array<uint8_t,32*24> pixels_{};
    uint32_t step_=0;
    static uint16_t clamp(int32_t x){return uint16_t(x<0?0:(x>one?one:x));}
    static int32_t lap(const Plane& p,std::size_t x,std::size_t y){
        return int32_t(p[y*w+(x+w-1)%w])+p[y*w+(x+1)%w]+p[((y+h-1)%h)*w+x]+p[((y+1)%h)*w+x]-4*int32_t(p[y*w+x]);
    }
    void inject(std::size_t cx,std::size_t cy){
        for(std::size_t dy=0;dy<5;++dy)for(std::size_t dx=0;dx<5;++dx){
            const auto i=((cy+dy)%h)*w+(cx+dx)%w;
            u_[i]=2048;v_[i]=1536;
        }
    }
    void substep(){
        for(std::size_t y=0;y<h;++y)for(std::size_t x=0;x<w;++x){
            const auto i=y*w+x;
            const int32_t u=u_[i],v=v_[i],reaction=((u*v)/one*v)/one;
            next_u_[i]=clamp(u+(640*lap(u_,x,y))/one-reaction+150*(one-u)/one);
            next_v_[i]=clamp(v+(320*lap(v_,x,y))/one+reaction-395*v/one);
        }
        u_.swap(next_u_);v_.swap(next_v_);
    }
    void project(){
        for(std::size_t y=0;y<24;++y)for(std::size_t x=0;x<32;++x){
            const auto i=2*y*w+2*x;
            const unsigned sum=unsigned(v_[i])+v_[i+1]+v_[i+w]+v_[i+w+1];
            const unsigned intensity=sum/32u;
            pixels_[y*32+x]=uint8_t(intensity>255u?255u:intensity);
        }
    }
public:
    static constexpr uint8_t variant=1;
    static constexpr unsigned substeps=4;
    static constexpr std::size_t lattice_cells=n;
    ReactionField(){u_.fill(uint16_t(one));inject(15,11);inject(43,30);project();}
    const auto& pixels()const{return pixels_;}
    uint32_t sequence()const{return step_;}
    bool advance(){
        if(step_==std::numeric_limits<uint32_t>::max())return false;
        ++step_;
        if(step_%17u==0)inject((step_/17u*13u)%w,(step_/17u*7u)%h);
        for(unsigned i=0;i<substeps;++i)substep();
        project();return true;
    }
    bool bounded()const{
        for(std::size_t i=0;i<n;++i)if(u_[i]>one||v_[i]>one)return false;
        return true;
    }
};
} // namespace cluster::spatial
