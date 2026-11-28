#pragma once
#include "checkpoint.hpp"
#include "spatial.hpp"

namespace cluster::spatial::checkpoint {
// One control-loop owner. Snapshot arrival freshness is not synchronized age.
// Keep the most recently admitted full state in the suspended local engine,
// so a newer incomplete transfer cannot destroy the fallback checkpoint.
class Follower {
    Receiver checkpoint_;
    spatial::Receiver view_;
    ReactionField local_;
    uint32_t ready_at_=0, saved_at_=0, saved_seq_=0, target_=0, last_step_=0;
    bool was_remote_=false;
public:
    static constexpr uint32_t max_gap=20, max_saved_age_ms=600, tick_ms=50;
    uint32_t admitted=0, refused=0, caught_up=0, cold_losses=0, slices=0;
    explicit Follower(uint32_t epoch):checkpoint_(epoch),view_(epoch,ReactionField::variant){}
    const auto& local()const{return local_;}
    const auto& view()const{return view_;}
    uint32_t saved_sequence()const{return saved_seq_;}
    uint32_t catchup_target()const{return target_;}
    bool remote()const{return was_remote_;}
    bool receive(const uint8_t* p,std::size_t n,uint32_t now){
        if(n==spatial::bytes)return view_.accept(p,n,now);
        if(!checkpoint_.accept(p,n,now))return false;
        if(checkpoint_.ready())ready_at_=now;
        return true;
    }
    // Called once per bounded poll iteration. At most one lattice slice or
    // projection per invocation, with checkpoint restore as a separate bounded
    // operation. No synchronous multi-tick catch-up.
    void tick(uint32_t now){
        checkpoint_.expire(now);
        const bool fresh=view_.fresh(now);
        if(checkpoint_.ready()){
            const auto seq=checkpoint_.sequence(), latest=view_.sequence();
            if(uint32_t(now-ready_at_)>=assembly_ms||seq<local_.sequence()||
               (seq<=latest&&latest-seq>max_gap)){
                checkpoint_.discard();++refused;
            }else if(fresh&&seq<=latest){
                if(checkpoint_.restore(local_,seq)){
                    saved_seq_=seq;saved_at_=now;++admitted;
                }else ++refused;
            }
        }
        if(fresh){
            was_remote_=true;target_=0;last_step_=now;return;
        }
        if(was_remote_){
            was_remote_=false;
            const auto latest=view_.sequence();
            if(saved_seq_&&local_.sequence()==saved_seq_&&saved_seq_<=latest&&
               latest-saved_seq_<=max_gap&&uint32_t(now-saved_at_)<max_saved_age_ms){
                target_=latest;
                if(local_.sequence()==target_){++caught_up;target_=0;}
            }else{target_=0;++cold_losses;}
        }
        if(!local_.busy()&&(target_>local_.sequence()||uint32_t(now-last_step_)>=tick_ms)){
            if(local_.begin())last_step_=now;
        }
        if(local_.busy()){
            ++slices;
            if(local_.slice()&&target_&&local_.sequence()==target_){
                ++caught_up;target_=0;last_step_=now;
            }
        }
    }
};
} // namespace cluster::spatial::checkpoint
