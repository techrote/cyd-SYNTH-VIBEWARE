#pragma once
#include "core.hpp"
#include <atomic>

namespace ecophon {
// One producer and one consumer. All 32 entries usable; unsigned distance wraps.
template<class T, unsigned N> class Queue {
    static_assert(N && !(N&(N-1)), "power of two capacity");
    static_assert(std::atomic<unsigned>::is_always_lock_free, "lock-free indices required");
    std::array<T,N> slots{};
    std::atomic<unsigned> write{0}, read{0};
public:
    bool push(const T& v){
        const auto w=write.load(std::memory_order_relaxed);
        if(w-read.load(std::memory_order_acquire)==N)return false;
        slots[w&(N-1)]=v;write.store(w+1,std::memory_order_release);return true;
    }
    bool pop(T& v){
        const auto r=read.load(std::memory_order_relaxed);
        if(r==write.load(std::memory_order_acquire))return false;
        v=slots[r&(N-1)];read.store(r+1,std::memory_order_release);return true;
    }
    // Consumer only, while producer is inhibited by the emergency flag.
    void discard(){read.store(write.load(std::memory_order_acquire),std::memory_order_release);}
};
struct Contact { uint32_t ms; int x,y; bool down; };
struct Input {
    Queue<Contact,32> queue;
    std::atomic<unsigned> fault{0};
    // Producer only. Retry a refused physical release after acknowledgement.
    bool submit(bool down,int x,int y,uint32_t ms){
        if(down&&(x<0||x>=320||y<0||y>=240||(x>=280&&y>=216))){emergency();return false;}
        if(fault.load(std::memory_order_acquire))return false;
        if(queue.push({ms,x,y,down}))return true;
        emergency();return false;
    }
    void emergency(){fault.store(1,std::memory_order_release);}
    // Consumer only. No producer writes while fault is set.
    template<class Emergency> bool recover(Emergency beforeClear){
        if(!fault.load(std::memory_order_acquire))return false;
        queue.discard();beforeClear();fault.store(0,std::memory_order_release);return true;
    }
    bool recover(){return recover([]{});}
};
// SPSC latest-value mailbox. Three independently owned slots. The atomic
// exchange transfers ownership of the middle slot, never the reader's front.
class Views {
    std::array<Engine,3> slots{};
    static constexpr unsigned dirty=4;
    std::atomic<unsigned> middle{1};
    unsigned back=2,front=0; // accessed only by their respective owners
public:
    void publish(const Engine& e){slots[back]=e;back=middle.exchange(back|dirty,std::memory_order_acq_rel)&3u;}
    bool latest(Engine& e){
        if(!(middle.load(std::memory_order_acquire)&dirty))return false;
        front=middle.exchange(front,std::memory_order_acq_rel)&3u;e=slots[front];return true;
    }
};
struct Runtime {
    static constexpr uint32_t lease_ms=150;
    Engine engine;
    Touch touch;
    bool inhibited=false;
    uint32_t last=0;
    // Audio-owner-only monotonic generation; a backend must drain on change,
    // even when a queued release/restart has already unmuted the engine.
    uint32_t panic_epoch=0;
    Runtime(){engine.reset(0,42);engine.panic();}
    static bool stale(uint32_t now,uint32_t then){const auto age=uint32_t(now-then);return age>=lease_ms&&age<0x80000000u;}
    void panic(){++panic_epoch;engine.panic();touch.release();inhibited=true;}
    template<class Emergency> void pump(Input& in,uint32_t now,Emergency beforePanic){
        if(in.recover(beforePanic)){panic();return;}
        if(touch.held&&stale(now,last)){beforePanic();panic();}
        Contact c{};
        for(unsigned i=0;i<32&&in.queue.pop(c);++i){
            if(!c.down){touch.release();inhibited=false;continue;}
            if(inhibited)continue;
            if(stale(now,c.ms)){beforePanic();panic();continue;}
            last=c.ms;touch.contact(engine,c.x,c.y);
        }
        if(in.recover(beforePanic))panic();
    }
    void pump(Input& in,uint32_t now){pump(in,now,[]{});}
    void render(Input& in,uint32_t now,int16_t* pcm,size_t n){pump(in,now);engine.render(pcm,n);}
};
}
