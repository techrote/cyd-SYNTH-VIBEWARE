#include "multi_field.hpp"
#include <iostream>
#include <cstdlib>
using namespace cluster;
using namespace cluster::av;
static unsigned checks=0;
#define CHECK(x) do { ++checks; if(!(x)) { std::cerr<<"FAIL line "<<__LINE__<<": "<<#x<<'\n'; std::exit(1); } } while(false)
int main() {
    for(unsigned nodes=0;nodes<=2;++nodes) {
        MultiField local, field; field.configure(nodes,91,92);
        std::array<Worker,2> workers{{Worker(91),Worker(92)}};
        std::array<Frame,2> response{};
        bool drop=false; unsigned sends=0;
        auto no_send=[](unsigned,const Frame&){return false;};
        auto send=[&](unsigned slot,const Frame& f) {
            ++sends;
            if(drop&&slot==1) return true;
            response[slot]=workers[slot].receive(f); return true;
        };
        Voice a,b; unsigned audible=0, mixed=0, fully_remote=0;
        const uint32_t base=0xffffff00u;
        for(uint32_t elapsed=0;elapsed<6000;elapsed+=5) {
            const uint32_t now=base+elapsed;
            if(elapsed%250==0) {
                const auto x=uint16_t(elapsed*31u),y=uint16_t(elapsed*17u+1234u);
                field.xy(x,y); local.xy(x,y);
            }
            if(elapsed==1000) drop=true;
            if(elapsed==2000) drop=false;
            if(elapsed==3000) field.remote(false);
            if(elapsed==4000) field.remote(true);
            const bool gate=elapsed<5000; local.gate(gate); field.gate(gate);
            for(unsigned slot=0;slot<nodes;++slot) if(response[slot].size) {
                const auto f=response[slot]; response[slot]={};
                field.receive(slot,f,now); field.receive(slot,f,now);
            }
            const unsigned before=sends;
            local.tick(now,no_send); field.tick(now,send);
            if(elapsed>=3000&&elapsed<4000) CHECK(sends==before);
            if(elapsed%250==200) {
                CHECK(!field.pending()); CHECK(local.grid()==field.grid());
                CHECK(local.shown().seed==field.shown().seed);
                CHECK(field.shown().particles==1024);
                unsigned sum=0; for(auto v:field.grid()) sum+=v; CHECK(sum==1024);
                CHECK(local.sound()==field.sound());
                std::array<uint16_t,width> r{},s{};
                for(unsigned y=0;y<height;y+=19) {
                    row(local.grid(),local.shown(),y,r.data()); row(field.grid(),field.shown(),y,s.data()); CHECK(r==s);
                }
                std::array<int16_t,128> p{},q{};
                a.render(local.sound(),p.data(),p.size()); b.render(field.sound(),q.data(),q.size()); CHECK(p==q);
                for(auto sample:q) if(sample) ++audible;
                if(field.remote_cohorts()==1) ++mixed;
                if(field.remote_cohorts()==2) ++fully_remote;
                if(elapsed>=3000&&elapsed<4000) CHECK(field.remote_cohorts()==0);
            }
            if(!gate) CHECK(field.sound()==0);
        }
        CHECK(audible>0); CHECK(field.stats().batches==24);
        CHECK(field.stats().remote+field.stats().local==48);
        if(nodes==2) CHECK(mixed>0);
        if(nodes>0) CHECK(fully_remote>0); else CHECK(sends==0);
        std::cout<<"nodes="<<nodes<<" batches="<<field.stats().batches<<" remote="<<field.stats().remote<<" local="<<field.stats().local<<" mixed="<<mixed<<'\n';
    }
    std::cout<<"PASS multi field checks="<<checks<<'\n';
}
