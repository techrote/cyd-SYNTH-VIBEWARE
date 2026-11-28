#pragma once
#include "core.hpp"
#include <atomic>
namespace portx {
// Single UI producer, single audio consumer. Unsigned counters wrap naturally.
struct Exchange {
  static constexpr uint32_t capacity=32;
  struct Event { uint32_t value=0, epoch=0; };
  std::array<Event,capacity> queue{};
  std::atomic<uint32_t> input{0},meter{0},panic{0},write{0},readpos{0},overflows{0};
  bool blocked=false; // producer-only: require release after overflow
  void stop(){input.store(0,std::memory_order_release);panic.fetch_add(1,std::memory_order_release);}
  void send(bool down,int x,int y){
    if(blocked){if(down)return;blocked=false;}
    uint32_t w=write.load(std::memory_order_relaxed);
    if(w-readpos.load(std::memory_order_acquire)>=capacity){
      overflows.fetch_add(1,std::memory_order_relaxed);blocked=down;stop();return;
    }
    uint32_t q=(uint32_t(down)<<17)|(uint32_t(std::clamp(y,0,239))<<9)|uint32_t(std::clamp(x,0,319));
    queue[w%capacity]={q,panic.load(std::memory_order_relaxed)};
    input.store(q,std::memory_order_release);write.store(w+1,std::memory_order_release);
  }
  void pump(Engine& e,uint32_t& last,uint32_t& alarm){
    auto sync=[&]{auto p=panic.load(std::memory_order_acquire);if(p!=alarm){e.panic();e.release();alarm=p;}};
    sync();const auto end=write.load(std::memory_order_acquire);
    // Snapshot bound: never wait for the producer or drain more than 32 events.
    while(last!=end){
      if(panic.load(std::memory_order_acquire)!=alarm)break;
      auto ev=queue[last%capacity];
      if(ev.epoch!=alarm&&panic.load(std::memory_order_acquire)!=alarm)break;
      ++last;readpos.store(last,std::memory_order_release);
      if(ev.epoch==alarm)e.input((ev.value>>17)&1,ev.value&511,(ev.value>>9)&255);
    }
    sync();
  }
  void discard(Engine& e,uint32_t& last,uint32_t& alarm){
    e.panic();e.release();alarm=panic.load(std::memory_order_acquire);
    last=write.load(std::memory_order_acquire);readpos.store(last,std::memory_order_release);
  }
  void publish(const View& v){uint32_t b=0;for(unsigned i=0;i<6;++i)b|=(v.level[i]>>4)<<(i*4);b|=(v.material<<24)|(uint32_t(v.coupled)<<26)|(uint32_t(v.held)<<27)|(uint32_t(v.rake)<<28);meter.store(b,std::memory_order_release);}
  View read(){auto b=meter.load(std::memory_order_acquire);View v;for(unsigned i=0;i<6;++i)v.level[i]=((b>>(i*4))&15)*17;v.material=(b>>24)&3;v.coupled=(b>>26)&1;v.held=(b>>27)&1;v.rake=(b>>28)&1;auto q=input.load(std::memory_order_acquire);v.x=q&511;v.y=(q>>9)&255;return v;}
};
static_assert(std::atomic<uint32_t>::is_always_lock_free,"32-bit snapshot atomics required");
}
