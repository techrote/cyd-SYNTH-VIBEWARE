#include "udp_transport.hpp"
#include "dual.hpp"
int main(int argc,char** argv) {
    if(argc!=4&&argc!=5)fail("usage: cluster_dual_udp leader|follower own_port peer_port [fault]");
    const bool fault=argc==5&&std::strcmp(argv[4],"fault")==0;
    if(argc==5&&!fault)fail("invalid scenario");
    const bool leader=std::strcmp(argv[1],"leader")==0;
    if(!leader&&std::strcmp(argv[1],"follower")!=0)fail("invalid role");
    const auto own=std::strtoul(argv[2],nullptr,10),other=std::strtoul(argv[3],nullptr,10);
    if(!own||own>65535||!other||other>65535||own==other)fail("invalid ports");
#ifdef _WIN32
    WSADATA data{};if(WSAStartup(MAKEWORD(2,2),&data)!=0)fail("WSAStartup failed");
#endif
    {
        Udp udp{unsigned(own),unsigned(other)};
        cluster::dual::Peer peer(77,leader);peer.network(true);
        const uint32_t start=ms();
        const auto now=[&](){return ms()+(leader?0u:123456u);};
        uint32_t injected=0,local_mid=0,remote_mid=0;
        bool checkpoint=false;
        auto send=[&](const Frame& f){
            const uint32_t age=ms()-start;
            if(fault&&leader&&age>=2200&&age<5200){++injected;return true;}
            return udp.send(f);
        };
        unsigned audible=0;
        const uint32_t duration=fault?8000u:5000u;
        while(uint32_t(ms()-start)<duration){
            for(unsigned i=0;i<8;++i){Frame f;if(!udp.receive(f))break;peer.receive(f,now(),send);}
            peer.tick(now(),send);if(peer.sound(now()))++audible;
            if(fault&&!checkpoint&&uint32_t(ms()-start)>=5300){checkpoint=true;local_mid=peer.stats.local;remote_mid=peer.stats.played;}
            std::this_thread::sleep_for(1ms);
        }
        const auto& s=peer.stats;
        std::cout<<"samples="<<s.samples<<" sent="<<s.events_sent<<" played="<<s.played
                 <<" local="<<s.local<<" audible_ticks="<<audible<<" late_ms="<<s.max_late_ms
                 <<" injected="<<injected<<" local_mid="<<local_mid<<" post_rejoin="<<(s.played-remote_mid)<<'\n';
        if(!audible||(leader?s.events_sent<10:s.samples<3||s.played<8))return 1;
        if(fault&&(leader?injected<8:(!checkpoint||local_mid<3||s.played-remote_mid<4)))return 1;
    }
#ifdef _WIN32
    WSACleanup();
#endif
}
