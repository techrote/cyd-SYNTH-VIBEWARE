#include "strip.hpp"
#include <cassert>
#include <chrono>
#include <iostream>
#include <vector>
using namespace grimoire;
using Clock=std::chrono::steady_clock;
using Frame=std::array<std::uint16_t,width*height>;
static Frame baseline{},fast{};
static std::array<std::uint16_t,width*8+2> buf{};
static std::uint64_t pixels=0;
void compare(const Snapshot& s,int first,int count){
    buf.fill(0x1234); assert(render_strip(s,first,count,buf.data()+1));
    assert(buf.front()==0x1234&&buf[width*count+1]==0x1234);
    for(int y=first;y<first+count;++y){row(s,y,baseline.data()+y*width);for(int x=0;x<width;++x){assert(baseline[y*width+x]==buf[1+(y-first)*width+x]);++pixels;}}
}
void baseline_frame(const Snapshot& s){for(int y=0;y<height;++y)row(s,y,baseline.data()+y*width);}
void fast_frame(const Snapshot& s){for(int y=0;y<height;y+=8)assert(render_strip(s,y,8,fast.data()+y*width));}
std::uint32_t rng=0x5a2381;
int random_coord(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return int(rng&127);}
int main(){
    Engine e; std::vector<double> old_us,new_us; std::uint64_t hash=0;
    buf.fill(0x1234);auto s=e.snapshot();
    assert(!render_strip(s,-1,1,buf.data())&&!render_strip(s,240,1,buf.data())&&!render_strip(s,239,2,buf.data())&&!render_strip(s,0,0,buf.data())&&!render_strip(s,0,9,buf.data())&&!render_strip(s,0,1,nullptr));
    for(auto v:buf)assert(v==0x1234);
    for(int m=0;m<3;++m){
        e.material(m); e.set_path(m==0?ribbon:m==1?crown:pendulum);e.start_orbit();
        for(int f=0;f<240;++f){
            if(f==20)e.down({0,127});
            if(f==70)e.up();
            if(f==90)e.direction(-1);
            if(f==110)e.speed(4);
            if(f==140){Path path{};for(auto& p:path)p={random_coord(),random_coord()};assert(e.set_path(path));}
            if(f==180)e.panic();
            if(f==200)e.down({127,0});
            if(f==220)e.up();
            if(f>=21&&f<70)e.move({random_coord(),random_coord()});
            for(int i=0;i<800;++i)e.sample();
            s=e.snapshot();
            auto t=Clock::now();baseline_frame(s);auto mid=Clock::now();fast_frame(s);auto end=Clock::now();
            assert(baseline==fast);pixels+=baseline.size();hash=hash*1315423911u+fast[(f*317u)%fast.size()];
            old_us.push_back(std::chrono::duration<double,std::micro>(mid-t).count());new_us.push_back(std::chrono::duration<double,std::micro>(end-mid).count());
            if(f==0||f==141||f==180){for(int y=0;y<height;++y)for(int n=1;n<=std::min(8,height-y);++n)compare(s,y,n);}
        }
    }
    std::sort(old_us.begin(),old_us.end());std::sort(new_us.begin(),new_us.end());
    for(auto* v:{&old_us,&new_us})std::cout<<"us median="<<(*v)[v->size()/2]<<" p99="<<(*v)[v->size()*99/100]<<" max="<<v->back()<<'\n';
    std::cout<<"PASS "<<pixels<<" RGB565 pixels, 720 actual-core frames, hash "<<hash<<'\n';
}
