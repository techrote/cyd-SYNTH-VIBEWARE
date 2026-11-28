#include "lan.hpp"
#include "link.hpp"
#include <cstdlib>
#include <iostream>
#include <string>
using namespace cluster;
unsigned checks=0;
void check(bool x){++checks;if(!x){std::cerr<<"FAIL check "<<checks<<'\n';std::exit(1);}}
int main(){
    lan::Peer p{};
    auto peer=[&](const std::string& s){return lan::parse_peer(s.data(),s.size(),p);};
    check(peer("peer 192.168.4.2 1234abcd"));check(p.epoch==0x1234abcd&&p.address[3]==2);
    check(peer("peer 1.2.3.4 00000001"));
    for(const auto* s:{"peer 0.2.3.4 00000001","peer 127.2.3.4 00000001","peer 224.2.3.4 00000001","peer 1.2.3.255 00000001","peer 1.2.3.0 00000001","peer 1.2.3.256 00000001","peer 1.2.3.4 00000000","peer 1.2.3.4 0000000g","peer 1.2.3.4 00000001 ","peer 1.2.3.4 1","peer 1.2..4 00000001","peer 1.2.3.4444 00000001"}){
        check(!peer(s));check(!p.epoch);
    }
    check(peer("peer 192.168.4.2 1234abcd"));
    lan::Credentials c{};
    auto join=[&](const std::string& s){return lan::parse_join(s.data(),s.size(),c);};
    check(join("join 4c414e 3132333435363738"));check(std::string(c.ssid.data())=="LAN"&&std::string(c.pass.data())=="12345678");
    for(const auto& s:{std::string("join 4c414e 31323334353637"),std::string("join 00 3132333435363738"),std::string("join 4 3132333435363738"),std::string("join 4g 3132333435363738"),std::string("join ")+std::string(66,'4')+" 3132333435363738",std::string("join 41 ")+std::string(128,'4')}){
        check(!join(s));check(!c.ssid[0]&&!c.pass[0]);
    }
    check(join("join "+std::string(64,'4')+" "+std::string(126,'4')));
    check(c.ssid[32]==0&&c.pass[63]==0);
    Frame out;auto f=frame(Kind::hello,p.epoch,0);
    const lan::Address wrong={192,168,4,3};
    check(!lan::admit(wrong,lan::port,p,f.bytes.data(),f.size,out));
    check(!lan::admit(p.address,lan::port+1,p,f.bytes.data(),f.size,out));
    for(std::size_t n=0;n<=max_frame+1;++n)check(lan::admit(p.address,lan::port,p,f.bytes.data(),n,out)==(n==f.size));
    check(!lan::admit(p.address,lan::port,p,nullptr,f.size,out));
    auto bad=frame(Kind::hello,p.epoch+1,0);
    check(!lan::admit(p.address,lan::port,p,bad.bytes.data(),bad.size,out));
    // Existing real compute/codec through the same admission gate, including
    // duplicates and wrong-endpoint injection (no target or socket claim).
    Link link(p.epoch);Worker worker(p.epoch);Frame request;
    auto send=[&](const Frame& x){request=x;return true;};
    link.tick(0,send);
    check(lan::admit(p.address,lan::port,p,request.bytes.data(),request.size,out));
    auto response=worker.receive(out);
    check(lan::admit(p.address,lan::port,p,response.bytes.data(),response.size,out));
    link.receive(out,1);check(link.ready());
    for(uint32_t i=1;i<=100;++i){
        Job j{i,512,64,uint16_t(i*631),uint16_t(i*317)};
        check(link.start(j,i*100,60,send));
        check(lan::admit(p.address,lan::port,p,request.bytes.data(),request.size,out));
        response=worker.receive(out);auto duplicate=worker.receive(out);
        check(response.bytes==duplicate.bytes);
        check(!lan::admit(wrong,lan::port,p,response.bytes.data(),response.size,out));
        check(lan::admit(p.address,lan::port,p,response.bytes.data(),response.size,out));
        check(link.receive(out,i*100+2)==Completion::remote);check(link.grid()==compute(j));
    }
    std::cout<<"LAN admission and matched jobs PASS "<<checks<<" checks\n";
}
