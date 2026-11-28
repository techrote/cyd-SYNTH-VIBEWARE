#pragma once
#include "core.hpp"
namespace cyd_platform {
// Exact PHASESPACE M3 row semantics, batched into caller-owned 320xN strips.
// UI thread only. No heap, no DMA or asynchronous lifetime assumptions.
class Strip565 {
    uint16_t* out_; unsigned y_,n_;
public:
    Strip565(uint16_t* out,unsigned y,unsigned n):out_(out),y_(std::min(y,240u)),n_(std::min(n,240u-y_)){}
    unsigned begin()const{return y_;} unsigned end()const{return y_+n_;}
    void span(int y,int x0,int x1,uint16_t ink)const{
        if(y<int(y_)||y>=int(end())||x0>x1)return;
        x0=std::max(0,x0); x1=std::min(319,x1);
        if(x0>x1)return;
        auto* row=out_+unsigned(y-int(y_))*320;
        std::fill(row+x0,row+x1+1,ink);
    }
    // Endpoints bounded to the display, preserves signed truncation and orientation.
    void segment(int ax,int ay,int bx,int by,uint16_t ink)const{
        if(ax<0||ax>=320||bx<0||bx>=320||ay<0||ay>=240||by<0||by>=240)return;
        const int dx=bx-ax,dy=by-ay;
        if(dy==0){span(ay,std::min(ax,bx),std::max(ax,bx),ink);return;}
        const int lo=std::max(int(begin()),std::min(ay,by));
        const int hi=std::min(int(end())-1,std::max(ay,by));
        for(int y=lo;y<=hi;++y){int x=ax+dx*(y-ay)/dy;span(y,x,x,ink);}
    }
};
class PhaseStripTrace {
    std::array<phase::Point,128> history_{};
    unsigned end_=0,count_=0;
public:
    void drain(phase::Points& p){phase::Point v;for(unsigned i=0;i<256&&p.pop(v);++i){history_[end_]=v;end_=(end_+1)&127u;count_=std::min(128u,count_+1);}}
    void strip(const phase::Control& c,unsigned start,unsigned rows,uint16_t* out)const{
        if(start>=phase::height||rows==0)return;
        rows=std::min(rows,phase::height-start);
        phase::Trace empty;auto bg=c;bg.gate=false;
        for(unsigned i=0;i<rows;++i)empty.row(bg,start+i,out+i*phase::width);
        if(start>=phase::field)return;
        Strip565 sink(out,start,std::min(rows,phase::field-start));
        const auto color=phase::patches[std::min(c.preset,5u)].color;
        for(unsigned i=0;i<count_;++i){
            const auto p=history_[(end_+128-count_+i)&127u];
            if(p.x>=phase::width||p.y>=phase::field)continue;
            const auto ink=phase::Trace::dim(color,1+i*7/std::max(1u,count_-1));
            if(i){const auto q=history_[(end_+127-count_+i)&127u];
                if(q.x<phase::width&&q.y<phase::field&&std::abs(int(p.x)-int(q.x))<=40&&std::abs(int(p.y)-int(q.y))<=40)
                    sink.segment(q.x,q.y,p.x,p.y,ink);
            }
            for(int y=int(p.y)-1;y<=int(p.y)+1;++y)sink.span(y,int(p.x)-1,int(p.x)+1,ink);
        }
        if(c.gate&&c.x<phase::width&&c.y<phase::field){
            for(int y=int(c.y)-4;y<=int(c.y)+4;++y)sink.span(y,y==int(c.y)?int(c.x)-4:int(c.x),y==int(c.y)?int(c.x)+4:int(c.x),0xffff);
        }
    }
};
}
