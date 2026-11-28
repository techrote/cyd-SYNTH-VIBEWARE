#include "exchange.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
using namespace portx;
static void le(std::ofstream& f,unsigned v,unsigned bytes){while(bytes--){f.put(char(v&255));v>>=8;}}
static void wav(const std::string& p,const std::vector<int16_t>& a){std::ofstream f(p,std::ios::binary);f.write("RIFF",4);le(f,36+unsigned(a.size()*2),4);f.write("WAVEfmt ",8);le(f,16,4);le(f,1,2);le(f,1,2);le(f,rate,4);le(f,rate*2,4);le(f,2,2);le(f,16,2);f.write("data",4);le(f,unsigned(a.size()*2),4);for(auto v:a)le(f,uint16_t(v),2);assert(f.good());}
static void ppm(const std::string& p,const View& v){std::ofstream f(p,std::ios::binary);f<<"P6\n320 240\n255\n";for(int y=0;y<240;++y)for(int x=0;x<320;++x){auto c=pixel(v,x,y);f.put(char(((c>>11)&31)*255/31));f.put(char(((c>>5)&63)*255/63));f.put(char((c&31)*255/31));}assert(f.good());}
static std::vector<int16_t> phrase(unsigned mat,bool coupling,unsigned block,View* view=nullptr){Engine e;e.input(true,int(mat*53+20),210);e.input(false,0,0);if(!coupling){e.input(true,235,210);e.input(false,0,0);}std::vector<int16_t> a(rate*4);for(unsigned i=0;i<a.size();){unsigned t=i%(rate);unsigned beat=i/rate;unsigned step=std::min({block, unsigned(a.size()-i),128u-i%128, unsigned(rate-t)});if(i%128==0||t==0)e.input(t<rate/3,40+int(beat)*66,20+int(t/128)%100);e.render(a.data()+i,step);i+=step;if(view&&i==128*16)*view=e.view();}return a;}

static void exchange_tests(){
  Exchange q;Engine e;uint32_t last=0,alarm=0;int16_t a[128];
  auto pump=[&]{q.pump(e,last,alarm);e.render(a,128);};
  auto silent=[&]{for(auto x:a)assert(x==0);};
  auto audible=[&]{assert(std::any_of(std::begin(a),std::end(a),[](int16_t x){return x!=0;}));};
  q.send(true,110,20);q.send(false,110,20);pump();audible();assert(!e.view().held);
  q.send(true,235,210);q.send(false,0,0);q.send(true,235,210);q.send(false,0,0);pump();assert(e.view().coupled);
  q.send(true,180,210);q.send(false,0,0);q.send(true,110,20);pump();assert(e.view().material==3&&e.view().held);audible();
  q.send(true,160,20);q.stop();pump();silent();assert(!e.view().held);
  q.send(true,110,20);q.stop();q.send(true,80,30);pump();audible();assert(e.view().held);
  q.stop();pump();silent();
  for(unsigned i=0;i<=Exchange::capacity;++i)q.send(true,100,20);
  assert(q.overflows.load()==1);pump();silent();q.send(true,90,20);pump();silent();
  q.send(false,0,0);q.send(true,90,20);pump();audible();
  q.send(true,120,20);q.discard(e,last,alarm);pump();silent();
  last=UINT32_MAX-1;q.readpos.store(last);q.write.store(last);
  q.send(true,150,30);q.send(false,0,0);pump();audible();assert(last==0&&!e.view().held);
  std::cout<<"PASS: ordered short tap, double toggle, preset/strike, panic epoch, overflow release, disarm discard and cursor wrap; Exchange bytes "<<sizeof(Exchange)<<"\n";
}
static std::vector<int16_t> rake_phrase(unsigned mat,bool rake,unsigned block,View* frame=nullptr){
  Engine e;e.input(true,int(mat*53+20),210);e.input(false,0,0);e.input(true,rake?180:50,232);e.input(false,0,0);
  std::vector<int16_t> out(rate*4);
  for(unsigned i=0;i<out.size();){const unsigned t=i%(rate*2), step=std::min({block,unsigned(out.size()-i),240u-i%240});
    if(i%240==0){const int x=t<rate?20+int(t*280/rate):300-int((t-rate)*280/rate);e.input(t<rate*3/2,x,45+int(t/240)%80);}
    e.render(out.data()+i,step);i+=step;if(frame&&i==rate/2)*frame=e.view();
  }return out;
}
static void rake_tests(int argc,char** argv){
  for(unsigned m=0;m<4;++m){View v;auto a=rake_phrase(m,true,128,&v),b=rake_phrase(m,true,17),c=rake_phrase(m,false,128);assert(a==b&&a!=c&&v.rake);for(auto x:a)assert(std::abs(int(x))<=12000);if(argc>1){const std::string p=std::string(argv[1])+"/rake-"+std::to_string(m);wav(p+".wav",a);ppm(p+".ppm",v);}}
  Engine a,b;for(auto* e:{&a,&b}){e->input(true,180,232);e->input(false,0,0);e->input(true,235,210);e->input(false,0,0);e->input(true,20,40);}
  int16_t x[480],y[480];a.input(true,280,40);a.render(x,480);b.render(y,480);assert(std::equal(x,x+480,y)); // no retrigger before sample cooldown
  a.input(true,280,40);a.render(x,480);b.render(y,480);assert(!std::equal(x,x+480,y));
  a.panic();a.release();a.render(x,480);for(auto v:x)assert(v==0);assert(!a.view().held);
  Exchange q;q.publish(a.view());assert(q.read().rake);a.input(true,50,232);a.input(false,0,0);q.publish(a.view());assert(!q.read().rake);
  std::cout<<"PASS: four rake phrases partition-identical; strike contrast, sample cooldown, panic and mode snapshot\n";
}
int main(int argc,char** argv){exchange_tests();rake_tests(argc,argv);int peak=0;for(unsigned m=0;m<4;++m){View v;auto a=phrase(m,true,128,&v),b=phrase(m,true,32);assert(a==b);assert(a!=phrase(m,false,128));for(int x:a)peak=std::max(peak,std::abs(x));assert(peak<=12000);if(argc>1){std::string p=std::string(argv[1])+"/material-"+std::to_string(m);wav(p+".wav",a);ppm(p+".ppm",v);}}Engine e;int16_t a[128];e.input(true,235,210);e.input(false,0,0);e.input(true,110,20);e.render(a,128);auto v=e.view();assert(v.level[0]>0&&v.level[3]==0&&v.level[4]==0&&v.level[5]==0);e.input(false,0,0);e.input(true,235,210);e.input(false,0,0);e.input(true,110,20);for(int i=0;i<32;++i)e.render(a,128);assert(e.view().level[3]>0);assert(!e.view().held);e.input(false,0,0);e.input(true,300,210);e.render(a,128);for(auto x:a)assert(x==0);for(int i=0;i<20000;++i){e.input(false,0,0);e.input(true,i%320,i%184);e.render(a,128);for(auto x:a)assert(std::abs(int(x))<=12000);}std::cout<<"PASS: four 96000-sample phrases partition-identical; coupling ablation, lease, panic; 2560000 stress samples; phrase peak "<<peak<<"; engine bytes "<<sizeof(Engine)<<"\n";}
