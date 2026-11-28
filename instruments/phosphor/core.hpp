#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <cstdlib>
namespace phosphor {
constexpr int W=80,H=50,rate=24000,step=800;
struct Scene { int swirl,zoom,decay,spread; uint8_t r,g,b; uint16_t hz; };
constexpr std::array<Scene,6> scenes{{
 {12,2,245,3,255,100,28,110},{-18,-3,249,2,64,210,255,147},
 {0,5,251,1,220,70,255,196},{28,0,239,5,84,255,130,98},
 {-32,3,247,4,255,220,64,131},{6,-5,253,2,140,150,255,165}}};
struct AudioControl {
 std::array<uint32_t,4> increment{};
 std::array<int,4> target{};
 uint32_t reset=0;
 bool muted=true;
};
inline int16_t render_sample(std::array<uint32_t,4>& phase,std::array<int,4>& amplitude,
                             const std::array<uint32_t,4>& increment,const std::array<int,4>& target,bool muted){
 if(muted)return 0;
 int sum=0;
 for(unsigned i=0;i<4;++i){
  int d=target[i]-amplitude[i];amplitude[i]+=d==0?0:(d>0?std::max(1,d/256):std::min(-1,d/256));
  phase[i]+=increment[i];int v=int(phase[i]>>16);int triangle=v<32768?v-16384:49151-v;
  sum+=triangle*amplitude[i]/16384;
 }
 return int16_t(sum);
}
// Single-owner engine: simulation and synthesis share a sample clock. A target
// adapter must isolate simulation/scan from the audio deadline before adoption.
class Engine {
 std::array<uint8_t,W*H> a_{},b_{};
 std::array<uint32_t,4> phase_{},increment_{};
 std::array<int,4> amplitude_{},target_{};
 uint32_t tick_=0,age_=0,reset_=0; int until_=step,scene_=0,x_=W/2,y_=H/2,dx_=0,dy_=0;
 bool held_=false,frozen_=false,muted_=true;
 static int clip(int x,int n){return std::max(0,std::min(n-1,x));}
 int get(int x,int y)const{return a_[clip(y,H)*W+clip(x,W)];}
 void inject(int x,int y,int radius){
  for(int j=-radius;j<=radius;++j)for(int i=-radius;i<=radius;++i){
   int d=i*i+j*j;if(d>radius*radius||x+i<0||x+i>=W||y+j<0||y+j>=H)continue;
   auto &v=a_[(y+j)*W+x+i];v=uint8_t(std::min(255,int(v)+std::max(0,150-9*d)));
  }
 }
 void evolve(){
  if(held_ && ++age_>=5)held_=false; // 167 ms contact lease, renewed by move
  if(!frozen_){
   const auto s=scenes[scene_];
   for(int y=0;y<H;++y)for(int x=0;x<W;++x){
    int u=x-W/2,v=y-H/2;
    int sx=x+(s.swirl*v+s.zoom*u)/128;
    int sy=y+(-s.swirl*u+s.zoom*v)/128;
    int c=get(sx,sy),n=get(sx-1,sy)+get(sx+1,sy)+get(sx,sy-1)+get(sx,sy+1);
    b_[y*W+x]=uint8_t(((c*(16-2*s.spread)+n*s.spread/2)*s.decay)/(16*256));
   }
   a_.swap(b_);
   if(held_)inject(x_,y_,2+std::min(4,(std::abs(dx_)+std::abs(dy_))/2));
  }
  std::array<int,4> sums{};
  for(int y=0;y<H;++y)for(int x=0;x<W;++x)sums[(y>=H/2)*2+(x>=W/2)]+=a_[y*W+x];
  for(int i=0;i<4;++i)target_[i]=muted_?0:std::min(2048,sums[i]/16);
  ++tick_;
 }
public:
 void panic(){++reset_;muted_=true;held_=false;amplitude_.fill(0);target_.fill(0);}
 void select(int s){
  if(s<0||s>=6)return;
  constexpr unsigned ratios[6][4]={{2,3,4,5},{2,4,6,9},{2,3,5,7},{2,2,3,6},{2,5,7,8},{2,3,4,8}};
  for(int i=0;i<4;++i)increment_[i]=uint32_t((uint64_t(scenes[s].hz)*ratios[s][i]*(uint64_t(1)<<31))/rate);
  scene_=s;muted_=false;frozen_=false;held_=false;
 }
 void clear(){++reset_;a_.fill(0);b_.fill(0);amplitude_.fill(0);target_.fill(0);}
 void up(){held_=false;}
 void touch(int x,int y,bool down){
  if(x<0||x>=320||y<0||y>=240){panic();return;}
  if(x>=240&&y>=220){panic();return;}
  if(down){held_=false;if(y<20){select(x*6/320);return;}if(y>=220){if(x<120)frozen_=!frozen_;else clear();return;}if(muted_)return;}
  else if(!held_)return;
  if(y<20||y>=220){held_=false;return;}
  int nx=x/4,ny=(y-20)/4;dx_=down?0:nx-x_;dy_=down?0:ny-y_;x_=nx;y_=ny;held_=true;age_=0;
  if(!frozen_)inject(x_,y_,2+std::min(4,(std::abs(dx_)+std::abs(dy_))/2));
 }
 int16_t sample(){
  if(--until_==0){evolve();until_=step;}
  return render_sample(phase_,amplitude_,increment_,target_,muted_);
 }
 // Separate-owner adapter uses only these methods on the visual thread.
 void visual_step(){evolve();}
 AudioControl audio_control()const{return {increment_,target_,reset_,muted_};}
 bool held()const{return held_;} bool muted()const{return muted_;} bool frozen()const{return frozen_;}
 uint32_t ticks()const{return tick_;}
 const auto& field()const{return a_;}
 void row(int y,uint16_t* out)const{
  if(y<0||y>=240)return;
  auto s=scenes[scene_];
  for(int x=0;x<320;++x){
   if(y<20){out[x]=x*6/320==scene_?0xffff:0x2945;continue;}
   if(y>=220){out[x]=x>=240?(muted_?0xf800:0x7800):x<120?(frozen_?0x07ff:0x2945):0x39e7;continue;}
   unsigned v=a_[((y-20)/4)*W+x/4];
   out[x]=uint16_t(((v*s.r/255)>>3)<<11|((v*s.g/255)>>2)<<5|((v*s.b/255)>>3));
  }
 }
};
}
