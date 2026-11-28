#pragma once
#include <atomic>
#include <cstdint>
#include <cstddef>

namespace ecophon {
// One audio producer, one sample-clock consumer. No DMA, heap, locks or
// catch-up work in tick. Reset must be acknowledged before publishing again.
// Producer/consumer must be on the same target core for reset-vs-tick ordering.
class SampleSink {
    static_assert(std::atomic<unsigned>::is_always_lock_free);
    static constexpr unsigned capacity=512, mask=capacity-1;
    uint8_t codes[capacity]{};
    std::atomic<unsigned> write{0}, read{0}, request{0}, acknowledged{0};
    unsigned seen=0; // consumer only
public:
    static constexpr unsigned block=128;
    static constexpr uint8_t midpoint=128;
    static constexpr uint8_t encode(int16_t v){return uint8_t((32768+int32_t(v)/2)>>8);}
    void invalidate(){request.store(request.load(std::memory_order_relaxed)+1,std::memory_order_release);}
    bool ready()const{
        return request.load(std::memory_order_relaxed)==acknowledged.load(std::memory_order_acquire)&&
               write.load(std::memory_order_relaxed)-read.load(std::memory_order_acquire)<=capacity-block;
    }
    bool push(const int16_t* pcm){
        if(!ready())return false;
        const auto w=write.load(std::memory_order_relaxed);
        for(unsigned i=0;i<block;++i)codes[(w+i)&mask]=encode(pcm[i]);
        write.store(w+block,std::memory_order_release);return true;
    }
    // Always inline into the sample ISR. The platform must verify the final
    // target disassembly/IRAM placement independently before physical use.
    __attribute__((always_inline)) inline uint8_t tick(bool inhibit){
        const auto epoch=request.load(std::memory_order_acquire);
        if(epoch!=seen){
            read.store(write.load(std::memory_order_acquire),std::memory_order_release);
            seen=epoch;acknowledged.store(epoch,std::memory_order_release);return midpoint;
        }
        if(inhibit)return midpoint;
        const auto r=read.load(std::memory_order_relaxed);
        if(r==write.load(std::memory_order_acquire))return midpoint;
        const auto code=codes[r&mask];read.store(r+1,std::memory_order_release);return code;
    }
};
}
