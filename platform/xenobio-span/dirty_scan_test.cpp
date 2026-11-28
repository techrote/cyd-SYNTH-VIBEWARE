#include "dirty_scan.hpp"
#include <cassert>
#include <cstdio>
#ifdef NDEBUG
#error Qualification requires assertions enabled
#endif
using namespace xenobio;
using cyd::platform::XenobioDirtyScan;
struct Fixture {
    XenobioDirtyScan scan;
    std::array<uint16_t,320*240> lcd{};
    std::array<uint16_t,320*4+2> scratch{};
    std::array<uint16_t,320> ref{};
    unsigned frames=0,calls=0;
    void transfer() {
        const int y=scan.next_row();assert(y>=0);
        scratch.front()=0xa55a;scratch.back()=0x5aa5;
        assert(scan.render_next(scratch.data()+1));
        assert(scratch.front()==0xa55a&&scratch.back()==0x5aa5);
        assert(!scan.commit(y+4));
        std::copy_n(scratch.data()+1,1280,lcd.data()+320*y);
        assert(scan.commit(y));++calls;
    }
    void verify(const Engine& e) {
        for(int y=0;y<240;++y) {labeled_row(e,y,ref.data());
            assert(std::equal(ref.begin(),ref.end(),lcd.data()+320*y));}
        assert(!scan.render_next(scratch.data()+1));assert(!scan.commit(-1));++frames;
    }
    unsigned show(const Engine& e) {
        const auto old=calls;assert(scan.begin(e));
        while(scan.next_row()>=0) transfer();
        verify(e);return calls-old;
    }
};
int main() {
    unsigned live_calls=0,live_frames=0;uint64_t pcm=0;
    Fixture f;Engine a,b;
    for(int s=0;s<6;++s) {
        a.reset(s);b.reset(s);f.scan.invalidate();
        const auto start=f.calls;assert(f.show(a)==60);
        for(int k=0;k<240;++k) {
            if(k%40==0) {const int x=(k/40)*47+5,y=40+(k%3)*45;a.touch(x,y,true);b.touch(x,y,true);}
            if(k%40==10) {a.release();b.release();}
            if(k%80==20) {a.touch(90,230,true);b.touch(90,230,true);}
            for(int j=0;j<1200;++j) {assert(a.sample()==b.sample());++pcm;}
            f.show(a);
        }
        const auto scene_calls=f.calls-start;
        live_calls+=scene_calls;live_frames+=241;
        std::printf("scene=%d calls=%u full=%u\n",s,scene_calls,241*60);
        a.stop();f.show(a);assert(f.show(a)==0);
    }
    // Exhaust quantized cell and marker transitions, including all edges.
    a.reset(0);f.show(a);
    for(int i=0;i<N;++i) {a.food[i]=uint8_t((a.food[i]+4)%101);f.show(a);
        a.touch(i%W*20+1,i/W*20+21,true);f.show(a);a.release();f.show(a);}
    // Coincident agents, slot order and all valid radius bins.
    for(auto& p:a.a) p={};
    f.show(a);
    for(int y=0;y<H;++y) for(int x=0;x<W;++x) {
        a.a[0]={x,y,100,0};a.a[1]={x,y,1,2};f.show(a);
        a.a[1].e=100;f.show(a);std::swap(a.a[0],a.a[1]);f.show(a);
        a.a[0].e=0;f.show(a);
    }
    // Sweep all radius transitions at the screen corners and center.
    for(int x:{0,7,15}) for(int y:{0,4,9}) {
        for(int e=0;e<=100;++e) {a.a[0]={x,y,e,1};f.show(a);}
        for(int e=100;e>=0;--e) {a.a[0]={x,y,e,2};f.show(a);}
    }
    // A partial frame must never be accepted as a complete baseline.
    a.reset(1);f.scan.invalidate();assert(f.scan.begin(a));f.transfer();
    b.reset(4);assert(!f.scan.begin(b));while(f.scan.next_row()>=0) f.transfer();f.verify(a);
    assert(f.scan.begin(b));f.transfer();f.scan.invalidate();
    f.lcd.fill(0xffff);assert(f.show(a)==60);assert(f.show(a)==0);
    std::printf("PASS frames=%u pixels=%llu pcm=%llu liveFrames=%u liveCalls=%u fullCalls=%u state=%zu\n",f.frames,static_cast<unsigned long long>(f.frames)*76800,static_cast<unsigned long long>(pcm),live_frames,live_calls,live_frames*60,sizeof(XenobioDirtyScan));
}
