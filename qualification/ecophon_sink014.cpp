// Independent QA: build against exact Ecophon M4 source headers, not a HAL simulation.
// Sequential interleaving models same-core ISR scheduling at API boundaries only.
#include "runtime.hpp"
#include "sample_sink.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <deque>
using namespace ecophon;
static uint64_t checks=0;
static void check(bool b){++checks;if(!b){std::fprintf(stderr,"FAIL check %llu\n",(unsigned long long)checks);std::abort();}}
static uint32_t random_word(uint32_t& s){s^=s<<13;s^=s>>17;s^=s<<5;return s;}
int main(){
    SampleSink sink; Runtime rt; Input in;
    std::array<int16_t,128> pcm{};std::deque<uint8_t> expected;
    uint32_t seed=0xc9a531f2u, now=0xfffffff0u;
    unsigned blocks=0, ticks=0, nonmid=0, refused=0, resets=0;
    check(in.submit(false,0,0,now));check(in.submit(true,20,10,now));
    rt.pump(in,now,[]{});
    for(unsigned i=0;i<250000;++i){
        auto op=random_word(seed)%32;
        if(op<6){
            const bool can=expected.size()<=384;
            check(sink.ready()==can);
            if(can){rt.engine.render(pcm.data(),pcm.size());check(sink.push(pcm.data()));++blocks;
                for(auto v:pcm)expected.push_back(uint8_t((32768+int(v)/2)/256));}
            else {check(!sink.push(pcm.data()));++refused;}
        }else if(op==6){
            // Fault before pump: every tick must produce midpoint, even with queued sound.
            in.emergency();for(unsigned k=0;k<5;++k)check(sink.tick(true)==128);
            auto prev=rt.panic_epoch;
            rt.pump(in,now,[&]{check(in.fault.load()!=0);sink.invalidate();check(!sink.ready());});
            check(rt.panic_epoch==prev+1);check(!sink.push(pcm.data()));
            if(random_word(seed)&1){sink.invalidate();check(!sink.ready());}
            check(sink.tick(false)==128);expected.clear();check(sink.ready());++resets;
            check(in.submit(false,0,0,now));check(in.submit(true,int((random_word(seed)%4)*80+20),10,now));
            rt.pump(in,now,[]{});check(!rt.engine.muted);
        }else {
            const auto n=random_word(seed)%63+1;
            for(unsigned k=0;k<n;++k){uint8_t want=128;if(!expected.empty()){want=expected.front();expected.pop_front();}
                auto got=sink.tick(false);check(got==want);nonmid+=got!=128;++ticks;}
        }
        ++now;
    }
    // Test lease expiry on both sides of wrap and with release/restart coalesced.
    for(uint32_t start: {0u,0xffffff80u}){
        for(unsigned offset: {0u,127u,255u,383u,511u}){
            SampleSink q;Runtime r;Input e;
            check(e.submit(false,0,0,start));check(e.submit(true,20,10,start));r.pump(e,start,[]{});
            for(int b=0;b<4;++b){r.engine.render(pcm.data(),128);check(q.push(pcm.data()));}
            for(unsigned j=0;j<offset;++j)(void)q.tick(false);
            auto prior=r.panic_epoch;unsigned hooks=0;
            r.pump(e,start+149,[&]{++hooks;q.invalidate();});check(hooks==0&&r.panic_epoch==prior);
            check(e.submit(false,0,0,start+150));check(e.submit(true,180,10,start+150));
            r.pump(e,start+150,[&]{++hooks;q.invalidate();});check(hooks==1&&r.panic_epoch==prior+1&&!r.engine.muted);
            check(!q.push(pcm.data()));check(q.tick(false)==128);for(int j=0;j<512;++j)check(q.tick(false)==128);
            r.engine.render(pcm.data(),128);check(q.push(pcm.data()));
            for(auto v:pcm)check(q.tick(false)==uint8_t((32768+int(v)/2)/256));
        }
    }
    check(nonmid>1000&&refused>0&&resets>0);
    std::printf("PASS %llu checks; %u blocks, %u ordered ticks (%u nonmidpoint), %u full refusals, %u resets; 10 lease/wrap/restart cases\n",(unsigned long long)checks,blocks,ticks,nonmid,refused,resets);
}
