#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
namespace portx {
constexpr int rate=24000, width=320, height=240;
struct Material { const char* name; std::array<float,3> ratio; float decay; };
inline constexpr Material materials[]={{"Ceramic",{1,2.71f,4.08f},1.1f},{"Woodglass",{1,1.49f,2.42f},.45f},{"Alien metal",{1,2.19f,3.87f},1.6f},{"Membrane",{1,1.59f,2.14f},.7f}};
struct Mode { float a=0,b=0,c=1,s=0; };
struct View { std::array<unsigned,6> level{}; unsigned material=0,x=160,y=90; bool coupled=true,held=false,rake=false; };
class Engine {
  std::array<Mode,6> m{}; unsigned mat=0,x=160,y=90,note=12; bool coupled=true,held=false,contact=false,rake=false; unsigned lease=0,cooldown=0,lastStrikeX=160; float damp=.999f,mix=0,dc=0;
  void tune(){
    const float base=110.f*std::pow(2.f,float(note)/12.f);
    for(unsigned i=0;i<6;++i){float f=base*materials[mat].ratio[i%3]*(i<3?1.f:1.013f);float w=6.28318530718f*f/rate;m[i].c=std::cos(w);m[i].s=std::sin(w);}
  }
  void controls(){damp=std::exp(-1.f/(rate*materials[mat].decay*(.12f+1.2f*float(183-y)/183.f)*(held?1.f:.58f)));mix=coupled?(.00008f+.0008f*float(x)/319.f):0.f;}
  void strike(float strength=.38f){for(unsigned i=0;i<3;++i){float pos=float(x+1)/321.f;float g=strength*std::abs(std::sin(3.14159265f*pos*float(i+1)))/(1.f+.3f*i);m[i].a=std::clamp(m[i].a+g,-1.f,1.f);m[i].b=std::clamp(m[i].b,-1.f,1.f);}}
 public:
  Engine(){tune();controls();}
  void panic(){for(auto& v:m){v.a=v.b=0;}dc=0;held=false;lease=0;cooldown=0;controls();}
  void release(){contact=false;held=false;lease=0;cooldown=0;controls();}
  void input(bool down,int px,int py){
    if(!down){if(contact)release();return;} const bool fresh=!contact;contact=true;lease=rate/6;
    if(py>=184){if(fresh){held=false;if(px>=266)panic();else if(py>=224)rake=px>=133;else if(px>=212)coupled=!coupled;else {mat=unsigned(std::clamp(px,0,211))/53;tune();}controls();}return;}
    x=unsigned(std::clamp(px,0,319));y=unsigned(std::clamp(py,0,183));
    if(fresh){note=x*25/320;held=true;tune();controls();strike();lastStrikeX=x;cooldown=rate/50;}
    else if(held){controls();const unsigned next=x*25/320;if(rake&&!cooldown&&next!=note){
      const float travel=float(std::abs(int(x)-int(lastStrikeX)));
      note=next;tune();strike(.14f+.24f*std::min(1.f,travel/64.f));lastStrikeX=x;cooldown=rate/50;
    }}
  }
  void render(int16_t* out,unsigned n){for(unsigned k=0;k<n;++k){
    if(cooldown)--cooldown;
    if(lease&&!--lease){release();}
    float total=0;for(auto& v:m){float a=damp*(v.a*v.c-v.b*v.s),b=damp*(v.a*v.s+v.b*v.c);v.a=std::abs(a)<1e-15f?0:a;v.b=std::abs(b)<1e-15f?0:b;}
    for(unsigned i=0;i<3;++i){auto& a=m[i];auto& b=m[i+3];float da=mix*(b.a-a.a),db=mix*(b.b-a.b);a.a+=da;b.a-=da;a.b+=db;b.b-=db;}
    for(auto& v:m)total+=v.a;
    dc+=.002f*(total-dc);if(std::abs(dc)<1e-15f)dc=0;
    out[k]=int16_t(std::clamp((total-dc)*3900.f,-12000.f,12000.f));
  }}
  View view()const{View v;v.material=mat;v.x=x;v.y=y;v.held=held;v.coupled=coupled;v.rake=rake;for(unsigned i=0;i<6;++i)v.level[i]=unsigned(std::min(255.f,255.f*std::hypot(m[i].a,m[i].b)));return v;}
};
inline uint16_t pixel(const View& v,int x,int y){
  if(y>=224){if(x>=266)return 0x7800;return (x>=133)==v.rake?0x4d36:0x2104;}
  if(y>=184){int cell=std::min(x/53,5);bool on=cell==int(v.material)||(cell==4&&v.coupled);return cell==5?0x7800:on?0x4d36:0x2104;}
  const int body=x<160?0:1,local=x%160;
  const int mode=std::min(2,local/53);int h=int(v.level[body*3+mode])*140/255;
  if(y>=166-h&&y<166&&local%53>=7&&local%53<46)return body?0xfd20:0x57ff;
  if(v.held&&std::abs(x-int(v.x))<=2&&std::abs(y-int(v.y))<=8)return 0xffff;
  if(v.held&&std::abs(y-int(v.y))<=2&&std::abs(x-int(v.x))<=8)return 0xffff;
  if(x==159||y==166||y%40==0)return 0x2945;return 0x0862;
}
}
