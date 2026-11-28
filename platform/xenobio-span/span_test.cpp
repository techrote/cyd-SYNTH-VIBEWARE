#include "span.hpp"
#include <chrono>
#include <cstdio>
#include <array>
#include <vector>
#include <cstdlib>
using xenobio::Engine;
using Row=void(*)(const Engine&,int,uint16_t*);
static void require(bool b){if(!b)std::abort();}
static unsigned long long pixels=0,frames=0;
static void compare(const Engine& e){
    std::array<uint16_t,322> a{},b{};
    a.fill(0xbeef);b=a;
    for(int y=-1;y<=240;++y){
        xenobio::labeled_row(e,y,a.data()+1);
        cyd::platform::xenobio_span_row(e,y,b.data()+1);
        require(a==b && b.front()==0xbeef && b.back()==0xbeef);
        if(y>=0&&y<240)pixels+=320;
    }
    ++frames;
}
static uint64_t bench(Row row,const std::vector<Engine>& states,uint64_t& hash){
    std::array<uint16_t,320> a{};hash=0;
    const auto start=std::chrono::steady_clock::now();
    for(const auto& e:states)for(int y=0;y<240;++y){row(e,y,a.data());for(int x=0;x<320;x+=29)hash=hash*33+a[x];}
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();
}
int main(){
    std::vector<Engine> states;
    for(int s=0;s<6;++s){Engine e;e.reset(s);
        for(int t=0;t<120;++t){
            if(t%20==0){e.touch(40+80*((t/20)%3),230,true);e.touch((t*11+s*47)%320,20+(t*7+s*13)%200,true);}
            if(t%20==1)e.touch((t*13+s*47)%320,20+(t*11+s*13)%200,false);
            if(t%20==3)e.release();
            for(int i=0;i<xenobio::tick_samples;++i)(void)e.sample();
            compare(e);if(t%3==0)states.push_back(e);
        }
        e.stop();compare(e);
    }
    // Exhaustive legal cell positions, food shades, contact modes and agent radii;
    // overlapping agents with distinct species preserve painter order.
    Engine e;e.reset(0);
    for(int n=0;n<160;++n){e.a={};e.held=true;e.tx=n%16;e.ty=n/16;e.mode=n%3;
        for(int i=0;i<160;++i)e.food[i]=uint8_t((n+i)%101);
        for(int i=0;i<24;++i)e.a[i]={n%16,n/16,1+(n*24+i)%100,i%3};
        compare(e);
    }
    printf("PASS frames=%llu pixels=%llu states=%zu\n",frames,pixels,states.size());
    std::array<uint64_t,7>a{},b{};uint64_t ha=0,hb=0;
    for(int i=0;i<7;++i){if(i%2){b[i]=bench(cyd::platform::xenobio_span_row,states,hb);a[i]=bench(xenobio::labeled_row,states,ha);}else{a[i]=bench(xenobio::labeled_row,states,ha);b[i]=bench(cyd::platform::xenobio_span_row,states,hb);}require(ha==hb);}
    std::sort(a.begin(),a.end());std::sort(b.begin(),b.end());
    printf("240-frame host batch ns baseline min/median/max=%llu/%llu/%llu span=%llu/%llu/%llu hash=%llu\n",(unsigned long long)a[0],(unsigned long long)a[3],(unsigned long long)a[6],(unsigned long long)b[0],(unsigned long long)b[3],(unsigned long long)b[6],(unsigned long long)ha);
}
