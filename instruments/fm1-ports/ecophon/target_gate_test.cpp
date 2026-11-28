#include "target_gate.hpp"
#include <cassert>
#include <cstdio>
using namespace ecophon;
int main(){
    Input in; Runtime r; TargetGate g; uint32_t now=0; int16_t pcm[128]{};
    auto poll=[&](bool b,bool d=false,int x=20,int y=10,bool fault=false){
        g.poll(in,b,d,x,y,now+=12,fault);r.render(in,now,pcm,128);
        return std::any_of(pcm,pcm+128,[](int16_t v){return v!=0;});
    };
    for(int i=0;i<10;++i)assert(!poll(true,true));
    assert(!g.armed);
    auto arm=[&]{for(int i=0;i<3;++i)assert(!poll(false,true));for(int i=0;i<3;++i)assert(!poll(true,true));assert(g.armed&&g.releaseRequired);for(int i=0;i<4;++i)assert(!poll(true,true));for(int i=0;i<4;++i)assert(!poll(false,true));assert(!poll(false));assert(poll(false,true));};
    arm();assert(poll(false,false)); // autonomous drone
    assert(!poll(false,true,300,230));assert(!g.armed&&r.engine.muted);
    assert(!poll(false,true));arm();
    for(int i=0;i<2;++i)poll(true,true);
    assert(!poll(true,true));assert(!g.armed);
    arm();assert(!poll(false,true,20,10,true));assert(g.failed&&!g.armed);
    for(int i=0;i<3;++i)poll(false);
    for(int i=0;i<3;++i)assert(!poll(true));
    assert(!g.armed);
    for(int v=-32768;v<32767;++v)assert(dacWord(int16_t(v))<=dacWord(int16_t(v+1)));
    assert(dacWord(-32768)==0x4000&&dacWord(0)==0x8000&&dacWord(32767)==0xbf00);
    assert(touchAxis(0,320)==0&&touchAxis(4095,320)==319);
    assert(touchAxis(200,240)==0&&touchAxis(3900,240)==239);
    // Idle release polls cannot fill the event FIFO.
    TargetGate idle;Input idleIn;idle.armed=true;idle.history=0;
    for(unsigned i=0;i<1000;++i)idle.poll(idleIn,false,false,0,0,i,false);
    Contact c{};assert(idleIn.queue.pop(c)&&!c.down&&!idleIn.queue.pop(c));
    // Saturation while audio is not servicing the queue must disarm.
    TargetGate overflow;Input overIn;Runtime overRt;
    overflow.armed=true;overflow.releaseRequired=false;overflow.history=0;
    for(unsigned i=0;i<32;++i)overflow.poll(overIn,false,true,20,10,i,false);
    assert(overflow.armed);
    overflow.poll(overIn,false,true,20,10,32,false);
    overRt.render(overIn,32,pcm,128);
    assert(!overflow.armed&&overRt.engine.muted&&overRt.inhibited);
    // Every invalid boundary/panic coordinate must disarm, even before release.
    unsigned n=0;
    for(int y=-1;y<=240;++y)for(int x=-1;x<=320;++x){
        if(x>=0&&x<320&&y>=0&&y<240&&!(x>=280&&y>=216))continue;
        TargetGate gate;Input input;Runtime rt;gate.armed=true;gate.history=0;
        gate.poll(input,false,true,x,y,0,false);rt.render(input,0,pcm,128);
        assert(!gate.armed&&rt.engine.muted);++n;
    }
    std::printf("PASS arming, held contact, panic, sticky driver fault, %u boundary coordinates, 65535 DAC pairs; gate %zu B\n",n,sizeof(TargetGate));
}
