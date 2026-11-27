#pragma once
#include "link.hpp"

namespace cluster {
// Two independent seeded cohorts of the same VECTORFIELD scene. Each can be
// executed by an existing v1 worker; sum bins only after both are complete.
// No frame format change, allocations, shared mutable worker state or audio work.
using Cohorts=std::array<Job,2>;
inline Cohorts cohorts(uint32_t seed,uint16_t x,uint16_t y,uint16_t steps=64) {
    return {{Job{seed,512,steps,x,y},Job{seed^0x9e3779b9u,512,steps,x,y}}};
}
inline Grid merge_grids(const Grid& a,const Grid& b) {
    Grid result{};
    for(std::size_t i=0;i<result.size();++i) result[i]=uint16_t(a[i]+b[i]);
    return result;
}
inline Grid local_cohorts(const Cohorts& jobs) {
    return merge_grids(compute(jobs[0]),compute(jobs[1]));
}
struct MultiStats { uint32_t remote=0,local=0,batches=0; };
class Multi {
    std::array<Link,2> links_;
    std::array<int,2> assigned_{{-1,-1}};
    std::array<Grid,2> results_{};
    Cohorts jobs_{};
    unsigned nodes_,next_=0,finished_=0;
    uint32_t budget_=0;
    bool active_=false,remote_=true;
    void finish(unsigned slot,Completion c) {
        if(c==Completion::none||assigned_[slot]<0) return;
        results_[std::size_t(assigned_[slot])]=links_[slot].grid();
        assigned_[slot]=-1;
        if(c==Completion::remote) ++stats.remote; else ++stats.local;
        ++finished_;
        if(finished_==2) { grid=merge_grids(results_[0],results_[1]); active_=false; ++stats.batches; }
    }
public:
    // Each physical/logical peer must have its own authenticated transport
    // endpoint, even when an experiment uses the same session epoch.
    Multi(unsigned nodes,uint32_t epoch0,uint32_t epoch1):links_{{Link(epoch0),Link(epoch1)}},nodes_(nodes<=2?nodes:0) {}
    Grid grid{};
    MultiStats stats{};
    bool active() const { return active_; }
    void remote(bool on) { if(!on&&remote_) for(auto& link:links_) link.disconnect(); remote_=on; }
    bool remote_enabled() const { return remote_; }
    bool ready(unsigned slot) const { return slot<nodes_&&links_[slot].ready(); }
    const LinkStats& link_stats(unsigned slot) const { return links_[slot].stats; } // slot 0 or 1
    bool begin(const Cohorts& jobs,uint32_t budget) {
        if(active_||!valid(jobs[0])||!valid(jobs[1])||!budget||budget>60000) return false;
        jobs_=jobs; budget_=budget; next_=finished_=0; active_=true;
        return true;
    }
    // Call only from control/compute loop. Send(slot,frame) must not block.
    // Per-cohort deadline, not a hard whole-batch deadline (one-node execution
    // can take two budgets). Local fallback compute time is additional.
    template<class Send> void tick(uint32_t now,Send& send) {
        if(nodes_==0) {
            if(active_) { grid=local_cohorts(jobs_); stats.local+=2; ++stats.batches; active_=false; }
            return;
        }
        for(unsigned slot=0;slot<nodes_;++slot) {
            auto tx=[&](const Frame& f){return send(slot,f);};
            finish(slot,links_[slot].tick(now,tx,remote_));
        }
        for(unsigned slot=0;slot<nodes_&&active_&&next_<2;++slot) {
            if(assigned_[slot]>=0) continue;
            auto tx=[&](const Frame& f){return send(slot,f);};
            if(links_[slot].start(jobs_[next_],now,budget_,tx)) assigned_[slot]=int(next_++);
            else {
                // Sequence exhausted or invalid session: do not spin forever.
                results_[next_]=compute(jobs_[next_]); ++next_; ++finished_; ++stats.local;
                if(finished_==2) { grid=merge_grids(results_[0],results_[1]); active_=false; ++stats.batches; }
            }
        }
    }
    void receive(unsigned slot,const Frame& f,uint32_t now) {
        if(remote_&&slot<nodes_) finish(slot,links_[slot].receive(f,now));
    }
    void disconnect(unsigned slot) { if(slot<nodes_) links_[slot].disconnect(); }
};
} // namespace cluster
