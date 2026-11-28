#pragma once
#include "checkpoint.hpp"
namespace cluster::spatial::checkpoint {
// Single-owner immutable export. One attempt per poll, never catch up a burst.
class PacedSender {
    ReactionField::Checkpoint state_{};
    uint32_t epoch_=0,seq_=0,whole_=0,started_=0,last_=0;
    std::size_t index_=parts;
public:
    static constexpr uint32_t interval_ms=5, deadline_ms=150;
    bool active()const{return index_<parts;}
    void cancel(){index_=parts;}
    bool start(uint32_t epoch,const ReactionField& field,uint32_t now){
        if(active()||!epoch||!field.sequence()||field.sequence()==UINT32_MAX||
           !field.export_checkpoint(state_))return false;
        epoch_=epoch;seq_=field.sequence();whole_=hash(state_.data(),state_.size());
        started_=now;last_=now-interval_ms;index_=0;return true;
    }
    enum class Result { idle, sent, complete, failed, expired };
    template<class Send> Result poll(uint32_t now,Send&& send){
        if(!active())return Result::idle;
        if(uint32_t(now-started_)>=deadline_ms){cancel();return Result::expired;}
        if(uint32_t(now-last_)<interval_ms)return Result::idle;
        last_=now;
        const auto p=encode_hashed(epoch_,seq_,state_,index_,whole_);
        if(!send(p)){cancel();return Result::failed;}
        ++index_;return active()?Result::sent:Result::complete;
    }
};
}
