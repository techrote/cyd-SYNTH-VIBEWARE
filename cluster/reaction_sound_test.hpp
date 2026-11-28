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
    const auto full=controls(im,3,true);
    for(unsigned i=0;i<4;++i){
        const auto x=256u*(i%2u),y=256u*(i/2u);
        check(focus_word(full,x,y)==(0x03000000u|(63u<<(6u*i))),"focus corner isolation");
    }
    check(focus_word(full,999u,999u)==focus_word(full,256u,256u),"focus clamps");
    for(unsigned mode=0;mode<4;++mode)for(unsigned level=0;level<64;++level){
        const uint32_t raw=(uint32_t(mode)<<24)|level|((63u-level)<<6)|(level<<12)|((63u-level)<<18);
        check(focus_word(raw,128u,128u)==raw,"focus neutral exact");
    }
    for(unsigned y=0;y<=256u;y+=8u)for(unsigned x=0;x<=256u;x+=8u){
        const auto focused=focus_word(full,x,y);
        const auto mirror=focus_word(full,256u-x,y);
        check((focused>>24)==3u&&focus_word(0,x,y)==0,"focus palette and mute");
        for(unsigned i=0;i<4;++i){
            const unsigned gain=(focused>>(6u*i))&63u;
            check(gain==((mirror>>(6u*(i^1u)))&63u),"focus mirror symmetry");
            if(x<256u){
                const unsigned next=(focus_word(full,x+1u,y)>>(6u*i))&63u;
                check(i%2u?next>=gain:next<=gain,"focus monotone horizontal");
            }
        }
    }
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
    Voice focused_voice,focused_split;
    for(unsigned pos=0;pos<a.size();pos+=120){
        const auto control=focus_word(full,pos*256u/2400u,256u-pos*256u/2400u);
        focused_voice.render(control,a.data()+pos,120);
        focused_split.render(control,b.data()+pos,37);
        focused_split.render(control,b.data()+pos+37,83);
    }
    check(a==b,"moving focus audio block partition");
    for(auto sample:a)check(sample>=-24192&&sample<=24192,"moving focus headroom");
    focused_voice.render(focus_word(0,0,0),a.data(),a.size());
    for(unsigned i=768;i<a.size();++i)check(a[i]==0,"moving focus release");
    check(colour_word(0,3)==0,"colour mute");
    check(colour_word(full,999)==colour_word(full,3),"colour clamp");
    for(unsigned c=0;c<4;++c)for(unsigned x=0;x<=256;x+=8)for(unsigned y=0;y<=256;y+=8)
        check(focus_word(colour_word(full,c),x,y)==colour_word(focus_word(full,x,y),c),"colour focus commute");
    std::array<int16_t,2400> baseline{};
    Voice neutral_colour;
    neutral_colour.render(full,baseline.data(),baseline.size());
    for(unsigned c=0;c<4;++c){
        Voice cv,cs;
        cv.render(colour_word(full,c),a.data(),a.size());
        cs.render(colour_word(full,c),b.data(),37);
        cs.render(colour_word(full,c),b.data()+37,b.size()-37);
        check(a==b,"colour partition");
        check(c?a!=baseline:a==baseline,"colour neutral or distinct");
        for(auto sample:a)check(sample>=-24192&&sample<=24192,"colour headroom");
        cv.render(0,a.data(),a.size());
        for(unsigned i=768;i<a.size();++i)check(a[i]==0,"colour release");
    }
    Voice moving_colour,split_colour;
    for(unsigned pos=0;pos<a.size();pos+=120){
        const auto cw=colour_word(full,(pos/120)%4);
        moving_colour.render(cw,a.data()+pos,120);
        split_colour.render(cw,b.data()+pos,37);
        split_colour.render(cw,b.data()+pos+37,83);
    }
    check(a==b,"moving colour partition");
}
}
