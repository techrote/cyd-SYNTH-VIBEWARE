#pragma once
#include "span.hpp"
namespace cyd::platform {
// Opt-in XENOBIO M2/M3 scan scheduler. Valid Engine snapshots only.
// Single display-thread owner. Caller submits four rows synchronously, or
// waits for DMA before commit(). On reset/error/external drawing invalidate().
class XenobioDirtyScan {
    xenobio::Engine previous_{}, pending_{};
    uint64_t mask_=0;
    bool valid_=false;
    static constexpr uint64_t full=(uint64_t(1)<<60)-1;
    static void mark(uint64_t& m,int lo,int hi) {
        for(int s=lo/4;s<=hi/4;++s) m|=uint64_t(1)<<s;
    }
    static bool marked(const xenobio::Engine& e,int x,int y) {
        return e.held && xenobio::Engine::distance(x,y,e.tx,e.ty)<=2;
    }
    static void agent(uint64_t& m,const xenobio::Agent& a) {
        if(a.e>0) {const int c=30+20*a.y,r=2+a.e/17;mark(m,c-r,c+r);}
    }
    static uint64_t changes(const xenobio::Engine& a,const xenobio::Engine& b) {
        uint64_t m=0;
        if(a.scene!=b.scene) mark(m,0,19);
        if(a.mode!=b.mode||a.panic!=b.panic) mark(m,220,239);
        for(int y=0;y<xenobio::H;++y) for(int x=0;x<xenobio::W;++x) {
            const int i=y*xenobio::W+x, top=20+20*y;
            if(a.food[i]/4!=b.food[i]/4||marked(a,x,y)!=marked(b,x,y))
                mark(m,top,top+19);
        }
        for(unsigned i=0;i<a.a.size();++i) {
            const auto& p=a.a[i];const auto& q=b.a[i];
            if(p.e<=0&&q.e<=0) continue;
            if(p.e>0&&q.e>0&&p.x==q.x&&p.y==q.y&&p.s==q.s&&p.e/17==q.e/17) continue;
            agent(m,p);agent(m,q);
        }
        return m;
    }
public:
    bool begin(const xenobio::Engine& e) {
        if(mask_) return false; // immutable pending frame until complete
        pending_=e;mask_=valid_?changes(previous_,pending_):full;
        if(!mask_) {previous_=pending_;valid_=true;}
        return true;
    }
    int next_row() const {
        for(int s=0;s<60;++s) if(mask_&(uint64_t(1)<<s)) return 4*s;
        return -1;
    }
    // Borrow the immutable pending snapshot for each row of the next strip.
    template<class Row> bool for_each_next_row(Row&& write) const {
        const int y=next_row(); if(y<0) return false;
        for(int dy=0;dy<4;++dy) write(pending_,y+dy,dy);
        return true;
    }
    bool render_next(uint16_t* out) const {
        const int y=next_row();if(y<0) return false;
        for(int dy=0;dy<4;++dy) xenobio_span_row(pending_,y+dy,out+320*dy);
        return true;
    }
    bool commit(int y) {
        if(y<0||y!=next_row()) return false;
        mask_&=~(uint64_t(1)<<(y/4));
        if(!mask_) {previous_=pending_;valid_=true;}
        return true;
    }
    void invalidate() {mask_=0;valid_=false;}
};
}
