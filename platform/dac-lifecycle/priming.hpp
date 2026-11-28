#pragma once
#include <cstdint>
namespace cyd_platform {
// A proof token for a backend with verified, exclusive descriptor ownership.
// This is NOT an adapter for the public legacy I2S API: a successful write
// or a TX_DONE count does not establish the required descriptor identity.
class PrimingWitness {
    uint32_t generation_ = 0;
    unsigned mask_ = 0;
    bool valid_ = false;
public:
    void begin(uint32_t generation) { generation_=generation; mask_=0; valid_=true; }
    void fault() { mask_=0; valid_=false; }
    void written(uint32_t generation, unsigned slot, bool full_midpoint) {
        if (!valid_ || generation!=generation_ || slot>=4) return;
        const unsigned bit=1u<<slot;
        if (full_midpoint) mask_|=bit; else mask_&=~bit;
    }
    void invalidated(unsigned slot) { if (slot<4) mask_&=~(1u<<slot); else fault(); }
    bool ready(uint32_t generation) const { return valid_ && generation==generation_ && mask_==15; }
};
}
