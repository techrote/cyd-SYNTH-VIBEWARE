#pragma once
#include "core.hpp"

namespace grimoire {
// Caller owns a contiguous RGB565 buffer of width*count pixels. The
// snapshot must remain immutable throughout the call. No scratch allocation.
inline bool render_strip(const Snapshot& s,int first,int count,std::uint16_t* dst) {
    if(!dst||first<0||first>=height||count<1||count>8||count>height-first) return false;
    const int end=first+count;
    for(int y=first;y<end;++y) {
        auto* line=dst+(y-first)*width;
        for(int x=0;x<width;++x) {
            if(y<32) {
                const int g=x/64;
                line[x]=((g<3&&g==s.material)||(g==3&&s.auto_on)||(g==4&&s.muted))?0x5d8b:0x2104;
            } else line[x]=(x%40==0||y%40==0)?0x2945:0x0842;
        }
    }
    if(end<=32) return true;
    for(std::size_t i=0;i<8;++i) {
        auto a=screen(s.path[i]); const auto b=screen(s.path[(i+1)%8]);
        const int dx=std::abs(b.x-a.x),dy=-std::abs(b.y-a.y),sx=a.x<b.x?1:-1,sy=a.y<b.y?1:-1;
        int err=dx+dy;
        for(;;) {
            if(a.y>=first&&a.y<end&&a.y>=32) dst[(a.y-first)*width+a.x]=0x7bef;
            if(a.x==b.x&&a.y==b.y) break;
            const int e=2*err;
            if(e>=dy){err+=dy;a.x+=sx;}
            if(e<=dx){err+=dx;a.y+=sy;}
        }
    }
    const auto p=screen(s.xy);
    constexpr std::uint16_t colors[3]={0xfd20,0x9e7f,0xb81f};
    for(int i=0;i<3;++i) {
        const int ox=triangle(s.phase[i])*25/32768,oy=triangle(s.phase[i]+0x40000000u)*25/32768;
        for(int y=std::max({first,32,p.y+oy-3});y<std::min(end,p.y+oy+4);++y)
            for(int x=std::max(0,p.x+ox-3);x<std::min(width,p.x+ox+4);++x)
                dst[(y-first)*width+x]=colors[i];
    }
    for(int y=std::max({first,32,p.y-6});y<std::min(end,p.y+7);++y)
        for(int x=std::max(0,p.x-6);x<std::min(width,p.x+7);++x)
            if(x==p.x||y==p.y) dst[(y-first)*width+x]=0xffff;
    for(int y=std::max(first,234);y<end;++y)
        for(int x=0;x<s.envelope*width/4096;++x) dst[(y-first)*width+x]=0x07e0;
    return true;
}
}
