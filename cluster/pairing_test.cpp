#include "pairing.hpp"
#include <cstdlib>
#include <iostream>
#include <string>
using namespace cluster;
static unsigned checks=0;
#define CHECK(x) do {++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<'\n';std::exit(1);}}while(false)
int main() {
    // Synthetic public test vectors only, never operational keys.
    const std::string good="pair 020304050607 1234ABCD 00112233445566778899AABBCCDDEEFF FFEEDDCCBBAA99887766554433221100";
    Pairing p;
    CHECK(parse_pairing(good.data(),good.size(),p));
    CHECK(p.epoch==0x1234abcdu&&p.peer[0]==2&&p.peer[5]==7&&p.pmk[15]==255&&p.lmk[0]==255);
    for(std::size_t i=0;i<good.size();++i) {
        CHECK(!parse_pairing(good.data(),i,p)); CHECK(!nonzero(p.pmk)&&!p.epoch);
    }
    for(std::size_t i=0;i<good.size();++i) {
        auto bad=good;bad[i]='!';CHECK(!parse_pairing(bad.data(),bad.size(),p));
    }
    auto bad=good;bad.replace(5,12,"FFFFFFFFFFFF");CHECK(!parse_pairing(bad.data(),bad.size(),p));
    bad=good;bad.replace(5,12,"000000000000");CHECK(!parse_pairing(bad.data(),bad.size(),p));
    bad=good;bad.replace(18,8,"00000000");CHECK(!parse_pairing(bad.data(),bad.size(),p));
    bad=good;bad.replace(27,32,32,'0');CHECK(!parse_pairing(bad.data(),bad.size(),p));
    bad=good;bad.replace(60,32,32,'0');CHECK(!parse_pairing(bad.data(),bad.size(),p));
    bad=good;bad.replace(60,32,good.substr(27,32));CHECK(!parse_pairing(bad.data(),bad.size(),p));
    CHECK(!parse_pairing(nullptr,92,p));
    std::cout<<"PASS pairing checks="<<checks<<'\n';
}
