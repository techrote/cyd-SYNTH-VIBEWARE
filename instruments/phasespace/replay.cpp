#include "core.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>
#include <array>
using namespace phase;
// Arduino.h defines this function-like macro after core.hpp on the target.
#define word(...) makeWord(__VA_ARGS__)
static unsigned checks=0;
static void require(bool b,const char* why){++checks;if(!b){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
static void le(std::ofstream& f,uint32_t n,unsigned count){for(unsigned i=0;i<count;++i)f.put(char((n>>(8*i))&255));}
static void wav(const std::string& path,const std::vector<int16_t>& s){
    std::ofstream f(path,std::ios::binary);require(bool(f),"wav open");
    f.write("RIFF",4);le(f,36+uint32_t(s.size()*2),4);f.write("WAVEfmt ",8);
    le(f,16,4);le(f,1,2);le(f,1,2);le(f,rate,4);le(f,rate*2,4);le(f,2,2);le(f,16,2);
    f.write("data",4);le(f,uint32_t(s.size()*2),4);for(auto v:s)le(f,uint16_t(v),2);
    require(bool(f),"wav write");
}
static void ppm(const std::string& path,const Trace& trace,const Control& c){
    std::ofstream f(path,std::ios::binary);require(bool(f),"ppm open");
    f<<"P6\n320 240\n255\n";std::array<uint16_t,width> row{};
    for(unsigned y=0;y<height;++y){trace.row(c,y,row.data());for(auto v:row){
        f.put(char((v>>11)*255/31));f.put(char(((v>>5)&63)*255/63));f.put(char((v&31)*255/31));
    }}require(bool(f),"ppm write");
}
static uint64_t hash(const std::vector<int16_t>& s){uint64_t h=1469598103934665603ull;for(auto v:s){h^=uint16_t(v);h*=1099511628211ull;}return h;}
static std::vector<int16_t> audition(unsigned patch,bool split,const std::string& dir){
    Engine e;Points p;Trace t;Surface s;s.c.preset=patch;
    std::vector<int16_t> all;all.reserve(128*600);
    std::array<int16_t,block> pcm{};
    for(unsigned b=0;b<600;++b){
        int x=35+int((b*3)%250),y=20+int((b*5)%175);
        if(b>=280&&b<410)s.c.alt=true;
        if(b>=410)s.c.quant=true;
        s.touch(b<570,x,y);
        auto w=s.c.pack();
        if(split){e.render(w,pcm.data(),37,&p);e.render(w,pcm.data()+37,block-37,&p);}
        else e.render(w,pcm.data(),pcm.size(),&p);
        t.drain(p);
        for(auto v:pcm){require(v>=-7800&&v<=7800,"PCM bounds");all.push_back(v);}
        if(!dir.empty()&&b==220)ppm(dir+"/patch-"+std::to_string(patch+1)+".ppm",t,s.c);
    }
    require(std::all_of(all.end()-512,all.end(),[](int16_t a){return a==0;}),"release silence");
    require(std::any_of(all.begin(),all.end(),[](int16_t a){return a!=0;}),"non-silent patch");
    if(!dir.empty())wav(dir+"/patch-"+std::to_string(patch+1)+".wav",all);
    return all;
}
int main(int argc,char** argv){
    std::string dir=argc>1?argv[1]:"";std::array<uint64_t,6> hashes{};
    for(unsigned i=0;i<6;++i){
        auto a=audition(i,false,dir),b=audition(i,true,"");require(a==b,"split block exact");
        hashes[i]=hash(a);for(unsigned j=0;j<i;++j)require(hashes[i]!=hashes[j],"distinct patches");
        std::printf("%s hash=%016llx\n",patches[i].name,(unsigned long long)hashes[i]);
    }
    Surface s;s.touch(true,-20,-1);require(s.c.gate&&s.c.x==0&&s.c.y==0,"clamped contact");
    s.touch(true,318,239);require(s.c.gate&&s.c.y==207&&!s.c.panic,"drag into toolbar");
    s.touch(false,0,0);require(!s.c.gate,"contact release");
    s.touch(true,180,235);require(s.c.preset==5&&!s.c.gate,"patch tap");
    s.touch(true,290,235);require(!s.c.panic,"toolbar drag not retriggered");
    s.touch(false,0,0);s.touch(true,300,235);require(s.c.panic,"panic");
    s.touch(false,0,0);s.touch(true,160,100);require(s.c.gate&&!s.c.panic,"fresh gesture");
    Engine e;std::array<int16_t,4096> a{};e.render(s.c.pack(),a.data(),a.size());
    require(std::all_of(a.end()-512,a.end(),[](int16_t v){return v==0;}),"stale heartbeat release");
    s.heartbeat();e.render(s.c.pack(),a.data(),128);
    require(std::any_of(a.begin(),a.begin()+128,[](int16_t v){return v!=0;}),"fresh heartbeat plays");
    s.cancel();e.render(s.c.pack(),a.data(),a.size());
    require(std::all_of(a.begin(),a.end(),[](int16_t v){return v==0;}),"panic immediate zero");
    // Scripted gesture recording, replay, wrap, bounded capacity and manual override.
    Surface loop;loop.touch(true,270,235);loop.touch(false,270,235);
    require(loop.c.loop==1&&!loop.c.gate,"R arms without sound");
    loop.touch(true,20,50);loop.tick(0);loop.touch(true,100,80);loop.tick(32);
    loop.touch(true,240,10);loop.tick(64);loop.touch(false,240,10);loop.tick(96);
    require(loop.c.loop==3&&!loop.c.gate,"release starts loop with gap");
    Engine le;std::vector<int16_t> loopAudio;std::array<int16_t,768> lp{};
    constexpr unsigned xs[]={20,100,240};
    for(unsigned i=0;i<16;++i){
        loop.touch(false,0,0);loop.tick(128+i*32);
        require(loop.c.gate==(i%4!=3),"loop gap cadence");
        if(i%4!=3)require(loop.c.x==xs[i%4],"ordered loop coordinates");
        le.render(loop.c.pack(),lp.data(),lp.size());
        if(i%4==3)require(std::all_of(lp.end()-128,lp.end(),[](int16_t v){return v==0;}),"loop gap silence");
        else require(std::any_of(lp.begin(),lp.end(),[](int16_t v){return v!=0;}),"loop sounds");
        loopAudio.insert(loopAudio.end(),lp.begin(),lp.end());
    }
    loop.touch(true,90,100);require(loop.c.loop==0&&loop.c.gate&&loop.c.x==90,"manual takeover");
    loop.cancel();le.render(loop.c.pack(),lp.data(),lp.size());
    require(std::all_of(lp.begin(),lp.end(),[](int16_t v){return v==0;}),"loop panic");
    if(!dir.empty())wav(dir+"/gesture-loop.wav",loopAudio);
    Surface cap;cap.touch(true,270,235);cap.touch(false,0,0);cap.touch(true,0,80);
    cap.tick(0xfffffff0u);
    for(unsigned i=1;i<100;++i){cap.touch(true,int(i),80);cap.tick(0xfffffff0u+i*32u);}
    cap.touch(false,0,0);cap.tick(3200);
    for(unsigned i=0;i<64;++i){cap.tick(3232+i*32);require(cap.c.gate&&cap.c.x==i,"bounded wrapped capture");}
    cap.tick(3232+64*32);require(!cap.c.gate,"capacity gap");
    cap.touch(true,305,235);require(cap.c.panic&&cap.c.loop==0&&!cap.c.gate,"toolbar panic stops loop");
    // Playback direction changes at cycle boundary, speed immediately; no catch-up bursts.
    Surface perf;perf.touch(true,270,235);perf.touch(false,270,235);
    perf.touch(true,20,50);perf.tick(0);perf.touch(true,100,80);perf.tick(32);
    perf.touch(true,240,10);perf.tick(64);perf.touch(false,240,10);perf.tick(96);
    perf.tick(128);require(perf.c.x==20,"normal first point");
    perf.touch(true,270,235);perf.touch(true,60,30);perf.touch(false,60,30);
    require(perf.c.loop==3&&perf.c.loopRate==2&&perf.c.loopReverse,"fast reverse selected");
    perf.tick(143);require(perf.c.x==20,"fast interval not early");
    perf.tick(144);require(perf.c.x==100,"current cycle stays forward");
    perf.tick(160);require(perf.c.x==240,"current cycle finishes forward");
    perf.tick(176);require(!perf.c.gate,"fast gap");
    perf.tick(192);require(perf.c.gate&&perf.c.x==240,"reverse first point");
    perf.tick(208);require(perf.c.x==100,"reverse middle point");
    perf.touch(true,270,235);perf.touch(true,250,215);perf.touch(false,250,215);
    require(perf.c.loopRate==0&&!perf.c.loopReverse,"slow forward selected");
    perf.tick(271);require(perf.c.x==100,"slow interval not early");
    perf.tick(272);require(perf.c.x==20,"reverse cycle preserved");
    perf.tick(1000);require(!perf.c.gate,"late tick advances only once");
    perf.tick(1064);require(perf.c.x==20&&perf.c.gate,"forward resumes");
    perf.touch(true,270,235);perf.touch(false,270,235);
    require(perf.c.loop==0&&!perf.c.gate,"R tap stops");
    // Renderer joins nearby samples, refuses long chords and remains clipped.
    Points seg;Trace connected;Control visual;std::array<uint16_t,width> row{};
    seg.push({10,20});seg.push({30,30});connected.drain(seg);
    connected.row(visual,25,row.data());require(row[20]==patches[0].color,"nearby trace connection");
    seg.push({300,30});connected.drain(seg);connected.row(visual,30,row.data());
    require(row[150]==0x0842,"no long invented chord");
    // Actual musical witness: identical captured path, normal/fast-reverse/slow-forward.
    if(!dir.empty()){
        Surface musical;Engine synth;Points plot;Trace view;
        std::vector<int16_t> performance;std::array<int16_t,384> slice{};
        uint32_t ms=0;
        auto tick=[&](){
            musical.tick(ms);synth.render(musical.c.pack(),slice.data(),slice.size(),&plot);
            view.drain(plot);performance.insert(performance.end(),slice.begin(),slice.end());ms+=16;
        };
        musical.c.quant=true;musical.c.preset=5;
        musical.touch(true,270,235);musical.touch(false,270,235);
        for(unsigned i=0;i<48;++i){
            int x=25+int(i*265/47),y=104+int(76*std::sin(tau*float(i)/47));
            musical.touch(true,x,y);tick();
        }
        musical.touch(false,0,0);
        for(unsigned i=0;i<100;++i){tick();if(i==48)ppm(dir+"/loop-normal.ppm",view,musical.c);}
        musical.touch(true,270,235);musical.touch(true,60,30);musical.touch(false,60,30);
        for(unsigned i=0;i<100;++i){tick();if(i==53)ppm(dir+"/loop-fast-reverse.ppm",view,musical.c);}
        musical.touch(true,270,235);musical.touch(true,250,215);musical.touch(false,250,215);
        for(unsigned i=0;i<200;++i){tick();if(i==92)ppm(dir+"/loop-slow-forward.ppm",view,musical.c);}
        musical.cancel();for(unsigned i=0;i<8;++i)tick();
        require(std::all_of(performance.end()-3072,performance.end(),[](int16_t v){return v==0;}),"performance panic tail");
        wav(dir+"/gesture-tempo-direction.wav",performance);
    }
    Points q;for(unsigned i=0;i<256;++i)require(q.push({uint16_t(i),0}),"queue capacity");
    require(!q.push({0,0}),"queue full drop");Point p;
    for(unsigned i=0;i<256;++i)require(q.pop(p)&&p.x==i,"FIFO");
    require(!q.pop(p),"queue empty");
    Trace t;std::array<uint16_t,width+2> guard{};guard.front()=123;guard.back()=321;
    for(unsigned y=0;y<height;++y)t.row(s.c,y,guard.data()+1);
    require(guard.front()==123&&guard.back()==321,"row bounds");
    std::printf("PASS %u checks; 6 patches, split replay, release, panic, lease, FIFO, RGB565 row bounds\n",checks);
}
