#include "core.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>
using namespace gridseq;
static unsigned checks=0;
static void req(bool b,const char* s){++checks;if(!b){std::fprintf(stderr,"FAIL %s\n",s);std::exit(1);}}
static void le(std::ofstream& f,uint32_t v,unsigned n){while(n--){f.put(char(v&255u));v>>=8;}}
static void wav(const std::string& path,const std::vector<int16_t>& s){std::ofstream f(path,std::ios::binary);f.write("RIFF",4);le(f,36+uint32_t(s.size()*2),4);f.write("WAVEfmt ",8);le(f,16,4);le(f,1,2);le(f,1,2);le(f,rate,4);le(f,rate*2,4);le(f,2,2);le(f,16,2);f.write("data",4);le(f,uint32_t(s.size()*2),4);for(auto v:s)le(f,uint16_t(v),2);req(bool(f),"wav");}
static void ppm(const std::string& path,const Control& c,uint32_t head){std::ofstream f(path,std::ios::binary);f<<"P6\n320 240\n255\n";std::array<uint16_t,width> line{};for(unsigned y=0;y<height;++y){row(c,head,y,line.data());for(auto v:line){f.put(char((v>>11)*255/31));f.put(char(((v>>5)&63)*255/63));f.put(char((v&31)*255/31));}}req(bool(f),"ppm");}
static std::vector<int16_t> scene(unsigned s,bool split,const std::string& dir){Engine e;Control c;c.scene=s;c.bpm=scenes[s].bpm;c.masks=scenes[s].masks;c.play=true;std::atomic<uint32_t> head{0};std::vector<int16_t> a;std::array<int16_t,block> p{};int peak=0;
 for(unsigned b=0;b<1500;++b){++c.seq;if(b==1200)c.bpm=std::min(180u,c.bpm+16);if(b==1350)c.mute=5;if(b==1460)c.play=false;
  if(split){e.render(c,p.data(),31,&head);e.render(c,p.data()+31,block-31,&head);}else e.render(c,p.data(),block,&head);
  for(int v:p){req(v>=-8000&&v<=8000,"gain");peak=std::max(peak,std::abs(v));a.push_back(int16_t(v));}if(b==600&&!dir.empty())ppm(dir+"/scene-"+std::to_string(s+1)+".ppm",c,head.load());}
 req(peak>100,"audible scene");req(std::all_of(a.end()-512,a.end(),[](auto v){return !v;}),"stop silence");if(!dir.empty())wav(dir+"/scene-"+std::to_string(s+1)+".wav",a);
 if(!split)std::printf("%s peak=%d events=%llu,%llu,%llu,%llu\n",scenes[s].name,peak,(unsigned long long)e.events()[0],(unsigned long long)e.events()[1],(unsigned long long)e.events()[2],(unsigned long long)e.events()[3]);
 return a;
}
int main(int argc,char** argv){std::string dir=argc>1?argv[1]:"";std::array<std::vector<int16_t>,6> a;
 for(unsigned s=0;s<6;++s){a[s]=scene(s,false,dir);req(a[s]==scene(s,true,""),"partition exact");for(unsigned j=0;j<s;++j)req(a[j]!=a[s],"distinct scenes");}
 Surface u;u.touch(true,48,25);u.touch(true,319,25);u.touch(false,0,0);req(u.c.masks[0]==0,"erase sweep");u.touch(true,48,25);u.touch(true,319,25);u.touch(false,0,0);req(u.c.masks[0]==65535,"paint sweep");u.touch(true,10,60);u.touch(true,10,60);req(u.c.mute==1,"mute edge");u.touch(false,0,0);u.touch(true,10,220);req(u.c.play,"transport");u.cancel();u.touch(true,10,220);req(!u.c.play,"held cancel");u.touch(false,0,0);u.touch(true,10,220);req(u.c.play,"fresh transport");
 Exchange ex;Control c;ex.publish(u.c);req(ex.read(c)&&c.masks==u.c.masks&&c.play==u.c.play&&c.mute==u.c.mute,"coherent roundtrip");
 for(unsigned bpm:{50u,137u,180u}){Engine e;c=Control{};c.masks.fill(65535);c.bpm=bpm;c.play=true;std::array<int16_t,block> p{};constexpr unsigned n=24000*60;for(unsigned i=0;i<n;i+=block){++c.seq;e.render(c,p.data(),std::min(block,n-i));}for(unsigned t=0;t<tracks;++t){unsigned ticks=bpm*divisions[t];req(e.events()[t]==ticks+1,"minute exact events");req(e.steps()[t]==ticks%lengths[t],"minute exact phase");}}
 Engine e;c=Control{};c.play=true;c.masks.fill(65535);++c.seq;std::array<int16_t,8192> pcm{};e.render(c,pcm.data(),pcm.size());req(std::all_of(pcm.end()-512,pcm.end(),[](auto v){return !v;}),"lease silence");++c.seq;e.render(c,pcm.data(),block);req(std::all_of(pcm.begin(),pcm.begin()+block,[](auto v){return !v;}),"lease no implicit resume");c.play=false;e.render(c,pcm.data(),block);c.play=true;++c.epoch;++c.seq;e.render(c,pcm.data(),block);req(std::any_of(pcm.begin(),pcm.begin()+block,[](auto v){return v!=0;}),"explicit restart");c.mute=15;++c.seq;e.render(c,pcm.data(),block);req(std::all_of(pcm.begin(),pcm.begin()+block,[](auto v){return !v;}),"all mute");
 Engine reset;Control r;r.play=true;r.seq=1;std::array<int16_t,block> first{},again{};reset.render(r,first.data(),block);for(unsigned i=0;i<100;++i){++r.seq;reset.render(r,again.data(),block);}++r.epoch;++r.seq;reset.render(r,again.data(),block);req(first==again,"deterministic reset");
 Engine drift;Control d;d.play=true;d.masks.fill(65535);for(unsigned i=0;i<300;++i){d.bpm=i<123?73:161;++d.seq;drift.render(d,again.data(),block);}for(unsigned t=0;t<tracks;++t){uint64_t ticks=uint64_t(block)*(123*73+177*161)*divisions[t]/denom;req(drift.events()[t]==ticks+1&&drift.steps()[t]==ticks%lengths[t],"tempo phase continuity");}
 std::array<uint16_t,width+2> line{};line.front()=0x1234;line.back()=0xabcd;for(unsigned y=0;y<=height;++y){row(c,0x12345,y,line.data()+1);req(line.front()==0x1234&&line.back()==0xabcd,"row bounds");}
 std::array<unsigned,6> labels{};for(unsigned y=0;y<24;++y){row(c,0,y,line.data()+1);for(unsigned x=0;x<width;++x)if(line[x+1]==0xffff)++labels[x*6/320];}for(unsigned n:labels)req(n>=12,"visible scene digit");
 // Accent editing is orthogonal to note enable and clock timing.
 Surface edit;auto original=edit.c.masks;edit.touch(true,35,25);edit.touch(false,0,0);req(edit.c.accentEdit==1&&edit.c.mute==0,"accent mode");
 edit.touch(true,48,25);edit.touch(true,319,25);edit.touch(false,0,0);req(edit.c.accents[0]==65535&&edit.c.masks==original,"accent paint independent");
 edit.touch(true,48,25);edit.touch(true,319,25);edit.touch(false,0,0);req(edit.c.accents[0]==0,"accent erase");
 edit.c.accents={0x1111,0x222,0x155,0x49};ex.publish(edit.c);req(ex.read(c)&&c.accents==edit.c.accents&&c.accentEdit==1,"accent exchange");
 edit.touch(true,100,10);edit.touch(false,0,0);req(edit.c.accents==Control{}.accents&&edit.c.accentEdit==0,"scene clears accents");
 Engine plain,accent,splitAccent;Control base; base.play=true;base.masks={0xffff,0,0,0};Control emph=base;emph.accents[0]=0x1111;
 std::vector<int16_t> pa,aa;std::array<int16_t,block> pb{},ab{},sb{};int ppeak=0,apeak=0;unsigned changed=0;
 for(unsigned i=0;i<750;++i){++base.seq;emph.seq=base.seq;plain.render(base,pb.data(),block);accent.render(emph,ab.data(),block);splitAccent.render(emph,sb.data(),31);splitAccent.render(emph,sb.data()+31,block-31);req(ab==sb,"accent partition");
  for(unsigned j=0;j<block;++j){ppeak=std::max(ppeak,std::abs(int(pb[j])));apeak=std::max(apeak,std::abs(int(ab[j])));changed+=pb[j]!=ab[j];}pa.insert(pa.end(),pb.begin(),pb.end());aa.insert(aa.end(),ab.begin(),ab.end());}
 req(plain.events()==accent.events()&&plain.steps()==accent.steps(),"accent clock identity");req(apeak>ppeak&&changed>1000,"accent audio causality");
 emph.accents.fill(65535);emph.masks.fill(65535);emph.bpm=180;for(unsigned i=0;i<1500;++i){++emph.seq;accent.render(emph,ab.data(),block);for(int v:ab)req(v>=-12000&&v<=12000,"accent stress headroom");}
 row(emph,0,40,line.data()+1);req(std::count(line.begin()+1,line.end()-1,0xffe0)>50,"visible accent markers");
 if(!dir.empty()){wav(dir+"/accent-off.wav",pa);wav(dir+"/accent-on.wav",aa);ppm(dir+"/accent-grid.ppm",emph,0);}
 std::printf("Accent comparison peak=%d/%d changed=%u/%zu\n",ppeak,apeak,changed,pa.size());
 std::printf("PASS %u checks; Engine=%zu Surface=%zu Exchange=%zu bytes\n",checks,sizeof(Engine),sizeof(Surface),sizeof(Exchange));
}
