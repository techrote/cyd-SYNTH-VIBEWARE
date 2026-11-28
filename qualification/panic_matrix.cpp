#include "runtime.hpp"
#include <cstdio>
using namespace chronocyd;
int main(){
 unsigned failed=0, checks=0;
 for(int y=199;y<=240;++y)for(int x=279;x<=320;++x){
  Runtime r; Input q; int16_t pcm[128]{}; uint32_t t=0;
  auto step=[&](bool down,int px,int py){bool accepted=q.send(down,px,py,++t);r.render(q,t,pcm,128);return accepted;};
  step(true,170,220); step(false,0,0); bool sound=false;
  for(int i=0;i<128;++i){r.render(q,++t,pcm,128);for(auto v:pcm)sound|=v!=0;}
  if(!sound||!r.engine.playing)return 2;
  step(true,40,10); if(!r.touch.held)return 2;
  auto bpm=r.engine.s.bpm; if(!q.send(true,270,10,++t))return 2;
  const bool accepted=step(true,x,y);
  const bool panic=x>=280&&x<320&&y>=200&&y<240;
  bool silence=true;for(auto v:pcm)silence&=v==0;
  ++checks;
  if(panic){
   if(accepted||r.engine.playing||!r.inhibited||r.touch.held||!silence||r.engine.s.bpm!=bpm){++failed;continue;}
   step(true,170,220); if(r.engine.playing){++failed;continue;}
   step(false,0,0);step(true,170,220);if(!r.engine.playing)++failed;
  }else if(!accepted||!r.engine.playing)++failed;
 }
 std::printf("panic matrix: %u positions, %u failures\n",checks,failed);
 return failed?1:0;
}
