#pragma once
#include <array>
#include <cstdint>
#include <algorithm>
namespace chronocyd {
constexpr unsigned rate=24000, modulus=rate*60*100;
struct Scene { unsigned bpm; int warp; unsigned variation; std::array<unsigned,3> lengths,masks; };
inline constexpr Scene factory[]={{112,30,1,{7,5,11},{0x49,0x15,0x495}},{142,-45,2,{5,9,7},{0x13,0x125,0x55}},{84,55,0,{9,7,13},{0x111,0x49,0x1111}},{126,0,3,{8,8,8},{0x11,0x44,0xaa}}};
struct Lane { uint32_t clock=0,phase=0,inc=0,ticks=0,hits=0; unsigned step=0,env=0; };
struct Engine {
 Scene s=factory[0]; std::array<Lane,3> lane{}; uint32_t master=0,beat=0,rng=1; bool playing=false;
 void stop(){playing=false;for(auto& l:lane)l.env=0;}
 void reset(unsigned preset){*this=Engine{};s=factory[preset%4];}
 bool recall(const Scene& v){if(v.bpm<50||v.bpm>180||v.warp< -60||v.warp>60||v.variation>3)return false;for(unsigned i=0;i<3;++i)if(v.lengths[i]<1||v.lengths[i]>16||v.masks[i]>=(1u<<v.lengths[i]))return false;s=v;for(unsigned i=0;i<3;++i)lane[i].step%=s.lengths[i];return true;}
 unsigned random(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
 static int tri(uint32_t phase){unsigned p=phase>>16;return p<32768?int(p)*2-32768:98302-int(p)*2;}
 void tick(unsigned i){auto& l=lane[i];unsigned r=random();unsigned step=l.step; l.step=(step+1)%s.lengths[i]; ++l.ticks;
 if(!(s.masks[i]&(1u<<step)) && !(s.masks[i]&&s.variation&&r%32<s.variation))return;
 ++l.hits; l.env=32767; constexpr unsigned hz[8]={110,131,147,165,196,220,262,294};unsigned note=(step+(l.ticks/s.lengths[i])%4+i*2+(s.variation&&((r>>8)&7)==0?1:0))%8;
 l.inc=uint32_t((uint64_t(hz[note])*(i+1)<<32)/rate);l.phase=0;
 }
 int16_t sample(){if(!playing)return 0;master+=s.bpm*400;if(master>=modulus){master-=modulus;++beat;}int sign=(beat/4)%2? -1:1;int sum=0;
 for(unsigned i=0;i<3;++i){auto& l=lane[i];int factor=i==0?100:i==1?100+sign*s.warp:100-sign*s.warp/2;l.clock+=s.bpm*4*unsigned(factor);if(l.clock>=modulus){l.clock-=modulus;tick(i);}l.phase+=l.inc;int a=tri(l.phase);if(i==1)a=(a+tri(l.phase*3u)/2)/2;if(i==2)a=(a/2+int(random()>>16)-32768)/2;sum+=(a*int(l.env))/32768/4;unsigned decay=i==0?7:i==1?4:24;l.env=l.env>decay?l.env-decay:0;}
 return int16_t(sum);}
 void render(int16_t* p,unsigned n){while(n--)*p++=sample();}
};
struct Touch { bool held=false,paint=false;unsigned row=0;uint32_t visited=0; int last=-1;
 void cancel(){held=false;visited=0;last=-1;}
 void input(Engine& e,bool down,int x,int y){if(!down){cancel();return;}if(x<0||x>=320||y<0||y>=240){cancel();return;}
 bool fresh=!held;held=true;if(fresh){row=4;last=-1;visited=0;}
 if(y>=200){if(!fresh)return;unsigned b=unsigned(x)/40;if(b<4){auto p=e.playing;e.recall(factory[b]);e.playing=p;}else if(b==4){if(e.playing)e.stop();else e.playing=true;}else if(b==5){e.reset(0);}else if(b==6){e.s.variation=(e.s.variation+1)%4;}else e.stop();return;}
 if(y<40){if(fresh)row=3;if(row==3){e.s.bpm=50+unsigned(x)*130/319;e.s.warp=60-int(y)*120/39;}return;}
 unsigned r=unsigned(y-40)/53;if(r>=3)return;int cell=int(unsigned(x)*e.s.lengths[r]/320);if(fresh){row=r;paint=!(e.s.masks[r]&(1u<<cell));visited=0;last=cell;}if(row!=r||last<0)return;
 for(int j=std::min(last,cell);j<=std::max(last,cell);++j){unsigned b=1u<<j;if(!(visited&b)){if(paint)e.s.masks[r]|=b;else e.s.masks[r]&=~b;visited|=b;}}last=cell;
 }
};
inline uint16_t pixel(const Engine& e,int x,int y){if(x<0||x>=320||y<0||y>=240)return 0;if(y<40){int a=int((e.s.bpm-50)*319/130),b=(60-e.s.warp)*39/120;return (x>=a-2&&x<=a+2)||(y>=b-1&&y<=b+1)?0xffff:0x18e3;}if(y>=200){unsigned k=unsigned(x)/40;if(x%40<2||y==200)return 0;return k==7?0x7800:k==4&&e.playing?0x07e0:k<4?0x3290:0x4208;}
 unsigned i=unsigned(y-40)/53;if(i>=3)return 0;unsigned z=unsigned(x)*e.s.lengths[i],j=z/320; if(z%320<e.s.lengths[i]*2||y%53<2)return 0; if(j==e.lane[i].step)return 0xffff;constexpr uint16_t colors[]={0xfac0,0x07d7,0xb21f};return e.s.masks[i]&(1u<<j)?colors[i]:0x2104;}
inline void row(const Engine& e,int y,uint16_t* p){for(int x=0;x<320;++x)p[x]=pixel(e,x,y);}
}
