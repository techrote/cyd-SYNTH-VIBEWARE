#pragma once
#include "core.hpp"
#include <algorithm>
#include <cstddef>
namespace portx_strip {
// Native-endian RGB565, same contract as portx::pixel. No persistent storage.
inline void span(uint16_t* p,int a,int b,uint16_t color){
    a=std::clamp(a,0,320); b=std::clamp(b,0,320);
    if(a<b)std::fill(p+a,p+b,color);
}
inline void row(const portx::View& v,int y,uint16_t* p){
    if(y>=224){span(p,0,133,v.rake?0x2104:0x4d36);span(p,133,266,v.rake?0x4d36:0x2104);span(p,266,320,0x7800);return;}
    if(y>=184){for(int i=0;i<6;++i)span(p,i*53,i==5?320:(i+1)*53,i==5?0x7800:(i==int(v.material)||(i==4&&v.coupled))?0x4d36:0x2104);return;}
    span(p,0,320,(y==166||y%40==0)?0x2945:0x0862);p[159]=0x2945;
    if(v.held){const int dy=std::abs(y-int(v.y));if(dy<=8)span(p,int(v.x)-2,int(v.x)+3,0xffff);if(dy<=2)span(p,int(v.x)-8,int(v.x)+9,0xffff);}
    if(y<166){for(int i=0;i<6;++i){const int h=int(v.level[i])*140/255;if(y>=166-h){const int x=(i/3)*160+(i%3)*53;span(p,x+7,x+46,i<3?0x57ff:0xfd20);}}}
}
inline bool render(const portx::View& v,int y,int n,uint16_t* p,std::size_t capacity){
    if(!p||y<0||y>=240||n<1||n>8||n>240-y||capacity<std::size_t(n)*320)return false;
    for(int j=0;j<n;++j)row(v,y+j,p+j*320);
    return true;
}
}
