#include "strip.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
using namespace phase;
using namespace cyd_platform;
static uint64_t compared=0;
static void check(bool b){if(!b){std::fputs("FAIL\n",stderr);std::exit(1);}}
static void match(const Trace& t,const PhaseStripTrace& s,const Control& c,unsigned rows){
    std::array<uint16_t,width*16+2> buf{};std::array<uint16_t,width> ref{};
    for(unsigned y=0;y<height;y+=rows){unsigned n=std::min(rows,height-y);buf.fill(0xa55a);s.strip(c,y,n,buf.data()+1);
        check(buf[0]==0xa55a&&buf[n*width+1]==0xa55a);
        for(unsigned j=0;j<n;++j){t.row(c,y+j,ref.data());check(std::equal(ref.begin(),ref.end(),buf.begin()+1+j*width));compared+=width;}
    }
}
static void feed(Trace& a,PhaseStripTrace& b,Points& p){Points clone;Point v;while(p.pop(v)){check(clone.push(v));} // requeue identical bounded data into two queues
    Points other;while(clone.pop(v)){check(p.push(v));check(other.push(v));}a.drain(p);b.drain(other);
}
static void bench(const Trace& t,const PhaseStripTrace& s,const Control& c,unsigned work){
    std::array<uint16_t,width*8> buf{};volatile uint32_t witness=0;
    for(unsigned rep=0;rep<5;++rep)for(unsigned variant=0;variant<3;++variant){unsigned n=variant==0?1:variant==1?4:8;auto begin=std::chrono::steady_clock::now();
        for(unsigned f=0;f<400;++f)for(unsigned y=0;y<height;y+=n){if(variant==0)t.row(c,y,buf.data());else s.strip(c,y,n,buf.data());for(unsigned j=0;j<n;++j)witness+=buf[j*width+(f+y+j)%width];}
        auto end=std::chrono::steady_clock::now();std::printf("work=%u rows=%u rep=%u us/frame=%.2f witness=%u\n",work,n,rep,std::chrono::duration<double,std::micro>(end-begin).count()/400,unsigned(witness));
    }
}
int main(int argc,char**){
    for(unsigned patch=0;patch<6;++patch){Engine e,control;Points p;Trace t;PhaseStripTrace s;Surface touch;touch.c.preset=patch;std::array<int16_t,block>a{},b{};
        for(unsigned i=0;i<72;++i){touch.touch(i<65,int((i*17)%320),int((i*9)%208));e.render(touch.c.pack(),a.data(),a.size(),&p);control.render(touch.c.pack(),b.data(),b.size());check(a==b);feed(t,s,p);
            if(i%9==0)for(unsigned n:{1u,4u,8u,16u})match(t,s,touch.c,n);
            if(patch==5&&i==63&&argc>1)bench(t,s,touch.c,0);}
    }
    Trace t;PhaseStripTrace s;Points p;Control c;c.gate=true;c.preset=5;c.x=0;c.y=207;c.loop=3;c.loopReverse=true;c.loopRate=2;
    for(unsigned i=0;i<128;++i)check(p.push({uint16_t(i%2?80:40),100}));feed(t,s,p);for(unsigned n:{1u,4u,8u,16u})match(t,s,c,n);if(argc>1)bench(t,s,c,1);
    uint32_t rng=12345;for(unsigned i=0;i<80;++i){for(unsigned j=0;j<17;++j){rng=rng*1664525u+1013904223u;auto x=uint16_t(rng%340);rng=rng*1664525u+1013904223u;check(p.push({x,uint16_t(rng%230)}));}feed(t,s,p);c.x=i*4;c.y=i%208;c.preset=i%6;c.alt=i&1;c.quant=i&2;c.panic=i&4;c.loop=i%4;c.loopRate=i%3;match(t,s,c,4);match(t,s,c,8);}
    std::printf("PASS pixels=%llu trace_bytes=%zu baseline_bytes=%zu scratch4=%zu scratch8=%zu\n",(unsigned long long)compared,sizeof(PhaseStripTrace),sizeof(Trace),sizeof(uint16_t)*width*4,sizeof(uint16_t)*width*8);
}
