#pragma once
#include "board_session.hpp"
#include "clock_sink.hpp"
namespace phosphor {
// Serialized panel owner. P14 sink is supplied by the platform; only its tick
// may run in a same-core non-reentrant ISR. No driver is enabled here.
class AudioOwner {
 BoardSession board_;
 cyd_platform::ClockSink& sink_;
 std::array<int16_t,128> pcm_{};
 uint32_t generation_=0;
 bool muted_=true,started_=false,seen_fault_=false;
 void observe(){
  const bool fault=board_.clock_fault();
  if((fault&&!seen_fault_)||(sink_.faulted()&&sink_.acknowledged()))board_.clock_abort();
  seen_fault_=fault;
  const auto c=board_.visual().audio_control();
  if(c.reset!=generation_||c.muted!=muted_){
   generation_=c.reset;muted_=c.muted;started_=false;sink_.invalidate();
  }
 }
public:
 explicit AudioOwner(cyd_platform::ClockSink& sink):sink_(sink){sink_.invalidate();}
 void poll(bool valid,bool contact,int x,int y,uint64_t now){
  observe();board_.poll(valid,contact,x,y,now);observe();
 }
 unsigned advance(uint64_t now){observe();const auto n=board_.advance(now);observe();return n;}
 void abort(){board_.clock_abort();observe();}
 // At most one fresh block, and at most 384 codes in flight. Never reuse a
 // block after reset, including a sample-side lease fault during generation.
 bool service(){
  observe();
  if(muted_||seen_fault_||!sink_.acknowledged()||sink_.queued()>256)return false;
  const auto epoch=generation_;
  board_.clock_block(pcm_.data());observe();
  if(epoch!=generation_||muted_||seen_fault_||!sink_.acknowledged())return false;
  if(!sink_.push(pcm_.data())){observe();return false;}
  if(!started_&&sink_.queued()>=cyd_platform::ClockSink::prime)started_=sink_.start();
  return true;
 }
 void capture(){board_.capture();}
 bool strip(int y,int rows,uint16_t* out)const{return board_.strip(y,rows,out);}
 const Engine& visual()const{return board_.visual();}
};
}
