#pragma once
#include "core.hpp"
#include <atomic>
#include <cstddef>
#include <type_traits>

namespace xenobio {
static_assert(std::atomic<uint32_t>::is_always_lock_free, "32-bit atomics required");
static_assert(std::is_trivially_copyable<Engine>::value, "snapshot must be a value");
enum class Contact : uint8_t { Down, Move, Up };
struct Input { Contact kind; int16_t x, y; };

// Exactly one input producer and one audio consumer. Emergency requests bypass
// the FIFO; producer must observe release AND the consumer acknowledgement.
class Controls {
    static constexpr uint32_t slots = 32;
    std::array<Input, slots> queue_{};
    std::atomic<uint32_t> head_{0}, tail_{0}, requested_{0}, acknowledged_{0};
    bool pressed_ = false, inhibited_ = false; // producer-owned
    void trip() {
        inhibited_ = true;
        const auto r = requested_.load(std::memory_order_relaxed);
        if (r == acknowledged_.load(std::memory_order_acquire))
            requested_.store(r + 1, std::memory_order_release);
    }
    bool ready() {
        if (inhibited_ && !pressed_ && requested_.load(std::memory_order_relaxed) ==
            acknowledged_.load(std::memory_order_acquire)) inhibited_ = false;
        return !inhibited_;
    }
    bool push(Input e) {
        const auto h = head_.load(std::memory_order_relaxed);
        if (h - tail_.load(std::memory_order_acquire) == slots) { trip(); return false; }
        queue_[h % slots] = e;
        head_.store(h + 1, std::memory_order_release);
        return true;
    }
public:
    bool down(int x, int y) {
        if (pressed_) return false;
        const bool allowed = ready(); pressed_ = true;
        if (!allowed) return false;
        if (x < 0 || x >= 320 || y < 0 || y >= 240) { trip(); return false; }
        if (x >= 280 && y >= 220) { trip(); return false; }
        return push({Contact::Down, int16_t(x), int16_t(y)});
    }
    bool move(int x, int y) {
        if (!pressed_ || inhibited_) return false;
        if (x < 0 || x >= 320 || y < 0 || y >= 240 || (x >= 280 && y >= 220)) {
            trip(); return false;
        }
        return push({Contact::Move, int16_t(x), int16_t(y)});
    }
    void up() {
        if (!pressed_) return;
        pressed_ = false;
        if (!inhibited_) push({Contact::Up, 0, 0});
    }
    // Capture loss/escape: do not synthesize a new down from subsequent moves.
    void cancel() { pressed_ = false; trip(); }
    // BOOT / emergency key may be held while the touch contact is still down.
    void emergency() { trip(); }
    // Durable request generation, independent of consumer acknowledgement.
    uint32_t emergency_generation() const {
        return requested_.load(std::memory_order_acquire);
    }
    bool emergency_pending() const {
        return requested_.load(std::memory_order_acquire) != acknowledged_.load(std::memory_order_relaxed);
    }
    // Audio consumer only. No producer writes after trip until acknowledgement.
    bool apply_emergency(Engine& e) {
        const auto r = requested_.load(std::memory_order_acquire);
        if (r == acknowledged_.load(std::memory_order_relaxed)) return false;
        tail_.store(head_.load(std::memory_order_acquire), std::memory_order_release);
        e.stop();
        acknowledged_.store(r, std::memory_order_release);
        return true;
    }
    bool pop(Input& e) {
        const auto t = tail_.load(std::memory_order_relaxed);
        if (t == head_.load(std::memory_order_acquire)) return false;
        e = queue_[t % slots];
        tail_.store(t + 1, std::memory_order_release);
        return true;
    }
};

// Audio backend only. Acknowledge a generation only after a successful device
// queue reset; do not confuse the engine acknowledgement with a driver drain.
enum class DrainResult { Unchanged, Drained, Failed };
class EmergencyDrain {
    uint32_t drained_ = 0;
public:
    template<class Reset> DrainResult service(const Controls& controls, Reset reset) {
        const auto generation = controls.emergency_generation();
        if (generation == drained_) return DrainResult::Unchanged;
        if (!reset()) return DrainResult::Failed;
        drained_ = generation;
        return DrainResult::Drained;
    }
};

// SPSC triple buffer. The writer exclusively owns back, reader owns front;
// exchange transfers ownership of middle. No seqlock over non-atomic payload.
class Snapshots {
    std::array<Engine, 3> frames_{};
    std::atomic<uint32_t> middle_{1};
    uint32_t back_ = 2, front_ = 0;
    static constexpr uint32_t dirty = 4;
public:
    explicit Snapshots(const Engine& initial) : frames_{initial, initial, initial} {}
    void publish(const Engine& e) {
        frames_[back_] = e;
        back_ = middle_.exchange(back_ | dirty, std::memory_order_acq_rel) & 3u;
    }
    Engine read() {
        if (middle_.load(std::memory_order_acquire) & dirty)
            front_ = middle_.exchange(front_, std::memory_order_acq_rel) & 3u;
        return frames_[front_];
    }
};

class Runtime {
    static Engine initial() { Engine e; e.reset(2); e.stop(); return e; }
    Engine engine_ = initial();
    Snapshots frames_{engine_};
public:
    Controls input;
    Engine snapshot() { return frames_.read(); } // render thread only
    // Audio consumer only; bounded input work even under concurrent refill.
    void render(int16_t* pcm, std::size_t n) {
        input.apply_emergency(engine_);
        Input e{};
        for (unsigned i = 0; i < 32 && !input.emergency_pending() && input.pop(e); ++i) {
            if (e.kind == Contact::Up) engine_.release();
            else engine_.touch(e.x, e.y, e.kind == Contact::Down);
        }
        input.apply_emergency(engine_);
        for (std::size_t i = 0; i < n; ++i) pcm[i] = engine_.sample();
        frames_.publish(engine_);
    }
};
}
