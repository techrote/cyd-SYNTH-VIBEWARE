#pragma once
#include "reaction.hpp"
#include <algorithm>

namespace cluster::spatial::checkpoint {
// Native experiment only: a single bounded in-flight checkpoint, independent
// from the 788-byte presentation stream. UDP endpoint must be authorized by caller.
constexpr std::size_t payload=1024, prefix=24, parts=ReactionField::checkpoint_bytes/payload;
static_assert(parts==12 && parts*payload==ReactionField::checkpoint_bytes);
constexpr uint32_t assembly_ms=200;
using Packet=std::array<uint8_t,prefix+payload>;
inline uint32_t hash(const uint8_t* p,std::size_t n,uint32_t h=2166136261u){
    for(std::size_t i=0;i<n;++i){h^=p[i];h*=16777619u;}return h;
}
inline void put(uint8_t* p,uint32_t x){for(unsigned i=0;i<4;++i)p[i]=uint8_t(x>>(8*i));}
inline uint32_t get(const uint8_t* p){uint32_t x=0;for(unsigned i=0;i<4;++i)x|=uint32_t(p[i])<<(8*i);return x;}
inline Packet encode_hashed(uint32_t epoch,uint32_t seq,const ReactionField::Checkpoint& state,std::size_t index,uint32_t whole){
    Packet p{};
    if(!epoch||!seq||seq==UINT32_MAX||index>=parts)return p;
    p[0]='R';p[1]='C';p[2]=1;p[3]=ReactionField::variant;
    put(p.data()+4,epoch);put(p.data()+8,seq);p[12]=uint8_t(index);p[13]=uint8_t(parts);
    put(p.data()+16,whole);
    std::copy_n(state.data()+index*payload,payload,p.data()+prefix);
    put(p.data()+20,hash(p.data()+prefix,payload,hash(p.data(),20)));return p;
}
inline Packet encode(uint32_t epoch,uint32_t seq,const ReactionField::Checkpoint& state,std::size_t index){
    return encode_hashed(epoch,seq,state,index,hash(state.data(),state.size()));
}
// The caller owns immutable state for the lifetime of this encoder.
class Prepared {
    const ReactionField::Checkpoint& state_;
    uint32_t whole_;
public:
    explicit Prepared(const ReactionField::Checkpoint& state):state_(state),whole_(hash(state.data(),state.size())){}
    Prepared(ReactionField::Checkpoint&&)=delete;
    Packet packet(uint32_t epoch,uint32_t seq,std::size_t index)const{
        return encode_hashed(epoch,seq,state_,index,whole_);
    }
};
class Receiver {
    ReactionField::Checkpoint data_{};
    uint32_t epoch_,seq_=0,started_=0,whole_=0;
    uint16_t mask_=0;
    bool active_=false,complete_=false;
    static constexpr uint16_t all=uint16_t((1u<<parts)-1u);
public:
    explicit Receiver(uint32_t epoch):epoch_(epoch){}
    uint32_t sequence()const{return seq_;}
    bool ready()const{return complete_;}
    void discard(){complete_=false;}
    void expire(uint32_t now){if(active_&&uint32_t(now-started_)>=assembly_ms){active_=false;mask_=0;}}
    // Fixed first-arrival timeout, not extended by duplicates. Newer sequence
    // supersedes incomplete older state; expired sequence cannot be resurrected.
    bool accept(const uint8_t* p,std::size_t size,uint32_t now){
        expire(now);
        if(!p||size!=prefix+payload||!epoch_||p[0]!='R'||p[1]!='C'||p[2]!=1||p[3]!=ReactionField::variant||
           get(p+4)!=epoch_||p[12]>=parts||p[13]!=parts||p[14]||p[15]||
           get(p+20)!=hash(p+prefix,payload,hash(p,20)))return false;
        const auto seq=get(p+8),whole=get(p+16);
        if(!seq||seq==UINT32_MAX||seq<seq_)return false;
        if(seq>seq_){seq_=seq;whole_=whole;started_=now;mask_=0;active_=true;complete_=false;}
        if(!active_||whole!=whole_)return false;
        const auto bit=uint16_t(1u<<p[12]);
        if(mask_&bit)return false;
        std::copy_n(p+prefix,payload,data_.data()+std::size_t(p[12])*payload);
        mask_=uint16_t(mask_|bit);
        if(mask_==all){active_=false;complete_=hash(data_.data(),data_.size())==whole_;return complete_;}
        return true;
    }
    // Complete state is retained until consumed or superseded. Caller decides
    // whether its sequence is recent enough for handoff; this is NOT clock sync.
    bool restore(ReactionField& field,uint32_t minimum_sequence){
        if(!complete_||seq_<minimum_sequence)return false;
        complete_=false;return field.restore_checkpoint(seq_,data_);
    }
};
} // namespace cluster::spatial::checkpoint
