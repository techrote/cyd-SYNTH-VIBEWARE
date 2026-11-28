#include "audio_owner.hpp"
#include <cassert>
#include <deque>
#include <iostream>
using namespace phosphor;
using cyd_platform::ClockSink;
int main(){
 ClockSink sink;AudioOwner a(sink);BoardSession ref;
 std::array<int16_t,128> pcm{};std::array<uint16_t,4*320> strip{};
 std::deque<uint8_t> expected;
 uint64_t now=0;unsigned codes=0,nonzero=0,pixels=0,neutral=0,frames=0;
 auto tickNeutral=[&](unsigned n){while(n--){assert(sink.tick()==128);++neutral;}};
 auto poll=[&](bool valid,bool touch,int x,int y){a.poll(valid,touch,x,y,now);ref.poll(valid,touch,x,y,now);};
 auto block=[&](){assert(a.service());ref.clock_block(pcm.data());for(auto v:pcm)expected.push_back(ClockSink::encode(v));};
 auto drain=[&](unsigned n){while(n--){assert(!expected.empty());auto v=sink.tick();assert(v==expected.front());expected.pop_front();++codes;nonzero+=v!=128;}};
 auto select=[&](int scene){poll(true,false,0,0);poll(true,true,scene*53+5,5);poll(true,false,0,0);};
 tickNeutral(1);assert(!a.service());select(0);assert(!a.service());tickNeutral(1);
 poll(true,true,120,100);a.advance(now);ref.advance(now);block();tickNeutral(8);block();block();assert(sink.queued()==384);assert(!a.service());drain(128);
 for(unsigned i=0;i<1800;++i){
  now+=5333; if(i%3==0)poll(true,true,10+int(i%290),40+int(i%170));
  assert(a.advance(now)==ref.advance(now));drain(128);block();
  if(i%30==0){a.capture();ref.capture();std::array<uint16_t,4*320> comparison{};for(int y=0;y<240;y+=4){assert(a.strip(y,4,strip.data()));assert(ref.strip(y,4,comparison.data()));assert(strip==comparison);pixels+=strip.size();}++frames;}
 }
 assert(nonzero>10000);
 // Clear is a non-muted reset. Do not let pre-clear PCM survive, including
 // a scene selection before the interrupt acknowledges the invalidation.
 poll(true,false,0,0);poll(true,true,150,230);assert(!a.visual().muted());assert(!a.service());
 select(2);assert(!a.service());expected.clear();tickNeutral(1);assert(sink.queued()==0);
 block();block();drain(256);
 // Queue real music again, then drag into Panic; explicit scene required.
 poll(true,true,120,100);for(int i=0;i<30;++i){now+=5333;poll(true,true,120,100);a.advance(now);ref.advance(now);if(i==0){block();block();}else{drain(128);block();}}
 assert(sink.queued()==256);unsigned pending=0;for(auto v:expected)pending+=v!=128;assert(pending>0);
 poll(true,true,300,235);assert(a.visual().muted());assert(!a.service());expected.clear();tickNeutral(300);
 poll(true,false,0,0);poll(true,true,120,100);assert(!a.service());tickNeutral(10);
 select(1);tickNeutral(1);block();block();drain(256);tickNeutral(1);assert(sink.faulted());
 // A starvation fault cannot restart by ordinary contact or release.
 assert(!a.service());assert(a.visual().muted());tickNeutral(1);a.poll(true,false,0,0,now);a.poll(true,true,120,100,now);assert(!a.service());
 a.poll(true,false,0,0,now);a.poll(true,true,10,10,now);tickNeutral(1);assert(a.service());assert(a.service());
 // Invalid acquisition and contact lease cancellation must purge queue.
 a.poll(false,true,0,0,now);assert(!a.service());tickNeutral(300);a.poll(true,false,0,0,now);a.poll(true,true,10,10,now);tickNeutral(1);assert(a.service());assert(a.service());
 now+=70000;a.advance(now);assert(a.visual().muted());assert(!a.service());tickNeutral(300);
 // No visual heartbeat for 6000 generated samples: sample-side fault must
 // inhibit queued audio immediately, without waiting for the next UI tick.
 a.poll(true,false,0,0,now);a.poll(true,true,10,10,now);tickNeutral(1);assert(a.service());assert(a.service());
 bool stopped=false;for(int i=0;i<60;++i){for(int j=0;j<128;++j)sink.tick();if(!a.service()){stopped=true;break;}}
 assert(stopped&&a.visual().muted());tickNeutral(300);
 std::cout<<"audio owner PASS codes="<<codes<<" nonneutral="<<nonzero<<" neutral="<<neutral<<" pending="<<pending<<" frames="<<frames<<" pixels="<<pixels<<" owner_bytes="<<sizeof(AudioOwner)<<" sink_bytes="<<sizeof(ClockSink)<<'\n';
}
