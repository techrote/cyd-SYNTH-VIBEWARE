#include "dirty_grid.hpp"
#include <cstdlib>
#include <iostream>
#include <vector>
using namespace gridseq;
using cyd::platform::DirtyGrid;
static void check(bool v){if(!v)std::abort();}
int main(){
    DirtyGrid d;Engine engine;Surface surface;std::atomic<uint32_t> h{0};
    std::array<int16_t,block> pcm{};std::vector<uint16_t> lcd(width*height,0xdead),full(width*height);
    unsigned frames=0,rows=0,calls=0;uint64_t pixels=0;
    auto sink=[&](const Control& c,uint32_t head,unsigned y,unsigned n){
        check(n>0&&n<=4&&y+n<=height);++calls;rows+=n;
        for(unsigned i=0;i<n;++i)row(c,head,y+i,lcd.data()+(y+i)*width);
        return true;
    };
    auto frame=[&](const Control& c,uint32_t head){
        check(d.begin(c,head));unsigned before=rows;
        while(d.busy()){check(!d.begin(c,head));check(d.step(sink)>0);}
        for(unsigned y=0;y<height;++y)row(c,head,y,full.data()+y*width);
        check(lcd==full);++frames;pixels+=width*height;return rows-before;
    };
    check(frame(surface.c,0)==240);
    for(unsigned i=0;i<20;++i){++surface.c.seq;check(frame(surface.c,0)==0);}
    surface.touch(true,52,30);surface.touch(false,0,0);check(frame(surface.c,0)==44);
    surface.touch(true,180,220);surface.touch(false,0,0);check(frame(surface.c,0)==40);
    surface.touch(true,20,220);surface.touch(false,0,0);
    const unsigned startRows=rows,startFrames=frames;
    for(unsigned i=0;i<960;++i){
        if(i%40==0){unsigned lane=(i/40)%4;surface.touch(true,52+int(i%260),30+44*lane);surface.touch(false,0,0);}
        if(i%96==0){surface.touch(true,int(i%320),10);surface.touch(false,0,0);}
        if(i%67==0){surface.touch(true,10,52+44*((i/67)%4));surface.touch(false,0,0);}
        ++surface.c.seq;engine.render(surface.c,pcm.data(),pcm.size(),&h);if(i%4==0)frame(surface.c,h.load());
    }
    const unsigned liveRows=rows-startRows,liveFrames=frames-startFrames;
    check(d.begin(surface.c,h.load()));d.invalidate();check(d.begin(surface.c,h.load()));
    check(d.step([](const Control&,uint32_t,unsigned,unsigned){return false;})==0);
    check(d.step(sink)==4);d.invalidate();std::fill(lcd.begin(),lcd.end(),0xdead);check(frame(surface.c,h.load())==240);
    // Exercise every rendered control dependency separately, including inactive heads.
    Control c=surface.c;uint32_t head=0;
    frame(c,head);
    for(unsigned t=0;t<4;++t){
        c.masks[t]^=1;frame(c,head);c.accents[t]^=1;frame(c,head);
        c.mute^=1u<<t;frame(c,head);c.accentEdit^=1u<<t;frame(c,head);
        for(unsigned n:ratios){c.divs[t]=n;frame(c,head);}
        for(unsigned n=0;n<3;++n){c.rules[t]=n;frame(c,head);}
        head^=15u<<(4*t);check(frame(c,head)==0);
    }
    head|=0x10000u;check(frame(c,head)==176);
    for(unsigned t=0;t<4;++t){head^=1u<<(4*t);check(frame(c,head)==44);}
    std::cout<<"PASS frames="<<frames<<" pixels="<<pixels<<" rows="<<rows<<" calls="<<calls
             <<" liveFrames="<<liveFrames<<" liveRows="<<liveRows<<" state="<<sizeof(DirtyGrid)<<'\n';
}
