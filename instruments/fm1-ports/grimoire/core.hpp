#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <cstdlib>

namespace grimoire {
constexpr int rate=24000, width=320, height=240;
struct Point { int x,y; };
constexpr Point clamp(Point p) { return {std::clamp(p.x,0,127),std::clamp(p.y,0,127)}; }
using Path=std::array<Point,8>;
constexpr Path ribbon={{{16,64},{32,104},{64,116},{96,96},{112,64},{96,24},{64,12},{32,32}}};
constexpr Path crown={{{5,90},{24,110},{48,100},{72,120},{100,100},{122,90},{82,80},{40,85}}};
constexpr Path pendulum={{{22,24},{42,36},{64,48},{85,36},{104,24},{85,12},{64,8},{42,12}}};
constexpr std::array<int,24> pitch_mhz={110000,116541,123471,130813,138591,146832,155563,164814,174614,184997,195998,207652,220000,233082,246942,261626,277183,293665,311127,329628,349228,369994,391995,415305};
constexpr std::uint32_t increment(int x) { return std::uint32_t((std::uint64_t(pitch_mhz[std::clamp(x,0,127)*24/128])<<32)/(rate*1000ull)); }
constexpr int triangle(std::uint32_t p) { const int n=int(p>>16); return n<32768?2*n-32768:98303-2*n; }
struct Snapshot { Path path; Point xy; std::array<std::uint32_t,3> phase; int material, envelope; bool auto_on,manual,muted; std::uint32_t cursor; };
// Single-owner engine: commands and process() must run on the same serialized audio owner.
// A backend must provide a bounded event handoff and independently drain queued audio on panic.
class Engine {
    Path path_=ribbon; Point xy_{64,64}; std::array<std::uint32_t,3> phase_{};
    std::uint32_t cursor_=0, pitch_=increment(64), lease_=0;
    int material_=0, speed_=1, direction_=1, env_=0, low_=0, blend_=0;
    Point resume_{64,64}; bool automatic_=false,manual_=false,muted_=false;
    static constexpr std::uint32_t cycle=8u*65536u;
    Point orbit() const {
        const auto i=cursor_>>16, f=cursor_&65535u;
        const auto a=path_[i], b=path_[(i+1)&7u];
        return {a.x+(b.x-a.x)*int(f)/65536,a.y+(b.y-a.y)*int(f)/65536};
    }
public:
    void panic() { automatic_=manual_=false; muted_=true; env_=low_=blend_=0; lease_=0; }
    void material(int v) { panic(); material_=std::clamp(v,0,2); }
    bool set_path(const Path& p) { for(auto v:p) if(v.x<0||v.x>127||v.y<0||v.y>127) return false; resume_=xy_; blend_=1600; path_=p; return true; }
    void direction(int v) { direction_=v<0?-1:1; }
    void speed(int v) { speed_=v<=1?1:v<=2?2:4; }
    void start_orbit() { muted_=false; automatic_=true; resume_=xy_; blend_=1600; }
    void stop_orbit() { automatic_=false; }
    void down(Point p) { xy_=clamp(p); manual_=true; muted_=false; lease_=rate/4; }
    void move(Point p) { if(manual_) { xy_=clamp(p); lease_=rate/4; } }
    void up() { if(manual_) { manual_=false; lease_=0; resume_=xy_; blend_=1600; } }
    void cancel() { panic(); }
    Snapshot snapshot() const { return {path_,xy_,phase_,material_,env_,automatic_,manual_,muted_,cursor_}; }
    std::int16_t sample() {
        if(manual_ && lease_ && --lease_==0) up();
        if(automatic_) {
            const auto step=std::uint32_t(11*speed_);
            cursor_=(cursor_+cycle+(direction_>0?step:cycle-step))%cycle;
            if(!manual_) { const auto p=orbit(); if(blend_>0) { xy_={p.x+(resume_.x-p.x)*blend_/1600,p.y+(resume_.y-p.y)*blend_/1600}; --blend_; } else xy_=p; }
        }
        if(muted_) return 0;
        const bool gate=manual_||automatic_;
        env_=gate?std::min(4096,env_+16):std::max(0,env_-3);
        if(!env_) { low_=0; return 0; }
        const int diff=int(increment(xy_.x))-int(pitch_);
        pitch_=std::uint32_t(int(pitch_)+diff/32+(diff>0&&diff<32?1:diff<0&&diff>-32?-1:0));
        constexpr int ratio[3][2]={{2,3},{3,5},{1,7}};
        phase_[0]+=pitch_; phase_[1]+=pitch_*std::uint32_t(ratio[material_][0]); phase_[2]+=pitch_*std::uint32_t(ratio[material_][1]);
        const int mod=triangle(phase_[1])*(xy_.y+1)/128;
        const auto shift=std::uint32_t(mod)*std::uint32_t(1024*(material_+1));
        const int signal=(triangle(phase_[0]+shift)*3+triangle(phase_[2]-shift))/4;
        low_+=(signal-low_)*(8+xy_.y/4)/128;
        return std::int16_t(low_*env_/16384);
    }
    void process(std::int16_t* out,std::size_t n) { for(std::size_t i=0;i<n;++i) out[i]=sample(); }
};
constexpr Point screen(Point p) { return {8+p.x*303/127,38+p.y*193/127}; }
inline void row(const Snapshot& s,int y,std::uint16_t* dst) {
    if(y<0||y>=height) return;
    const auto p=screen(s.xy);
    for(int x=0;x<width;++x) dst[x]=(y<32?0x2104:(x%40==0||y%40==0?0x2945:0x0842));
    if(y<32) {
        for(int x=0;x<width;++x) { const int group=x/64; if((group<3&&group==s.material)||(group==3&&s.auto_on)||(group==4&&s.muted)) dst[x]=0x5d8b; }
        return;
    }
    for(std::size_t i=0;i<8;++i) {
        auto a=screen(s.path[i]); const auto b=screen(s.path[(i+1)%8]);
        const int dx=std::abs(b.x-a.x),dy=-std::abs(b.y-a.y),sx=a.x<b.x?1:-1,sy=a.y<b.y?1:-1;
        int err=dx+dy;
        for(;;) { if(a.y==y) dst[a.x]=0x7bef; if(a.x==b.x&&a.y==b.y) break; const int e=2*err; if(e>=dy){err+=dy;a.x+=sx;} if(e<=dx){err+=dx;a.y+=sy;} }
    }
    constexpr std::uint16_t colors[3]={0xfd20,0x9e7f,0xb81f};
    for(int i=0;i<3;++i) {
        const int ox=triangle(s.phase[i])*25/32768,oy=triangle(s.phase[i]+0x40000000u)*25/32768;
        for(int x=std::max(0,p.x+ox-3);x<std::min(width,p.x+ox+4);++x) if(std::abs(y-p.y-oy)<=3) dst[x]=colors[i];
    }
    if(std::abs(y-p.y)<=6) for(int x=std::max(0,p.x-6);x<std::min(width,p.x+7);++x) if(x==p.x||y==p.y) dst[x]=0xffff;
    if(y>=234) for(int x=0;x<s.envelope*width/4096;++x) dst[x]=0x07e0;
}
}
