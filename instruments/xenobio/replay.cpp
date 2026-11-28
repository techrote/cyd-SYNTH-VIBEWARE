#include "core.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>
using namespace xenobio;
static void le(std::ofstream &f,uint32_t v,int n){for(int i=0;i<n;++i)f.put(char(v>>(8*i)));}
static void wav(const std::string &p,const std::vector<int16_t> &s){std::ofstream f(p,std::ios::binary);f.write("RIFF",4);le(f,36+2*s.size(),4);f.write("WAVEfmt ",8);le(f,16,4);le(f,1,2);le(f,1,2);le(f,rate,4);le(f,rate*2,4);le(f,2,2);le(f,16,2);f.write("data",4);le(f,s.size()*2,4);for(auto v:s)le(f,uint16_t(v),2);assert(f.good());}
static void ppm(const std::string &p,const Engine &e){std::ofstream f(p,std::ios::binary);f<<"P6\n320 240\n255\n";uint16_t row[320];for(int y=0;y<240;++y){e.render(y,row);for(auto c:row){f.put(char((c>>11)*255/31));f.put(char(((c>>5)&63)*255/63));f.put(char((c&31)*255/31));}}assert(f.good());}
static void invariants(const Engine &e){assert(e.population()<=capacity);for(auto f:e.food)assert(f<=100);for(auto p:e.a){assert(p.e>=0&&p.e<=100);assert(p.x>=0&&p.x<W&&p.y>=0&&p.y<H&&p.s>=0&&p.s<3);}assert(e.events<=4);}
static std::vector<int16_t> run(Engine &e,int id,bool intervention,bool interaction,int chunk){e.reset(id);e.interactions=interaction;std::vector<int16_t>s;for(int i=0;i<rate*12;){if(i==rate*2&&intervention){e.touch(20,230,true);e.touch(160,120,true);}if(i==rate*3&&intervention)e.touch(240,100,false);if(i==rate*4)e.release();if(i==rate*6&&intervention){e.touch(180,230,true);e.touch(160,120,true);}if(i==rate*7)e.release();int n=std::min(chunk,rate-(i%rate));for(int j=0;j<n;++j){auto v=e.sample();assert(v>=-11200&&v<=11200);s.push_back(v);}i+=n;invariants(e);}return s;}
int main(int argc,char **argv){assert(argc==2);std::string out=argv[1];Engine a,b;for(int id=0;id<6;++id){auto s=run(a,id,true,true,128);auto t=run(b,id,true,true,37);assert(s==t);assert(a.food==b.food);assert(a.births==b.births&&a.deaths==b.deaths&&a.notes==b.notes);assert(a.ticks==240);assert(a.notes>0);wav(out+"/scene"+std::to_string(id)+".wav",s);ppm(out+"/scene"+std::to_string(id)+".ppm",a);std::printf("scene %d pop=%d births=%u deaths=%u moves=%u notes=%u\n",id,a.population(),a.births,a.deaths,a.moves,a.notes);}
 auto s=run(a,2,true,true,128),t=run(b,2,false,true,128);assert(s!=t);wav(out+"/no-touch.wav",t);auto u=run(b,2,true,false,128);assert(s!=u);wav(out+"/baseline.wav",u);
 a.reset(2);a.touch(100,100,true);for(int i=0;i<4*tick_samples;++i)a.sample();assert(!a.held);a.touch(100,100,false);assert(!a.held);a.touch(100,100,true);a.touch(319,239,false);assert(a.panic);for(int i=0;i<rate;++i)assert(a.sample()==0);a.touch(100,100,false);assert(a.panic);a.touch(250,230,true);assert(!a.panic&&!a.held);a.touch(-1,30,true);assert(!a.held);
 a.reset(3);for(int i=0;i<12000;++i){a.tick();invariants(a);}uint16_t row[322];row[0]=row[321]=0xdead;a.render(239,row+1);assert(row[0]==0xdead&&row[321]==0xdead);a.render(-1,row+1);assert(row[0]==0xdead&&row[321]==0xdead);std::printf("PASS six scenes, exact partition PCM, interventions, baseline, lease, drag panic, 12000 ecological ticks, Engine=%zu bytes\n",sizeof(Engine));}
