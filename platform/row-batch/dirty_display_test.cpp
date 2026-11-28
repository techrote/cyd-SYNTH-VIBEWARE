#include "dirty_display.hpp"
#include <cstdlib>
#include <iostream>
#include <vector>
using namespace gridseq;
using cyd::platform::DirtyDisplay;
static void check(bool v){if(!v)std::abort();}
struct Pixel { uint16_t value=0; Pixel& operator=(uint16_t p){value=p;return *this;} };
struct LCD {
    std::vector<uint16_t> frame=std::vector<uint16_t>(width*height,0xdead);
    unsigned stage=0,y=0,n=0,calls=0,rows=0;
    const Pixel* pending=nullptr;
    void startWrite(){check(stage==0);stage=1;}
    void pushImage(int x,int yy,int w,int h,const Pixel* p){
        check(stage==1&&x==0&&yy>=0&&w==int(width)&&h>0&&h<=4&&yy+h<=int(height));
        stage=2;y=unsigned(yy);n=unsigned(h);pending=p;++calls;rows+=n;
    }
    void waitDMA(){check(stage==2&&pending);for(unsigned i=0;i<n*width;++i)frame[y*width+i]=pending[i].value;pending=nullptr;stage=3;}
    void endWrite(){check(stage==3);stage=0;}
};
int main(){
    DirtyDisplay<Pixel> d;LCD lcd;Engine e,baseline;Surface s;std::atomic<uint32_t> head{0},other{0};
    std::array<int16_t,block> pcm{},reference{};std::vector<uint16_t> full(width*height);
    unsigned frames=0;uint64_t pixels=0,samples=0;
    auto compare=[&](const Control& c,uint32_t h){
        for(unsigned y=0;y<height;++y)row(c,h,y,full.data()+y*width);
        check(lcd.stage==0&&!d.busy()&&lcd.frame==full);++frames;pixels+=full.size();
    };
    auto frame=[&](const Control& c,uint32_t h){
        unsigned before=lcd.rows;check(d.begin(c,h));
        while(d.busy()){check(!d.begin(c,h));unsigned n=d.step(lcd);check(n&&n<=4);}
        check(d.step(lcd)==0);compare(c,h);return lcd.rows-before;
    };
    check(frame(s.c,0)==240);
    for(unsigned i=0;i<20;++i){++s.c.seq;check(frame(s.c,0)==0);}
    s.touch(true,52,30);s.touch(false,0,0);check(frame(s.c,0)==44);
    s.touch(true,180,220);s.touch(false,0,0);check(frame(s.c,0)==40);
    s.touch(true,20,220);s.touch(false,0,0);
    const unsigned startRows=lcd.rows,startCalls=lcd.calls,startFrames=frames;
    for(unsigned i=0;i<960;++i){
        if(i%40==0){unsigned lane=(i/40)%4;s.touch(true,52+int(i%260),30+44*lane);s.touch(false,0,0);}
        if(i%96==0){s.touch(true,int(i%320),10);s.touch(false,0,0);}
        if(i%67==0){s.touch(true,10,52+44*((i/67)%4));s.touch(false,0,0);}
        ++s.c.seq;e.render(s.c,pcm.data(),pcm.size(),&head);baseline.render(s.c,reference.data(),reference.size(),&other);
        check(pcm==reference&&head.load()==other.load());samples+=pcm.size();
        if(i%4==0)frame(s.c,head.load());
    }
    const unsigned liveRows=lcd.rows-startRows,liveCalls=lcd.calls-startCalls,liveFrames=frames-startFrames;
    // Partial transfer must not commit the previous frame. Force redraw after reset.
    d.invalidate();check(d.begin(s.c,head.load()));check(d.step(lcd)==4);
    d.invalidate();std::fill(lcd.frame.begin(),lcd.frame.end(),0xdead);check(frame(s.c,head.load())==240);
    // Snapshot ownership: changes after begin cannot alter a pending frame.
    d.invalidate();Control captured=s.c;uint32_t h=head.load();check(d.begin(s.c,h));
    check(d.step(lcd)==4);s.c.scene=(s.c.scene+1)%6;s.c.masks[0]^=1;s.c.play=!s.c.play;
    while(d.busy())check(d.step(lcd)>0);
    compare(captured,h);frame(s.c,0);
    std::cout<<"PASS frames="<<frames<<" pixels="<<pixels<<" pcm="<<samples<<" liveFrames="<<liveFrames
             <<" liveRows="<<liveRows<<" liveCalls="<<liveCalls<<" state="<<sizeof(d)<<'\n';
}
