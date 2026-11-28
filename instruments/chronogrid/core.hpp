#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstddef>
namespace gridseq {
constexpr unsigned rate=24000,block=128,width=320,height=240,tracks=4,denom=rate*60;
constexpr std::array<unsigned,tracks> lengths{16,12,10,7},divisions{4,4,3,5};
constexpr std::array<uint16_t,tracks> colors{0xf986,0x57df,0xff26,0xb61f};
struct Scene{const char* name;std::array<uint16_t,tracks> masks;unsigned bpm;};
inline constexpr std::array<Scene,6> scenes{{
 {"Crossings",{0x1111,0x0444,0x0155,0x0049},108},
 {"Sparse",{0x0101,0x0800,0x0201,0x0010},78},
 {"Orbit",{0x1249,0x0924,0x0129,0x0045},96},
 {"Broken",{0x4251,0x0842,0x022a,0x0023},132},
 {"Dense",{0x5555,0x0aaa,0x03b7,0x006d},150},
 {"Counterlight",{0x1089,0x010a,0x0249,0x0055},112}
}};
struct Control{
 std::array<uint16_t,tracks> masks=scenes[0].masks;unsigned bpm=scenes[0].bpm,mute=0,scene=0,epoch=0;bool play=false;uint32_t seq=0;
 std::array<uint16_t,tracks> accents{};unsigned accentEdit=0;
};
// Single UI writer, single audio reader. All payload and generation words are
// sequentially consistent atomics; one bounded read attempt, retain prior state on collision.
static_assert(std::atomic<uint32_t>::is_always_lock_free,"32-bit atomics required");
class Exchange{
 std::atomic<uint32_t> seq_{0},a_{0},b_{0},meta_{0},accentA_{0},accentB_{0};
public:
 Exchange(){publish(Control{});}
 void publish(const Control& c){
  seq_.fetch_add(1);a_.store(uint32_t(c.masks[0])|(uint32_t(c.masks[1])<<16));
  b_.store(uint32_t(c.masks[2])|(uint32_t(c.masks[3])<<16));
  accentA_.store(uint32_t(c.accents[0])|(uint32_t(c.accents[1])<<16));
  accentB_.store(uint32_t(c.accents[2])|(uint32_t(c.accents[3])<<16));
  meta_.store((std::clamp(c.bpm,50u,180u))|((c.mute&15u)<<8)|(uint32_t(c.play)<<12)|((c.scene&7u)<<13)|((c.epoch&255u)<<16)|((c.accentEdit&15u)<<24));seq_.fetch_add(1);
 }
 bool read(Control& c)const{
  uint32_t s=seq_.load();if(s&1u)return false;
  uint32_t a=a_.load(),b=b_.load(),m=meta_.load(),aa=accentA_.load(),ab=accentB_.load();if(s!=seq_.load())return false;
  c.masks={uint16_t(a),uint16_t(a>>16),uint16_t(b),uint16_t(b>>16)};c.bpm=m&255u;c.mute=(m>>8)&15u;c.play=(m>>12)&1u;c.scene=(m>>13)&7u;c.epoch=(m>>16)&255u;c.seq=s;c.accents={uint16_t(aa),uint16_t(aa>>16),uint16_t(ab),uint16_t(ab>>16)};c.accentEdit=(m>>24)&15u;return true;
 }
};
class Surface{
 bool held_=false,paint_=false,add_=false,blocked_=false;unsigned lane_=0,last_=0;
 void cell(unsigned n){uint16_t bit=uint16_t(1u<<n);auto& mask=(c.accentEdit&(1u<<lane_))?c.accents[lane_]:c.masks[lane_];if(add_)mask|=bit;else mask&=uint16_t(~bit);}
public:
 Control c{};
 void cancel(){c.play=false;c.epoch=(c.epoch+1)&255u;paint_=false;held_=false;blocked_=true;}
 void touch(bool down,int x,int y){
  if(!down){held_=paint_=blocked_=false;return;}if(blocked_)return;
  x=std::clamp(x,0,319);y=std::clamp(y,0,239);
  if(!held_){held_=true;
   if(y<24){c.scene=std::min(unsigned(x)*6/320,5u);c.masks=scenes[c.scene].masks;c.bpm=scenes[c.scene].bpm;c.accents.fill(0);c.accentEdit=0;}
   else if(y<200){lane_=unsigned(y-24)/44;if(x<24)c.mute^=1u<<lane_;else if(x<48)c.accentEdit^=1u<<lane_;else{paint_=true;last_=unsigned(x-48)*lengths[lane_]/272;add_=!(((c.accentEdit&(1u<<lane_))?c.accents[lane_]:c.masks[lane_])&(1u<<last_));cell(last_);}}
   else if(x<64){c.play=!c.play;c.epoch=(c.epoch+1)&255u;}
   else if(x<128)c.epoch=(c.epoch+1)&255u;
   else if(x<192)c.bpm=std::max(50u,c.bpm-4);
   else if(x<256)c.bpm=std::min(180u,c.bpm+4);
   else cancel();
  }else if(paint_&&y>=24&&y<200&&unsigned(y-24)/44==lane_&&x>=48){
   unsigned n=unsigned(x-48)*lengths[lane_]/272;for(unsigned i=std::min(n,last_);i<=std::max(n,last_);++i)cell(i);last_=n;
  }
 }
};
struct Voice{uint32_t phase=0,noise=0x6e624eb7u;int env=0,prior=0;unsigned inc=0;int gain=1700;
 void hit(unsigned t,unsigned step,bool accent){gain=accent?2600:1700;phase=0;env=32767;prior=0;constexpr unsigned notes[]={110,131,147,165,196,220,262};inc=178957u*(t==3?notes[step%7]:t==1?180u:440u);}
 int sample(unsigned t){if(!env)return 0;phase+=t==0?7200000u+unsigned(env)*400u:inc;unsigned p=phase>>16;int tri=p<32768?int(p)*2-32768:98303-int(p)*2;
  noise^=noise<<13;noise^=noise>>17;noise^=noise<<5;int white=int(noise>>16)-32768;int high=(white-prior)/2;prior=white;
  int raw=t==0?tri:t==1?(white+tri)/2:t==2?high:(tri+int((phase>>17)&1u)*6000-3000);
  int out=(raw*env/32768)*gain/32768;constexpr unsigned decays[]={9,8,7,10};env-=std::max(1,env>>decays[t]);return out;
 }
};
class Engine{
 std::array<Voice,tracks> voices_{};std::array<unsigned,tracks> phase_{},step_{};std::array<uint64_t,tracks> events_{};
 uint32_t lastSeq_=0,age_=0;unsigned epoch_=0;bool running_=false,expired_=false;
 void hit(const Control& c,unsigned t){if(!(c.mute&(1u<<t))&&(c.masks[t]&(1u<<step_[t]))){voices_[t].hit(t,step_[t],(c.accents[t]&(1u<<step_[t]))!=0);++events_[t];}}
 void stop(){running_=false;for(auto& v:voices_)v.env=0;}
public:
 const auto& events()const{return events_;}const auto& steps()const{return step_;}
 void render(const Control& c,int16_t* out,std::size_t n,std::atomic<uint32_t>* telemetry=nullptr){
  if(c.seq!=lastSeq_){age_=0;lastSeq_=c.seq;}
  bool changed=c.epoch!=epoch_;epoch_=c.epoch;
  if(!c.play){expired_=false;stop();}else if(changed){expired_=false;stop();}
  if(c.play&&!expired_&&!running_&&age_<rate/4){running_=true;voices_.fill(Voice{});phase_.fill(0);step_.fill(0);for(unsigned t=0;t<tracks;++t)hit(c,t);}
  const unsigned bpm=std::clamp(c.bpm,50u,180u);
  for(std::size_t i=0;i<n;++i){
   if(age_<rate)++age_;
   if(running_&&age_>rate/4){expired_=true;stop();}
   int sum=0;if(running_){for(unsigned t=0;t<tracks;++t){
    if(c.mute&(1u<<t))voices_[t].env=0;
    sum+=voices_[t].sample(t);phase_[t]+=bpm*divisions[t];if(phase_[t]>=denom){phase_[t]-=denom;step_[t]=(step_[t]+1)%lengths[t];hit(c,t);}
   }}out[i]=int16_t(sum);
  }
  if(telemetry){uint32_t w=uint32_t(running_)<<16;for(unsigned t=0;t<tracks;++t)w|=step_[t]<<(4*t);telemetry->store(w,std::memory_order_release);}
 }
};
inline bool digit(unsigned n,unsigned x,unsigned y){
 constexpr uint16_t glyph[]={0x7b6f,0x2492,0x73e7,0x73cf,0x5bc9,0x79cf,0x79ef,0x7249,0x7bef,0x7bcf};
 return n<10&&x<3&&y<5&&(glyph[n]&(1u<<(14-y*3-x)));
}
// Actual step masks and audio-clock playheads, no full framebuffer on CYD.
inline void row(const Control& c,uint32_t playhead,unsigned y,uint16_t* dst){
 if(y>=height)return;
 for(unsigned x=0;x<width;++x){uint16_t v=0x0842;
  if(y<24){unsigned s=x*6/320;v=s==c.scene?0x35ad:0x2104;if(y<3||y>20||x*6%320<12)v=0x0842;}
  else if(y<200){unsigned t=(y-24)/44,dy=(y-24)%44;uint16_t color=colors[t];if(x<48){v=c.mute&(1u<<t)?0x3186:color;if(x>=24)v=(c.accentEdit&(1u<<t))?0xffe0:0x3186;if(dy<5||dy>38||x<5||x>42||x==23||x==24)v=0x0842;
    if(x>=30&&x<=36&&dy>=13&&dy<=30&&(x==33||dy==21))v=0xffff;}
   else{unsigned k=(x-48)*lengths[t]/272;unsigned pos=(x-48)*lengths[t]%272;bool on=c.masks[t]&(1u<<k),head=(playhead&(1u<<16))&&((playhead>>(4*t))&15u)==k;
    if(dy>=4&&dy<=39&&pos>=lengths[t]&&pos<272-lengths[t]){v=on?(c.mute&(1u<<t)?0x4208:color):0x18e3;if((c.accents[t]&(1u<<k))&&dy>=15&&dy<=19)v=on?0xffe0:0x6300;if(head&&(dy<9||dy>34))v=0xffff;}
   }}else{unsigned b=x/64;v=b==0?(c.play?0x3d85:0x3186):b==4?0x9904:0x3186;if(y<204||y>236||x%64<3||x%64>60)v=0x0842;
   if(b==0&&y>=213&&y<=227&&x%64>=23&&x%64<=39){unsigned u=x%64-23;v=(c.play?(u<5||u>11):(u<=(y<=220?y-213:227-y)*2))?0xffff:v;}
   if(b==1&&y>=211&&y<=229&&(x%64==22||x%64==42||y==211||y==229)&&x%64>=22&&x%64<=42)v=0xffff;
   if((b==2||b==3)&&y>=219&&y<=221&&x%64>=20&&x%64<=44)v=0xffff;
   if(b==3&&x%64>=30&&x%64<=34&&y>=208&&y<=232)v=0xffff;
   if(b==4&&y>=210&&y<=230&&(x%64==22+y-210||x%64==42-(y-210)))v=0xffff;
  }
  if(y>=7&&y<17){unsigned s=x*6/320;unsigned left=(s*320+5)/6+20;if(x>=left&&x<left+6&&digit(s+1,(x-left)/2,(y-7)/2))v=0xffff;}
  if(y>=231&&y<236&&x>=176&&x<187){unsigned i=(x-176)/4;unsigned n=i==0?c.bpm/100:i==1?(c.bpm/10)%10:c.bpm%10;if(digit(n,(x-176)%4,y-231))v=0xffff;}
  dst[x]=v;
 }
}
}
