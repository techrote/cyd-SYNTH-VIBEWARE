#include "udp_transport.hpp"
#include "checkpoint_follow.hpp"
namespace sf=cluster::spatial;
namespace cp=sf::checkpoint;
static void check(bool ok,const char* why){if(!ok)fail(why);}
static unsigned port(const char* s){
    char* end=nullptr;auto p=std::strtoul(s,&end,10);
    if(!*s||*end||p<1024||p>65535)fail("port");
    return unsigned(p);
}
static void sender(Udp& udp,bool faults){
    sf::ReactionField f;
    sf::ReactionField::Checkpoint state{};
    for(uint32_t i=1;i<=80;++i){
        check(f.advance(),"worker tick");
        if(!faults||i<20||i>50){
            const auto snapshot=sf::encode(91,f);
            check(udp.send_bytes(snapshot.data(),snapshot.size()),"snapshot send");
            if(i%5==0||(faults&&i==18)){
                check(f.export_checkpoint(state),"export");
                const cp::Prepared prepared(state);
                for(std::size_t j=cp::parts;j>0;--j){
                    if(faults&&i==18&&j==1)continue;
                    const auto p=prepared.packet(91,i,j-1);
                    check(udp.send_bytes(p.data(),p.size()),"checkpoint send");
                    if(faults&&j==cp::parts)check(udp.send_bytes(p.data(),p.size()),"duplicate send");
                }
            }
        }
        std::this_thread::sleep_for(15ms);
    }
}
static void receiver(Udp& udp,bool faults){
    cp::Follower follower(91);
    sf::ReactionField reference;
    sf::ReactionField::Checkpoint actual{},expected{};
    unsigned rejected=0,accepted=0,matched=0,remote_polls=0;
    bool loss=false,rejoin=false;
    auto start=Clock::now();
    std::cout<<"READY"<<std::endl;
    uint32_t checked=0;
    while(Clock::now()-start<2400ms){
        for(unsigned i=0;i<8;++i){
            cp::Packet p{};std::size_t n=0;
            if(!udp.receive_bytes(p.data(),p.size(),n))break;
            if(follower.receive(p.data(),n,ms()))++accepted;else ++rejected;
        }
        const auto before=follower.slices;
        const bool remote_before=follower.remote();
        follower.tick(ms());
        check(follower.slices-before<=1,"unbounded slice");
        if(follower.remote()){++remote_polls;if(loss)rejoin=true;}
        if(remote_before&&!follower.remote()&&follower.view().sequence()<80)loss=true;
        if(!follower.local().busy()&&follower.local().sequence()!=checked){
            checked=follower.local().sequence();
            check(checked<=160,"unexpected local sequence");
            // Reference is a witness, not part of the measured fallback path.
            while(reference.sequence()<checked)check(reference.advance(),"reference advance");
            check(reference.sequence()==checked,"local rollback");
            check(reference.export_checkpoint(expected)&&follower.local().export_checkpoint(actual)&&actual==expected,"state mismatch");
            ++matched;
        }
        std::this_thread::sleep_for(1ms);
    }
    check(follower.view().sequence()==80&&follower.admitted>= (faults?5u:10u)&&matched>0&&remote_polls>0,"insufficient stream");
    if(faults)check(loss&&rejoin&&rejected>0&&follower.caught_up>0,"no failover witness");
    std::cout<<"PASS checkpoint_udp accepted="<<accepted<<" rejected="<<rejected
        <<" admitted="<<follower.admitted<<" refused="<<follower.refused
        <<" matched="<<matched<<" caught_up="<<follower.caught_up<<" cold_losses="<<follower.cold_losses
        <<" loss="<<loss<<" rejoin="<<rejoin<<" last_seq="<<follower.view().sequence()<<'\n';
}
int main(int argc,char** argv){
    if(argc!=5)fail("usage: checkpoint_udp send|receive own-port peer-port clean|faults");
    const bool faults=std::strcmp(argv[4],"faults")==0;
    check(faults||std::strcmp(argv[4],"clean")==0,"mode");
    const auto own=port(argv[2]),peer=port(argv[3]);check(own!=peer,"distinct ports");
#ifdef _WIN32
    WSADATA data{};if(WSAStartup(MAKEWORD(2,2),&data)!=0)fail("WSAStartup");
#endif
    {Udp udp(own,peer);
    if(std::strcmp(argv[1],"send")==0)sender(udp,faults);
    else if(std::strcmp(argv[1],"receive")==0)receiver(udp,faults);
    else fail("role");}
#ifdef _WIN32
    WSACleanup();
#endif
}
