#pragma once
#include "core.hpp"
#include <atomic>
namespace phosphor {
// SPSC latest-value mailbox. Only the visual owner calls publish; only the
// audio owner calls consume. Each owns a distinct slot, with a third exchanged
// atomically. The dirty bit travels with the index, not as a separate flag.
class Mailbox {
 std::array<AudioControl,3> slots_{};
 static constexpr uint32_t dirty=4;
 std::atomic<uint32_t> middle_{1};
 uint32_t back_=2,front_=0;
public:
 static_assert(std::atomic<uint32_t>::is_always_lock_free,"32-bit atomic required");
 void publish(const AudioControl& c){
  slots_[back_]=c;
  back_=middle_.exchange(back_|dirty,std::memory_order_acq_rel)&3;
 }
 AudioControl consume(){
  if(middle_.load(std::memory_order_acquire)&dirty)
   front_=middle_.exchange(front_,std::memory_order_acq_rel)&3;
  return slots_[front_];
 }
};
class Split {
 Engine visual_;
 Mailbox controls_;
 std::array<uint32_t,4> phase_{};
 std::array<int,4> amplitude_{};
 uint32_t reset_=0;
 void publish(){controls_.publish(visual_.audio_control());}
public:
 // UI/simulation owner only. No audio-side access to field memory.
 void select(int s){visual_.select(s);publish();}
 void touch(int x,int y,bool down){visual_.touch(x,y,down);publish();}
 void up(){visual_.up();publish();}
 void panic(){visual_.panic();publish();}
 void step_visual(){visual_.visual_step();publish();}
 const Engine& visual()const{return visual_;}
 // Audio owner only. Fixed work, no field evolution, allocation, I/O or lock.
 // Poll per sample so a published emergency is seen at the next consumer poll.
 int16_t sample(){
  const auto c=controls_.consume();
  if(c.reset!=reset_){amplitude_.fill(0);reset_=c.reset;}
  return render_sample(phase_,amplitude_,c.increment,c.target,c.muted);
 }
};
}
