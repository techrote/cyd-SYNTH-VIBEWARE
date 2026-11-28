#include "udp_transport.hpp"
#include "reaction_view.hpp"
namespace sf=cluster::spatial;
namespace cp=sf::checkpoint;
static void check(bool ok,const char* why){if(!ok)fail(why);}
static unsigned port(const char* s){
    char* end=nullptr;auto p=std::strtoul(s,&end,10);
    if(!*s||*end||p<1024||p>65535)fail("port");
    return unsigned(p);
}
static void sender(Udp& udp,bool faults,bool audition=false,bool stream=false){
    sf::ReactionField f;
    sf::ReactionField::Checkpoint state{};
    for(uint32_t i=1;i<=(stream?2400u:80u);++i){
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
        std::this_thread::sleep_for(audition||stream?50ms:15ms);
    }
}
static void receiver(Udp& udp,bool faults){
    cp::Follower follower(91);
    sf::ReactionField reference;
    sf::ReactionField::Checkpoint actual{},expected{};
    unsigned rejected=0,accepted=0,matched=0,remote_polls=0;
    bool loss=false,rejoin=false,catchup_during_loss=false;
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
        if(loss&&!rejoin&&follower.caught_up>0&&!follower.remote())catchup_during_loss=true;
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
        if(follower.catchup_target())std::this_thread::yield();
        else std::this_thread::sleep_for(1ms);
    }
    check(follower.view().sequence()==80&&follower.admitted>= (faults?5u:10u)&&matched>0&&remote_polls>0,"insufficient stream");
    if(faults)check(loss&&rejoin&&rejected>0&&catchup_during_loss,"no failover witness");
    std::cout<<"PASS checkpoint_udp accepted="<<accepted<<" rejected="<<rejected
        <<" admitted="<<follower.admitted<<" refused="<<follower.refused
        <<" matched="<<matched<<" caught_up="<<follower.caught_up<<" cold_losses="<<follower.cold_losses
        <<" loss="<<loss<<" rejoin="<<rejoin<<" catchup_during_loss="<<catchup_during_loss<<" last_seq="<<follower.view().sequence()<<'\n';
}
static void view_policy_test(){
    sf::ReactionView v(91);
    sf::ReactionField worker;
    sf::ReactionField::Checkpoint state{},actual{};
    const uint32_t start=0xffffff00u;
    v.present(start);
    v.allow_remote(true);
    auto snap=[&](uint32_t now){
        const auto p=sf::encode(91,worker);
        return v.receive(p.data(),p.size(),now);
    };
    auto frag=[&](std::size_t j,uint32_t now){
        const auto p=cp::encode(91,worker.sequence(),state,j);
        return v.receive(p.data(),p.size(),now);
    };
    for(unsigned i=1;i<=15;++i)check(worker.advance(),"view worker advance");
    check(snap(start),"view snapshot");
    check(worker.export_checkpoint(state),"view export");
    for(std::size_t j=0;j<cp::parts;++j)check(frag(j,start),"view checkpoint");
    v.step(start);v.present(start);
    check(v.follower().admitted==1&&v.follower().remote(),"view remote admission");
    for(unsigned i=16;i<=19;++i){
        check(worker.advance(),"view worker advance");
        check(snap(start+50u*(i-15u)),"view next snapshot");
        v.step(start+50u*(i-15u));v.present(start+50u*(i-15u));
    }
    const auto frozen=v.image();
    const auto loss=start+200u+sf::lease_ms;
    v.step(loss);v.present(loss);
    check(v.frozen()&&v.image()==frozen,"view failed to freeze on loss");
    unsigned work=1;
    while(v.frozen()&&work<101){
        check(v.step(loss),"view catchup progress");++work;
        v.present(loss);
        if(v.frozen())check(v.image()==frozen,"view partial state leaked");
    }
    check(!v.frozen()&&v.follower().caught_up==1&&v.follower().local().sequence()==19,"view recovery");
    check(v.follower().local().export_checkpoint(actual)&&worker.export_checkpoint(state)&&actual==state,"view exact catchup");
    check(worker.advance()&&worker.export_checkpoint(state),"view manual setup");
    check(snap(loss+1)&&frag(0,loss+1),"view partial checkpoint");
    v.step(loss+1);v.present(loss+1);
    v.allow_remote(false);v.step(loss+2);v.present(loss+2);
    check(!v.follower().remote()&&!v.frozen()&&v.image()==v.follower().local().pixels(),"manual not immediate");
    check(!snap(loss+2)&&!frag(1,loss+2),"manual admitted traffic");
    v.allow_remote(true);v.step(loss+3);v.present(loss+3);
    check(!v.follower().remote()&&!snap(loss+3)&&!frag(1,loss+3),"manual stale rejoin");
    check(worker.advance()&&snap(loss+4),"view fresh rejoin");
    v.step(loss+4);v.present(loss+4);
    check(v.follower().remote()&&v.follower().admitted==1,"manual recovery");
    std::cout<<"PASS reaction view policy catchup_slices="<<work<<'\n';
}
static void policy_test(){
    sf::ReactionField worker;
    sf::ReactionField::Checkpoint state{},actual{};
    cp::Follower f(91);
    auto snapshot=[&](cp::Follower& dst,uint32_t now){
        const auto p=sf::encode(91,worker);
        check(dst.receive(p.data(),p.size(),now),"policy snapshot");
    };
    auto checkpoint=[&](cp::Follower& dst,uint32_t now,bool complete){
        check(worker.export_checkpoint(state),"policy export");
        const cp::Prepared enc(state);
        for(std::size_t j=0;j<cp::parts-(complete?0u:1u);++j){
            auto p=enc.packet(91,worker.sequence(),j);
            check(p==cp::encode(91,worker.sequence(),state,j),"prepared byte parity");
            check(dst.receive(p.data(),p.size(),now),"policy fragment");
        }
    };
    const uint32_t base=0xffffff80u;
    for(unsigned i=1;i<=15;++i)check(worker.advance(),"policy advance");
    checkpoint(f,base,true);f.tick(base);
    check(f.admitted==0,"future checkpoint restored without presentation");
    snapshot(f,base+1);f.tick(base+1);
    check(f.admitted==1&&f.saved_sequence()==15&&f.remote(),"checkpoint admitted");
    for(unsigned i=16;i<=19;++i){
        check(worker.advance(),"policy advance");
        snapshot(f,base+i);
        if(i==18)checkpoint(f,base+i,false);
        f.tick(base+i);
    }
    check(f.saved_sequence()==15,"incomplete transfer destroyed checkpoint");
    const uint32_t loss=base+19+sf::lease_ms;
    f.tick(loss);
    check(!f.remote()&&f.catchup_target()==19,"loss target");
    for(unsigned i=0;i<100&&f.catchup_target();++i){
        const auto n=f.slices;f.tick(loss);
        check(f.slices-n<=1,"policy slice bound");
    }
    check(f.caught_up==1&&f.local().sequence()==19&&f.local().export_checkpoint(actual),"catch-up endpoint");
    check(worker.export_checkpoint(state)&&actual==state,"catch-up state parity");
    // A completed future state without a fresh matching presentation must expire.
    cp::Follower stale(91);
    checkpoint(stale,base,true);stale.tick(base+cp::assembly_ms);
    snapshot(stale,base+cp::assembly_ms+1);stale.tick(base+cp::assembly_ms+1);
    check(stale.admitted==0&&stale.refused==1,"expired completion resurrected");
    // A valid but overly distant checkpoint must not trigger unbounded catch-up.
    cp::Follower gap(91);
    snapshot(gap,base);checkpoint(gap,base,true);gap.tick(base);
    check(gap.admitted==1,"gap setup");
    for(unsigned i=0;i<=cp::Follower::max_gap;++i)check(worker.advance(),"gap advance");
    snapshot(gap,base+50);gap.tick(base+50);
    gap.tick(base+50+sf::lease_ms);
    check(gap.cold_losses==1&&gap.catchup_target()==0,"unbounded gap accepted");
    std::cout<<"PASS checkpoint policy\n";
}
int main(int argc,char** argv){
    if(argc==1){policy_test();view_policy_test();return 0;}
    if(argc!=5)fail("usage: checkpoint_udp send|receive own-port peer-port clean|faults; sender also stream|view-faults");
    const bool audition=std::strcmp(argv[4],"view-faults")==0;
    const bool stream=std::strcmp(argv[4],"stream")==0;
    const bool faults=std::strcmp(argv[4],"faults")==0||audition;
    check(faults||stream||std::strcmp(argv[4],"clean")==0,"mode");
    const auto own=port(argv[2]),peer=port(argv[3]);check(own!=peer,"distinct ports");
#ifdef _WIN32
    WSADATA data{};if(WSAStartup(MAKEWORD(2,2),&data)!=0)fail("WSAStartup");
#endif
    {Udp udp(own,peer);
    if(std::strcmp(argv[1],"send")==0)sender(udp,faults,audition,stream);
    else if(std::strcmp(argv[1],"receive")==0&&!audition&&!stream)receiver(udp,faults);
    else fail("role");}
#ifdef _WIN32
    WSACleanup();
#endif
}
