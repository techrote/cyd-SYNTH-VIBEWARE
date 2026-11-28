#include "checkpoint.hpp"
#include "checkpoint_paced.hpp"
#include <chrono>
#include <cstdlib>
#include <iostream>
namespace sf=cluster::spatial;
namespace cp=sf::checkpoint;
static unsigned checks=0;
static void check(bool yes,const char* why){++checks;if(!yes){std::cerr<<why<<'\n';std::exit(1);}}
int main(){
    sf::ReactionField worker,local;
    sf::ReactionField::Checkpoint state{},actual{};
    cp::Receiver rx(91);
    for(unsigned tick=1;tick<=600;++tick){
        check(worker.advance(),"worker tick");
        if(tick%10!=0)continue;
        check(worker.export_checkpoint(state),"export");
        check(local.begin()&&!local.slice(),"suspended local");
        check(!local.export_checkpoint(actual),"partial export");
        const uint32_t now=0xffffff00u+tick;
        for(std::size_t j=cp::parts;j>0;--j){
            auto packet=cp::encode(91,tick,state,j-1);
            check(!rx.accept(packet.data(),packet.size()-1,now),"truncated");
            auto corrupt=packet;corrupt.back()^=1;
            check(!rx.accept(corrupt.data(),corrupt.size(),now),"corrupt");
            check(rx.accept(packet.data(),packet.size(),now),"reordered admission");
            check(!rx.accept(packet.data(),packet.size(),now),"duplicate");
            check(rx.ready()==(j==1),"partial publication");
        }
        check(!rx.restore(local,tick+1),"minimum sequence");
        check(rx.restore(local,tick)&&!rx.restore(local,tick),"single restore");
        check(!local.busy()&&local.sequence()==worker.sequence()&&local.pixels()==worker.pixels(),"phase restore");
        check(local.export_checkpoint(actual)&&actual==state,"all concentrations exact");
        sf::ReactionField reference=worker;
        for(unsigned i=0;i<19;++i){
            check(local.advance()&&reference.advance(),"continuation");
            check(local.pixels()==reference.pixels(),"future projection");
            check(local.export_checkpoint(actual)&&reference.export_checkpoint(state)&&actual==state,"future state");
        }
    }
    check(worker.export_checkpoint(state),"last export");
    cp::Receiver fault(91);
    auto first=cp::encode(91,600,state,0);
    check(fault.accept(first.data(),first.size(),0xffffff80u),"first");
    auto second=cp::encode(91,600,state,1);
    check(!fault.accept(second.data(),second.size(),0xffffff80u+cp::assembly_ms),"expiry wrap");
    check(!fault.ready()&&!fault.restore(local,0),"incomplete restore");
    check(!fault.accept(first.data(),first.size(),300),"expired replay");
    auto foreign=cp::encode(92,601,state,0);
    check(!fault.accept(foreign.data(),foreign.size(),301),"foreign epoch");
    for(std::size_t j=0;j<cp::parts;++j){auto p=cp::encode(91,601,state,j);check(fault.accept(p.data(),p.size(),302),"recovery");}
    check(fault.ready()&&fault.restore(local,601),"recovered state");
    // Valid checksum is not permission to inject an out-of-range concentration.
    state[0]=255;state[1]=255;
    for(std::size_t j=0;j<cp::parts;++j){auto p=cp::encode(91,602,state,j);check(fault.accept(p.data(),p.size(),303),"invalid value assembly");}
    const auto before=local.pixels();
    check(!fault.restore(local,602)&&local.sequence()==601&&local.pixels()==before,"atomic invalid restore");
    check(worker.export_checkpoint(state),"valid export");
    cp::Receiver mixed(91);auto p=cp::encode(91,700,state,0);
    check(mixed.accept(p.data(),p.size(),0),"mixed initial");
    state[0]^=1;p=cp::encode(91,700,state,1);
    check(!mixed.accept(p.data(),p.size(),1),"mixed digest");
    p=cp::encode(91,701,state,1);check(mixed.accept(p.data(),p.size(),2),"supersede");
    p=cp::encode(91,700,state,2);check(!mixed.accept(p.data(),p.size(),3),"old sequence");
    // Pacing uses immutable bytes while the worker continues its own trajectory.
    cp::PacedSender paced;
    cp::Receiver paced_rx(91);
    const uint32_t start_ms=0xffffffe0u;
    check(worker.export_checkpoint(state),"paced reference");
    const auto seq=worker.sequence();
    check(paced.start(91,worker,start_ms)&&!paced.start(91,worker,start_ms),"single flight");
    unsigned sent=0;
    auto sink=[&](const cp::Packet& packet){
        check(packet==cp::encode(91,seq,state,sent),"immutable paced wire");
        ++sent;return true;
    };
    for(uint32_t ms=0;ms<60;++ms){
        const auto now=start_ms+ms;
        auto receive=[&](const cp::Packet& packet){
            return sink(packet)&&paced_rx.accept(packet.data(),packet.size(),now);
        };
        if(ms%10==1)check(worker.advance(),"concurrent worker evolution");
        const auto result=paced.poll(now,receive);
        if(ms%5==0)check(result==(ms==55?cp::PacedSender::Result::complete:cp::PacedSender::Result::sent),"paced due");
        else check(result==cp::PacedSender::Result::idle,"paced idle");
        check(paced.poll(now,receive)==cp::PacedSender::Result::idle,"no same-time burst");
    }
    check(sent==cp::parts&&paced_rx.restore(local,seq),"paced complete");
    check(local.export_checkpoint(actual)&&actual==state,"paced exact state");
    check(paced.start(91,worker,100),"deadline start");
    check(paced.poll(250,sink)==cp::PacedSender::Result::expired&&!paced.active(),"deadline abort");
    check(paced.start(91,worker,300),"failed send start");
    check(paced.poll(300,[](const cp::Packet&){return false;})==cp::PacedSender::Result::failed&&!paced.active(),"failed send abort");
    check(paced.start(91,worker,400),"cancel start");paced.cancel();
    check(paced.poll(400,sink)==cp::PacedSender::Result::idle,"cancel no send");
    std::cout<<"paced_sender_bytes="<<sizeof(cp::PacedSender)<<'\\n';
    // Host only timing of complete encode/reassembly/restore; no sockets/radio.
    check(worker.export_checkpoint(state),"benchmark export");
    auto start=std::chrono::steady_clock::now();
    cp::Receiver bench(91);
    for(uint32_t s=1;s<=200;++s){
        for(std::size_t j=0;j<cp::parts;++j){p=cp::encode(91,s,state,j);check(bench.accept(p.data(),p.size(),s),"bench assembly");}
        check(bench.restore(local,s),"bench restore");
    }
    const auto us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
    std::cout<<"PASS checkpoint checks="<<checks<<" state_bytes="<<state.size()<<" packet_bytes="<<cp::Packet{}.size()<<" parts="<<cp::parts<<" receiver_bytes="<<sizeof(cp::Receiver)<<" host_200_roundtrip_us="<<us<<'\n';
}
