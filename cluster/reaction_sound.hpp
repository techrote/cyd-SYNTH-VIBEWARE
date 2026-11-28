#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace cluster::spatial::sound {
constexpr unsigned rate=24000;
using Image=std::array<uint8_t,768>;
// Four regional intensity voices, packed into one atomic-friendly word.
// Modes are four harmonic palettes; zero word always means release.
inline uint32_t controls(const Image& image,unsigned mode,bool enabled){
    if(!enabled)return 0;
    std::array<unsigned,4> sums{},peaks{};
    for(unsigned y=0;y<24;++y)for(unsigned x=0;x<32;++x){
        const unsigned i=(y/12)*2+x/16,v=image[y*32+x];
        sums[i]+=v;if(v>peaks[i])peaks[i]=v;
    }
    uint32_t word=uint32_t(mode&3u)<<24;
    for(unsigned i=0;i<4;++i){
        const unsigned level=(sums[i]/192+peaks[i])/8;
        word|=uint32_t(level)<<(6*i);
    }
    return word;
}
// Local performance focus: Q8 coordinates [0,256], centre preserves the
// original four-voice mix exactly. Edges isolate a row/column; corners a voice.
// This modifies only gains, never pitch, packet format or audio ownership.
inline uint32_t focus_word(uint32_t word,unsigned x,unsigned y){
    x=x>256u?256u:x;y=y>256u?256u:y;
    const unsigned wx[2]={x<=128u?256u:2u*(256u-x),x>=128u?256u:2u*x};
    const unsigned wy[2]={y<=128u?256u:2u*(256u-y),y>=128u?256u:2u*y};
    uint32_t result=word&0x0f000000u;
    for(unsigned i=0;i<4;++i){
        const unsigned level=(word>>(6u*i))&63u;
        const unsigned focused=(level*wx[i%2u]*wy[i/2u]+32768u)/65536u;
        result|=uint32_t(focused)<<(6u*i);
    }
    return result;
}
// Local spectral colour in spare packed-word bits; zero remains release.
inline uint32_t colour_word(uint32_t word,unsigned colour){
    if(!word)return 0;
    return (word&0x03ffffffu)|(uint32_t(colour>3u?3u:colour)<<26);
}
class Voice {
    std::array<uint32_t,4> phase_{};
    std::array<int32_t,4> gain_{};
    int32_t colour_=0;
    static int32_t triangle(uint32_t phase){
        const int32_t p=int32_t(phase>>16);
        return p<32768?2*p-32768:98303-2*p;
    }
public:
    void render(uint32_t word,int16_t* out,std::size_t n){
        // MIDI-like pitch palettes in Hz, integer increments at 24 kHz.
        constexpr unsigned pitches[4][4]={{110,165,220,330},{131,196,262,392},
            {98,147,233,294},{147,185,247,370}};
        const unsigned mode=(word>>24)&3u;
        const int32_t colour_target=int32_t((word>>26)&3u)*256;
        std::array<uint32_t,4> step{};
        std::array<int32_t,4> target{};
        for(unsigned i=0;i<4;++i){
            step[i]=uint32_t((uint64_t(pitches[mode][i])<<32)/rate);
            target[i]=int32_t((word>>(6*i))&63u)*96;
        }
        for(std::size_t s=0;s<n;++s){
            const int32_t cd=colour_target-colour_;
            colour_+=cd>2?2:(cd< -2?-2:cd);
            int32_t mix=0;
            for(unsigned i=0;i<4;++i){
                // Bounded linear slew, at most 32 ms from full gain to silence.
                const int32_t d=target[i]-gain_[i];
                gain_[i]+=d>8?8:(d< -8?-8:d);
                phase_[i]+=step[i];
                int32_t wave=triangle(phase_[i]);
                if(colour_){
                    // Convex mix of fundamental and two phase-locked partials.
                    const int32_t bright=(2*wave+triangle(phase_[i]*2u)+triangle(phase_[i]*3u))/4;
                    wave=(wave*(768-colour_)+bright*colour_)/768;
                }
                mix+=wave*gain_[i]/32768;
            }
            out[s]=int16_t(mix); // four voices, maximum absolute sum 24192
        }
    }
};
}
