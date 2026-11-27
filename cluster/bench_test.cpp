#include "bench.hpp"
#include "multi_field.hpp"
#include <cstdlib>
#include <iostream>
using namespace cluster;
static void check(bool condition){if(!condition){std::cerr<<"FAIL bench\n";std::exit(1);}}
int main(){
    using namespace bench;
    check(bucket(0)==0&&bucket(1)==0&&bucket(2)==1&&bucket(3)==2&&bucket(UINT32_MAX)==bins-1);
    Histogram h;
    for(uint32_t i=0;i<100000;++i)h.add(i);
    uint32_t sum=0;for(auto n:h.counts)sum+=n;check(sum==100000);
    Capture c;check(!c.record({2,1,0,0}));check(!c.record({1,1,0,3}));
    av::MultiField field;auto no_send=[](unsigned,const Frame&){return false;};
    for(unsigned run=0;run<2;++run){
        check(field.replay_reset());Capture current;
        uint32_t now=100;
        for(uint32_t seed=1;seed<=batches;++seed){
            const auto g=gesture(seed);field.xy(g.x,g.y);field.gate(g.gate);
            field.tick(now,no_send);check(!field.pending());
            check(field.shown().seed==seed);
            check(field.grid()==local_cohorts(cohorts(seed,g.x,g.y)));
            check(current.record({seed,10,digest(field.grid()),field.remote_cohorts()}));
            now+=250;
        }
        field.tick(now+1000,no_send,false);
        check(field.next_seed()==batches+1);
        check(!current.record({batches+1,0,0,0}));
        if(run==0)c=current;
        else for(unsigned i=0;i<batches;++i)check(c.rows[i].hash==current.rows[i].hash);
    }
    std::cout<<"PASS 128 matched scripted batches; histogram and replay reset checks\n";
}
