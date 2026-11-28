#include "spatial.hpp"
#include "udp_transport.hpp"
#include <fstream>
namespace sf=cluster::spatial;
static void check(bool yes,const char* why){if(!yes)fail(why);}
static void ppm(const char* path,const sf::Pixels& p){
    std::ofstream out(path,std::ios::binary);
    out<<"P6\n320 240\n255\n";
    std::array<uint16_t,320> row{};
    for(std::size_t y=0;y<240;++y){
        sf::row(p,y,row.data());
        for(auto v:row){
            const char rgb[]={char(((v>>11)&31u)*255u/31u),char(((v>>5)&63u)*255u/63u),char((v&31u)*255u/31u)};
            out.write(rgb,3);
        }
    }
    check(bool(out),"PPM write failed");
}
static void replay(){
    sf::Field a,b;sf::Receiver rx(91);unsigned checks=0;
    for(uint32_t i=1;i<=1000;++i){
        check(a.advance()&&b.advance()&&a.pixels()==b.pixels(),"determinism");++checks;
        auto p=sf::encode(91,a);const auto now=0xffffff00u+i;
        check(rx.accept(p.data(),p.size(),now)&&rx.pixels()==a.pixels(),"snapshot");++checks;
        check(!rx.accept(p.data(),p.size(),now+1),"duplicate");++checks;
        for(std::size_t pos: {std::size_t(0),std::size_t(4),std::size_t(12),std::size_t(19),sf::bytes-1}){
            auto corrupt=p;corrupt[pos]^=1;
            sf::Receiver virgin(91);
            check(!virgin.accept(corrupt.data(),corrupt.size(),now),"corruption accepted");++checks;
        }
        check(!rx.accept(p.data(),p.size()-1,now),"truncation accepted");++checks;
        check(rx.fresh(now+sf::lease_ms-1)&&!rx.fresh(now+sf::lease_ms)&&!rx.fresh(now),"lease wrap/resurrection");++checks;
    }
    check(a.advance(),"advance");auto p=sf::encode(91,a);
    check(rx.accept(p.data(),p.size(),0),"recover");rx.disconnect();
    check(!rx.fresh(1)&&!rx.accept(p.data(),p.size(),2),"disconnect replay");
    check(a.advance(),"advance");p=sf::encode(91,a);
    check(rx.accept(p.data(),p.size(),3),"rejoin");
    std::array<uint16_t,320> row{};
    sf::row(rx.pixels(),239,row.data());
    for(std::size_t x=0;x<row.size();++x)check(row[x]==sf::color(a.pixels()[23*32+x/10]),"row");
    std::cout<<"PASS spatial checks="<<checks+325<<" digest="<<sf::digest(a.pixels())<<'\n';
}
static void sender(Udp& udp,bool faults){
    sf::Field f;sf::Packet prior{};
    for(unsigned i=1;i<=80;++i){
        check(f.advance(),"advance");auto p=sf::encode(91,f);
        if(!faults||i<20||i>50){
            check(udp.send_bytes(p.data(),p.size()),"send");
            if(faults&&i%5==0){udp.send_bytes(p.data(),p.size());if(i>1)udp.send_bytes(prior.data(),prior.size());}
        }
        prior=p;std::this_thread::sleep_for(15ms);
    }
}
static void viewer(Udp& udp,bool faults,const char* path){
    sf::Field local,reference;sf::Receiver rx(91);
    unsigned accepted=0,rejected=0,fallback=0,recovered=0;
    bool saw_remote=false,saw_loss=false;
    auto start=Clock::now(),next=start;
    std::cout<<"READY"<<std::endl;
    while(Clock::now()-start<2300ms){
        for(unsigned j=0;j<8;++j){
            sf::Packet p{};std::size_t n=0;
            if(!udp.receive_bytes(p.data(),p.size(),n))break;
            if(!rx.accept(p.data(),n,ms())){++rejected;continue;}
            ++accepted;
            check(rx.sequence()<=80,"unexpected sequence");
            while(reference.sequence()<rx.sequence())check(reference.advance(),"reference");
            check(reference.pixels()==rx.pixels(),"remote field mismatch");
            if(saw_loss)++recovered;
        }
        const auto now=Clock::now();
        if(now>=next){
            check(local.advance(),"local advance");next=now+15ms;
            if(rx.fresh(ms()))saw_remote=true;
            else if(saw_remote){++fallback;saw_loss=true;}
        }
        std::this_thread::sleep_for(1ms);
    }
    check(accepted>= (faults?35u:65u)&&rx.sequence()>=75,"insufficient actual stream");
    if(faults)check(recovered>0&&fallback>0&&rejected>0,"no blackout/rejoin/duplicate witness");
    ppm(path,rx.pixels()); // last accepted remote snapshot, not live after timeout
    std::cout<<"PASS accepted="<<accepted<<" rejected="<<rejected<<" fallback_ticks="<<fallback
        <<" recovered="<<recovered<<" last_seq="<<rx.sequence()<<" payload_bytes="<<accepted*sf::bytes<<'\n';
}
int main(int argc,char** argv){
    if(argc==1){replay();return 0;}
    if(argc!=6)fail("usage: spatial send|receive own-port peer-port clean|faults output.ppm");
#ifdef _WIN32
    WSADATA data{};if(WSAStartup(MAKEWORD(2,2),&data)!=0)fail("WSAStartup");
#endif
    auto port=[](const char* s){char* end=nullptr;auto p=std::strtoul(s,&end,10);if(!*s||*end||p<1024||p>65535)fail("port");return unsigned(p);};
    const bool faults=std::strcmp(argv[4],"faults")==0;
    check(faults||std::strcmp(argv[4],"clean")==0,"mode");
    {Udp udp(port(argv[2]),port(argv[3]));
    if(std::strcmp(argv[1],"send")==0)sender(udp,faults);
    else if(std::strcmp(argv[1],"receive")==0)viewer(udp,faults,argv[5]);
    else fail("role");}
#ifdef _WIN32
    WSACleanup();
#endif
}
