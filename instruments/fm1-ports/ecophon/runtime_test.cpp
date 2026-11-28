#include "runtime.hpp"
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
using namespace ecophon;
static bool silent(const std::array<int16_t,128>& a){return std::all_of(a.begin(),a.end(),[](int16_t v){return v==0;});}
int main(){
    Input in;Runtime r;std::array<int16_t,128> a{},b{};
    r.render(in,0,a.data(),a.size());assert(silent(a));
    assert(in.submit(true,20,10,1));r.render(in,1,a.data(),a.size());assert(!silent(a));
    assert(in.submit(true,200,150,2));assert(in.submit(true,300,230,2)==false);
    r.render(in,2,a.data(),a.size());assert(silent(a)&&r.engine.muted&&r.inhibited);
    assert(in.submit(true,20,10,3));r.render(in,3,a.data(),a.size());assert(silent(a));
    assert(in.submit(false,0,0,4));assert(in.submit(true,20,10,4));r.render(in,4,a.data(),a.size());assert(!silent(a));
    for(unsigned i=0;i<32;++i)assert(in.submit(true,100,140,5));
    assert(!in.submit(true,101,140,5));assert(!in.submit(false,0,0,5));r.render(in,5,a.data(),a.size());assert(silent(a)&&r.inhibited);
    assert(in.submit(false,0,0,6));assert(in.submit(true,20,10,6));r.render(in,6,a.data(),a.size());assert(!silent(a));
    r.render(in,156,a.data(),a.size());assert(silent(a)&&r.inhibited);
    assert(in.submit(false,0,0,157));assert(in.submit(true,20,10,157));r.render(in,157,a.data(),a.size());assert(!silent(a));
    assert(!in.submit(true,-1,10,158));r.render(in,158,a.data(),a.size());assert(silent(a));
    assert(in.submit(false,0,0,0xffffff80u));assert(in.submit(true,20,10,0xffffff80u));r.render(in,0xffffff80u,a.data(),a.size());assert(!silent(a));
    r.render(in,21,a.data(),a.size());assert(!silent(a));r.render(in,22,a.data(),a.size());assert(silent(a));
    assert(in.submit(false,0,0,24));assert(in.submit(true,20,10,24));r.render(in,23,a.data(),a.size());assert(!silent(a));
    // Exact baseline with a scripted release and environmental gestures.
    Engine direct;Touch touch;Runtime rt;Input input;Views frameViews;
    unsigned pixels=0;
    direct.reset(0,42);direct.panic();uint32_t now=0;unsigned compared=0;
    for(unsigned k=0;k<4500;++k){
        now=k*5;
        if(k==0||k==500||k==1000||k==2000){
            int x=k==0?20:(k==500?0:(k==1000?319:170)), y=k==0?10:150;
            assert(input.submit(false,0,0,now));touch.release();
            assert(input.submit(true,x,y,now));touch.contact(direct,x,y);
            assert(input.submit(false,0,0,now));touch.release();
        }
        rt.render(input,now,a.data(),a.size());direct.render(b.data(),b.size());assert(a==b);compared+=a.size();
        if(k%45==0){frameViews.publish(rt.engine);Engine view;assert(frameViews.latest(view));
            std::array<uint16_t,320> expected{},actual{};
            for(unsigned y=0;y<240;++y){row(direct,y,expected.data());row(view,y,actual.data());assert(expected==actual);pixels+=320;}
        }
    }
    // Concurrent FIFO and ownership transfer under publication overload.
    constexpr unsigned total=200000;
    Queue<unsigned,32> fifo;std::thread producer([&]{for(unsigned i=0;i<total;++i)while(!fifo.push(i))std::this_thread::yield();});
    for(unsigned i=0;i<total;++i){unsigned v;while(!fifo.pop(v))std::this_thread::yield();assert(v==i);}producer.join();
    Views views;std::atomic<bool> done{false};
    std::thread writer([&]{Engine e;for(unsigned i=1;i<=total;++i){e.ticks=i;e.births=i^0x12345678u;e.deaths=~i;for(auto& agent:e.agents)agent.phase=i;views.publish(e);}done.store(true,std::memory_order_release);});
    unsigned last=0,observed=0;Engine e;
    auto check=[&]{assert(e.ticks>last&&e.births==(e.ticks^0x12345678u)&&e.deaths==~e.ticks);for(auto ag:e.agents)assert(ag.phase==e.ticks);last=e.ticks;++observed;};
    while(!done.load(std::memory_order_acquire)){if(views.latest(e))check();}
    if(views.latest(e))check();
    writer.join();assert(last==total&&observed>0);
    // Host workload, including snapshot publication. No target extrapolation.
    Runtime perf;Input p;Views v;assert(p.submit(true,180,10,0));assert(p.submit(false,0,0,0));
    std::array<long long,12000> times{};
    for(unsigned i=0;i<times.size();++i){auto t=std::chrono::steady_clock::now();perf.render(p,i*5,a.data(),a.size());v.publish(perf.engine);times[i]=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-t).count();}
    std::sort(times.begin(),times.end());
    std::cout<<"PASS "<<compared<<" matched PCM; "<<pixels<<" matched pixels; "<<total<<" FIFO events; "<<total<<" publications, "<<observed<<" observed; Input/Runtime/Views "<<sizeof(Input)<<'/'<<sizeof(Runtime)<<'/'<<sizeof(Views)<<" B\n";
    std::cout<<"host block+publish ns p50/p95/p99/max "<<times[5999]<<'/'<<times[11399]<<'/'<<times[11879]<<'/'<<times.back()<<'\n';
}
