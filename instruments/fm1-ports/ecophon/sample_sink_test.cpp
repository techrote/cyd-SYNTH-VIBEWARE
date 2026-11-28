#include "runtime.hpp"
#include "sample_sink.hpp"
#include <cassert>
#include <cstdio>
using namespace ecophon;
int main(){
    Runtime rt; Input in; SampleSink sink; int16_t pcm[128]{};
    assert(in.submit(false,0,0,0));assert(in.submit(true,20,10,0));
    unsigned nonmid=0, compared=0;
    for(unsigned i=0;i<2000;++i){
        rt.render(in,i,pcm,128);assert(sink.push(pcm));
        for(auto v:pcm){auto c=sink.tick(false);assert(c==SampleSink::encode(v));nonmid+=c!=128;++compared;}
        assert(sink.tick(false)==128);
        if(i%100==0){assert(in.submit(false,0,0,i));assert(in.submit(true,20,10,i));}
    }
    assert(nonmid>1000);
    // Actual sound fills all four blocks; for every possible partial drain,
    // panic invalidates every queued code and inhibits fresh publication until ack.
    for(unsigned consumed=0;consumed<=512;++consumed){
        SampleSink q;Runtime r;Input events;
        assert(events.submit(false,0,0,0));assert(events.submit(true,20,10,0));
        for(unsigned b=0;b<4;++b){r.render(events,b,pcm,128);assert(q.push(pcm));}
        assert(!q.push(pcm));
        for(unsigned n=0;n<consumed;++n)(void)q.tick(false);
        events.emergency();assert(q.tick(true)==128);
        const auto epoch=r.panic_epoch;
        r.pump(events,5,[&]{assert(events.fault.load());q.invalidate();assert(!q.ready());});
        r.engine.render(pcm,128);assert(r.panic_epoch!=epoch);
        assert(!q.push(pcm));assert(q.tick(false)==128);
        for(unsigned n=0;n<512;++n)assert(q.tick(false)==128);
        assert(events.submit(false,0,0,6));assert(events.submit(true,20,10,6));
        r.render(events,6,pcm,128);assert(q.push(pcm));
        for(auto v:pcm)assert(q.tick(false)==SampleSink::encode(v));
        assert(q.tick(false)==128);
    }
    // Multiple resets before acknowledgement and midpoint underflow.
    sink.invalidate();sink.invalidate();assert(!sink.ready());assert(sink.tick(false)==128);assert(sink.ready());
    for(int v=-32768;v<=32767;++v){const auto c=SampleSink::encode(int16_t(v));assert(c>=64&&c<=191);}
    std::printf("PASS %u exact DAC codes, %u nonmidpoint, 513 panic occupancies, 65536 bounded conversions, sink %zu B\n",compared,nonmid,sizeof(SampleSink));
}
