#pragma once
#include "desktop_session.hpp"
#include <cstring>
namespace phosphor {
// Single-owner front-panel scheduler. Digital witness only; no hardware sink.
class BoardSession {
 DesktopSession session_;
 Engine frame_;
 uint64_t last_poll_=0,due_=0;
 unsigned fraction_=0;
 bool contact_=false,blocked_=true,clock_=false;
 void cancel(){session_.panic();contact_=false;blocked_=true;}
 void expire(uint64_t now){if(contact_&&(now<last_poll_||now-last_poll_>60000))cancel();}
public:
 void poll(bool valid,bool contact,int x,int y,uint64_t now){
  expire(now);
  if(!valid||(contact&&(x<0||x>=320||y<0||y>=240))){cancel();return;}
  last_poll_=now;
  if(!contact){session_.up();contact_=false;blocked_=false;return;}
  if(blocked_)return;
  if(contact_)session_.move(x,y);else session_.down(x,y);
  contact_=true;
 }
 unsigned advance(uint64_t now){expire(now);return session_.advance(now);}
 // Return one 128-sample block when due. Never catch up in an unbounded loop.
 bool digital(uint64_t now,int16_t* out){
  if(!out)return false;
  expire(now);
  if(!clock_){due_=now;clock_=true;}
  if(now<due_)return false;
  if(now-due_>10667){cancel();due_=now;fraction_=0;}
  for(int i=0;i<128;++i)out[i]=session_.sample();
  due_+=5333;if(++fraction_==3){++due_;fraction_=0;}
  return true;
 }
 void capture(){frame_=session_.visual();}
 // P12 row grouping, scoped to the stable copied frame (not live simulation).
 // Based on same-repo PLATFORM #16, 46285f83, strip.hpp; no external source.
 bool strip(int y,int rows,uint16_t* out)const{
  if(!out||y<0||y>=240||rows<=0||rows>8||rows>240-y)return false;
  const int end=y+rows;
  while(y<end){
   const int boundary=y<20?20:y>=220?240:y+4-(y-20)%4;
   const int n=std::min(end,boundary)-y;
   frame_.row(y,out);
   for(int j=1;j<n;++j)std::memcpy(out+j*320,out,320*sizeof(uint16_t));
   y+=n;out+=n*320;
  }
  return true;
 }
 const Engine& visual()const{return session_.visual();}
};
}
