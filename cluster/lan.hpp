#pragma once
#include "cluster.hpp"
#include "pairing.hpp"
namespace cluster::lan {
constexpr uint16_t port=42001;
using Address=std::array<uint8_t,4>;
struct Peer { Address address{}; uint32_t epoch=0; };
struct Credentials { std::array<char,33> ssid{}; std::array<char,64> pass{}; };
inline bool valid_address(const Address& a) {
    return a[0]>0 && a[0]!=127 && a[0]<224 && a[3]!=0 && a[3]!=255;
}
inline bool parse_peer(const char* s,std::size_t n,Peer& out) {
    out={};
    if(!s||n<21||n>29||std::memcmp(s,"peer ",5)!=0) return false;
    Peer p{}; std::size_t pos=5;
    for(std::size_t i=0;i<4;++i) {
        unsigned value=0,digits=0;
        while(pos<n&&s[pos]>='0'&&s[pos]<='9') {
            if(++digits>3) return false;
            value=value*10+unsigned(s[pos++]-'0');
        }
        if(!digits||value>255||pos>=n||s[pos++]!=(i==3?' ':'.')) return false;
        p.address[i]=uint8_t(value);
    }
    if(n-pos!=8||!valid_address(p.address)) return false;
    for(;pos<n;++pos) { const int d=nibble(s[pos]); if(d<0)return false; p.epoch=(p.epoch<<4)|uint32_t(d); }
    if(!p.epoch) return false;
    out=p; return true;
}
// Hex-encoded printable WPA2 personal credentials, never written to flash.
// Explicit size checks before decode; no default credentials or echo.
template<std::size_t N> bool decode_printable(const char* s,std::size_t n,std::array<char,N>& out,std::size_t min) {
    if(n%2||n/2<min||n/2>=N) return false;
    for(std::size_t i=0;i<n/2;++i) {
        int hi=nibble(s[i*2]),lo=nibble(s[i*2+1]);
        if(hi<0||lo<0) return false;
        int c=hi*16+lo; if(c<32||c>126)return false;
        out[i]=char(c);
    }
    return true;
}
inline bool parse_join(const char* s,std::size_t n,Credentials& out) {
    wipe(&out,sizeof(out));
    if(!s||n<24||n>196||std::memcmp(s,"join ",5)!=0)return false;
    std::size_t sep=5;while(sep<n&&s[sep]!=' ')++sep;
    Credentials c{};
    const bool ok=sep<n&&decode_printable(s+5,sep-5,c.ssid,1)&&decode_printable(s+sep+1,n-sep-1,c.pass,8);
    if(ok)out=c;
    wipe(&c,sizeof(c));return ok;
}
inline bool admit(const Address& sender,uint16_t sender_port,const Peer& peer,const uint8_t* bytes,std::size_t size,Frame& out) {
    out={};
    if(!peer.epoch||sender!=peer.address||sender_port!=port||!bytes||size<header||size>max_frame)return false;
    Frame f{}; f.size=size;std::memcpy(f.bytes.data(),bytes,size);
    View v{};if(!parse(f,v)||v.epoch!=peer.epoch)return false;
    out=f;return true;
}
} // namespace cluster::lan
