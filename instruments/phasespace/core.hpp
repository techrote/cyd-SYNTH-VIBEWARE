#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstddef>

namespace phase {
constexpr unsigned rate=24000, block=128, width=320, height=240, field=208;
constexpr float tau=6.2831853071795864769f;
static_assert(std::atomic<uint32_t>::is_always_lock_free,"32-bit atomics required");
struct Control {
    unsigned x=160,y=104,preset=0,seq=0,loop=0; // loop is UI-only; not serialized
    bool alt=false,quant=false,gate=false,panic=false;
    uint32_t pack() const {
        return std::min(x,319u)|(std::min(y,207u)<<9)|(std::min(preset,5u)<<17)|
            (uint32_t(alt)<<20)|(uint32_t(quant)<<21)|(uint32_t(gate)<<22)|
            (uint32_t(panic)<<23)|((seq&255u)<<24);
    }
    static Control decode(uint32_t w) {
        Control c;
        c.x=std::min(w&511u,319u); c.y=std::min((w>>9)&255u,207u);
        c.preset=std::min((w>>17)&7u,5u); c.alt=(w>>20)&1;
        c.quant=(w>>21)&1;c.gate=(w>>22)&1;c.panic=(w>>23)&1;c.seq=w>>24;
        return c;
    }
};
class Surface {
    struct XY {uint16_t x,y;};
    std::array<XY,64> gesture_{};
    unsigned count_=0,index_=0;
    uint32_t lastTick_=0;
    bool down_=false,playing_=false,clockArmed_=false;
    void append() {
        if(count_<gesture_.size())gesture_[count_++]={uint16_t(c.x),uint16_t(c.y)};
    }
    void clearLoop() {c.loop=0;count_=index_=0;clockArmed_=false;c.gate=false;}
public:
    Control c{};
    // R arms one field gesture; release loops it with one silent control tick.
    // UI owner only. Fixed capacity, no unbounded catch-up or audio-thread work.
    void touch(bool down,int x,int y) {
        x=std::clamp(x,0,319); y=std::clamp(y,0,239);
        if(down&&!down_) {
            playing_=y<int(field);
            if(playing_) {
                if(c.loop==3)clearLoop();
                c.panic=false;
                if(c.loop==1){c.loop=2;count_=index_=0;clockArmed_=false;}
            } else if(x<192)c.preset=unsigned(x/32);
            else if(x<224)c.quant=!c.quant;
            else if(x<256)c.alt=!c.alt;
            else if(x<288){
                if(c.loop)clearLoop();else{c.loop=1;c.gate=false;}
            } else {clearLoop();c.panic=true;}
        }
        if(down&&playing_) {
            c.x=unsigned(x);c.y=unsigned(std::min(y,207));
            if(c.loop==2&&count_==0)append();
        }
        if(!down&&playing_) {
            if(c.loop==2) {
                if(count_>=2){c.loop=3;index_=0;clockArmed_=false;c.gate=false;}
                else clearLoop();
            }
            playing_=false;
        }
        down_=down;
        if(c.loop!=3)c.gate=down&&playing_&&!c.panic;
        ++c.seq;
    }
    void tick(uint32_t now) {
        if(c.loop<2)return;
        if(!clockArmed_){lastTick_=now;clockArmed_=true;return;}
        const uint32_t elapsed=now-lastTick_;
        if(elapsed<32)return;
        lastTick_=elapsed>=64?now:lastTick_+32;
        if(c.loop==2){if(playing_)append();}
        else if(index_==count_){c.gate=false;index_=0;}
        else {
            const auto p=gesture_[index_++];c.x=p.x;c.y=p.y;c.gate=!c.panic;
        }
        ++c.seq;
    }
    void cancel() {down_=false;playing_=false;clearLoop();c.panic=true;++c.seq;}
    void heartbeat() {++c.seq;}
};
struct Patch {const char* name;float ratio,couple,morph,detune,mix;uint16_t color;};
inline constexpr std::array<Patch,6> patches{{
    {"Glass",1.5f,0.12f,0.15f,0.002f,0.30f,0x67df},
    {"Metal",1.414214f,0.68f,0.43f,0.007f,0.65f,0xfd25},
    {"Soft",1.0f,0.04f,0.03f,0.003f,0.25f,0x8790},
    {"Detune",1.0f,0.24f,0.10f,0.034f,0.52f,0xc55f},
    {"Lock",2.0f,0.88f,0.35f,0.008f,0.45f,0xff08},
    {"Flux",1.618034f,0.92f,0.56f,0.018f,0.72f,0xf4b6}
}};
struct Point {uint16_t x=160,y=104;};
class Points {
    std::array<Point,256> data_{};
    std::atomic<uint32_t> w_{0},r_{0};
public:
    bool push(Point p) {
        auto w=w_.load(std::memory_order_relaxed);
        if(w-r_.load(std::memory_order_acquire)>=data_.size())return false;
        data_[w&255u]=p;w_.store(w+1,std::memory_order_release);return true;
    }
    bool pop(Point& p) {
        auto r=r_.load(std::memory_order_relaxed);
        if(r==w_.load(std::memory_order_acquire))return false;
        p=data_[r&255u];r_.store(r+1,std::memory_order_release);return true;
    }
};
class Engine {
    std::array<float,257> sine_{};
    std::array<float,320> free_{},scale_{};
    uint32_t a_=0,b_=0,last_=0,age_=rate,decim_=0;
    float f_=220,ratio_=1.5f,depth_=0,morph_=0,mix_=0.3f;
    float env_=0,prevIn_=0,prevOut_=0,sa_=0,sb_=0;
    float wave(uint32_t p)const {
        unsigned i=p>>24;
        float t=float((p>>8)&65535u)*(1.0f/65536.0f);
        return sine_[i]+t*(sine_[i+1]-sine_[i]);
    }
    static uint32_t step(float hz) {
        return uint32_t(std::clamp(hz,20.0f,3200.0f)*(4294967296.0f/float(rate)));
    }
public:
    Engine(){
        for(unsigned i=0;i<257;++i)sine_[i]=std::sin(tau*float(i)/256.0f);
        sine_[256]=sine_[0];
        constexpr int notes[]={0,2,3,5,7,9,10};
        for(unsigned x=0;x<320;++x){
            free_[x]=130.8128f*std::pow(2.0f,2.5f*float(x)/319.0f);
            unsigned n=x*18/320;
            scale_[x]=130.8128f*std::pow(2.0f,float(12*(n/7)+notes[n%7])/12.0f);
        }
    }
    // Audio owner only. No allocations, locks, I/O or display.
    void render(uint32_t word,int16_t* out,std::size_t n,Points* points=nullptr) {
        if(word!=last_){age_=0;last_=word;}
        const auto c=Control::decode(word);const auto& p=patches[c.preset];
        const float v=1.0f-float(c.y)/207.0f;
        const float targetF=c.quant?scale_[c.x]:free_[c.x];
        const float targetRatio=p.ratio+(c.alt?0.16f*(v-0.5f):p.detune);
        const float targetDepth=p.couple*(c.alt?0.30f+0.70f*v:v)*0.28f;
        const float targetMorph=c.alt?p.morph:std::min(0.62f,p.morph*(0.35f+0.65f*v));
        for(std::size_t i=0;i<n;++i){
            const bool gate=c.gate&&!c.panic&&age_<rate*120/1000;
            if(age_<rate)++age_;
            f_+=(targetF-f_)*0.006f;ratio_+=(targetRatio-ratio_)*0.006f;
            depth_+=(targetDepth-depth_)*0.006f;morph_+=(targetMorph-morph_)*0.006f;
            mix_+=(p.mix-mix_)*0.006f;
            a_+=step(f_*(1.0f+depth_*sb_));
            b_+=step(f_*ratio_*(1.0f+depth_*sa_));
            sa_=wave(a_);sb_=wave(b_);
            float wa=sa_+(wave(a_+a_)-sa_)*morph_;
            float wb=sb_+(wave(b_+b_)-sb_)*morph_;
            if(c.panic)env_=0;
            else if(gate)env_=std::min(1.0f,env_+1.0f/240.0f);
            else env_=std::max(0.0f,env_-1.0f/360.0f);
            if(env_==0){prevIn_=prevOut_=0;out[i]=0;}
            else {
                const float input=((1.0f-mix_)*wa+mix_*wb)*env_;
                const float hp=input-prevIn_+0.995f*prevOut_;
                prevIn_=input;prevOut_=hp;
                out[i]=int16_t(std::clamp(hp*5800.0f,-7800.0f,7800.0f));
            }
            if((++decim_&7u)==0&&points){
                float x=160.0f+145.0f*wa*env_,y=104.0f-94.0f*wb*env_;
                points->push({uint16_t(std::clamp(x,0.0f,319.0f)),
                              uint16_t(std::clamp(y,0.0f,207.0f))});
            }
        }
    }
};
class Trace {
    std::array<Point,128> history_{};
    unsigned end_=0,count_=0;
public:
    void drain(Points& p) {
        Point v;for(unsigned i=0;i<256&&p.pop(v);++i){
            history_[end_]=v;end_=(end_+1)&127u;count_=std::min(128u,count_+1);
        }
    }
    static uint16_t dim(uint16_t rgb,unsigned level){
        return uint16_t((((rgb>>11)*level/8)<<11)|
            ((((rgb>>5)&63)*level/8)<<5)|((rgb&31)*level/8));
    }
    static unsigned button(unsigned x) {
        return std::min(x/32,9u);
    }
    void row(const Control& c,unsigned y,uint16_t* dst)const {
        if(y>=height)return;
        const auto color=patches[std::min(c.preset,5u)].color;
        for(unsigned x=0;x<width;++x){
            if(y<field){
                dst[x]=(x==160||y==104)?0x2947:((x%40==0||y%26==0)?0x18a4:0x0842);
            } else {
                unsigned b=button(x);
                bool sel=b==c.preset||(b==6&&c.quant)||(b==7&&c.alt)||(b==8&&c.loop)||(b==9&&c.panic);
                uint16_t ink=b<6?patches[b].color:(b==9?0xf986:(b==8&&c.loop==2?0xf986:0xadb5));
                bool edge=x==0||button(x-1)!=b||y==field||y==239;
                dst[x]=edge?0x0000:dim(ink,sel?4:1);
            }
        }
        if(y<field){
            for(unsigned i=0;i<count_;++i){
                const auto p=history_[(end_+128-count_+i)&127u];
                if(p.x>=width||p.y>=field)continue;
                if(std::abs(int(y)-int(p.y))<=1){
                    const auto ink=dim(color,1+i*7/std::max(1u,count_-1));
                    for(int dx=-1;dx<=1;++dx){
                        int x=int(p.x)+dx;if(x>=0&&x<int(width))dst[x]=ink;
                    }
                }
            }
            if(c.gate&&c.x<width&&c.y<field&&std::abs(int(y)-int(c.y))<=4){
                for(int x=std::max(0,int(c.x)-4);x<=std::min(319,int(c.x)+4);++x)
                    if(x==int(c.x)||y==c.y)dst[x]=0xffff;
            }
        } else {
            constexpr uint8_t glyph[10][7]={{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
              {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
              {14,16,16,30,17,17,14},{14,17,17,17,21,18,13},
              {17,27,21,21,17,17,17},{30,17,17,30,20,18,17},{17,17,10,4,10,17,17}};
            constexpr unsigned start[]={0,32,64,96,128,160,192,224,256,288};
            if(y>=217&&y<231)for(unsigned b=0;b<10;++b){
                unsigned bits=glyph[b][(y-217)/2];
                for(unsigned k=0;k<5;++k)if(bits&(16u>>k)){
                    dst[start[b]+12+2*k]=0xffff;dst[start[b]+13+2*k]=0xffff;
                }
            }
        }
    }
};
} // namespace phase
