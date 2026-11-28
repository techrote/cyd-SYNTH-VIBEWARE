#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace cluster {
struct Pairing {
    std::array<uint8_t,6> peer{};
    uint32_t epoch=0;
    std::array<uint8_t,16> pmk{},lmk{};
};
inline void wipe(void* p,std::size_t n) {
    auto* b=static_cast<volatile uint8_t*>(p);
    while(n--) *b++=0;
}
inline int nibble(char c) {
    if(c>='0'&&c<='9') return c-'0';
    if(c>='a'&&c<='f') return c-'a'+10;
    if(c>='A'&&c<='F') return c-'A'+10;
    return -1;
}
template<std::size_t N> bool hex_bytes(const char* p,std::array<uint8_t,N>& a) {
    for(std::size_t i=0;i<N;++i) {
        const int h=nibble(p[2*i]),l=nibble(p[2*i+1]);
        if(h<0||l<0) return false;
        a[i]=uint8_t(h*16+l);
    }
    return true;
}
template<std::size_t N> bool nonzero(const std::array<uint8_t,N>& a) {
    for(auto b:a) if(b) return true;
    return false;
}
// Strict serial-only provisioning grammar; no persistent/default secrets.
// Caller supplies explicit length and wipes its own input afterwards.
inline bool parse_pairing(const char* p,std::size_t n,Pairing& out) {
    wipe(&out,sizeof(out));
    if(!p||n!=92||std::memcmp(p,"pair ",5)!=0||p[17]!=' '||p[26]!=' '||p[59]!=' ') return false;
    Pairing t; std::array<uint8_t,4> e{};
    const bool ok=hex_bytes(p+5,t.peer)&&hex_bytes(p+18,e)&&hex_bytes(p+27,t.pmk)&&hex_bytes(p+60,t.lmk);
    for(auto b:e) t.epoch=(t.epoch<<8)|b;
    const bool valid=ok&&nonzero(t.peer)&&!(t.peer[0]&1)&&t.epoch&&nonzero(t.pmk)&&nonzero(t.lmk)&&t.pmk!=t.lmk;
    if(valid) out=t;
    wipe(&t,sizeof(t)); return valid;
}
} // namespace cluster
