#include "core.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>
#include <numeric>
#include <string>
using namespace phosphor;
static void need(bool v){if(!v)std::abort();}
static void le(std::ostream& f,uint32_t v,int n){while(n--){f.put(char(v&255));v>>=8;}}
static void wav(const std::string& p,const std::vector<int16_t>& a){std::ofstream f(p,std::ios::binary);f.write("RIFF",4);le(f,36+2*a.size(),4);f.write("WAVEfmt ",8);le(f,16,4);le(f,1,2);le(f,1,2);le(f,rate,4);le(f,rate*2,4);le(f,2,2);le(f,16,2);f.write("data",4);le(f,2*a.size(),4);for(auto v:a)le(f,uint16_t(v),2);need(bool(f));}
static void ppm(const std::string& p,const Engine& e){std::ofstream f(p,std::ios::binary);f<<"P6\n320 240\n255\n";std::array<uint16_t,322> row{};for(int y=0;y<240;++y){row.fill(0xdead);e.row(y,row.data()+1);need(row.front()==0xdead&&row.back()==0xdead);for(int x=1;x<=320;++x){auto v=row[x];f.put(char(((v>>11)&31)*255/31));f.put(char(((v>>5)&63)*255/63));f.put(char((v&31)*255/31));}}need(bool(f));}
int main(int argc,char**argv){need(argc==2);std::filesystem::create_directories(argv[1]);std::array<uint64_t,6> hashes{};
 for(int s=0;s<6;++s){Engine e,other,blank;e.select(s);other.select(s);blank.select(s);std::vector<int16_t> audio;uint64_t nonzero=0,hash=1469598103934665603ULL;int peak=0;
  for(int i=0;i<rate*8;++i){if(i%400==0){int t=i/400;int x=20+(t*7)%280,y=40+(t*11)%160;e.touch(x,y,i==0);other.touch(x,y,i==0);}auto a=e.sample(),b=other.sample();need(a==b&&blank.sample()==0);audio.push_back(a);nonzero+=a!=0;peak=std::max(peak,std::abs(int(a)));hash=(hash^uint16_t(a))*1099511628211ULL;}
  need(nonzero>rate&&peak<=8192&&e.field()==other.field());need(e.ticks()==240);hashes[s]=hash;std::string p=std::string(argv[1])+"/scene-"+std::to_string(s+1);wav(p+".wav",audio);ppm(p+".ppm",e);e.up();need(!e.held());e.touch(10,230,true);need(e.frozen());auto before=e.field();for(int i=0;i<step*10;++i)e.sample();need(before==e.field());e.touch(150,230,true);for(auto v:e.field())need(v==0);e.touch(10,230,true);need(!e.frozen());e.touch(120,100,true);need(e.held());for(int i=0;i<step*5;++i)e.sample();need(!e.held());e.touch(300,230,true);for(int i=0;i<rate;++i)need(e.sample()==0);need(e.muted());e.touch(100,100,true);need(!e.held());e.touch(10,5,true);need(!e.muted());
  std::printf("scene %d: nonzero=%llu peak=%d hash=%016llx\n",s+1,(unsigned long long)nonzero,peak,(unsigned long long)hash);}
 for(int i=0;i<6;++i)for(int j=0;j<i;++j)need(hashes[i]!=hashes[j]);
 Engine e;std::array<uint16_t,322> row{};row.fill(0xdead);e.row(-1,row.data()+1);e.row(240,row.data()+1);for(auto v:row)need(v==0xdead);
 std::printf("PASS: six distinct scenes, 1152000 exact paired PCM samples, silence ablation, freeze/clear/lease/panic, row guards; engine=%zu B\n",sizeof(Engine));}
