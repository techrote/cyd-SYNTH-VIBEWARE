#include "runtime.hpp"
#include <cassert>
#include <iostream>
using namespace ecophon;
using Block=std::array<int16_t,128>;
static bool sound(const Block& b){return std::any_of(b.begin(),b.end(),[](int16_t v){return v!=0;});}
int main(){
    // Exact portable event ordering used by the Windows timer replay.
    Input in;Runtime rt;Block b{};Views views;Engine view;
    bool played=false,changed=false,stopped=false,restarted=false;
    unsigned silentBlocks=0;
    for(unsigned tick=1;tick<=68;++tick){
        const auto now=tick*16;
        if(tick==3){assert(in.submit(true,20,10,now));assert(in.submit(false,0,0,now));}
        if(tick==12){assert(in.submit(true,300,150,now));assert(in.submit(true,310,150,now));assert(in.submit(false,0,0,now));}
        if(tick==24){assert(in.submit(true,200,150,now));assert(!in.submit(true,300,230,now));}
        if(tick==40){assert(in.submit(true,180,10,now));assert(in.submit(false,0,0,now));}
        for(unsigned i=0;i<3;++i){
            rt.render(in,now,b.data(),b.size());views.publish(rt.engine);assert(views.latest(view));
            assert(view.muted==rt.engine.muted&&view.env==rt.engine.env);
            if(tick<24)played|=sound(b);
            if(tick==12)changed|=view.env.fertility>=240&&!view.muted;
            if(tick>=24&&tick<40){assert(view.muted&&!sound(b));stopped=true;++silentBlocks;}
            if(tick>=40)restarted|=sound(b);
        }
        if(tick==24)assert(in.submit(false,0,0,now));
    }
    assert(played&&changed&&stopped&&restarted&&silentBlocks==48);
    // A stale event and a queued release/restart in one pump can leave the
    // final engine unmuted. The old muted-edge backend misses this emergency.
    Input q;Runtime r;assert(q.submit(true,20,10,0));assert(q.submit(false,0,0,0));
    r.render(q,0,b.data(),b.size());assert(sound(b)&&!r.engine.muted);
    const auto before=r.panic_epoch;const bool wasMuted=r.engine.muted;
    assert(q.submit(true,100,150,1));assert(q.submit(false,0,0,151));assert(q.submit(true,180,10,151));
    r.render(q,151,b.data(),b.size());assert(sound(b)&&!r.engine.muted);
    assert(!(!wasMuted&&r.engine.muted));assert(r.panic_epoch==before+1);
    // Device queue full: service a pending request without waiting for a free
    // buffer. Pump acknowledges it; the epoch remains visible to the backend.
    auto serviced=r.panic_epoch;q.emergency();assert(q.fault.load(std::memory_order_acquire));
    r.pump(q,152);assert(!q.fault.load(std::memory_order_acquire));
    assert(r.panic_epoch!=serviced&&r.engine.muted);serviced=r.panic_epoch;
    r.render(q,152,b.data(),b.size());assert(!sound(b)&&r.panic_epoch==serviced);
    // Held-contact lease expiry is also a drain-worthy emergency.
    assert(q.submit(false,0,0,153));assert(q.submit(true,20,10,153));
    r.render(q,153,b.data(),b.size());assert(sound(b));serviced=r.panic_epoch;
    r.render(q,303,b.data(),b.size());assert(!sound(b)&&r.panic_epoch==serviced+1);
    std::cout<<"PASS 68 timer stages, "<<silentBlocks<<" silent blocks, restart, stale-event same-block restart generation, full-queue emergency and lease expiry\n";
}
