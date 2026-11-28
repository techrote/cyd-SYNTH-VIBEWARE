#include "split.hpp"
#include <thread>
#include <cstdio>
#include <cstdlib>
#include <chrono>
using namespace phosphor;
static void need(bool b){if(!b)std::abort();}
int main(){
 uint64_t samples=0,pixels=0;
 for(int s=0;s<6;++s){
  Engine reference;Split split;reference.select(s);split.select(s);
  for(int i=0;i<rate*8;++i){
   if(i%400==0){int t=i/400,x=20+(t*7)%280,y=40+(t*11)%160;reference.touch(x,y,i==0);split.touch(x,y,i==0);}
   if(i==rate*2){reference.touch(300,230,true);split.touch(300,230,true);}
   if(i==rate*3){reference.select(s);split.select(s);reference.touch(100,100,true);split.touch(100,100,true);}
   if(i==rate*4){reference.touch(150,230,true);split.touch(150,230,true);}
   if(i==rate*5){reference.touch(10,230,true);split.touch(10,230,true);}
   if(i==rate*6){reference.touch(10,230,true);split.touch(10,230,true);}
   if((i+1)%step==0)split.step_visual();
   need(reference.sample()==split.sample());++samples;
   if((i+1)%step==0){need(reference.field()==split.visual().field());
    std::array<uint16_t,320> a{},b{};
    for(int y=0;y<240;++y){reference.row(y,a.data());split.visual().row(y,b.data());need(a==b);pixels+=320;}
   }
  }
 }
 Mailbox m;std::atomic<bool> done{false};std::atomic<unsigned> seen{0};
 std::thread reader([&]{uint32_t last=0;while(!done.load(std::memory_order_acquire)||last!=200000){auto c=m.consume();need(c.reset>=last);for(unsigned j=0;j<4;++j){need(c.increment[j]==c.reset);need(c.target[j]==int(c.reset));}if(c.reset!=last)++seen;last=c.reset;}});
 for(uint32_t i=1;i<=200000;++i){AudioControl c;c.reset=i;c.increment.fill(i);c.target.fill(int(i));m.publish(c);}
 done.store(true,std::memory_order_release);reader.join();need(seen>0);
 Split e;e.select(0);e.touch(120,100,true);e.step_visual();bool audible=false;for(int i=0;i<800;++i)audible|=e.sample()!=0;need(audible);
 e.panic();e.select(1); // restart before consumer poll: reset must survive coalescing
 for(int i=0;i<800;++i)need(e.sample()==0);
 e.touch(120,100,true);e.step_visual();audible=false;for(int i=0;i<800;++i)audible|=e.sample()!=0;need(audible);
 e.panic();for(int i=0;i<800;++i)need(e.sample()==0);
 std::printf("PASS: %llu exact PCM samples, %llu exact RGB565 pixels, 200000 concurrent publications, coalesced panic/restart, sizeof Split=%zu mailbox=%zu\n",(unsigned long long)samples,(unsigned long long)pixels,sizeof(Split),sizeof(Mailbox));
}
