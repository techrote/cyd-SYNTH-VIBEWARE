#include "runtime.hpp"
#include "target_input.hpp"
#include "boot_arm.hpp"
#include "labeled.hpp"
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
static void boot_arm_regression() {
    Runtime r; TargetInput gate; BootArm boot; std::array<int16_t,128> pcm{};
    auto render=[&]{r.render(pcm.data(),pcm.size());};
    unsigned arms=0, disarms=0;
    auto tick=[&](uint32_t now,bool low,bool healthy=true){
        auto a=boot.poll(now,low,gate.armed(),healthy);
        if(a==BootAction::Arm){++arms;gate.arm(r.input);}
        if(a==BootAction::Disarm){++disarms;gate.disarm(r.input);}
        render(); return a;
    };
    tick(0,true); tick(5000,true); tick(5001,false); tick(5051,false);
    require(arms==0 && !gate.armed());
    tick(5100,true); tick(5899,false); tick(5949,false); require(arms==0);
    tick(6000,true); tick(6800,false); tick(6820,true); tick(9000,true);
    tick(9001,false); tick(9051,false); require(arms==0);
    tick(9100,true); tick(9900,false); tick(9949,false); require(arms==0);
    require(tick(9950,false)==BootAction::Arm && arms==1 && gate.armed());
    gate.poll(r.input,true,100,5); render(); require(r.snapshot().panic);
    gate.poll(r.input,false,0,0); gate.poll(r.input,true,100,5); render();
    require(!r.snapshot().panic);
    unsigned audible=0; for(int i=0;i<200;++i){render();for(auto v:pcm)audible+=v!=0;}
    require(audible>0);
    require(tick(10000,true)==BootAction::Disarm && disarms==1);
    for(auto v:pcm)require(v==0);
    tick(12000,true); tick(12001,false); tick(12051,false); require(arms==1);
    tick(12100,true); tick(12900,false); tick(12950,false,false); require(arms==1);
    tick(13000,false); tick(13050,false); tick(13100,true);
    tick(13900,false); tick(13950,false); require(arms==2);
    require(tick(13951,false,false)==BootAction::Disarm && disarms==2);
    BootArm wrap; uint32_t base=0xffffff00u;
    require(wrap.poll(base,false,false,true)==BootAction::None);
    require(wrap.poll(base+50,false,false,true)==BootAction::None);
    require(wrap.poll(base+60,true,false,true)==BootAction::None);
    require(wrap.poll(base+860,false,false,true)==BootAction::None);
    require(wrap.poll(base+910,false,false,true)==BootAction::Arm);
    std::printf("BOOT arm PASS: held at startup, short hold, bounce, threshold, touch release, immediate disarm, fault, wrap; audible=%u/25600\n",audible);
}
static void status_regression() {
    Engine e; e.reset(2);
    std::array<uint16_t,322> a{}, b{}, c{}, d{};
    unsigned disarmedChanges=0,faultChanges=0;
    for(int y=-1;y<=240;++y){
        a.fill(0xdead); b=a; c=a; d=a;
        labeled_row(e,y,a.data()+1);
        target_status_row(e,true,false,y,b.data()+1);
        target_status_row(e,false,false,y,c.data()+1);
        target_status_row(e,false,true,y,d.data()+1);
        require(a==b);
        require(c.front()==0xdead && c.back()==0xdead && d.front()==0xdead && d.back()==0xdead);
        if(y<0 || y>=20) require(a==c && a==d);
        else for(int x=1;x<=320;++x){disarmedChanges+=a[x]!=c[x];faultChanges+=c[x]!=d[x];}
    }
    require(disarmedChanges>0 && faultChanges>0);
    std::printf("target status PASS: %u disarmed header pixel changes, %u fault differences; all other rows exact\n",disarmedChanges,faultChanges);
}
int main(){
    boot_arm_regression();
    status_regression();
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
