// Independent QA fixture for HYBRID RC v1; no socket or physical timing claim.
#include "checkpoint.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <random>
namespace sf=cluster::spatial;
namespace cp=sf::checkpoint;
using State=sf::ReactionField::Checkpoint;
static unsigned checks=0;
static void check(bool ok,const char* why){++checks;if(!ok){std::cerr<<"FAIL "<<why<<" at "<<checks<<'\n';std::exit(1);}}
static void seal(cp::Packet& p){cp::put(p.data()+20,cp::hash(p.data()+cp::prefix,cp::payload,cp::hash(p.data(),20)));}
int main(){
    constexpr uint32_t epoch=91;
    sf::ReactionField worker,local;
    State state{},observed{};
    for(unsigned i=0;i<17;++i)check(worker.advance(),"initial tick");
    check(worker.export_checkpoint(state),"initial export");
    unsigned flips=0;
    const cp::Prepared prepared(state);
    for(std::size_t j=0;j<cp::parts;++j){
        const auto packet=prepared.packet(epoch,17,j);
        check(packet==cp::encode(epoch,17,state,j),"prepared parity");
        for(std::size_t i=0;i<packet.size();++i)for(unsigned bit=0;bit<8;++bit){
            auto corrupt=packet;corrupt[i]^=uint8_t(1u<<bit);
            cp::Receiver rx(epoch);
            check(!rx.accept(corrupt.data(),corrupt.size(),0)&&!rx.ready()&&rx.sequence()==0,"single bit corruption");++flips;
        }
    }
    const auto packet=prepared.packet(epoch,17,0);
    cp::Receiver length(epoch);
    for(std::size_t n=0;n<packet.size();++n)check(!length.accept(packet.data(),n,0),"truncated");
    check(!length.accept(packet.data(),packet.size()+1,0)&&!length.accept(nullptr,packet.size(),0),"invalid length/null");
    // Independently re-seal malformed headers: packet integrity is not semantic validity.
    for(auto i:std::array<unsigned,8>{0,1,2,3,4,13,14,15}){
        auto bad=packet;bad[i]^=1;seal(bad);
        check(!length.accept(bad.data(),bad.size(),0)&&length.sequence()==0,"semantic header");
    }
    auto bad=packet;bad[12]=uint8_t(cp::parts);seal(bad);
    check(!length.accept(bad.data(),bad.size(),0),"out of range part");
    for(uint32_t seq:std::array<uint32_t,2>{0,UINT32_MAX}){
        bad=packet;cp::put(bad.data()+8,seq);seal(bad);
        check(!length.accept(bad.data(),bad.size(),0),"reserved sequence");
    }
    // Every fragment has a valid packet checksum but a shared false whole-state digest.
    cp::Receiver wrong(epoch);
    for(std::size_t i=0;i<cp::parts;++i){
        bad=prepared.packet(epoch,17,i);cp::put(bad.data()+16,cp::get(bad.data()+16)^1u);seal(bad);
        check(wrong.accept(bad.data(),bad.size(),1)==(i+1<cp::parts),"wrong whole digest");
    }
    check(!wrong.ready()&&!wrong.restore(local,0),"wrong whole not published");
    // Test every concentration position, including last byte pair, without mutating
    // the local completed state or a suspended next tick on rejected restore.
    check(local.restore_checkpoint(17,state),"restore starting state");
    const auto before=local.pixels();
    unsigned invalid=0;
    for(std::size_t i=0;i<state.size()/2;++i){
        State malformed=state;malformed[2*i]=1;malformed[2*i+1]=16; // 4097
        check(!local.restore_checkpoint(18,malformed),"out of range concentration");
        check(local.sequence()==17&&local.pixels()==before&&!local.busy(),"transaction metadata");
        check(local.export_checkpoint(observed)&&observed==state,"transaction planes");++invalid;
    }
    check(local.begin()&&!local.slice(),"suspend next tick");
    State malformed=state;malformed.back()=255;
    check(!local.restore_checkpoint(18,malformed)&&local.busy(),"failed restore retains partial tick");
    while(!local.slice()){}
    check(worker.advance()&&worker.export_checkpoint(state)&&local.export_checkpoint(observed)&&state==observed,"partial continuation exact");
    // Fixed seed shuffles every transfer; duplicate and expired packets cannot
    // publish a partial state, even across wrap of the millisecond clock.
    std::mt19937 rng(0x435944u);
    cp::Receiver rx(epoch);
    std::array<std::size_t,cp::parts> order{};for(std::size_t i=0;i<order.size();++i)order[i]=i;
    unsigned restored=0,expired=0,continued=0;
    for(unsigned run=0;run<80;++run){
        check(worker.advance()&&worker.export_checkpoint(state),"worker tick");
        const auto seq=worker.sequence();const uint32_t now=0xffffff80u+run*250u;
        std::shuffle(order.begin(),order.end(),rng);const cp::Prepared enc(state);
        const bool drop=(run%4==0);
        for(std::size_t k=0;k<order.size()-(drop?1u:0u);++k){
            const auto p=enc.packet(epoch,seq,order[k]);
            check(rx.accept(p.data(),p.size(),now+uint32_t(k)),"shuffled accept");
            check(!rx.accept(p.data(),p.size(),now+uint32_t(k)),"duplicate reject");
            check(rx.ready()==(!drop&&k+1==order.size()),"publication boundary");
        }
        if(drop){const auto late=enc.packet(epoch,seq,order.back());
            check(!rx.accept(late.data(),late.size(),now+cp::assembly_ms)&&!rx.ready(),"late final part");
            check(!rx.restore(local,0),"expired not restorable");++expired;continue;}
        check(rx.restore(local,seq)&&!rx.restore(local,seq),"single complete restore");
        check(local.export_checkpoint(observed)&&observed==state&&local.pixels()==worker.pixels(),"restored state and projection");++restored;
        auto reference=worker;
        for(unsigned j=0;j<3;++j){check(reference.advance()&&local.advance(),"continue tick");
            check(reference.export_checkpoint(state)&&local.export_checkpoint(observed)&&state==observed&&local.pixels()==reference.pixels(),"continuation exact");++continued;}
    }
    std::cout<<"PASS checks="<<checks<<" bit_flips="<<flips<<" invalid_concentrations="<<invalid<<" restored="<<restored<<" expired="<<expired<<" continued="<<continued<<'\n';
}
