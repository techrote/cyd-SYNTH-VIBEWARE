#include "audio_owner.hpp"
#include "clock_sink.hpp" // exact external PLATFORM P14 pin; see M5.md
#include <cassert>
#include <iostream>
using namespace grimoire;
using Sink=cyd_platform::ClockSink;
int main(){
    Sink sink;AudioOwner<Sink> owner(sink);BoardSession reference;
    std::uint64_t compared=0,nonneutral=0,neutral=0,frames=0;
    std::array<std::int16_t,128> pcm{};
    auto tickNeutral=[&](unsigned n){while(n--){assert(sink.tick()==Sink::midpoint);++neutral;}};
    auto poll=[&](bool valid,bool down,int x,int y,std::uint32_t now){owner.poll(valid,down,x,y,now);reference.poll(valid,down,x,y,now);};
    auto arm=[&](){assert(owner.arm());assert(reference.arm());tickNeutral(1);};
    auto fill=[&](std::uint32_t now){assert(owner.fill(now));assert(!reference.audio(now,pcm.data(),pcm.size()));};
    // Reference PCM is generated at exactly the same admitted block boundary.
    auto compare=[&](const std::array<std::int16_t,128>& block){for(auto v:block){auto code=sink.tick();assert(code==Sink::encode(v));++compared;nonneutral+=code!=Sink::midpoint;}};
    auto prime=[&](std::uint32_t now){fill(now);auto first=pcm;tickNeutral(4);fill(now);assert(sink.queued()==256);compare(first);compare(pcm);};
    assert(!owner.arm());tickNeutral(1);arm();
    poll(true,true,180,100,0);assert(!owner.fill(0)); // held before release
    poll(true,false,0,0,1);poll(true,true,180,100,2);prime(2);
    PanelRaster raster;std::array<std::uint16_t,width*4> pixels{};
    // 2400 real engine blocks, with matched input and occasional full panels.
    // Queue always has one look-ahead block at each consumer boundary.
    fill(3);auto previous=pcm;
    for(unsigned i=0;i<2400;++i){
        const auto now=4u+i*5u;
        poll(true,true,10+int(i%300),40+int(i%160),now);
        fill(now);compare(previous);previous=pcm;
        if(i%40==0){raster.begin(owner.frame());for(int y=0;y<height;y+=4)assert(raster.render(y,4,pixels.data()));++frames;}
    }
    compare(previous);assert(nonneutral>100000);
    auto queue=[&](std::uint32_t now){fill(now);fill(now);fill(now);assert(sink.queued()==384);assert(!owner.fill(now));};
    auto flushed=[&](std::uint32_t now){assert(!owner.fill(now));tickNeutral(385);assert(sink.queued()==0);assert(!owner.fill(now));};
    // Panic on a drag, material selection, sketch entry, orbit stop, invalid
    // acquisition, expired contact and explicit disarm all invalidate real PCM.
    std::uint32_t now=12010;queue(now);poll(true,true,300,10,now);flushed(now);
    poll(true,false,0,0,++now);poll(true,true,160,100,++now);prime(now);queue(now);
    poll(true,false,0,0,++now);poll(true,true,80,10,++now);flushed(now);
    poll(true,false,0,0,++now);poll(true,true,180,100,++now);prime(now);queue(now);
    poll(true,false,0,0,++now);poll(true,true,185,220,++now);flushed(now);
    // Leave sketch via factory path, then latch orbit and stop it.
    poll(true,false,0,0,++now);poll(true,true,10,220,++now);poll(true,false,0,0,++now);
    poll(true,true,200,10,++now);prime(now);queue(now);
    poll(true,false,0,0,++now);poll(true,true,200,10,++now);flushed(now);
    poll(true,false,0,0,++now);poll(true,true,160,100,++now);prime(now);queue(now);
    poll(false,false,0,0,++now);flushed(now);
    poll(true,false,0,0,++now);poll(true,true,160,100,++now);prime(now);queue(now);
    now+=100;owner.service(now);assert(reference.expire(now));flushed(now);
    poll(true,true,160,100,++now);assert(!owner.fill(now)); // must release
    poll(true,false,0,0,++now);poll(true,true,160,100,++now);prime(now);queue(now);
    owner.disarm();reference.disarm();flushed(now);assert(!owner.armed());
    // Faulted consumer stays muted; release or a touch cannot restart it.
    arm();poll(true,false,0,0,++now);poll(true,true,160,100,++now);prime(now);
    tickNeutral(1);assert(sink.faulted());owner.service(now);reference.disarm();
    tickNeutral(1);assert(!owner.armed()&&!sink.faulted());
    poll(true,false,0,0,++now);poll(true,true,160,100,++now);assert(!owner.fill(now));tickNeutral(32);
    arm();poll(true,true,160,100,++now);assert(!owner.fill(now));
    poll(true,false,0,0,++now);poll(true,true,160,100,++now);prime(now);
    owner.disarm();reference.disarm();tickNeutral(1);
    std::cout<<"PASS exact_codes="<<compared<<" nonneutral="<<nonneutral<<" neutral="<<neutral<<" rendered_frames="<<frames<<" owner_bytes="<<sizeof(owner)<<'\n';
}
