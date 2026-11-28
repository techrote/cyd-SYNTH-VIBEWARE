#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
namespace cyd::platform {
// Opt-in PHOSPHOR M2 layout only: header/footer constant in Y, field 4x.
// Visual owner calls this against stable engine state for each strip. Caller
// owns >= rows*320 RGB565 pixels and must finish transfer before reuse.
// No retained cache, allocations, extra scratch, or cross-thread access.
template<class Visual>
bool phosphor_strip(const Visual& visual, int y, int rows, uint16_t* out) {
    if (!out || y < 0 || y >= 240 || rows <= 0 || rows > 8 || rows > 240-y) return false;
    const int end = y + rows;
    while (y < end) {
        const int boundary = y < 20 ? 20 : y >= 220 ? 240 : y + 4 - (y-20)%4;
        const int n = std::min(end, boundary)-y;
        visual.row(y, out);
        for (int j=1; j<n; ++j) std::memcpy(out+j*320, out, 320*sizeof(uint16_t));
        y += n;
        out += n*320;
    }
    return true;
}
}
