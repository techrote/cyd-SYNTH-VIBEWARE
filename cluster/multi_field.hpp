#pragma once
#include "multi.hpp"
#include "field_av.hpp"

namespace cluster::av {
// A two-cohort audiovisual consumer. Both cohorts use identical local/remote
// kernels; only complete merged snapshots enter display and audio controls.
// Single control-loop ownership; no calls from the audio task.
class MultiField {
    Multi multi_{0,1,2};
    Job next_{1,1024,64,32768,32768},active_{},shown_{};
    Grid grid_{};
    uint32_t last_=0,remote_before_=0;
    bool started_=false,gate_=false;
    unsigned last_remote_=0;
    void completed() {
        grid_=multi_.grid; shown_=active_;
        last_remote_=unsigned(multi_.stats.remote-remote_before_);
    }
public:
    // Called once after successful physical provisioning. No active radio
    // session may be replaced with this function; session identities are fresh.
    void configure(unsigned nodes,uint32_t epoch0,uint32_t epoch1) {
        multi_=Multi(nodes,epoch0,epoch1); started_=false;
    }
    void xy(uint16_t x,uint16_t y) { next_.x=x; next_.y=y; }
    void gate(bool on) { gate_=on; }
    void remote(bool on) { multi_.remote(on); }
    bool remote_enabled() const { return multi_.remote_enabled(); }
    bool pending() const { return multi_.active(); }
    const Grid& grid() const { return grid_; }
    // Aggregate presentation metadata only; never serialize this 1024-particle Job.
    const Job& shown() const { return shown_; }
    unsigned remote_cohorts() const { return last_remote_; }
    Completion last_completion() const { return last_remote_==2?Completion::remote:Completion::local; }
    const char* mode() const { return last_remote_==2?"REMOTE 2/2":(last_remote_==1?"MIXED 1/2":"LOCAL 0/2"); }
    const MultiStats& stats() const { return multi_.stats; }
    const LinkStats& link_stats(unsigned slot) const { return multi_.link_stats(slot); }
    bool ready(unsigned slot) const { return multi_.ready(slot); }
    uint32_t sound() const { return controls(grid_,gate_); }
    void receive(unsigned slot,const Frame& f,uint32_t now) {
        const bool was=multi_.active();
        multi_.receive(slot,f,now);
        if(was&&!multi_.active()) completed();
    }
    template<class Send> void tick(uint32_t now,Send& send) {
        auto route=[&](unsigned slot,const Frame& f){ return multi_.remote_enabled()&&send(slot,f); };
        const bool was=multi_.active();
        multi_.tick(now,route);
        if(was&&!multi_.active()) completed();
        // Two 60-ms per-cohort budgets; no catch-up bursts or overlapping batches.
        if(!multi_.active()&&(!started_||uint32_t(now-last_)>=250)) {
            active_=next_;
            remote_before_=multi_.stats.remote;
            if(multi_.begin(cohorts(active_.seed,active_.x,active_.y,active_.steps),60)) {
                started_=true; last_=now; ++next_.seed;
                multi_.tick(now,route);
                if(!multi_.active()) completed();
            }
        }
    }
};
} // namespace cluster::av
