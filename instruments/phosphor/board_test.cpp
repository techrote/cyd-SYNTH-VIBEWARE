#include "board_session.hpp"
#include <cassert>
#include <iostream>
using namespace phosphor;
int main(){
 BoardSession b;DesktopSession d;
 std::array<int16_t,128> pcm{};
 std::array<uint16_t,8*320+2> strip{};
 std::array<uint16_t,320> row{};
 uint64_t now=0;unsigned count=0,nonzero=0,pixels=0;
 b.poll(true,true,10,10,now);assert(b.visual().muted());
 b.poll(true,false,0,0,now);b.poll(true,true,10,10,now);d.down(10,10);
 b.poll(true,false,0,0,now);d.up();b.poll(true,true,120,100,now);d.down(120,100);
 for(unsigned i=0;i<750;++i){
  now=(uint64_t(i)*16000)/3;
  if(i%3==0){const int x=30+int(i%250);b.poll(true,true,x,100,now);d.move(x,100);}
  assert(b.advance(now)==d.advance(now));assert(b.digital(now,pcm.data()));
  for(auto v:pcm){assert(v==d.sample());++count;nonzero+=v!=0;}
  if(i%5==0){
   b.capture();Engine frozen=b.visual();
   // The next touch may change the live field but the in-flight frame must not.
   b.poll(true,true,200,130,now);d.move(200,130);
   for(int y=0;y<240;){
    const int n=std::min(7,240-y);strip.front()=0x5555;strip[1+n*320]=0x5555;
    assert(b.strip(y,n,strip.data()+1));
    assert(strip.front()==0x5555&&strip[1+n*320]==0x5555);
    for(int j=0;j<n;++j){frozen.row(y+j,row.data());for(int x=0;x<320;++x){assert(row[x]==strip[1+j*320+x]);++pixels;}}
    y+=n;
   }
  }
 }
 assert(nonzero>80000);assert(!b.strip(-1,1,strip.data())&&!b.strip(239,2,strip.data())&&!b.strip(0,9,strip.data())&&!b.strip(0,1,nullptr));
 now+=70000;b.advance(now);assert(b.visual().muted());assert(b.digital(now,pcm.data()));for(auto v:pcm)assert(v==0);
 b.poll(true,true,10,10,now);assert(b.visual().muted());b.poll(true,false,0,0,now);b.poll(true,true,10,10,now);assert(!b.visual().muted());
 b.poll(false,true,0,0,now);assert(b.visual().muted());
 b.poll(true,false,0,0,now);b.poll(true,true,10,10,now);assert(!b.visual().muted());
 now+=20000;assert(b.digital(now,pcm.data()));assert(b.visual().muted());for(auto v:pcm)assert(v==0);
 std::cout<<"board PASS pcm="<<count<<" nonzero="<<nonzero<<" pixels="<<pixels<<" session_bytes="<<sizeof(BoardSession)<<'\n';
}
