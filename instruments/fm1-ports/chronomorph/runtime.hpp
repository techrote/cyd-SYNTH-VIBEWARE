#pragma once
#include "core.hpp"
#include <atomic>
namespace chronocyd {
// Exactly one producer and one consumer; one slot remains empty.
template<class T,unsigned N> struct Ring {
 static_assert(N>1 && std::atomic<unsigned>::is_always_lock_free,"bounded lock-free indices required");
 std::array<T,N> slots{};
 std::atomic<unsigned> head{0},tail{0};
 bool push(const T& v){auto h=head.load(std::memory_order_relaxed);auto n=(h+1)%N;if(n==tail.load(std::memory_order_acquire))return false;slots[h]=v;head.store(n,std::memory_order_release);return true;}
 bool pop(T& v){auto t=tail.load(std::memory_order_relaxed);if(t==head.load(std::memory_order_acquire))return false;v=slots[t];tail.store((t+1)%N,std::memory_order_release);return true;}
 void discard(){tail.store(head.load(std::memory_order_acquire),std::memory_order_release);}
};
struct Contact {uint32_t ms;int x,y;bool down;};
struct Input {
 Ring<Contact,32> q;
 std::atomic<unsigned> fault{0};
 // Producer only, including emergency. Overflow and panic inhibit enqueue until acknowledged.
 bool send(bool down,int x,int y,uint32_t ms){if(fault.load(std::memory_order_acquire))return false;if(q.push({ms,x,y,down}))return true;emergency();return false;}
 void emergency(){fault.store(1,std::memory_order_release);}
 // Consumer only. Producer cannot enqueue while fault is set.
 bool recover(){if(!fault.load(std::memory_order_acquire))return false;q.discard();fault.store(0,std::memory_order_release);return true;}
};
struct Runtime {
 static constexpr uint32_t lease_ms=150;
 Engine engine;Touch touch;bool inhibited=false;uint32_t last=0;
 static bool stale(uint32_t now,uint32_t then){uint32_t age=now-then;return age>=lease_ms&&age<0x80000000u;}
 void panic(){engine.stop();touch.cancel();inhibited=true;}
 void expire(){touch.cancel();inhibited=true;}
 void pump(Input& in,uint32_t now){
  if(in.recover())panic();
  if(touch.held&&stale(now,last))expire();
  Contact c{};
  for(unsigned i=0;i<31&&in.q.pop(c);++i){
   if(!c.down){touch.cancel();inhibited=false;continue;}
   if(inhibited)continue;
   if(stale(now,c.ms)||c.x<0||c.x>=320||c.y<0||c.y>=240){expire();continue;}
   last=c.ms;touch.input(engine,true,c.x,c.y);
  }
  if(in.recover())panic();
 }
 void render(Input& in,uint32_t now,int16_t* pcm,unsigned n){pump(in,now);engine.render(pcm,n);}
};
}
