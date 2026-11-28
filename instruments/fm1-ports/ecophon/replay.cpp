#include "core.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <vector>
using namespace ecophon;
void le(std::ofstream& f,uint32_t v,unsigned n){while(n--){f.put(char(v&255));v>>=8;}}
void wav(const std::filesystem::path& p,const std::vector<int16_t>& a){std::ofstream f(p,std::ios::binary);f.write("RIFF",4);le(f,36+uint32_t(a.size()*2),4);f.write("WAVEfmt ",8);le(f,16,4);le(f,1,2);le(f,1,2);le(f,rate,4);le(f,rate*2,4);le(f,2,2);le(f,16,2);f.write("data",4);le(f,uint32_t(a.size()*2),4);for(auto v:a)le(f,uint16_t(v),2);assert(f.good());}
void ppm(const std::filesystem::path& p,const Engine& e){std::ofstream f(p,std::ios::binary);f<<"P6\n320 240\n255\n";std::array<uint16_t,322> pixels{};pixels.front()=123;pixels.back()=456;for(unsigned y=0;y<240;++y){row(e,y,pixels.data()+1);assert(pixels.front()==123&&pixels.back()==456);for(unsigned x=1;x<=320;++x){auto v=pixels[x];f.put(char(((v>>11)&31)*255/31));f.put(char(((v>>5)&63)*255/63));f.put(char((v&31)*255/31));}}row(e,240,pixels.data()+1);assert(f.good());}
void bounds(const Engine& e){for(auto a:e.agents){assert(a.energy>=0&&a.energy<=1024&&a.amplitude>=0&&a.amplitude<=2048&&a.patch<4&&a.species<3&&a.trait<7);}for(auto v:e.food)assert(v>=0&&v<=4096);assert(e.population()<=8);assert(e.population()+e.deaths==8+e.births);}
std::vector<int16_t> performance(unsigned scene,bool arc,unsigned partition,Engine& e){e.reset(scene,42);std::vector<int16_t> a(rate*24);for(unsigned block=0;block<240;++block){if(arc&&block==40)e.set({0,0,255});if(arc&&block==120)e.set({255,220,0});size_t at=block*2400,end=at+2400;while(at<end){size_t n=std::min<size_t>(partition,end-at);e.render(a.data()+at,n);at+=n;}bounds(e);}return a;}
int main(int argc,char** argv){assert(argc==2);std::filesystem::path out=argv[1];std::filesystem::create_directories(out);Engine e,b;Touch touch;uint64_t comparisons=0;
    for(unsigned s=0;s<4;++s){auto a=performance(s,false,127,e),z=performance(s,false,2400,b);assert(a==z);comparisons+=a.size();assert(e.ticks==2400);assert(std::any_of(a.begin(),a.end(),[](int16_t v){return v!=0;}));int peak=0;for(auto v:a)peak=std::max(peak,std::abs(int(v)));assert(peak<=16384);wav(out/("scene"+std::to_string(s)+".wav"),a);ppm(out/("scene"+std::to_string(s)+".ppm"),e);std::cout<<"scene "<<s<<" population "<<e.population()<<" births "<<e.births<<" deaths "<<e.deaths<<" moves "<<e.moves<<" gifts "<<e.gifts<<" peak "<<peak<<'\n';}
    auto arc=performance(0,true,113,e),steady=performance(0,false,2400,b);assert(std::equal(arc.begin(),arc.begin()+rate*4,steady.begin()));size_t changed=0;for(size_t i=0;i<arc.size();++i)changed+=arc[i]!=steady[i];assert(changed>rate);wav(out/"habitat-arc.wav",arc);ppm(out/"habitat-arc.ppm",e);
    e.reset(0,42);e.capture(0);auto initial=e.env;e.set({0,0,255});assert(e.recall(0));for(unsigned i=0;i<399*tick_samples;++i)e.sample();assert(!(e.env==initial));e.render(arc.data(),tick_samples);assert(e.env==initial&&e.ramp==0);e.set({0,0,255});assert(e.recall(0));e.set({33,44,55});assert(!e.ramp);e.sample();assert((e.env==Environment{33,44,55}));
    touch.contact(e,200,150);assert(touch.held);touch.expire(150);auto expired=e.env;touch.contact(e,0,150);assert(e.env==expired);touch.release();touch.contact(e,0,150);assert(e.env.fertility==0);touch.contact(e,300,230);assert(e.muted);for(int i=0;i<2400;++i)assert(e.sample()==0);touch.contact(e,20,10);assert(e.muted);touch.release();touch.contact(e,20,10);assert(!e.muted);touch.release();touch.contact(e,-1,0);assert(touch.inhibited);touch.release();
    e.reset(0,9);for(unsigned t=0;t<12000;++t){if(t%73==0)e.set({int(e.random()%256),int(e.random()%256),int(e.random()%256)});for(unsigned i=0;i<tick_samples;++i)assert(std::abs(int(e.sample()))<=16384);bounds(e);}
    std::cout<<"PASS "<<comparisons<<" partition samples; "<<changed<<"/"<<arc.size()<<" intervention differences; 12000 stress ticks; Engine "<<sizeof(Engine)<<" B\n";
}
