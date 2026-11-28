#include "timer_lifecycle.hpp"
#include <cassert>
#include <cstdio>
#include <string>
struct Sink {unsigned invalidations=0;void invalidate(){++invalidations;}};
struct Driver {
    std::string trace;char fail=0,cleanupFail=0;bool core=true;
    bool step(char c){trace+=c;return c!=fail&&c!=cleanupFail;}
    bool owner_core(){return core;}
    bool acquire_clock(){return step('c');} bool init_timer(){return step('i');}
    bool configure_timer(){return step('f');} bool install(){return step('h');}
    bool prepare_dac(){return step('p');} bool enable(){return step('e');}
    bool start_timer(){return step('s');} bool pause(){return step('P');}
    void midpoint(){trace+='M';} bool disable(){return step('D');}
    bool remove(){return step('H');} bool deinit(){return step('I');}
    bool release_clock(){return step('C');}
};
int main(){
    const char* failures="cifhpes";
    const char* expected[]={"c","ciC","cifPIC","cifhPIC","cifhpPMDHIC","cifhpePMDHIC","cifhpesPMDHIC"};
    unsigned cases=0;
    for(unsigned i=0;i<7;++i){Driver d;Sink s;d.fail=failures[i];cyd_platform::TimerLifecycle<Driver,Sink> life(d,s);
        assert(!life.start()&&!life.live()&&life.cleanup_ok());assert(d.trace==expected[i]);
        assert(!life.start());assert(life.stop());assert(d.trace==expected[i]);++cases;}
    for(char c:std::string("PDHIC")){Driver d;Sink s;d.cleanupFail=c;cyd_platform::TimerLifecycle<Driver,Sink> life(d,s);
        assert(life.start()&&life.live());assert(!life.stop()&&!life.live()&&!life.cleanup_ok());
        assert(d.trace==(c=='I'?"cifhpesPMDHI":"cifhpesPMDHIC"));assert(!life.stop()&&!life.start());assert(s.invalidations==2);++cases;}
    Driver d;Sink s;cyd_platform::TimerLifecycle<Driver,Sink> life(d,s);d.core=false;
    assert(!life.start()&&!life.stop()&&d.trace.empty());d.core=true;
    assert(life.start()&&life.live()&&s.invalidations==1);d.core=false;assert(!life.stop()&&life.live());
    d.core=true;assert(life.stop()&&life.cleanup_ok()&&!life.live());assert(d.trace=="cifhpesPMDHIC");
    assert(s.invalidations==2&&!life.start()&&life.stop());++cases;
    std::printf("%u lifecycle cases PASS\n",cases);
}
