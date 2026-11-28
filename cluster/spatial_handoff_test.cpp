#include "spatial_handoff.hpp"
#include <cstdlib>
#include <iostream>
using cluster::spatial::Handoff;
static void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
int main(){
    Handoff h;Handoff::Image a{},b{},c{};b.fill(255);c.fill(80);
    h.select(a,false,0xfffffff0u);check(h.pixels()==a&&!h.remote()&&!h.fading(),"initial");
    h.select(b,true,0xfffffff0u);check(h.pixels()==a&&h.remote()&&h.fading(),"switch origin");
    h.select(b,true,134);check(h.pixels()[0]==128,"half across wrap");
    const auto previous=h.pixels();h.select(c,false,135);check(h.pixels()==previous&&!h.remote(),"reverse without jump");
    h.select(c,false,285);check(h.pixels()[0]==104,"reverse half");
    h.select(a,false,435);check(h.pixels()==a&&!h.fading(),"live destination at end");
    h.select(b,true,436);h.select(c,false,437,true);check(h.pixels()==c&&!h.fading()&&!h.remote(),"immediate override");
    for(unsigned i=0;i<256;++i){
        a.fill(uint8_t(i));b.fill(uint8_t(255-i));h.select(a,false,1000,true);
        h.select(b,true,1000);unsigned prior=i;
        for(unsigned t=0;t<=300;++t){
            h.select(b,true,1000+t);const unsigned v=h.pixels()[0];
            check(v==(i*(300-t)+(255-i)*t+150)/300,"rounded interpolation");
            check(i<128?v>=prior:v<=prior,"monotonic");prior=v;
            for(auto cell:h.pixels())check(cell==v,"coherent cells");
        }
        check(h.pixels()==b&&!h.fading(),"exact endpoint");
    }
    std::cout<<"PASS handoff: 256 intensity pairs, 301 time points each; wrap, reversal, immediate override\n";
}
