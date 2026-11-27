#include "udp_transport.hpp"
#include "dual.hpp"
int main(int argc,char** argv) {
    if(argc!=4)fail("usage: cluster_dual_udp leader|follower own_port peer_port");
    const bool leader=std::strcmp(argv[1],"leader")==0;
    if(!leader&&std::strcmp(argv[1],"follower")!=0)fail("invalid role");
    const auto own=std::strtoul(argv[2],nullptr,10),other=std::strtoul(argv[3],nullptr,10);
    if(!own||own>65535||!other||other>65535||own==other)fail("invalid ports");
#ifdef _WIN32
    WSADATA data{};if(WSAStartup(MAKEWORD(2,2),&data)!=0)fail("WSAStartup failed");
#endif
    {
        Udp udp(unsigned(own),unsigned(other));
        cluster::dual::Peer peer(77,leader);peer.network(true);
        const uint32_t start=ms();
        const auto now=[&](){return ms()+(leader?0u:123456u);};
        auto send=[&](const Frame& f){return udp.send(f);};
        unsigned audible=0;
        while(uint32_t(ms()-start)<5000){
            for(unsigned i=0;i<8;++i){Frame f;if(!udp.receive(f))break;peer.receive(f,now(),send);}
            peer.tick(now(),send);if(peer.sound(now()))++audible;
            std::this_thread::sleep_for(1ms);
        }
        const auto& s=peer.stats;
        std::cout<<"samples="<<s.samples<<" sent="<<s.events_sent<<" played="<<s.played
                 <<" local="<<s.local<<" audible_ticks="<<audible<<" late_ms="<<s.max_late_ms<<'\n';
        if(!audible||(leader?s.events_sent<10:s.samples<3||s.played<8))return 1;
    }
#ifdef _WIN32
    WSACleanup();
#endif
}
