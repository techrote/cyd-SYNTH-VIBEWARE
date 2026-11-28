#pragma once
#include "reaction_sound.hpp"
namespace cluster::spatial::sound {
template<class Check> void regression(Check check){
    Image im{};
    check(controls(im,0,true)==0,"silent field");
    im.fill(255);
    check(controls(im,0,true)==0xffffffu,"full field");
    check(controls(im,3,false)==0,"explicit mute");
    for(unsigned region=0;region<4;++region){
        im.fill(0);
        for(unsigned y=0;y<12;++y)for(unsigned x=0;x<16;++x)
            im[(y+12*(region/2))*32+x+16*(region%2)]=255;
        check(controls(im,0,true)==(63u<<(6*region)),"regional independence");
    }
    im.fill(255);
    std::array<int16_t,2400> a{},b{};
    Voice whole,split;
    const auto word=controls(im,2,true);
    whole.render(word,a.data(),a.size());
    for(unsigned pos=0;pos<a.size();pos+=120)split.render(word,b.data()+pos,120);
    check(a==b,"audio block partition");
    bool audible=false;
    for(auto x:a){check(x>=-24192&&x<=24192,"headroom");if(x>1000||x< -1000)audible=true;}
    check(audible,"nonzero synthesis");
    whole.render(0,a.data(),a.size());
    for(unsigned i=768;i<a.size();++i)check(a[i]==0,"bounded release");
    Voice first,other;
    first.render(controls(im,0,true),a.data(),a.size());
    other.render(controls(im,1,true),b.data(),b.size());
    check(a!=b,"distinct palette");
}
}
