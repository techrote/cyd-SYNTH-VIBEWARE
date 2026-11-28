#pragma once
#include "split.hpp"
namespace phosphor {
// UI owns clock, pointer and field; callback owns audio lease counters.
class DesktopSession {
 Split split_;
 std::atomic<uint32_t> heartbeat_{0},fault_{0};
 uint32_t seen_=0,age_=0;
 uint64_t last_=0,credit_=0;
 bool started_=false,drag_=false;
 int x_=0,y_=0;
public:
 void panic(){drag_=false;split_.panic();}
 void down(int x,int y){
  drag_=false;
  if(x>=0&&x<320&&y>=0&&y<20){
   if(fault_.load(std::memory_order_acquire))split_.panic();
   split_.touch(x,y,true);
   heartbeat_.fetch_add(1,std::memory_order_release);
   fault_.store(0,std::memory_order_release);
   return;
  }
  if(fault_.load(std::memory_order_acquire))return;
  split_.touch(x,y,true);drag_=split_.visual().held();x_=x;y_=y;
 }
 void move(int x,int y){
  if(!drag_)return;
  split_.touch(x,y,false);drag_=split_.visual().held();x_=x;y_=y;
 }
 void up(){drag_=false;split_.up();}
 bool dragging()const{return drag_;}
 bool fault()const{return fault_.load(std::memory_order_acquire)!=0;}
 const Engine& visual()const{return split_.visual();}
 // Monotonic microseconds. At most two ticks per UI service; discard excess.
 unsigned advance(uint64_t now){
  if(fault())panic();
  if(!started_){started_=true;last_=now;return 0;}
  if(now<last_||now-last_>166666){last_=now;credit_=0;panic();fault_.store(1,std::memory_order_release);return 0;}
  credit_+=(now-last_)*30;last_=now;
  unsigned n=unsigned(std::min<uint64_t>(2,credit_/1000000));
  credit_%=1000000;
  for(unsigned i=0;i<n;++i){
   if(drag_)move(x_,y_);
   split_.step_visual();heartbeat_.fetch_add(1,std::memory_order_release);
  }
  return n;
 }
 int16_t sample(){
  uint32_t h=heartbeat_.load(std::memory_order_acquire);
  if(h!=seen_){seen_=h;age_=0;}
  else if(age_<6000&&++age_==6000)fault_.store(1,std::memory_order_release);
  const auto v=split_.sample();return fault()?0:v;
 }
};
}
