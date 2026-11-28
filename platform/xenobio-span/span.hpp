#pragma once
#include "labeled.hpp"
namespace cyd::platform {
// Optional renderer for exact XENOBIO M2 valid Engine snapshots. No cached
// state, framebuffer, allocation or engine mutation. Caller owns 320 pixels.
inline void xenobio_span_row(const xenobio::Engine& e, int y, uint16_t* out) {
    if (y < 0 || y >= 240) return;
    // UI rows are a small fraction of the screen; retain the reference path.
    if (y < 20 || y >= 220) { xenobio::labeled_row(e, y, out); return; }
    const int gy = (y-20)/20, ry = (y-20)%20;
    for (int gx=0; gx<xenobio::W; ++gx) {
        auto* cell=out+gx*20;
        const bool marked=e.held && xenobio::Engine::distance(gx,gy,e.tx,e.ty)<=2;
        const uint16_t green=uint16_t((e.food[gy*xenobio::W+gx]/4)<<5);
        std::fill_n(cell,20,marked && ry==1 ? uint16_t(0xffff) : ry==0 ? uint16_t(0x0841) : green);
        if (!(marked && ry==1)) cell[0]=0x0841;
        if (marked) cell[1]=0xffff;
    }
    constexpr uint16_t colors[]={0xfca0,0x4e9f,0xe35f};
    // Preserve original agent ordering, including coincident organisms.
    for (const auto& p:e.a) if (p.e>0 && p.y==gy) {
        const int r=2+p.e/17, yy=ry-10, remaining=r*r-yy*yy;
        if (remaining<0) continue;
        int half=r;
        while (half*half>remaining) --half;
        std::fill_n(out+p.x*20+10-half,2*half+1,colors[p.s]);
    }
}
}
