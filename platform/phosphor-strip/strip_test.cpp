#include "split.hpp"
#include "strip.hpp"
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace phosphor;
using cyd::platform::phosphor_strip;
using Clock=std::chrono::steady_clock;
static void need(bool b){if(!b)std::abort();}
struct CountedConst {
    const Engine& visual;
    mutable unsigned calls=0;
    void row(int y,uint16_t* p)const{++calls;visual.row(y,p);}
};
static void report(const char* label,std::vector<double>& v){
    std::sort(v.begin(),v.end());
    std::printf("%s us p50=%.3f p95=%.3f p99=%.3f max=%.3f\n",label,v[v.size()/2],v[(v.size()*95+99)/100-1],v[(v.size()*99+99)/100-1],v.back());
}
int main(){
    uint64_t pixels=0,samples=0,nonzero=0,rows=0,hash=14695981039346656037ull;
    std::vector<double> base,fast;
    std::array<uint16_t,320*240> a{},b{};
    std::array<uint16_t,320*8+2> scratch{};
    for(int s=0;s<6;++s){
        Split reference,optimized;reference.select(s);optimized.select(s);
        for(int f=0;f<240;++f){
            for(auto* e:{&reference,&optimized}){
                if(f==0||f==95||f==170)e->touch(20,100,true);
                else if(f==35||f==55)e->touch(10,230,true);
                else if(f==75)e->touch(150,230,true);
                else if(f==115)e->panic();
                else if(f==130)e->select(s);
                else if(f==200)e->up();
                else e->touch((f*17)%320,20+(f*13)%200,false);
                e->step_visual();
            }
            for(int i=0;i<step;++i){const auto p=reference.sample();need(p==optimized.sample());++samples;nonzero+=p!=0;}
            CountedConst c{optimized.visual()};
            auto original=[&]{for(int y=0;y<240;++y)reference.visual().row(y,a.data()+y*320);};
            auto candidate=[&]{for(int y=0;y<240;y+=8)need(phosphor_strip(c,y,8,b.data()+y*320));};
            auto timed=[&](auto fn,std::vector<double>& v){auto t=Clock::now();fn();v.push_back(std::chrono::duration<double,std::micro>(Clock::now()-t).count());};
            if(f%2){timed(candidate,fast);timed(original,base);}else{timed(original,base);timed(candidate,fast);}
            need(a==b);need(c.calls==56);rows+=c.calls;pixels+=a.size();
            for(auto p:b){hash^=p;hash*=1099511628211ull;}
            // Every legal start and length at least once per scene, all UI boundaries.
            if(f==1||f==36||f==116){
                for(int y=0;y<240;++y)for(int n=1;n<=8&&y+n<=240;++n){
                    scratch.fill(0xa55a);need(phosphor_strip(c,y,n,scratch.data()+1));
                    need(scratch[0]==0xa55a&&scratch[n*320+1]==0xa55a);
                    need(std::equal(scratch.begin()+1,scratch.begin()+1+n*320,a.begin()+y*320));
                }
            }
        }
    }
    Engine e;scratch.fill(0xa55a);
    for(auto y:{-1,240,2147483647})need(!phosphor_strip(e,y,1,scratch.data()));
    for(auto n:{-1,0,9,2147483647})need(!phosphor_strip(e,0,n,scratch.data()));
    need(!phosphor_strip(e,239,2,scratch.data()));need(!phosphor_strip(e,0,1,nullptr));
    need(std::all_of(scratch.begin(),scratch.end(),[](auto p){return p==0xa55a;}));
    need(nonzero>0);
    std::printf("PASS frames=%zu pixels=%llu PCM=%llu nonzero=%llu row_calls=%llu hash=%016llx\n",base.size(),(unsigned long long)pixels,(unsigned long long)samples,(unsigned long long)nonzero,(unsigned long long)rows,(unsigned long long)hash);
    report("original",base);report("row-reuse",fast);
}
