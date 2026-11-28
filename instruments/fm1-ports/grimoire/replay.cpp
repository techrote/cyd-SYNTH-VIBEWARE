#include "core.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
using namespace grimoire;
void le(std::ostream& o,unsigned v,int n){while(n--){o.put(char(v&255));v>>=8;}}
void wav(const std::filesystem::path& p,const std::vector<std::int16_t>& v){std::ofstream o(p,std::ios::binary);o.write("RIFF",4);le(o,36+unsigned(v.size()*2),4);o.write("WAVEfmt ",8);le(o,16,4);le(o,1,2);le(o,1,2);le(o,rate,4);le(o,rate*2,4);le(o,2,2);le(o,16,2);o.write("data",4);le(o,unsigned(v.size()*2),4);for(auto s:v)le(o,std::uint16_t(s),2);assert(o.good());}
void ppm(const std::filesystem::path& p,Snapshot s){std::ofstream o(p,std::ios::binary);o<<"P6\n320 240\n255\n";std::array<std::uint16_t,322> pixels{};pixels.front()=pixels.back()=0x1234;for(int y=0;y<height;++y){row(s,y,pixels.data()+1);assert(pixels.front()==0x1234&&pixels.back()==0x1234);for(int x=1;x<=320;++x){auto c=pixels[x];o.put(char((c>>11)*255/31));o.put(char(((c>>5)&63)*255/63));o.put(char((c&31)*255/31));}}assert(o.good());}
std::vector<std::int16_t> phrase(int m,int partition,const std::filesystem::path& image={}){
    Engine e; e.material(m); std::vector<std::int16_t> v(8*rate); int peak=0,nonzero=0;
    for(int t=0;t<800;++t){
        if(t==0){e.set_path(m==0?ribbon:m==1?crown:pendulum);e.start_orbit();}
        if(t==100)e.speed(2);
        if(t==200)e.direction(-1);
        if(t==300)e.down({16,100});
        if(t>=301&&t<400)e.move({(t-300)*127/100,115-(t-300)});
        if(t==400)e.up();
        if(t==500){Path sketch=ribbon;sketch[0]={0,127};sketch[3]={127,0};assert(e.set_path(sketch));}
        if(t==600)e.stop_orbit();
        if(t==650)e.down({88,90});
        if(t==700)e.panic();
        if(t==450&&!image.empty())ppm(image,e.snapshot());
        for(int j=0;j<240;j+=partition)e.process(v.data()+t*240+j,std::min(partition,240-j));
        for(int j=0;j<240;++j){int s=v[t*240+j];peak=std::max(peak,std::abs(s));nonzero+=s!=0;if(t>=700)assert(s==0);}
    }
    assert(peak>1000&&peak<=8192&&nonzero>rate*4);std::cout<<"material "<<m<<" peak "<<peak<<" nonzero "<<nonzero<<'\n';return v;
}
int main(int argc,char** argv){
    assert(argc==2);const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
    Engine e;for(int i=0;i<rate;++i)assert(e.sample()==0);
    for(int x=1;x<128;++x)assert(increment(x)>=increment(x-1));
    e.down({-100,900});assert(e.snapshot().xy.x==0&&e.snapshot().xy.y==127);for(int i=0;i<rate;++i)e.sample();assert(!e.snapshot().manual&&e.snapshot().envelope==0);
    e.start_orbit();for(int i=0;i<1000;++i)e.sample();auto s=e.snapshot();e.direction(-1);assert(e.snapshot().cursor==s.cursor&&e.snapshot().envelope==s.envelope);e.sample();assert(e.snapshot().cursor!=s.cursor);auto bad=ribbon;bad[0].x=-1;assert(!e.set_path(bad));assert(e.snapshot().path[0].x==ribbon[0].x);
    e.down({70,50});for(int i=0;i<100;++i)e.sample();assert(e.snapshot().manual);e.cancel();for(int i=0;i<rate;++i)assert(e.sample()==0);e.move({1,1});assert(e.sample()==0);assert(!e.snapshot().auto_on);
    std::array<std::uint16_t,320> guard{};guard.fill(0x4321);row(e.snapshot(),-1,guard.data());row(e.snapshot(),240,guard.data());for(auto c:guard)assert(c==0x4321);
    std::array<std::vector<std::int16_t>,3> recordings;for(int m=0;m<3;++m){auto base=out/("material-"+std::to_string(m));recordings[m]=phrase(m,128,base.string()+".ppm");assert(recordings[m]==phrase(m,17));wav(base.string()+".wav",recordings[m]);}
    assert(recordings[0]!=recordings[1]&&recordings[0]!=recordings[2]&&recordings[1]!=recordings[2]);
    std::cout<<"PASS 576000 partition samples; 3 WAV/PPM pairs; engine bytes "<<sizeof(Engine)<<'\n';
}
