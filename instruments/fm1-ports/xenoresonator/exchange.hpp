#pragma once
#include "core.hpp"
#include <atomic>
namespace portx {
struct Exchange {
  std::atomic<uint32_t> input{0},meter{0},panic{0};
  uint32_t seq=0;
  void send(bool down,int x,int y){input.store(((++seq&16383u)<<18)|(uint32_t(down)<<17)|(uint32_t(std::clamp(y,0,239))<<9)|uint32_t(std::clamp(x,0,319)),std::memory_order_release);}
  void stop(){send(false,0,0);panic.fetch_add(1,std::memory_order_release);}
  void pump(Engine& e,uint32_t& last,uint32_t& alarm){auto p=panic.load(std::memory_order_acquire);auto q=input.load(std::memory_order_acquire);if(p!=alarm){e.panic();e.release();alarm=p;last=q;return;}if(q!=last){e.input((q>>17)&1,q&511,(q>>9)&255);last=q;}}
  void publish(const View& v){uint32_t b=0;for(unsigned i=0;i<6;++i)b|=(v.level[i]>>4)<<(i*4);b|=(v.material<<24)|(uint32_t(v.coupled)<<26)|(uint32_t(v.held)<<27);meter.store(b,std::memory_order_release);}
  View read(){auto b=meter.load(std::memory_order_acquire);View v;for(unsigned i=0;i<6;++i)v.level[i]=((b>>(i*4))&15)*17;v.material=(b>>24)&3;v.coupled=(b>>26)&1;v.held=(b>>27)&1;auto q=input.load(std::memory_order_acquire);v.x=q&511;v.y=(q>>9)&255;return v;}
};
static_assert(std::atomic<uint32_t>::is_always_lock_free,"32-bit snapshot atomics required");
}
