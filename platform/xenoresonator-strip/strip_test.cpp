#include "strip.hpp"
#include "exchange.hpp"
#include <array>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <vector>
using namespace portx;
using Clock=std::chrono::steady_clock;
static uint64_t pixels=0, samples=0, hash=0, active=0;
static void verify(const View& v,int y,int n){
    std::array<uint16_t,2562> b; b.fill(0xa55a);
    assert(portx_strip::render(v,y,n,b.data()+1,n*320));
    assert(b.front()==0xa55a&&b[n*320+1]==0xa55a&&b.back()==0xa55a);
    for(int j=0;j<n;++j)for(int x=0;x<320;++x){assert(b[1+j*320+x]==pixel(v,x,y+j));++pixels;}
}
static void report(const char* name,std::vector<long long>& v){std::sort(v.begin(),v.end());std::printf("%s p50/p95/p99/max ns=%lld/%lld/%lld/%lld\n",name,v[v.size()/2],v[(v.size()-1)*95/100],v[(v.size()-1)*99/100],v.back());}
int main(){
    View v;std::array<uint16_t,2562> guard;guard.fill(0xa55a);
    assert(!portx_strip::render(v,-1,1,guard.data(),guard.size()));
    assert(!portx_strip::render(v,239,2,guard.data(),guard.size()));
    assert(!portx_strip::render(v,0,9,guard.data(),guard.size()));
    assert(!portx_strip::render(v,0,0,guard.data(),guard.size()));
    assert(!portx_strip::render(v,0,1,guard.data(),319));
    assert(!portx_strip::render(v,0,1,nullptr,320));
    for(auto a:guard)assert(a==0xa55a);
    // All 256 amplitude levels, all six modes; edge and bar/crosshair priority.
    for(unsigned k=0;k<256;++k){for(unsigned i=0;i<6;++i)v.level[i]=(k+i*37)%256;v.x=k%2?319:0;v.y=k%184;v.held=k%3!=0;v.rake=k%2;v.coupled=k%3;v.material=k%4;for(int y=0;y<240;y+=8)verify(v,y,8);}
    Engine e,ref;Exchange q,r;uint32_t last=0,alarm=0,rl=0,ra=0;std::array<int16_t,128> a{},b{};
    std::array<uint16_t,320*240> old{},fast{};std::vector<long long> t0,t1;
    for(unsigned block=0;block<6000;++block){
        const unsigned phase=block%750, scene=(block/750)%4;
        auto send=[&](bool d,int x,int y){q.send(d,x,y);r.send(d,x,y);};
        if(phase==0){q.stop();r.stop();send(false,0,0);send(true,scene*53+4,200);send(false,0,0);send(true,160,230);send(false,0,0);}
        else if(phase==600){q.stop();r.stop();send(false,0,0);}
        else if(phase>4&&phase<500&&phase%3==0)send(true,(phase*7)%320,(phase*11)%184);
        else if(phase==500)send(false,0,0);
        q.pump(e,last,alarm);r.pump(ref,rl,ra);e.render(a.data(),a.size());ref.render(b.data(),b.size());assert(a==b);
        if(phase==600)for(auto s:a)assert(s==0);
        for(auto s:a){++samples;active+=s!=0;hash=hash*131+uint16_t(s);}
        q.publish(e.view());r.publish(ref.view());
        if(block%6==0){v=q.read();const View rv=r.read();auto start=Clock::now();for(int y=0;y<240;++y)for(int x=0;x<320;++x)old[y*320+x]=pixel(rv,x,y);auto mid=Clock::now();for(int y=0;y<240;y+=8)assert(portx_strip::render(v,y,8,fast.data()+y*320,2560));auto end=Clock::now();assert(old==fast);pixels+=old.size();t0.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(mid-start).count());t1.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end-mid).count());if(block%60==0)for(int n=1;n<=8;++n)verify(v,(block+n)%233,n);}
    }
    assert(samples==768000&&active>100000&&t0.size()==1000);
    std::printf("PASS pixels=%llu samples=%llu nonzero=%llu pcm_hash=%llu frames=%zu\n",(unsigned long long)pixels,(unsigned long long)samples,(unsigned long long)active,(unsigned long long)hash,t0.size());report("original",t0);report("strip",t1);
}
