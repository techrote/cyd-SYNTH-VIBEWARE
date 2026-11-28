#include "priming.hpp"
#include "xeno/source/runtime.hpp"
#include "xeno/source/target_input.hpp"
#include "eco/target_gate.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
using Pcm=std::array<int16_t,128>;
using Words=std::array<uint8_t,128>;
static void check(bool ok){if(!ok){std::fputs("FAIL\n",stderr);std::abort();}}
static bool silent(const Pcm& p){return std::all_of(p.begin(),p.end(),[](int16_t v){return v==0;});}
static Words encode(const Pcm& p){Words w{};for(unsigned i=0;i<p.size();++i)w[i]=uint8_t(xenobio::dac_word(p[i])>>8);return w;}
static unsigned countOther(const Words& w,uint8_t v){return unsigned(std::count_if(w.begin(),w.end(),[=](uint8_t a){return a!=v;}));}
struct Ring {
    std::array<Words,4> slots{};
    void write(unsigned slot,const Words& w){slots.at(slot)=w;}
    void clear(unsigned slot){slots.at(slot).fill(0);}
    unsigned other(uint8_t v) const {unsigned n=0;for(const auto& w:slots)n+=countOther(w,v);return n;}
};
int main(){
    for(int i=-32768;i<=32767;++i)
        check((xenobio::dac_word(int16_t(i))>>8)==(ecophon::dacWord(int16_t(i))>>8));
    // Use actual engine output, not a synthetic nonzero PCM constant.
    xenobio::Runtime x; Pcm p{}; std::array<Words,4> active{}; unsigned consecutive=0;
    check(x.input.down(80,10)); x.input.up();
    for(unsigned i=0;i<2000 && consecutive<4;++i){
        x.render(p.data(),p.size()); auto w=encode(p);
        if(countOther(w,128)) active[consecutive++]=w; else consecutive=0;
    }
    check(consecutive==4); x.input.cancel(); x.render(p.data(),p.size()); check(silent(p));
    const Words midpoint=encode(p); check(countOther(midpoint,128)==0);
    unsigned residualMin=512,residualMax=0;
    for(unsigned slot=0;slot<4;++slot){
        Ring r; r.slots=active; r.write(slot,midpoint);
        unsigned residual=r.other(128); check(residual>0);
        residualMin=std::min(residualMin,residual); residualMax=std::max(residualMax,residual);
    }
    // Ecophon exact runtime: play, panic, release, deliberate scene restart.
    ecophon::Runtime e; ecophon::Input in;
    check(in.submit(true,80,10,0)); e.render(in,0,p.data(),p.size()); check(!silent(p));
    in.emergency(); e.render(in,1,p.data(),p.size()); check(silent(p));
    check(in.submit(false,0,0,2)); check(in.submit(true,80,10,2));
    e.render(in,2,p.data(),p.size()); check(!silent(p)); const Words restart=encode(p);
    for(auto w:restart)check(w>=64&&w<=191);
    unsigned unsafe=0;
    for(unsigned slot=0;slot<4;++slot){
        Ring r; r.write(slot,restart); // zero-clear then one successful write
        for(const auto& w:r.slots)unsafe+=unsigned(std::count(w.begin(),w.end(),uint8_t(0)));
    }
    check(unsafe==4*3*128);
    // Candidate rearm witness only where the backend can prove slot ownership.
    // Duplicate writes, stale epochs, partial writes and subsequent clear do
    // not count as proof. No physical or driver timing is simulated.
    std::array<unsigned,4> order{0,1,2,3}; unsigned permutations=0,checks=0;
    do {
        Ring r; cyd_platform::PrimingWitness proof; proof.begin(9);
        check(!proof.ready(9)); ++checks;
        for(unsigned i=0;i<4;++i){
            const auto s=order[i]; proof.written(8,s,true); check(!proof.ready(9)); ++checks;
            proof.written(9,s,false); check(!proof.ready(9)); ++checks;
            r.write(s,midpoint); proof.written(9,s,true); proof.written(9,s,true);
            check(proof.ready(9)==(i==3)); ++checks;
        }
        check(r.other(128)==0 && !proof.ready(8)); ++checks;
        for(unsigned s=0;s<4;++s){
            r.clear(s); proof.invalidated(s); check(!proof.ready(9)&&r.other(128)==128); ++checks;
            r.write(s,midpoint); proof.written(9,s,true); check(proof.ready(9)); ++checks;
        }
        proof.fault(); proof.written(9,0,true); check(!proof.ready(9)); ++checks;
        proof.begin(10); check(!proof.ready(10)); ++checks;
        ++permutations;
    }while(std::next_permutation(order.begin(),order.end()));
    std::puts("PASS 65536 exact DAC high-byte comparisons");
    std::printf("PASS XENOBIO modeled residual non-midpoint samples after one neutral write: %u..%u\n",residualMin,residualMax);
    std::printf("PASS ECOPHON modeled zero DAC codes after one write: %u total, 384 per ring\n",unsafe);
    std::printf("PASS descriptor proof: %u orders, %u assertions, %zu bytes host state\n",permutations,checks,sizeof(cyd_platform::PrimingWitness));
}
