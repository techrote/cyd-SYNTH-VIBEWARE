#pragma once
#include <atomic>
#include <cstdint>
namespace cyd_platform {
// Experimental software sample-clock sink. One producer (serialized panel
// owner) and one non-reentrant interrupt consumer ON THE SAME CORE. Not a DAC
// driver, not a DMA descriptor protocol. No other producer/reset caller.
class ClockSink {
    using U=std::uint32_t;
    static_assert(std::atomic<U>::is_always_lock_free,"32-bit atomics required");
    static constexpr U capacity=512, mask=capacity-1;
    std::uint8_t data_[capacity]{};
    std::atomic<U> write_{0},read_{0},request_{0},ack_{0},running_{0},fault_{0};
    U seen_=0; // interrupt only
public:
    static constexpr U block=128, prime=2*block;
    static constexpr std::uint8_t midpoint=128;
    static constexpr std::uint8_t encode(std::int16_t v) {
        return std::uint8_t((32768+std::int32_t(v)/2)>>8);
    }
    // Producer only. Immediately inhibit, even before the interrupt flushes.
    // Do not restart on a release alone; instrument owner decides rearming.
    void invalidate() {
        running_.store(0,std::memory_order_release);
        request_.store(request_.load(std::memory_order_relaxed)+1,std::memory_order_release);
    }
    bool acknowledged() const {
        return request_.load(std::memory_order_relaxed)==ack_.load(std::memory_order_acquire);
    }
    bool faulted() const { return fault_.load(std::memory_order_acquire)!=0; }
    U queued() const {
        return write_.load(std::memory_order_relaxed)-read_.load(std::memory_order_acquire);
    }
    bool push(const std::int16_t* pcm) {
        if(!pcm||!acknowledged()||faulted()||queued()>capacity-block)return false;
        const U w=write_.load(std::memory_order_relaxed);
        for(U i=0;i<block;++i)data_[(w+i)&mask]=encode(pcm[i]);
        write_.store(w+block,std::memory_order_release);
        // A starvation interrupt may have happened during conversion. The
        // newly published block remains inhibited and will be discarded.
        return !faulted();
    }
    bool start() {
        if(!acknowledged()||faulted()||queued()<prime||running_.load(std::memory_order_relaxed))return false;
        running_.store(1,std::memory_order_release);
        return true;
    }
    __attribute__((always_inline)) inline std::uint8_t tick() {
        const U epoch=request_.load(std::memory_order_acquire);
        if(epoch!=seen_) {
            read_.store(write_.load(std::memory_order_acquire),std::memory_order_release);
            seen_=epoch;
            fault_.store(0,std::memory_order_relaxed);
            ack_.store(epoch,std::memory_order_release);
            return midpoint;
        }
        if(!running_.load(std::memory_order_acquire))return midpoint;
        const U r=read_.load(std::memory_order_relaxed);
        if(r==write_.load(std::memory_order_acquire)) {
            fault_.store(1,std::memory_order_release);
            running_.store(0,std::memory_order_release);
            return midpoint;
        }
        const auto v=data_[r&mask];
        read_.store(r+1,std::memory_order_release);
        return v;
    }
};
}
