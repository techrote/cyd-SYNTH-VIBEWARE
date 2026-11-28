#include "multi.hpp"
#include <cstdlib>
#include <iostream>
using namespace cluster;
static unsigned checks=0;
static void check(bool pass) { ++checks; if(!pass) { std::cerr<<"multi check "<<checks<<" failed\n"; std::exit(1); } }
int main() {
    for(unsigned nodes=0;nodes<=2;++nodes) {
        Multi multi(nodes,91,92);
        std::array<Worker,2> workers{{Worker(91),Worker(92)}};
        std::array<Frame,2> pending{};
        bool outage=false;
        auto send=[&](unsigned slot,const Frame& f) {
            if(slot==1&&outage) return true; // radio accepted but lost
            pending[slot]=workers[slot].receive(f);
            return true;
        };
        uint32_t now=0xfffffff0u; // deadline and retry arithmetic crosses wrap
        auto pump=[&](){
            multi.tick(now,send);
            for(unsigned s=0;s<nodes;++s) if(pending[s].size) {
                const auto f=pending[s]; pending[s]={};
                multi.receive(s,f,now);
                multi.receive(s,f,now); // duplicate
            }
            ++now;
        };
        for(unsigned i=0;i<100;++i) {
            outage=nodes==2&&i>=20&&i<40;
            for(unsigned n=0;n<120;++n) pump();
            const auto jobs=cohorts(i+1,uint16_t(i*503u),uint16_t(i*113u));
            const auto remote_before=multi.stats.remote;
            const auto local_before=multi.stats.local;
            check(multi.begin(jobs,30)); check(!multi.begin(jobs,30));
            for(unsigned t=0;t<100&&multi.active();++t) pump();
            check(!multi.active()); check(multi.grid==local_cohorts(jobs));
            uint32_t sum=0; for(auto n:multi.grid) sum+=n; check(sum==1024);
            check(multi.stats.batches==i+1);
            const unsigned expected_remote=nodes==0?0u:(outage?1u:2u);
            check(multi.stats.remote-remote_before==expected_remote);
            check(multi.stats.local-local_before==2-expected_remote);
        }
        auto bad=cohorts(1,1,1); bad[1].steps=0;
        check(!multi.begin(bad,30)); check(!multi.begin(cohorts(1,1,1),0));
        std::cout<<"nodes="<<nodes<<" batches="<<multi.stats.batches<<" remote="<<multi.stats.remote<<" local="<<multi.stats.local<<'\n';
    }
    std::cout<<"PASS multi checks="<<checks<<'\n';
}
