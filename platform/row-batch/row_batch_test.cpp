#include "row_batch.hpp"
#include "grid.hpp"
#include "xeno.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <vector>
using namespace cyd::platform;
static unsigned long long comparisons=0, frames=0, baseline=0, calls=0;
static void check(bool ok){if(!ok)std::abort();}
template<unsigned R, class Raster> void compare(Raster raster){
    RowBatch<320,240,R> batch{};
    std::array<uint16_t,320> row{};
    std::vector<uint16_t> expected(320*240), actual(320*240);
    for(unsigned y=0;y<240;++y){raster(y,row.data());std::copy(row.begin(),row.end(),expected.begin()+y*320);}
    unsigned next=0, n=0;
    auto sink=[&](unsigned y,unsigned h,const uint16_t* p){check(y==next && h>0 && h<=R && y+h<=240);std::copy_n(p,h*320,actual.begin()+y*320);next+=h;++n;};
    for(unsigned y=0;y<240;){unsigned h=batch.submit(y,R,raster,sink);check(h>0);y+=h;}
    check(next==240 && n==(240+R-1)/R);check(expected==actual);
    check(batch.submit(240,R,raster,sink)==0);check(batch.submit(0,0,raster,sink)==0);
    comparisons+=expected.size();++frames;baseline+=240;calls+=n;
    unsigned seen=0;check(batch.submit(239,1,raster,[&](unsigned y,unsigned h,const uint16_t* p){check(y==239 && h==1);check(std::equal(p,p+320,expected.end()-320));++seen;})==1 && seen==1);
}
template<class Raster> void exercise(Raster raster){compare<4>(raster);compare<8>(raster);compare<7>(raster);}
int main(){
    for(unsigned s=0;s<6;++s){gridseq::Engine e;gridseq::Control c;c.scene=s;c.masks=gridseq::scenes[s].masks;c.bpm=gridseq::scenes[s].bpm;c.play=true;c.accents=c.masks;c.accentEdit=1u<<(s%4);std::atomic<uint32_t> head{0};std::array<int16_t,128> pcm{};
        for(unsigned i=0;i<64;++i){++c.seq;e.render(c,pcm.data(),pcm.size(),&head);if(i%16==0){auto h=head.load();exercise([&](unsigned y,uint16_t* p){gridseq::row(c,h,y,p);});}}
    }
    for(unsigned m=0;m<4;++m)for(unsigned mode=0;mode<2;++mode){portx::Engine e;e.input(true,int(m*53+2),200);e.input(false,0,0);e.input(true,mode?140:20,230);e.input(false,0,0);std::array<int16_t,128> pcm{};
        for(unsigned i=0;i<64;++i){e.input(true,int((i*7)%320),90);e.render(pcm.data(),pcm.size());if(i%16==0){auto v=e.view();exercise([&](unsigned y,uint16_t* p){for(int x=0;x<320;++x)p[x]=portx::pixel(v,x,int(y));});}}
    }
    std::cout<<"PASS frames="<<frames<<" pixels="<<comparisons<<" baseline_calls="<<baseline<<" strip_calls="<<calls<<" scratch4="<<sizeof(RowBatch<320,240,4>)<<" scratch8="<<sizeof(RowBatch<320,240,8>)<<'\n';
}
