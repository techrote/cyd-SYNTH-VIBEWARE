#include "typed_strip.hpp"
#include "core.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <vector>
using namespace cyd::platform;
static void check(bool v){if(!v)std::abort();}
struct Pixel { uint16_t value=0; Pixel& operator=(uint16_t p){value=p;return *this;} };
struct Display {
    std::vector<uint16_t> frame=std::vector<uint16_t>(320*240);
    unsigned next=0,calls=0,stage=0;
    const Pixel* pending=nullptr;unsigned h=0;
    void startWrite(){check(stage==0);stage=1;}
    void pushImage(int x,int y,int w,int rows,const Pixel* p){check(stage==1&&x==0&&y==int(next)&&w==320&&rows>0&&rows<=240-y);stage=2;pending=p;h=unsigned(rows);++calls;}
    void waitDMA(){check(stage==2);for(unsigned i=0;i<h*320;++i)frame[next*320+i]=pending[i].value;next+=h;pending=nullptr;stage=3;}
    void endWrite(){check(stage==3);stage=0;}
};
template<unsigned R,class Raster> unsigned compare(Raster raster,unsigned budget){
    TypedStrip<Pixel,320,240,R> batch{};Display d;
    std::vector<uint16_t> expected(320*240);
    for(unsigned y=0;y<240;++y)raster(y,expected.data()+320*y);
    check(batch.submit(0,0,raster,d)==0&&batch.submit(240,R,raster,d)==0&&d.calls==0);
    for(unsigned y=0;y<240;){const unsigned h=batch.submit(y,budget,raster,d);check(h&&h<=R&&h<=budget);y+=h;}
    check(d.next==240&&d.stage==0&&d.frame==expected);
    const unsigned step=std::min(R,budget);check(d.calls==(240+step-1)/step);
    return d.calls;
}
int main(){
    unsigned frames=0,calls=0;
    auto exercise=[&](auto raster){calls+=compare<4>(raster,4);calls+=compare<8>(raster,7);calls+=compare<4>(raster,2);frames+=3;};
    exercise([](unsigned y,uint16_t* p){const uint16_t colors[]={0xf800,0x07e0,0x001f,0x1234,0xabcd,0xffff,0};for(unsigned x=0;x<320;++x)p[x]=colors[(x+y)%7];});
    for(unsigned s=0;s<6;++s){gridseq::Engine e;gridseq::Control c;c.scene=s;c.masks=gridseq::scenes[s].masks;c.bpm=gridseq::scenes[s].bpm;c.play=true;c.accents=c.masks;std::atomic<uint32_t> head{0};std::array<int16_t,128> pcm{};
        for(unsigned i=0;i<64;++i){++c.seq;e.render(c,pcm.data(),pcm.size(),&head);if(i%16==0){auto h=head.load();exercise([&](unsigned y,uint16_t* p){gridseq::row(c,h,y,p);});}}
    }
    std::cout<<"PASS frames="<<frames<<" pixels="<<frames*76800u<<" calls="<<calls<<" scratch4="<<sizeof(TypedStrip<Pixel,320,240,4>)<<'\n';
}
