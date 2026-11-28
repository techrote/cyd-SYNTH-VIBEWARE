#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace cyd::platform {
// Pixel is an explicit driver color type (not a reinterpretation of uint16_t).
// One raw raster row and one typed strip; main-loop only, no async DMA.
template<class Pixel, unsigned Width, unsigned Height, unsigned Rows>
struct TypedStrip {
    static_assert(Width && Height && Rows && Rows <= Height);
    static_assert(sizeof(Pixel)==sizeof(uint16_t));
    std::array<uint16_t,Width> row{};
    std::array<Pixel,std::size_t(Width)*Rows> pixels{};
    template<class Raster,class Display>
    unsigned submit(unsigned first,unsigned budget,Raster&& raster,Display& display) {
        if(first>=Height || !budget) return 0;
        unsigned count=Height-first;
        if(count>Rows) count=Rows;
        if(count>budget) count=budget;
        for(unsigned i=0;i<count;++i) {
            raster(first+i,row.data());
            for(unsigned x=0;x<Width;++x) pixels[std::size_t(i)*Width+x]=row[x];
        }
        display.startWrite();
        display.pushImage(0,int(first),int(Width),int(count),pixels.data());
        display.waitDMA();
        display.endWrite();
        return count;
    }
};
}
