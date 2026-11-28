#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
namespace xenobio {
constexpr int rate=24000, tick_samples=1200, W=16, H=10, N=W*H, capacity=24;
struct Agent { int x=0,y=0,e=0,s=0; };
struct Voice { uint32_t phase=0,step=0; int amp=0; };
struct Scene { int count, food, growth, cost; uint32_t seed; };
constexpr Scene scenes[]={{4,22,1,10,11},{22,75,3,3,23},{12,45,1,7,37},{12,25,1,8,53},{16,9,1,9,71},{3,12,2,5,97}};
struct Engine {
 std::array<uint8_t,N> food{};
 std::array<Agent,capacity> a{};
 std::array<Voice,8> v{};
 uint32_t rng=1,ticks=0,births=0,deaths=0,moves=0,notes=0;
 int scene=0,until=tick_samples,mode=0,tx=0,ty=0,lease=0,events=0;
 bool held=false,panic=false,interactions=true;
 uint32_t random(){ rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return rng; }
 int population() const { int n=0; for(auto p:a) n+=p.e>0; return n; }
 void reset(int id){ *this=Engine{}; scene=std::clamp(id,0,5); const auto c=scenes[scene]; rng=c.seed;
  for(auto &f:food) f=uint8_t(c.food+random()%16);
  for(int i=0;i<c.count;++i) a[i]={int(random()%W),int(random()%H),int(32+random()%44),i%3};
 }
 void stop(){ panic=true; held=false; lease=0; for(auto &q:v) q={}; }
 void touch(int x,int y,bool down){
  if(x<0||x>=320||y<0||y>=240){release();return;}
  if(y>=220&&x>=280){stop();return;}
  if(down){ if(y<20){reset(x*6/320);return;} if(y>=220){if(x>=240){reset(scene);return;} mode=x/80;release();return;} }
  if(panic||(!down&&!held)||y<20||y>=220){release();return;}
  held=true;tx=x/20;ty=(y-20)/20;lease=4;
 }
 void release(){held=false;lease=0;}
 void sound(int x,int species,int kind){
  if(events>=4||panic)return;
  Voice *q=&v[0];for(auto &z:v) if(z.amp<q->amp)q=&z;
  if(q->amp>200)return;
  const int hz=110+x*19+species*83+kind*127;
  *q={0x40000000u,uint32_t((uint64_t(hz)<<32)/rate),kind==1?1400:900};++events;++notes;
 }
 static int distance(int x,int y,int u,int w){return std::abs(x-u)+std::abs(y-w);}
 void tick(){
  if(panic)return;
  ++ticks;events=0;const auto c=scenes[scene];
  const int growth=scene==3&&((ticks/40)&1)?4:c.growth;
  for(auto &f:food)f=uint8_t(std::min(100,int(f)+growth));
  if(held&&lease>0){
   for(int y=0;y<H;++y)for(int x=0;x<W;++x)if(distance(x,y,tx,ty)<=2){auto &f=food[y*W+x];if(mode==0)f=uint8_t(std::min(100,int(f)+18));if(mode==2)f=uint8_t(std::max(0,int(f)-25));}
  }
  std::array<bool,capacity> existed{};for(int i=0;i<capacity;++i)existed[i]=a[i].e>0;
  for(int i=0;i<capacity;++i){if(!existed[i])continue;auto &p=a[i];
   int bx=p.x,by=p.y,best=-10000;
   constexpr int dx[]={0,1,0,-1,0},dy[]={0,0,1,0,-1};
   for(int j=0;j<5;++j){int x=p.x+dx[j],y=p.y+dy[j];if(x<0||x>=W||y<0||y>=H)continue;
    int score=int(food[y*W+x])+int(random()%13);
    if(interactions)for(int k=0;k<capacity;++k)if(k!=i&&a[k].e>0&&a[k].x==x&&a[k].y==y)score-=a[k].s==p.s?7:18;
    if(held&&mode!=0)score+=(mode==1?-12:12)*distance(x,y,tx,ty);
    if(score>best){best=score;bx=x;by=y;}
   }
   if(bx!=p.x||by!=p.y){++moves;if((random()&7)==0)sound(bx,p.s,0);}p.x=bx;p.y=by;
   auto &f=food[p.y*W+p.x];int eat=std::min(10,int(f));f=uint8_t(f-eat);p.e=std::min(100,p.e+eat-c.cost);
   if(p.e<=0){p.e=0;++deaths;sound(p.x,p.s,2);continue;}
   if(interactions&&p.e>=90){for(int k=0;k<capacity;++k)if(!existed[k]&&a[k].e==0){p.e-=40;a[k]={p.x,p.y,40,p.s};++births;sound(p.x,p.s,1);break;}}
  }
  if(held&&--lease==0)held=false;
 }
 int16_t sample(){
  if(panic)return 0;
  if(--until==0){until=tick_samples;tick();}
  int sum=0;for(auto &q:v){if(q.amp<=0)continue;const int u=int(q.phase>>16);const int tri=u<32768?u*2-32768:98303-u*2;sum+=tri*q.amp/32768;q.phase+=q.step;--q.amp;}
  return int16_t(sum);
 }
 void render(int y,uint16_t *out)const{
  if(y<0||y>=240)return;
  constexpr uint16_t colors[]={0xfca0,0x4e9f,0xe35f};
  for(int x=0;x<320;++x){uint16_t c=0;
   if(y<20)c=x*6/320==scene?0x7bef:0x2104;
   else if(y>=220)c=x>=280?(panic?0xf800:0x7800):x>=240?0x7bef:x/80==mode?0x4e9f:0x2104;
   else {int gx=x/20,gy=(y-20)/20;int f=food[gy*W+gx];c=uint16_t((f/4)<<5);
    if(x%20==0||(y-20)%20==0)c=0x0841;
    if(held&&distance(gx,gy,tx,ty)<=2&&(x%20==1||(y-20)%20==1))c=0xffff;
   }
   out[x]=c;
  }
  if(y>=20&&y<220)for(auto p:a)if(p.e>0&&p.y==(y-20)/20){int yy=(y-20)%20-10,r=2+p.e/17;for(int xx=-r;xx<=r;++xx)if(xx*xx+yy*yy<=r*r)out[p.x*20+10+xx]=colors[p.s];}
 }
};
}
