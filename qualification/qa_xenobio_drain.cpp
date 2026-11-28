// QA batch010: deterministic schedules around the actual M4R drain helper.
// Device headers are modeled PCM buffers, not WinMM execution or latency.
#include "runtime.hpp"
#include <array>
#include <cassert>
#include <cstdio>
using namespace xenobio;
using Block=std::array<int16_t,128>;
static bool silent(const Block& b){return std::all_of(b.begin(),b.end(),[](int16_t v){return v==0;});}
static Block arm(Runtime& r){
    assert(r.input.down(75,10));r.input.up();Block b{};
    for(unsigned i=0;i<400;++i){r.render(b.data(),b.size());if(!silent(b))return b;}
    assert(false&&"expected audible scene");return b;
}
struct Device {
    std::array<Block,4> q{};unsigned used=0,resets=0;
    void write(const Block& b){assert(used<q.size());q[used++]=b;}
    bool reset(){used=0;++resets;return true;}
    bool contains_sound()const{for(unsigned i=0;i<used;++i)if(!silent(q[i]))return true;return false;}
};
int main(){
    unsigned schedules=0,late_writes=0;
    // For each occupancy, place a panic at each reachable backend boundary.
    for(unsigned occupancy=0;occupancy<=4;++occupancy)for(unsigned stage=0;stage<(occupancy==4?2u:5u);++stage){
        Runtime r;EmergencyDrain d;Device dev;Block b=arm(r);
        for(unsigned i=0;i<occupancy;++i)dev.write(b);
        const auto reset=[&]{return dev.reset();};
        if(stage==0)r.input.emergency();
        assert(d.service(r.input,reset)==(stage==0?DrainResult::Drained:DrainResult::Unchanged));
        if(stage==1)r.input.emergency();
        if(dev.used<4){
            r.render(b.data(),b.size());
            if(stage==2)r.input.emergency();
            const auto result=d.service(r.input,reset);
            assert(result==(stage==1||stage==2?DrainResult::Drained:DrainResult::Unchanged));
            if(result==DrainResult::Unchanged){
                if(stage==3)r.input.emergency();
                dev.write(b);
                if(stage==4)r.input.emergency();
                if(stage>=3&&dev.contains_sound())++late_writes;
            }
        }
        // Next outer check must drain any late request, including full queue.
        assert(d.service(r.input,reset)==((stage>=3||(occupancy==4&&stage==1))?DrainResult::Drained:DrainResult::Unchanged));
        assert(!dev.contains_sound()&&dev.resets==1);
        r.render(b.data(),b.size());assert(silent(b)&&r.snapshot().panic);
        assert(d.service(r.input,reset)==DrainResult::Unchanged);
        assert(!silent(arm(r)));++schedules;
    }
    // Multiple acknowledged epochs before device service must coalesce safely;
    // a new epoch *during* reset must remain outstanding, even after failure.
    Runtime r;EmergencyDrain d;Device dev;Block b=arm(r);dev.write(b);
    for(unsigned i=0;i<1000;++i){r.input.emergency();r.render(b.data(),b.size());assert(silent(b));assert(!r.input.emergency_pending());}
    assert(r.input.emergency_generation()==1000);
    assert(d.service(r.input,[]{return false;})==DrainResult::Failed&&dev.contains_sound());
    assert(d.service(r.input,[&]{r.input.emergency();return dev.reset();})==DrainResult::Drained);
    assert(r.input.emergency_generation()==1001);
    assert(d.service(r.input,[&]{return dev.reset();})==DrainResult::Drained&&dev.resets==2);
    assert(d.service(r.input,[&]{return dev.reset();})==DrainResult::Unchanged);
    r.render(b.data(),b.size());assert(silent(b)&&r.snapshot().panic);
    assert(!silent(arm(r)));
    std::printf("PASS %u reachable occupancy/boundary schedules, %u late-write sound witnesses, 1001 request epochs, failed reset retry and fresh restart\n",schedules,late_writes);
}
