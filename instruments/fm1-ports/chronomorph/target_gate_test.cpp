#include "core.hpp"
#include "target_gate.hpp"
#include <cassert>
#include <cstdio>
using namespace chronocyd;
struct Harness {
 Engine e;Touch t;unsigned emergencyCount=0,events=0;bool refuse=false;
 void emergency(){++emergencyCount;e.stop();t.cancel();}
 bool send(bool d,int x,int y,uint32_t){if(refuse)return false;++events;t.input(e,d,x,y);return true;}
};
int main(){
 TargetGate g;Harness h;uint32_t now=0;
 auto poll=[&](bool b,bool d=false,int x=170,int y=220,bool f=false){g.poll(h,b,d,x,y,now+=12,f);};
 for(int i=0;i<9;++i)poll(true,true);
 assert(!g.armed&&!h.e.playing&&h.events==0);
 for(int i=0;i<3;++i)poll(false,true);
 for(int i=0;i<3;++i)poll(true,true);
 assert(g.armed&&g.awaitRelease&&h.emergencyCount==1);
 for(int i=0;i<9;++i)poll(true,true);
 assert(!h.e.playing&&h.events==0);
 poll(false);poll(false,true);assert(h.e.playing);
 poll(false);poll(false,true,300,220);assert(!h.e.playing&&g.awaitRelease);
 poll(false,true);assert(!h.e.playing);
 h.refuse=true;poll(false);assert(g.awaitRelease);h.refuse=false;poll(false);poll(false,true);assert(h.e.playing);
 for(int i=0;i<3;++i)poll(true,true);
 assert(!g.armed&&!h.e.playing);
 for(int i=0;i<3;++i)poll(false);
 for(int i=0;i<3;++i)poll(true);
 poll(false);poll(false,true);assert(h.e.playing);
 poll(false,true,170,220,true);assert(!g.armed&&!h.e.playing);
 assert(dacWord(0)==0x8000&&dacWord(-32768)==0x4000&&dacWord(32767)==0xbf00);
 for(int v=-32768;v<32767;++v)assert(dacWord(int16_t(v))<=dacWord(int16_t(v+1)));
 std::printf("PASS boot-held refusal, release-to-arm, panic, refused release, disarm, fault and 65535 adjacent DAC pairs; Gate=%zu B\n",sizeof(TargetGate));
}
