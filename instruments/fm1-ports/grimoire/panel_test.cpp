#include "panel.hpp"
#include <cassert>
#include <iostream>
using namespace grimoire;
int main(){
    Engine e;Touch t(e);PanelRaster raster;
    std::array<std::uint16_t,width+2> expected{};
    std::array<std::uint16_t,width*8+2> output{};
    std::uint64_t pixels=0;
    assert(!raster.render(0,1,output.data()));
    for(int frame=0;frame<360;++frame){
        if(frame%120==0){t.up();t.down((frame/120)*64+10,10);t.up();t.down(200,10);t.up();}
        if(frame%30==0){t.up();t.down((frame/30%6)*53+5,220);t.up();}
        if(frame%5==0){t.up();t.down(20+frame%280,40+frame%160);}
        else t.move(20+frame%280,40+frame%160);
        if(frame%7==0)t.up();
        for(int n=0;n<128;++n)e.sample();
        auto snapshot=e.snapshot();raster.begin(capture_panel(e,t));
        for(int y=0;y<height;){
            const int count=std::min(1+(frame+y)%8,height-y);
            output.fill(0xa55a);assert(raster.render(y,count,output.data()+1));
            assert(output[0]==0xa55a&&output[width*count+1]==0xa55a);
            for(int j=0;j<count;++j){
                int sy=source_y(y+j);
                if(sy<0)for(int x=0;x<width;++x)expected[x]=t.toolbar_pixel(x,y+j);
                else row(snapshot,sy,expected.data());
                for(int x=0;x<width;++x){assert(output[1+j*width+x]==expected[x]);++pixels;}
            }
            y+=count;
        }
        output.fill(0xa55a);
        assert(!raster.render(-1,1,output.data()+1));assert(!raster.render(239,2,output.data()+1));
        assert(!raster.render(0,9,output.data()+1));assert(!raster.render(0,0,output.data()+1));
        assert(!raster.render(0,1,nullptr));for(auto v:output)assert(v==0xa55a);
    }
    BoardSession s;std::array<std::int16_t,128> pcm{};std::uint64_t samples=0,nonzero=0;
    auto render=[&](std::uint32_t now){bool flush=s.audio(now,pcm.data(),pcm.size());for(auto v:pcm){assert(v>=-8192&&v<=8192);++samples;nonzero+=v!=0;}return flush;};
    auto quiet=[&](){for(auto v:pcm)assert(v==0);};
    render(0);quiet();assert(!s.poll(true,true,160,100,1));assert(s.arm());
    s.poll(true,true,160,100,2);render(2);quiet(); // held at arm cannot trigger
    s.poll(true,false,0,0,3);s.poll(true,true,160,100,4);
    for(unsigned i=0;i<20;++i){s.poll(true,true,160+i,100,4+i*5);render(4+i*5);}assert(nonzero>1000);
    s.poll(true,false,0,0,105);for(unsigned i=0;i<20;++i)render(105+i*4);quiet();
    s.poll(true,true,200,10,190);s.poll(true,false,0,0,191);render(192);assert(s.frame().engine.auto_on);
    assert(render(291));quiet();assert(!s.frame().engine.auto_on);
    s.poll(true,true,160,120,292);render(292);quiet(); // must release after loss
    s.poll(true,false,0,0,293);s.poll(true,true,160,120,294);render(294);assert(s.frame().engine.manual);
    assert(s.poll(true,true,300,10,295));render(295);quiet();assert(s.frame().engine.muted);
    s.poll(true,false,0,0,296);s.poll(true,true,160,120,297);assert(s.poll(false,false,0,0,298));render(298);quiet();
    s.poll(true,false,0,0,299);s.poll(true,true,160,120,300);assert(s.poll(true,true,-1,120,301));render(301);quiet();
    assert(s.disarm());render(302);quiet();assert(!s.armed());assert(s.arm());
    s.poll(true,false,0,0,0xfffffff0u);s.poll(true,true,120,80,0xfffffff1u);render(20);assert(s.frame().engine.manual);
    assert(render(100));quiet();assert(!s.frame().engine.manual);
    assert(raw_axis(0,320)==0&&raw_axis(4095,320)==319&&raw_axis(200,240)==0&&raw_axis(3900,240)==239);
    std::cout<<"PASS panel pixels="<<pixels<<" frames=360 PCM="<<samples<<" nonzero="<<nonzero<<" session_bytes="<<sizeof(BoardSession)<<" raster_bytes="<<sizeof(PanelRaster)<<'\n';
}
