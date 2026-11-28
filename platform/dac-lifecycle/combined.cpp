// Original PLATFORM host experiment. Logical clock, not physical timing.
#include "runtime.hpp"
#include "sample_sink.hpp"
#include <cassert>
#include <chrono>
#include <iostream>
#include <vector>
using namespace ecophon;
using Clock=std::chrono::steady_clock;
struct Times {
    std::vector<uint64_t> v;
    template<class F> void run(F f){const auto a=Clock::now();f();v.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-a).count());}
    void report(const char* name){std::sort(v.begin(),v.end());std::cout<<name<<" n="<<v.size()<<" p50_ns="<<v[v.size()/2]<<" p95_ns="<<v[(v.size()-1)*95/100]<<" p99_ns="<<v[(v.size()-1)*99/100]<<" max_ns="<<v.back()<<'\n';}
};
int main(){
    Runtime rt,reference; Input in,ri; Views views; SampleSink sink;
    Engine frame{}; std::array<int16_t,128> pcm{},expected{}; std::array<uint16_t,320*4> pixels{};
    Times dsp,ring,strip; uint32_t nextTouch=0,nextStrip=0;
    unsigned scan=240, frames=0, emergencies=0; uint64_t pcmCount=0,active=0,renderHash=0,pcmHash=0;
    constexpr unsigned blocks=11250; // 60 seconds at 24 kHz logical time
    for(unsigned b=0;b<blocks;++b){
        const uint32_t now=b*128*1000/rate;
        auto submit=[&](bool down,int x,int y,uint32_t ms){const bool a=in.submit(down,x,y,ms),c=ri.submit(down,x,y,ms);assert(a==c&&a);};
        while(nextTouch<=now){
            const unsigned t=nextTouch%3000;
            if(t==0){in.emergency();ri.emergency();++emergencies;}
            else if(t==12)submit(false,0,0,nextTouch);
            else if(t==24)submit(true,int((nextTouch/3000)%4)*80+8,10,nextTouch);
            else if(t==36||t==2400)submit(false,0,0,nextTouch);
            else if(t>36&&t<2400)submit(true,int(nextTouch*7%320),112+int(nextTouch*3%80),nextTouch);
            nextTouch+=12;
        }
        dsp.run([&]{rt.pump(in,now,[&]{sink.invalidate();});rt.engine.render(pcm.data(),pcm.size());views.publish(rt.engine);});
        reference.render(ri,now,expected.data(),expected.size());assert(pcm==expected);
        ring.run([&]{
            if(!sink.ready())assert(sink.tick(false)==SampleSink::midpoint);
            assert(sink.push(pcm.data()));
            for(auto s:pcm){const auto c=sink.tick(false);assert(c==SampleSink::encode(s));++pcmCount;active+=c!=128;pcmHash=pcmHash*131+c;}
        });
        while(nextStrip<=now){
            if(scan==240){views.latest(frame);scan=0;}
            strip.run([&]{for(unsigned i=0;i<4;++i)row(frame,scan+i,pixels.data()+320*i);});
            for(auto p:pixels)renderHash=renderHash*131+p;
            scan+=4;if(scan==240)++frames;
            ++nextStrip; // modeled 1 ms loop yield; no SPI transfer or touch hardware
        }
    }
    assert(pcmCount==1440000&&active>100000&&frames>=999&&emergencies==20);
    std::cout<<"samples="<<pcmCount<<" nonmidpoint="<<active<<" frames="<<frames<<" emergency="<<emergencies<<" pcm_hash="<<pcmHash<<" rgb565_hash="<<renderHash<<'\n';
    dsp.report("pump_render_publish");ring.report("encode_and_128_ticks");strip.report("four_row_raster");
}
