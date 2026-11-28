#include "target_gate.hpp"
#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace ecophon;
using PCM=std::array<int16_t,128>;
static unsigned checks=0;
static void require(bool ok){++checks;if(!ok){std::cerr<<"FAIL check "<<checks<<'\n';std::exit(1);}}
static bool audible(const PCM& p){return std::any_of(p.begin(),p.end(),[](int16_t x){return x!=0;});}
static void render(Runtime& rt,Input& in,uint32_t now,PCM& p){rt.render(in,now,p.data(),p.size());}
static void start(Runtime& rt,Input& in,uint32_t now,PCM& p){
    require(in.submit(true,20,10,now));require(in.submit(false,0,0,now));
    render(rt,in,now,p);require(audible(p)&&!rt.engine.muted);
}
int main(){
    // Every queue occupancy at the emergency boundary, including exactly full.
    // The runtime must clear pending controls, retain a durable epoch and require
    // a release before any deliberate restart. Repeat near uint32 wrap.
    unsigned schedules=0;
    for(uint32_t origin: {uint32_t(1000),std::numeric_limits<uint32_t>::max()-20}){
        for(unsigned occupancy=0;occupancy<=32;++occupancy){
            Input in;Runtime rt;PCM p{};start(rt,in,origin,p);
            for(unsigned i=0;i<occupancy;++i)require(in.submit(true,180,10,origin+1));
            const auto before=rt.panic_epoch;in.emergency();
            render(rt,in,origin+2,p);require(!audible(p)&&rt.engine.muted&&rt.inhibited);
            require(rt.panic_epoch==before+1);require(!in.fault.load());
            Contact c{};require(!in.queue.pop(c));
            require(in.submit(true,180,10,origin+3));render(rt,in,origin+3,p);require(!audible(p));
            require(in.submit(false,0,0,origin+4));start(rt,in,origin+5,p);
            ++schedules;
        }
        // A stale event and fresh restart share a pump; a final mute-edge test
        // would miss the drain, but the persistent epoch must not disappear.
        Input in;Runtime rt;PCM p{};start(rt,in,origin,p);
        const auto epoch=rt.panic_epoch;
        require(in.submit(true,100,150,origin+1));
        require(in.submit(false,0,0,origin+151));
        require(in.submit(true,180,10,origin+151));
        render(rt,in,origin+151,p);require(audible(p)&&!rt.engine.muted);
        require(rt.panic_epoch==epoch+1);
        // Exactly 149 ms remains held; at 150 ms lease expires, including wrap.
        render(rt,in,origin+300,p);require(audible(p)&&rt.touch.held);
        render(rt,in,origin+301,p);require(!audible(p)&&rt.inhibited);
        require(rt.panic_epoch==epoch+2);
    }
    // Target gate: BOOT arming with no audio-owner servicing until after arm.
    // Held touch and refused release cannot arm sound or bypass the fresh edge.
    unsigned targetCases=0;
    for(unsigned delay=0;delay<=32;++delay){
        Input in;Runtime rt;TargetGate gate;PCM p{};uint32_t now=0xfffffff0u;
        auto poll=[&](bool boot,bool down,int x=20,int y=10,bool fault=false){gate.poll(in,boot,down,x,y,now+=12,fault);};
        for(unsigned i=0;i<3;++i)poll(false,true);
        for(unsigned i=0;i<3;++i)poll(true,true);
        require(gate.armed&&gate.releaseRequired&&in.fault.load());
        for(unsigned i=0;i<delay;++i)poll(false,true);
        render(rt,in,now,p);require(!audible(p));
        for(unsigned i=0;i<3;++i){poll(false,true);render(rt,in,now,p);require(!audible(p));}
        poll(false,false);render(rt,in,now,p);require(!audible(p)&&!gate.releaseRequired);
        poll(false,true);render(rt,in,now,p);require(audible(p));
        poll(false,true,300,230);render(rt,in,now,p);require(!gate.armed&&!audible(p));
        for(unsigned i=0;i<3;++i)poll(false,false);
        for(unsigned i=0;i<3;++i)poll(true,false);
        render(rt,in,now,p);require(gate.armed&&!audible(p));
        poll(false,false,20,10,true);render(rt,in,now,p);require(gate.failed&&!gate.armed&&!audible(p));
        ++targetCases;
    }
    std::cout<<"PASS "<<checks<<" checks; "<<schedules<<" queue schedules; "<<targetCases<<" delayed target arming schedules; rollover and same-pump restart\n";
}
