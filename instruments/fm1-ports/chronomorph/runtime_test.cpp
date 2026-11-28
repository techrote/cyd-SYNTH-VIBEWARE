#include "runtime.hpp"
#include <cassert>
#include <cstdio>
#include <thread>
using namespace chronocyd;
int main(){
 Input in;Runtime r;int16_t pcm[128]{};
 auto send=[&](bool d,int x,int y,uint32_t t){assert(in.send(d,x,y,t));r.pump(in,t);};
 send(true,170,220,0);assert(r.engine.playing);send(false,0,0,1);
 send(true,12,50,2);auto before=r.engine.s.masks[0];send(true,310,50,3);auto painted=r.engine.s.masks[0];assert(before!=painted);r.pump(in,153);assert(!r.touch.held&&r.inhibited&&r.engine.playing);
 send(true,170,220,154);assert(r.engine.playing&&r.engine.s.masks[0]==painted);send(false,0,0,155);send(true,170,220,156);assert(!r.engine.playing);send(false,0,0,157);
 // Reject expired pending presses, invalid coordinates, and require real release.
 assert(in.send(true,170,220,160));r.pump(in,310);assert(r.inhibited&&!r.engine.playing);send(true,170,220,311);assert(!r.engine.playing);send(false,0,0,312);send(true,170,220,313);assert(r.engine.playing);send(false,0,0,314);send(true,-1,220,315);assert(r.inhibited&&r.engine.playing);send(false,0,0,316);
 // Overflow drops the batch and stops the sequencer, not just the gesture.
 for(unsigned i=0;i<31;++i){assert(in.send(true,170,220,320));}assert(!in.send(true,170,220,320));assert(!in.send(false,0,0,320));r.render(in,320,pcm,128);assert(r.inhibited&&!r.engine.playing);for(auto x:pcm)assert(x==0);
 send(true,170,220,321);assert(!r.engine.playing);send(false,0,0,322);send(true,170,220,323);assert(r.engine.playing);send(false,0,0,324);
 in.emergency();r.render(in,325,pcm,128);assert(!r.engine.playing&&r.inhibited);for(auto x:pcm)assert(x==0);
 // Clock wrap and heartbeat keep a legitimate contact alive.
 send(false,0,0,0xfffffff0u);send(true,170,220,0xfffffff1u);send(true,170,220,5);r.pump(in,100);assert(r.touch.held&&!r.inhibited&&r.engine.playing);r.pump(in,155);assert(r.inhibited&&!r.touch.held);
 // A producer may stamp a contact just after the consumer captures now.
 send(false,0,0,156);assert(in.send(true,170,220,158));r.pump(in,157);assert(r.touch.held&&!r.inhibited);send(false,0,0,159);
 // A panic cell is an emergency even during an existing XY/paint contact.
 // Exercise an actual active voice, not just a playing flag.
 {
  Runtime d;Input c;uint32_t now=0;int16_t block[128]{};
  auto event=[&](bool down,int x,int y){bool accepted=c.send(down,x,y,++now);d.render(c,now,block,128);return accepted;};
  assert(event(true,170,220));assert(event(false,0,0));
  bool sound=false;for(unsigned i=0;i<120;++i){d.render(c,++now,block,128);sound|=std::any_of(block,block+128,[](int16_t v){return v!=0;});}assert(sound&&d.engine.playing);
  assert(event(true,50,10));assert(d.touch.held);
  auto bpm=d.engine.s.bpm;
  assert(c.send(true,270,10,++now)); // pending edit must be discarded
  bool accepted=c.send(true,280,200,++now);
  d.render(c,now,block,128);
  assert(!d.engine.playing&&d.inhibited&&!d.touch.held); // M3 fails here
  assert(!accepted&&d.engine.s.bpm==bpm);
  for(auto v:block)assert(v==0);
  assert(event(true,170,220));assert(!d.engine.playing); // still held
  assert(event(false,0,0));assert(event(true,170,220));assert(d.engine.playing);
  assert(event(false,0,0));assert(event(true,319,199));assert(d.engine.playing); // outside panic
  assert(event(false,0,0));assert(event(true,320,220));assert(d.engine.playing); // invalid, not panic
  assert(!event(true,319,239));assert(!d.engine.playing); // panic while inhibited
  assert(event(false,0,0));assert(event(true,170,220));assert(d.engine.playing);
  assert(event(false,0,0));assert(!event(true,300,220));assert(!d.engine.playing&&d.inhibited);
  std::puts("PASS: held and fresh panic, pending edit discard, exact bounds, inhibited panic, release-to-rearm and zero next PCM block");
 }
 // Queued and serialized gestures have identical engine output.
 Runtime q;Input controls;Engine e;Touch t;
 for(unsigned i=0;i<1000;++i){unsigned now=i*6;bool d=i%5!=4;int x=i%17==0?170:int(i%320),y=i%17==0?220:12;assert(controls.send(d,x,y,now));t.input(e,d,x,y);int16_t expected[128];e.render(expected,128);q.render(controls,now,pcm,128);assert(std::equal(pcm,pcm+128,expected));}
 Ring<unsigned,32> fifo;constexpr unsigned count=200000;std::thread writer([&]{for(unsigned i=0;i<count;++i)while(!fifo.push(i))std::this_thread::yield();});for(unsigned i=0;i<count;++i){unsigned n;while(!fifo.pop(n))std::this_thread::yield();assert(n==i);}writer.join();
 std::printf("PASS: expiry, wrap, overflow, panic, rearm; 128000 exact PCM samples; %u ordered concurrent events; Input=%zu Runtime=%zu\n",count,sizeof(Input),sizeof(Runtime));
}
