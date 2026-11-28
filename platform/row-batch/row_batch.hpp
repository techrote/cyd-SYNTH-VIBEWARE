#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace cyd::platform {
// Caller-owned synchronous scratch. Sink must consume before returning; no DMA
// ownership is implied. A snapshot must remain fixed for a complete frame.
template<unsigned Width, unsigned Height, unsigned Rows>
struct RowBatch {
    static_assert(Width && Height && Rows && Rows <= Height);
    std::array<uint16_t, std::size_t(Width)*Rows> pixels;
    template<class Raster, class Sink>
    unsigned submit(unsigned first, unsigned budget, Raster&& raster, Sink&& sink) {
        if(first >= Height || budget == 0) return 0;
        unsigned count = Height-first;
        if(count > Rows) count = Rows;
        if(count > budget) count = budget;
        for(unsigned i=0; i<count; ++i)
            raster(first+i, pixels.data()+std::size_t(i)*Width);
        sink(first, count, pixels.data());
        return count;
    }
};
}
