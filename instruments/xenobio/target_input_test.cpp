#include "runtime.hpp"
#include "target_input.hpp"
#include <cstdio>
#include <cstdlib>
using namespace xenobio;
static void require(bool ok) { if(!ok) std::abort(); }
struct Direct {
    Engine e; unsigned downs=0, moves=0, ups=0, cancels=0; bool accept=true;
    bool down(int x,int y){++downs; if(accept)e.touch(x,y,true); return accept;}
    bool move(int x,int y){++moves; if(accept)e.touch(x,y,false); return accept;}
    void up(){++ups;e.release();}
    void cancel(){++cancels;e.stop();}
};
int main(){
    Direct c; c.e.reset(2); c.e.stop(); TargetInput g;
    g.poll(c,true,100,5); require(c.downs==0 && c.e.panic);
    g.arm(c); g.poll(c,true,100,5); require(c.downs==0);
    g.poll(c,false,0,0); g.poll(c,true,100,5); require(c.downs==1 && !c.e.panic);
    g.poll(c,false,0,0); g.poll(c,true,110,100); require(c.e.held);
    g.poll(c,true,140,120); require(c.moves==1 && c.e.held);
    unsigned audible=0; for(int i=0;i<24000;++i)audible+=c.e.sample()!=0;
    require(audible>0);
    g.poll(c,true,280,220); require(!g.armed() && c.e.panic && !c.e.held);
    for(int i=0;i<24000;++i)require(c.e.sample()==0);
    g.poll(c,false,0,0); g.poll(c,true,100,5); require(c.downs==2);
    g.arm(c); g.poll(c,true,100,5); require(c.downs==2);
    g.poll(c,false,0,0); g.poll(c,true,100,5); require(c.downs==3 && !c.e.panic);
    c.accept=false; g.poll(c,true,105,6); require(!g.armed() && c.e.panic);
    for(int y=219;y<=240;++y)for(int x=279;x<=320;++x){
        Direct d; d.e.reset(2); TargetInput t; t.arm(d);t.poll(d,false,0,0);
        t.poll(d,true,x,y); bool valid=x<320&&y<240&&!(x>=280&&y>=220);
        require(t.armed()==valid);require(d.downs==unsigned(valid));
    }
    Runtime runtime; TargetInput t; std::array<int16_t,128> pcm{};
    auto render=[&]{runtime.render(pcm.data(),pcm.size());};
    t.arm(runtime.input); render(); t.poll(runtime.input,false,0,0);
    t.poll(runtime.input,true,100,5); render(); require(!runtime.snapshot().panic);
    t.poll(runtime.input,false,0,0); t.poll(runtime.input,true,110,100); render();
    require(runtime.snapshot().held);
    unsigned realAudible=0; for(int b=0;b<200;++b){render();for(auto v:pcm)realAudible+=v!=0;}
    require(realAudible>0);
    for(int i=0;i<33;++i)t.poll(runtime.input,true,110+i,100);
    require(!t.armed()); render(); require(runtime.snapshot().panic);
    for(auto v:pcm)require(v==0);
    t.arm(runtime.input); render(); t.poll(runtime.input,true,100,5); render(); require(runtime.snapshot().panic);
    t.poll(runtime.input,false,0,0); t.poll(runtime.input,true,100,5); render(); require(!runtime.snapshot().panic);
    t.poll(runtime.input,true,300,230); render(); require(runtime.snapshot().panic && !t.armed());
    for(auto v:pcm)require(v==0);
    std::printf("actual Runtime: audible=%u/25600; overflow and drag-panic silence PASS\n",realAudible);
    uint16_t prev=0; for(int i=-32768;i<=32767;++i){auto v=dac_word(int16_t(i));require(v>=prev && v>=16384 && v<=49151);prev=v;}
    require(dac_word(0)==32768); require(touch_axis(-500,320)==0&&touch_axis(5000,320)==319);
    std::printf("target gate PASS: audible=%u/24000; panic silence=24000; boundary=924; DAC=65536\n",audible);
}
