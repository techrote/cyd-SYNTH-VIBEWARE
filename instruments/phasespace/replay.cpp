#include "core.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>
#include <array>
using namespace phase;
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
        auto w=s.c.word();
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
    Engine e;std::array<int16_t,4096> a{};e.render(s.c.word(),a.data(),a.size());
    require(std::all_of(a.end()-512,a.end(),[](int16_t v){return v==0;}),"stale heartbeat release");
    s.heartbeat();e.render(s.c.word(),a.data(),128);
    require(std::any_of(a.begin(),a.begin()+128,[](int16_t v){return v!=0;}),"fresh heartbeat plays");
    s.cancel();e.render(s.c.word(),a.data(),a.size());
    require(std::all_of(a.begin(),a.end(),[](int16_t v){return v==0;}),"panic immediate zero");
    Points q;for(unsigned i=0;i<256;++i)require(q.push({uint16_t(i),0}),"queue capacity");
    require(!q.push({0,0}),"queue full drop");Point p;
    for(unsigned i=0;i<256;++i)require(q.pop(p)&&p.x==i,"FIFO");
    require(!q.pop(p),"queue empty");
    Trace t;std::array<uint16_t,width+2> guard{};guard.front()=123;guard.back()=321;
    for(unsigned y=0;y<height;++y)t.row(s.c,y,guard.data()+1);
    require(guard.front()==123&&guard.back()==321,"row bounds");
    std::printf("PASS %u checks; 6 patches, split replay, release, panic, lease, FIFO, RGB565 row bounds\n",checks);
}
