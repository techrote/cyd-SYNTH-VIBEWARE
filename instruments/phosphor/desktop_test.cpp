#include "desktop_session.hpp"
#include <cassert>
#include <iostream>
using namespace phosphor;
int main(){
 DesktopSession s;assert(s.advance(0)==0);s.down(10,10);s.down(100,90);
 unsigned audible=0,ticks=0;uint64_t t=0;
 for(int i=0;i<120;++i){t+=33334;ticks+=s.advance(t);for(int j=0;j<800;++j)audible+=s.sample()!=0;}
 assert(ticks==120&&audible>90000&&s.dragging()&&!s.fault());
 s.up();assert(!s.visual().held());
 s.down(10,230);assert(s.visual().frozen());s.down(150,230);for(int i=0;i<800;++i)assert(s.sample()==0);
 s.down(10,230);s.down(220,100);t+=33334;s.advance(t);assert(s.dragging());
 for(int i=0;i<6001;++i)s.sample();
 assert(s.fault());for(int i=0;i<128;++i)assert(s.sample()==0);
 t+=33334;s.advance(t);assert(s.visual().muted()&&!s.dragging());s.down(220,100);assert(s.fault());
 s.down(100,10);assert(!s.fault());s.down(220,100);t+=33334;s.advance(t);unsigned resumed=0;for(int i=0;i<800;++i)resumed+=s.sample()!=0;assert(resumed>0);
 s.move(280,230);assert(s.visual().muted());for(int i=0;i<800;++i)assert(s.sample()==0);
 s.down(10,10);t+=150000;assert(s.advance(t)==2);t+=200000;assert(s.advance(t)==0&&s.fault());
 std::array<uint16_t,322> row{};row.front()=0x5555;row.back()=0x5555;
 for(int y=0;y<240;++y){s.visual().row(y,row.data()+1);assert(row.front()==0x5555&&row.back()==0x5555);}
 std::cout<<"desktop session PASS ticks="<<ticks<<" audible="<<audible<<" resumed="<<resumed<<'\n';
}
