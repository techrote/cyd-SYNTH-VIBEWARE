#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstddef>

namespace ecophon {
constexpr unsigned rate=24000, tick_samples=240;
struct Environment { int fertility, sharing, pressure; };
inline bool operator==(Environment a,Environment b){return a.fertility==b.fertility&&a.sharing==b.sharing&&a.pressure==b.pressure;}
inline Environment bounded(Environment e){return {std::clamp(e.fertility,0,255),std::clamp(e.sharing,0,255),std::clamp(e.pressure,0,255)};}
struct Agent { unsigned patch=0, species=0, trait=0; int energy=0, amplitude=0; uint32_t phase=0, step=0; };
struct Engine {
    std::array<Agent,8> agents{};
    std::array<int,4> food{};
    Environment env{180,200,40}, from=env, to=env;
    std::array<Environment,2> memory{{{180,200,40},{0,0,255}}};
    std::array<bool,2> saved{{false,false}};
    uint32_t rng=1, ticks=0, births=0, deaths=0, moves=0, gifts=0;
    unsigned countdown=tick_samples, ramp=0, scene=0;
    bool muted=false, frozen=false;
    uint32_t random(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
    void reset(unsigned preset,uint32_t seed){
        scene=preset%4; rng=seed?seed:1; ticks=births=deaths=moves=gifts=0; countdown=tick_samples; ramp=0; muted=false; frozen=scene==3;
        constexpr Environment presets[]={{180,200,40},{100,20,150},{230,90,85},{180,200,40}};
        env=from=to=presets[scene]; food.fill(scene==1?900:2200);
        for(unsigned i=0;i<agents.size();++i){agents[i]={};agents[i].patch=i%4;agents[i].species=i%3;agents[i].trait=random()%7;agents[i].energy=300+int(random()%301);}
        pitch();
    }
    void set(Environment e){env=bounded(e);ramp=0;}
    void capture(unsigned i){if(i<2){memory[i]=env;saved[i]=true;}}
    bool recall(unsigned i){if(i>=2||!saved[i]||muted)return false;from=env;to=memory[i];ramp=400;return true;}
    void panic(){muted=true;ramp=0;for(auto& a:agents)a.amplitude=0;}
    unsigned population()const{unsigned n=0;for(auto& a:agents)n+=a.energy>0;return n;}
    void pitch(){
        // Independently selected pentatonic pitches, 32-bit phase increments at 24 kHz.
        constexpr uint32_t notes[]={19685267,22096618,24802206,29495427,33108349,39370534,44193236};
        for(auto& a:agents){a.step=notes[a.trait%7]+uint32_t(a.patch*1200000u)+uint32_t(food[a.patch])*600u;}
    }
    void tick(){
        ++ticks;
        if(ramp){const int n=401-int(ramp); auto lerp=[&](int x,int y){return x+(y-x)*n/400;};env={lerp(from.fertility,to.fertility),lerp(from.sharing,to.sharing),lerp(from.pressure,to.pressure)};--ramp;}
        for(unsigned p=0;p<4;++p){int growth=1+env.fertility/8;if(scene==2&&p==(ticks/100)%4)growth+=28;food[p]=std::min(4096,food[p]+growth);}
        if(!frozen){
            std::array<bool,8> alive{};for(unsigned i=0;i<8;++i)alive[i]=agents[i].energy>0;
            for(unsigned k=0;k<8;++k){unsigned i=(k+ticks)%8;auto& a=agents[i];if(!alive[i]||!a.energy)continue;
                int meal=std::min(food[a.patch],5+int(a.species)*2+env.pressure/32);food[a.patch]-=meal;
                a.energy=std::clamp(a.energy+meal*2-8-env.pressure/9,0,1024);
                if(!a.energy){++deaths;continue;}
                if((ticks+i)%13==0){unsigned best=a.patch;for(unsigned p=0;p<4;++p)if(food[p]>food[best])best=p;if(food[best]>food[a.patch]+240){a.patch=best;++moves;}}
                if(env.sharing>0&&a.energy>450){for(unsigned j=0;j<8;++j){auto& b=agents[j];if(j!=i&&b.energy>0&&b.patch==a.patch&&b.energy+160<a.energy){int give=1+env.sharing/32;a.energy-=give;b.energy+=give;++gifts;break;}}}
                if(a.energy>850&&env.fertility>80&&(random()%64)<unsigned(env.fertility/32)){for(unsigned j=0;j<8;++j)if(!agents[j].energy&&agents[j].amplitude==0){auto& b=agents[j];a.energy-=300;b={};b.energy=280;b.patch=a.patch;b.species=a.species;b.trait=(a.trait+random()%3+6)%7;++births;break;}}
            }
            if(!population()&&env.fertility>120&&env.pressure<100){unsigned p=unsigned(std::max_element(food.begin(),food.end())-food.begin());if(food[p]>=700){food[p]-=700;agents[0]={};agents[0].energy=350;agents[0].patch=p;agents[0].trait=random()%7;++births;}}
        }
        pitch();
    }
    int16_t sample(){
        if(muted)return 0;
        if(--countdown==0){tick();countdown=tick_samples;}
        int mix=0;
        for(auto& a:agents){int target=a.energy*2; if(a.amplitude<target)a.amplitude+=std::min(4,target-a.amplitude);else if(a.amplitude>target)a.amplitude-=std::min(4,a.amplitude-target);
            if(!a.amplitude)continue;
            a.phase+=a.step;int t=int(a.phase>>20);int tri=(t<2048?t:4095-t)*2-2047;
            int wave=a.species==0?tri:(a.species==1?(tri+(t<2048?1536:-1536))/2:(tri*std::abs(tri))/2048);
            mix+=wave*a.amplitude/2048;
        }
        return int16_t(mix); // <= 8*2048, no saturation needed
    }
    void render(int16_t* out,size_t n){for(size_t i=0;i<n;++i)out[i]=sample();}
};
struct Touch {
    bool held=false,inhibited=false;unsigned lease=0;
    void release(){held=false;inhibited=false;lease=0;}
    void expire(unsigned ms){if(held){if(ms>=lease){held=false;inhibited=true;lease=0;}else lease-=ms;}}
    void contact(Engine& e,int x,int y){
        if(x<0||x>=320||y<0||y>=240){held=false;inhibited=true;lease=0;return;}
        if(y>=216&&x>=280){e.panic();held=true;inhibited=true;lease=150;return;}
        if(inhibited)return;
        bool fresh=!held;held=true;lease=150;
        if(y<24){if(fresh)e.reset(unsigned(x/80),e.rng);return;}
        if(y>=192&&y<216){if(fresh){unsigned key=unsigned(x/80);if(key<2)e.capture(key);else e.recall(key-2);}return;}
        if(y>=216){if(fresh&&!e.muted)e.frozen=!e.frozen;return;}
        if(y>=112&&y<192&&!e.muted)e.set({x*255/319,e.env.sharing,(191-y)*255/79});
        else if(y>=80&&y<112&&!e.muted)e.set({e.env.fertility,x*255/319,e.env.pressure});
    }
};
inline void row(const Engine& e,unsigned y,uint16_t* out){
    if(y>=240)return;
    constexpr uint16_t species[]={0xfd20,0x07ff,0xf81f};
    for(unsigned x=0;x<320;++x){unsigned p=x/80, local=x%80;uint16_t c=0x0842;
        if(y<24)c=(p==e.scene?0x6c12:0x2104);
        else if(y<80){if(local>2&&local<77&&y>=80-unsigned(e.food[p])*55/4096)c=0x0385;
            for(unsigned i=0;i<8;++i){const auto& a=e.agents[i];if(a.energy&&a.patch==p){int cx=8+int(i%4)*19,cy=31+int(i/4)*22;int dx=int(local)-cx,dy=int(y)-cy;int rad=2+a.energy/200;if(dx*dx+dy*dy<=rad*rad)c=species[a.species];}}}
        else if(y<112)c=x<=unsigned(e.env.sharing)*319/255?0x5d2b:0x2104;
        else if(y<192){c=uint16_t(((191-y)*24/79)<<11 | (x*40/319)<<5 | 6);int dx=int(x)-e.env.fertility*319/255,dy=int(y)-(191-e.env.pressure*79/255);if(dx*dx+dy*dy<=25)c=0xffff;}
        else if(y<216)c=e.saved[p%2]?(p<2?0x3d2a:0x5c9f):0x2104;
        else c=x>=280?0xf800:(e.frozen?0x7be0:0x0385);
        if(x%80==0&&y<112)c=0x0000;
        if(e.muted&&y>=216&&x<280)c=0x0000;
        out[x]=c;
    }
}
}
