#pragma once
#include "dirty_grid.hpp"
#include "typed_strip.hpp"
namespace cyd::platform {
// Main-loop only. No external drawing without invalidate(). Driver must
// implement synchronous completion; waitDMA precedes scratch reuse.
template<class Pixel> class DirtyDisplay {
    DirtyGrid grid_;
    TypedStrip<Pixel,gridseq::width,gridseq::height,4> strip_;
public:
    bool busy() const { return grid_.busy(); }
    void invalidate() { grid_.invalidate(); }
    bool begin(const gridseq::Control& c,uint32_t head) { return grid_.begin(c,head); }
    template<class Display> unsigned step(Display& display) {
        return grid_.step([&](const gridseq::Control& c,uint32_t head,unsigned y,unsigned n){
            return strip_.submit(y,n,[&](unsigned row,uint16_t* p){
                gridseq::row(c,head,row,p);
            },display)==n;
        });
    }
};
}
