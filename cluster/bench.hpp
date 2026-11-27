#pragma once
#include <array>
#include <cstdint>
namespace cluster::bench {
constexpr unsigned batches=64, bins=24;
inline unsigned bucket(uint32_t us) {
    unsigned i=0;
    while(i+1<bins&&us>(uint32_t(1)<<i))++i;
    return i;
}
struct Histogram {
    std::array<uint32_t,bins> counts{};
    void add(uint32_t us){++counts[bucket(us)];}
};
struct Gesture {uint16_t x,y;bool gate;};
inline Gesture gesture(uint32_t seed) {
    return {uint16_t(seed*3011u+1024u),uint16_t(seed*6007u+1234u),seed%8u!=0};
}
template<class Grid> uint32_t digest(const Grid& grid) {
    uint32_t hash=2166136261u;
    for(auto v:grid){hash^=v;hash*=16777619u;}
    return hash;
}
struct Sample {uint32_t seed,us,hash;unsigned remote;};
struct Capture {
    std::array<Sample,batches> rows{};
    unsigned count=0;
    bool record(Sample s) {
        if(count>=batches||s.seed!=count+1u||s.remote>2u)return false;
        rows[count++]=s;return true;
    }
};
} // namespace cluster::bench
