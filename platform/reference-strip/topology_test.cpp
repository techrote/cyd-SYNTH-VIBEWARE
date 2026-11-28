#include "strip.hpp"
#include <cstdio>
#include <cstdlib>
using namespace phase;
using namespace cyd_platform;
static uint64_t pixels=0,samples=0;
static void check(bool ok){if(!ok){std::fputs("topology strip mismatch\n",stderr);std::exit(1);}}
static void compare(const Trace& ref,const PhaseStripTrace& opt,const Control& c,unsigned rows){
    std::array<uint16_t,width*17+2> buf{};
    std::array<uint16_t,width> line{};
    for(unsigned y=0;y<height;y+=rows){
        const auto n=std::min(rows,height-y);buf.fill(0xa55a);opt.strip(c,y,rows,buf.data()+1);
        check(buf.front()==0xa55a);
        check(std::all_of(buf.begin()+1+n*width,buf.end(),[](uint16_t v){return v==0xa55a;}));
        for(unsigned i=0;i<n;++i){ref.row(c,y+i,line.data());check(std::equal(line.begin(),line.end(),buf.begin()+1+i*width));pixels+=width;}
    }
    buf.fill(0xa55a);opt.strip(c,height,rows,buf.data()+1);opt.strip(c,0,0,buf.data()+1);
    check(std::all_of(buf.begin(),buf.end(),[](uint16_t v){return v==0xa55a;}));
}
int main(){
    for(unsigned patch=0;patch<6;++patch){
        Engine a,b;Trace ref;PhaseStripTrace opt;Points pa,pb;Control c;c.preset=patch;
        std::array<int16_t,block> x{},y{};
        for(unsigned i=0;i<144;++i){
            c.topology=(i/48)%3;c.x=(i*29)%width;c.y=(i*17)%field;c.gate=i%48<42;c.alt=(i/12)%2;c.quant=(i/24)%2;c.panic=i%48==47;c.seq=i;
            a.render(c.pack(),x.data(),x.size(),&pa);b.render(c.pack(),y.data(),y.size(),&pb);check(x==y);samples+=x.size();ref.drain(pa);opt.drain(pb);
            if(i%12==11){c.loop=3;c.loopRate=(i/12)%3;c.loopReverse=(i/12)%2;for(unsigned n:{1u,4u,7u,8u,17u})compare(ref,opt,c,n);}
        }
    }
    std::printf("PASS topology pixels=%llu PCM=%llu\n",(unsigned long long)pixels,(unsigned long long)samples);
}
